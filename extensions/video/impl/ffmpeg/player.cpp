/**
 * @file player.cpp
 * @brief FFmpeg 媒体播放器（V1 批 4）— MediaPlayer 实现
 *
 * 结构（音频预留）：会话线程内 MediaSession（av_read_frame → 按流类型分发
 * packet：视频 → 解码 → 时钟对表 → 私有流推帧；音频 → 丢弃〔NullAudioSink
 * 预留，hasAudio 真实上报〕）。控制命令（play/pause/seek/setLoop/setMuted）
 * 经邮箱由会话线程串行消费；事件经主线程队列投递回 UI 线程（onEvent 契约）。
 * 时钟为 IMediaClock 形态的 SystemClock（steady 主时钟）——音频子系统接入时
 * 换 AudioClock 即音画同步，本文件其余不动。
 * 显示通路：解码帧写入私有 VideoStream（与 stream:// 推流同一条 GPU 路径），
 * 本实现不触碰任何渲染对象。
 * 像素映射：AV_PIX_FMT_YUV420P/NV12/NV21/YUYV422/UYVY422 直拷（零转换），
 * 其余格式经 swscale 兜底转 NV12。
 */
module;
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

// FFmpeg 为 C 库：头文件不自带 extern "C"，C++ 侧须包裹（否则按 C++ 修饰名链接失败）
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

module kwik.ext.video;

import kwik.render.video_stream;    // VideoStream / PlaneView
import kwik.engine.channel;         // Channel::getMainThreadQueue（事件回 UI 线程）
import kwik.core.log;

import std;

namespace {

// ── SystemClock — steady 主时钟（音频预留：换 AudioClock 即音画同步）──
// 播放段：now = offset + (steady - start)；暂停：now = offset
class SystemClock {
public:
    double now() const {
        std::lock_guard<std::mutex> lk(m_);
        if (!running_) return offset_;
        return offset_ + std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }
    void play() {
        std::lock_guard<std::mutex> lk(m_);
        if (running_) return;
        start_ = std::chrono::steady_clock::now();
        running_ = true;
    }
    void pause() {
        std::lock_guard<std::mutex> lk(m_);
        if (!running_) return;
        offset_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
        running_ = false;
    }
    void seekTo(double t) {
        std::lock_guard<std::mutex> lk(m_);
        offset_ = t < 0 ? 0 : t;
        start_ = std::chrono::steady_clock::now();
    }

private:
    mutable std::mutex m_;
    bool running_ = false;
    double offset_ = 0;
    std::chrono::steady_clock::time_point start_{};
};

// ── 控制命令邮箱（UI 线程生产 / 会话线程串行消费）──
struct SessionCmd {
    enum class Kind : uint8_t { Play, Pause, Seek, Stop } kind;
    double seekTo = 0;
};

// ═══════════════════════════════════════════════════════════
// FfmpegPlayer
// ═══════════════════════════════════════════════════════════
class FfmpegPlayer final : public MediaPlayer {
public:
    FfmpegPlayer() = default;
    ~FfmpegPlayer() override { stopSession(); }

    bool open(std::string url) override {
        stopSession();    // 重复 open（换源）先停旧会话
        {
            std::lock_guard<std::mutex> lk(cmdMtx_);
            cmds_.clear();
        }
        url_ = std::move(url);
        quit_ = false;
        paused_ = true;
        ended_ = false;
        duration_ = 0;
        thread_ = std::thread([this] { session(); });
        return true;    // 异步：成败经 Loaded/Error 事件
    }

    void play() override { push(SessionCmd{SessionCmd::Kind::Play}); }
    void pause() override { push(SessionCmd{SessionCmd::Kind::Pause}); }
    void seek(double seconds) override { push(SessionCmd{SessionCmd::Kind::Seek, seconds}); }
    void setLoop(bool loop) override { loop_ = loop; }
    void setMuted(bool muted) override { muted_ = muted; }    // 音频子系统未接入：仅记录（预留）

    std::shared_ptr<VideoStream> stream() override { return stream_; }
    double currentTime() const override { return clock_.now(); }
    double duration() const override { return duration_.load(); }
    bool isPaused() const override { return paused_.load(); }

private:
    std::shared_ptr<VideoStream> stream_ = std::make_shared<VideoStream>(3);
    SystemClock clock_;
    std::thread thread_;
    mutable std::mutex cmdMtx_;    // 命令邮箱 + onEvent 快照锁（hasCmds 为 const 查询）
    std::deque<SessionCmd> cmds_;
    std::atomic<bool> quit_{false}, paused_{true}, ended_{false}, loop_{false}, muted_{false};
    std::atomic<double> duration_{0};
    std::string url_;

