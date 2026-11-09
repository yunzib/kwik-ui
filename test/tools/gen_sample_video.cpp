// gen_sample_video.cpp — 生成 test/assets/sample.mp4（Video 播放 demo 用示例视频）
//
// 主构建的 FFmpeg 只编解码器（最小 LGPL 集），本工具需要一份带 mpeg4 编码器 +
// mp4 封装器的 FFmpeg 构建，仅生成素材时手工编一次，不进主构建：
//   configure ... --disable-everything --enable-encoder=mpeg4 --enable-muxer=mp4
//                 --enable-protocol=file --enable-swscale（其余同 Extension.cmake）
//   clang++ -std=c++20 gen_sample_video.cpp -I<ffbuild> -I<ffsrc> \
//       <ffbuild>/libavformat/libavformat.a <ffbuild>/libavcodec/libavcodec.a \
//       <ffbuild>/libswscale/libswscale.a <ffbuild>/libavutil/libavutil.a -lbcrypt -lws2_32
//   ./gen_sample_video test/assets/sample.mp4
// 内容：640×360 / 30fps / 6 秒——8 色竖条 + 底部灰阶 + 移动反色竖线 + 进度条
// （进度条长度 ∝ 时间，seek/循环/播放进度目视可辨）
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
}
#include <cstdio>

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "sample.mp4";
    constexpr int W = 640, H = 360, FPS = 30, SECONDS = 6;
    AVFormatContext *fmt = nullptr;
    if (avformat_alloc_output_context2(&fmt, nullptr, "mp4", out) < 0) return 1;
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    AVStream *st = avformat_new_stream(fmt, nullptr);
    AVCodecContext *c = avcodec_alloc_context3(codec);
    c->width = W;
    c->height = H;
    c->time_base = {1, FPS};
    c->framerate = {FPS, 1};
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->gop_size = FPS;    // 每秒一个关键帧（seek 粒度）
    c->bit_rate = 300000;
    if (fmt->oformat->flags & AVFMT_GLOBALHEADER) c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (avcodec_open2(c, codec, nullptr) < 0) return 2;
    avcodec_parameters_from_context(st->codecpar, c);
    st->time_base = c->time_base;
    if (avio_open(&fmt->pb, out, AVIO_FLAG_WRITE) < 0) return 3;
    if (avformat_write_header(fmt, nullptr) < 0) return 4;

    AVFrame *f = av_frame_alloc();
    f->format = c->pix_fmt;
    f->width = W;
    f->height = H;
    av_frame_get_buffer(f, 0);
    AVPacket *pkt = av_packet_alloc();
    // BT.601 limited（mpeg4 默认）RGB → Y'CbCr
    auto yuv = [](int r, int g, int b, int &y, int &u, int &v) {
        y = 16 + (65 * r + 129 * g + 25 * b) / 256;
        u = 128 + (-38 * r - 74 * g + 112 * b) / 256;
        v = 128 + (112 * r - 94 * g - 18 * b) / 256;
    };
    static const int bars[8][3] = {{235, 235, 235}, {235, 235, 16}, {16, 235, 235}, {16, 235, 16},
                                   {235, 16, 235},  {235, 16, 16},  {16, 16, 235},  {16, 16, 16}};
    auto flush = [&](AVFrame *in) {
        avcodec_send_frame(c, in);
        while (avcodec_receive_packet(c, pkt) == 0) {
            av_packet_rescale_ts(pkt, c->time_base, st->time_base);
            pkt->stream_index = st->index;
            av_interleaved_write_frame(fmt, pkt);
        }
    };
    for (int i = 0; i < FPS * SECONDS; ++i) {
        av_frame_make_writable(f);
        const int lineX = (i * 6) % W;
        const int progress = W * i / (FPS * SECONDS);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int r, g, b;
                if (y >= H - 12) {    // 底部进度条
                    r = g = b = x < progress ? 235 : 40;
                } else if (y * 3 < H * 2) {
                    r = bars[x * 8 / W][0], g = bars[x * 8 / W][1], b = bars[x * 8 / W][2];
                } else {
                    r = g = b = 16 + 219 * x / (W - 1);
                }
                if (x >= lineX && x < lineX + 4) r = 255 - r, g = 255 - g, b = 255 - b;
                int Y, U, V;
                yuv(r, g, b, Y, U, V);
                f->data[0][y * f->linesize[0] + x] = uint8_t(Y);
                if ((x | y) % 2 == 0) {
                    f->data[1][(y / 2) * f->linesize[1] + x / 2] = uint8_t(U);
                    f->data[2][(y / 2) * f->linesize[2] + x / 2] = uint8_t(V);
                }
            }
        f->pts = i;
        flush(f);
    }
    flush(nullptr);
    av_write_trailer(fmt);
    avio_closep(&fmt->pb);
    std::printf("wrote %s (%dx%d, %d frames)\n", out, W, H, FPS * SECONDS);
    return 0;
}