    void push(SessionCmd c) {
        std::lock_guard<std::mutex> lk(cmdMtx_);
        cmds_.push_back(c);
    }
    // 取一条命令（无则 Invalid）；Stop 置 quit
    SessionCmd poll() {
        std::lock_guard<std::mutex> lk(cmdMtx_);
        if (cmds_.empty()) return SessionCmd{SessionCmd::Kind::Pause};    // 借 Pause 作"无命令"标记
        auto c = cmds_.front();
        cmds_.pop_front();
        return c;
    }
    bool hasCmds() const {
        std::lock_guard<std::mutex> lk(cmdMtx_);
        return !cmds_.empty();
    }
    void stopSession() {
        if (thread_.joinable()) {
            quit_ = true;
            thread_.join();
        }
    }
    // 事件投递：会话线程 → 主线程队列（onEvent 契约：UI 线程回调）。
    // onEvent 可能被 UI 线程并发改写——锁内取快照再投。headless/无 Channel
    // 初始化时队列不可用：直接丢弃（无消费者可投，解引用空指针会崩）
    void post(MediaEvent e) {
        if (!Channel::hasMainThreadQueue()) return;
        std::function<void(const MediaEvent &)> f;
        {
            std::lock_guard<std::mutex> lk(cmdMtx_);    // 与命令邮箱共用锁（低频路径）
            f = onEvent;
        }
        if (!f) return;
        Channel::getMainThreadQueue().post([f, e = std::move(e)]() mutable { f(e); });
    }
    void postError(const char *what, int err) {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(err, buf, sizeof(buf));
        post(MediaEvent{.kind = MediaEvent::Kind::Error, .message = std::string(what) + ": " + buf});
    }

    // ── 会话线程主体：打开 → 解复用/解码/时钟对表 → 命令循环 ──
    void session() {
        AVFormatContext *fmt = nullptr;
        int err = avformat_open_input(&fmt, url_.c_str(), nullptr, nullptr);
        if (err < 0 || !fmt) {
            postError("avformat_open_input", err);
            return;
        }
        if ((err = avformat_find_stream_info(fmt, nullptr)) < 0) {
            postError("avformat_find_stream_info", err);
            avformat_close_input(&fmt);
            return;
        }
        const int vidx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (vidx < 0) {
            post(MediaEvent{.kind = MediaEvent::Kind::Error, .message = "no video stream"});
            avformat_close_input(&fmt);
            return;
        }
        AVStream *vs = fmt->streams[vidx];
        bool hasAudio = false;    // 音频轨真实上报（音频子系统接入后经 sink 消费）
        for (unsigned i = 0; i < fmt->nb_streams; ++i)
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) hasAudio = true;

        // 解码器打开
        const AVCodec *codec = avcodec_find_decoder(vs->codecpar->codec_id);
        AVCodecContext *cdc = codec ? avcodec_alloc_context3(codec) : nullptr;
        if (!cdc || (err = avcodec_parameters_to_context(cdc, vs->codecpar)) < 0
            || (err = avcodec_open2(cdc, codec, nullptr)) < 0) {
            postError("avcodec_open2", err);
            if (cdc) avcodec_free_context(&cdc);
            avformat_close_input(&fmt);
            return;
        }
        duration_ = fmt->duration > 0 ? double(fmt->duration) / AV_TIME_BASE
                                      : vs->duration > 0 ? double(vs->duration) * av_q2d(vs->time_base) : 0;
        const double fps = av_q2d(av_guess_frame_rate(fmt, vs, nullptr));
        post(MediaEvent{.kind = MediaEvent::Kind::Loaded,
                        .info = {.duration = duration_.load(),
                                 .width = unsigned(cdc->width),
                                 .height = unsigned(cdc->height),
                                 .fps = fps > 0 && std::isfinite(fps) ? fps : 0,
                                 .hasAudio = hasAudio}});
        Log::info("[player] open '{}' {}x{} {}s {}fps audio={}", url_, cdc->width, cdc->height, duration_.load(),
                  fps, hasAudio);

        AVPacket *pkt = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        SwsContext *sws = nullptr;    // 懒建：仅当源格式需兜底转换
        bool playing = false, eof = false, firstFrame = false;
        double lastPost = -1;
        auto pump = [&]() -> bool { return av_read_frame(fmt, pkt) >= 0; };    // 读一包

        // 主循环：命令优先；播放中逐包解码推送；EOF/暂停空转 20ms 轮询命令
        while (!quit_) {
            // ① 命令消费（一次性取空）
            while (hasCmds()) {
                SessionCmd c = poll();
                switch (c.kind) {
                case SessionCmd::Kind::Play:
                    if (eof) {    // 播完后再播：回到起点
                        av_seek_frame(fmt, vidx, 0, AVSEEK_FLAG_BACKWARD);
                        avcodec_flush_buffers(cdc);
                        eof = false;
                        clock_.seekTo(0);
                    }
                    clock_.play();
                    playing = true;
                    paused_ = false;
                    break;
                case SessionCmd::Kind::Pause:
                    clock_.pause();
                    playing = false;
                    paused_ = true;
                    break;
                case SessionCmd::Kind::Seek:
                    if (av_seek_frame(fmt, vidx, int64_t(c.seekTo / av_q2d(vs->time_base)), AVSEEK_FLAG_BACKWARD)
                        >= 0) {
                        avcodec_flush_buffers(cdc);
                        eof = false;
                        clock_.seekTo(c.seekTo);
                        lastPost = -1;    // 触发即时 TimeUpdate
                    }
                    break;
                case SessionCmd::Kind::Stop: quit_ = true; break;
                }
            }
            if (quit_) break;

            // ② 播放中：解复用 → 解码 → 时钟对表推帧
            if (playing && !eof) {
                if (!pump()) {    // EOF：循环回起点，否则报 End 进入暂停
                    if (loop_) {
                        av_seek_frame(fmt, vidx, 0, AVSEEK_FLAG_BACKWARD);
                        avcodec_flush_buffers(cdc);
                        clock_.seekTo(0);
                        continue;
                    }
                    eof = true;
                    playing = false;
                    paused_ = true;
                    clock_.pause();
                    clock_.seekTo(duration_.load());
                    post(MediaEvent{.kind = MediaEvent::Kind::End});
                    continue;
                }
                if (pkt->stream_index == vidx) {
                    avcodec_send_packet(cdc, pkt);
                    while (!quit_ && avcodec_receive_frame(cdc, frame) >= 0) {
                        const double pts = (frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp
                                                                                           : frame->pts)
                                           * av_q2d(vs->time_base);
                        const double now = clock_.now();
                        if (pts < now - 0.15) continue;    // 迟到帧丢弃（seek 余帧/慢解追帧）
                        // 到点前小睡并穿插命令（超前 > 40ms 等时钟）
                        while (!quit_ && playing && pts > clock_.now() + 0.04) {
                            if (hasCmds()) break;
                            std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        }
                        if (quit_ || !playing) break;
                        pushFrame(frame, &sws);
                        if (!firstFrame) {
                            firstFrame = true;
                            post(MediaEvent{.kind = MediaEvent::Kind::Playing});
                        }
                        if (double t = clock_.now(); t - lastPost >= 0.25) {    // TimeUpdate 4Hz
                            lastPost = t;
                            post(MediaEvent{.kind = MediaEvent::Kind::TimeUpdate, .currentTime = t});
                        }
                    }
                }
                av_packet_unref(pkt);    // 音频包在此丢弃（NullAudioSink 预留：接入后改投 sink）
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));    // 暂停/播完：低速轮询命令
        }

        if (sws) sws_freeContext(sws);
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&cdc);
        avformat_close_input(&fmt);
    }

    // ── 帧出口：AVFrame → 私有流（直拷或 sws 兜底 NV12）──
    void pushFrame(AVFrame *f, SwsContext **sws) {
        // 像素格式映射：可直拷的五族之外（10bit/P010/RGB 等）转 NV12
        PixelFormat pf = PixelFormat::RGBA8;
        bool direct = true;
        switch (f->format) {
        case AV_PIX_FMT_YUV420P: pf = PixelFormat::I420; break;
        case AV_PIX_FMT_NV12: pf = PixelFormat::NV12; break;
        case AV_PIX_FMT_NV21: pf = PixelFormat::NV21; break;
        case AV_PIX_FMT_YUYV422: pf = PixelFormat::YUYV; break;
        case AV_PIX_FMT_UYVY422: pf = PixelFormat::UYVY; break;
        default: pf = PixelFormat::NV12; direct = false; break;
        }
        const uint32_t w = unsigned(f->width), h = unsigned(f->height);
        auto wr = stream_->acquire({.format = pf, .width = w, .height = h});
        if (!wr) return;    // 池满丢帧（显示端跟不上解码节奏）
        auto planes = wr.planes();

        if (direct) {
            // 逐平面按行拷贝（src linesize → 池 stride）
            for (uint32_t p = 0; p < planes.size(); ++p) {
                const PlaneView &dv = planes[p];
                const uint8_t *src = f->data[p];
                const int ls = f->linesize[p];
                if (!src) continue;
                const uint32_t rowBytes = std::min<uint32_t>(dv.stride, uint32_t(ls < 0 ? -ls : ls));
                for (uint32_t y = 0; y < dv.height; ++y)
                    std::memcpy(dv.data + size_t(dv.stride) * y, src + size_t(ls) * y, rowBytes);
            }
        } else {
            // swscale 兜底 → NV12（换格式即换源，cached context 复用）
            *sws = sws_getCachedContext(*sws, int(w), int(h), AVPixelFormat(f->format), int(w), int(h),
                                        AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!*sws) return;    // 转换器建失败：丢帧
            uint8_t *dst[2] = {planes[0].data, planes[1].data};    // NV12 两平面
            const int dstStride[2] = {int(planes[0].stride), int(planes[1].stride)};
            sws_scale(*sws, f->data, f->linesize, 0, int(h), dst, dstStride);
        }
        wr.commit();
    }
};

}    // namespace

/** @brief FFmpeg 播放器注册（bindings.cpp 的 registerVideoElement 在编入时调用） */
void registerFfmpegPlayer() {
    registerMediaPlayerFactory([] { return std::make_unique<FfmpegPlayer>(); });
}
