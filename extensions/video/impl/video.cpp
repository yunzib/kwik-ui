/** @brief Video 元素实现 (引擎中立, 不 import kwik.engine.*)。 */
module;
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED
#include <stdint.h>

module kwik.ext.video;

import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.texture_manager;
import kwik.render.backend;         // RenderBackend（纹理按本树后端路由）
import kwik.render.video_stream;    // VideoStream / findVideoStream（两模式共用显示通路）
import kwik.element.image;          // Image::resolveFit（fit 求解与 Image 同源）
import kwik.core.timer;             // CoreTimer（StreamPoller 4ms 轮询）

import std;

// ═══════════════════════════════════════════════════════════
// 播放器工厂钩子 (Meyer's Singleton, 与 lazyListSourceFactory 同构)
// ═══════════════════════════════════════════════════════════
namespace {
MediaPlayerFactory &playerFactory() {
    static MediaPlayerFactory f;
    return f;
}
}    // namespace

void registerMediaPlayerFactory(MediaPlayerFactory f) {
    playerFactory() = std::move(f);
}
const MediaPlayerFactory &mediaPlayerFactory() {
    return playerFactory();
}

// ═══════════════════════════════════════════════════════════
// Null 播放器 — 无外部依赖兜底（KWIK_VIDEO_FFMPEG=OFF / 工厂未注册）。
// 推一帧静态图进私有流 + 立即 Loaded/End 事件：管线（打开→事件→流显示）全链
// 可验证，只是画面不动——与 FFmpeg 播放器同一条显示通路
// ═══════════════════════════════════════════════════════════
namespace {
class NullMediaPlayer final : public MediaPlayer {
public:
    bool open(std::string url) override {
        url_ = std::move(url);
        // 私有流推一帧 480×270 深灰 + 中部亮带（静态"画面"）
        constexpr std::uint32_t w = 480, h = 270;
        auto wr = stream_->acquire({.format = PixelFormat::RGBA8, .width = w, .height = h});
        if (wr) {
            const PlaneView &p = wr.planes()[0];
            for (std::uint32_t y = 0; y < h; ++y) {
                std::uint8_t *row = p.data + std::size_t(p.stride) * y;
                for (std::uint32_t x = 0; x < w; ++x) {
                    const std::uint8_t v = (y > h / 2 - 8 && y < h / 2 + 8) ? 90 : 26;
                    row[x * 4] = row[x * 4 + 1] = row[x * 4 + 2] = v;
                    row[x * 4 + 3] = 255;
                }
            }
            wr.commit();
        }
        post(MediaEvent{.kind = MediaEvent::Kind::Loaded,
                        .info = {.duration = 1.0, .width = w, .height = h, .fps = 1, .hasAudio = false}});
        return true;
    }
    void play() override { playing_ = true; post(MediaEvent{.kind = MediaEvent::Kind::Playing}); }
    void pause() override { playing_ = false; }
    void seek(double s) override { time_ = s; }
    void setLoop(bool) override {}
    void setMuted(bool) override {}
    std::shared_ptr<VideoStream> stream() override { return stream_; }
    double currentTime() const override { return time_; }
    double duration() const override { return 1.0; }
    bool isPaused() const override { return !playing_; }

private:
    void post(MediaEvent e) {
        if (onEvent) onEvent(e);    // Null 无工作线程：调用即 UI 线程，直投
    }
    std::shared_ptr<VideoStream> stream_ = std::make_shared<VideoStream>(2);
    bool playing_ = false;
    double time_ = 0;
    std::string url_;
};
}    // namespace

// ═══════════════════════════════════════════════════════════
// StreamPoller — 全部流显示 Video 共用一个 4ms 定时器
// （与主循环空闲休眠同粒度，新帧显示延迟 ≤ 4ms）。逐个读原子 seq，变了
// 才标脏；未找到流的同时重查注册表。首个注册启动，末个注销停止。
// 仅 UI 线程访问（CoreTimer 在主循环 tick 内回调）
// ═══════════════════════════════════════════════════════════
namespace {
struct StreamPoller {
    std::vector<Video *> list;
    CoreTimer::Id timer = 0;

    static StreamPoller &get() {
        static StreamPoller p;
        return p;
    }
    void add(Video *v) {
        if (std::find(list.begin(), list.end(), v) != list.end()) return;
        list.push_back(v);
        if (timer == 0) timer = CoreTimer::setInterval(4, [] { StreamPoller::get().tick(); });
    }
    void remove(Video *v) {
        std::erase(list, v);
        if (list.empty() && timer != 0) {
            CoreTimer::clear(timer);
            timer = 0;
        }
    }
    void tick() {
        auto snapshot = list;    // 快照遍历：回调期间注销不致迭代器失效
        for (Video *v : snapshot) v->pollStream();
    }
};
}    // namespace

// ═══════════════════════════════════════════════════════════
// Video 实现
// ═══════════════════════════════════════════════════════════
Video::Video(ViewProps vp, VideoProps lp) : View(std::move(vp)), video_(std::move(lp)) {
    alive_ = std::make_shared<std::atomic<bool>>(true);
    // 播放器惰性创建（ensureOpened）：流模式（stream://）全程不建，
    // 文件源首次绘制/play 时按工厂创建（未注册回退 Null 播放器）
}

Video::~Video() {
    alive_->store(false);    // 在途事件转发不再解引用 this（投递方持 alive 副本检查）
    unbindStream();          // 注销轮询 + 释放流纹理别名（析构前 this 仍有效）
    player_.reset();         // 播放器析构：停会话线程 + join
}

void Video::ensureOpened() {
    if (opened_ || !streamNameOf(video_.src).empty()) return;    // 流源不走播放器
    if (!player_) {
        const auto &f = mediaPlayerFactory();
        player_ = f ? f() : std::make_unique<NullMediaPlayer>();
        // 事件桥：播放器保证 onEvent 在 UI 线程回调；alive 防析构后在途解引用
        player_->onEvent = [this, alive = alive_](const MediaEvent &e) {
            if (!alive->load()) return;
            onPlayerEvent(e);
        };
    }
    player_->setLoop(video_.loop);
    player_->setMuted(video_.muted);
    opened_ = player_->open(video_.src);
    // autoplay 直调播放器（不经 play()：open 失败时 play→ensureOpened→play 会无限递归）
    if (opened_ && video_.autoplay) {
        player_->play();
        paused_ = false;
    }
}

void Video::onPlayerEvent(const MediaEvent &e) {
    using K = MediaEvent::Kind;
    switch (e.kind) {
    case K::Loaded:
        duration_ = e.info.duration;
        videoW_ = e.info.width;
        videoH_ = e.info.height;
        break;
    case K::TimeUpdate: curTime_ = e.currentTime; break;
    case K::End: paused_ = true; break;
    case K::Playing: paused_ = false; buffering_ = false; break;
    case K::Buffering: buffering_ = true; break;
    case K::Error: paused_ = true; break;
    }
    if (onMediaEvent) onMediaEvent(e);
    markDirty();    // 状态变化（时间文本等）随事件刷新
}

void Video::play() {
    ensureOpened();    // 惰性播放器：先建再播
    if (!player_) return;
    player_->play();
    paused_ = false;
    markDirty();
}

void Video::pause() {
    if (!player_) return;
    player_->pause();
    paused_ = true;
    markDirty();
}

void Video::seek(float t) {
    if (!player_) return;
    player_->seek(t);
    curTime_ = t;
    markDirty();
}

bool Video::setPropertyTyped(const char *name, const TypedProp &value) {
    std::string_view n(name);
    if (n == "src") {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (*s != video_.src) {
                unbindStream();     // 流名/源变更：释放旧绑定；文件源播放器随解绑一并停播
                player_.reset();
                opened_ = false;    // 下次绘制按新源重新 open
                curTime_ = duration_ = 0;
                paused_ = true;
                videoW_ = videoH_ = 0;
            }
            video_.src = *s;
            markDirty();
            return true;
        }
        return false;
    }
    if (n == "fit") {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (*s == "fill") video_.fit = ImageFit::Fill;
            else if (*s == "contain") video_.fit = ImageFit::Contain;
            else if (*s == "cover") video_.fit = ImageFit::Cover;
            else if (*s == "none") video_.fit = ImageFit::None;
            else return false;    // 未知串不消费（不静默落默认值）
            markDirty();
            return true;
        }
        return false;
    }
    if (n == "autoplay") {
        if (auto b = typedToBool(value)) { video_.autoplay = *b; markDirty(); return true; }
        return false;
    }
    if (n == "loop") {
        if (auto b = typedToBool(value)) {
            video_.loop = *b;
            if (player_) player_->setLoop(video_.loop);
            return true;
        }
        return false;
    }
    if (n == "muted") {
        if (auto b = typedToBool(value)) {
            video_.muted = *b;
            if (player_) player_->setMuted(video_.muted);
            return true;
        }
        return false;
    }
    // 播放控制命令面（PropBus setProp）：paused=false 播放 / true 暂停；currentTime=秒 seek
    if (n == "paused") {
        if (auto b = typedToBool(value)) {
            *b ? pause() : play();
            return true;
        }
        return false;
    }
    if (n == "currentTime") {
        double t = 0;
        if (auto *d = std::get_if<double>(&value)) t = *d;
        else if (auto *i = std::get_if<std::int64_t>(&value)) t = double(*i);
        else return false;
        seek(float(t));
        return true;
    }
    return View::setPropertyTyped(name, value);
}

// ── 属性读取：专有属性 + 两模式只读读数（getProp 命令式读出）──
std::string Video::getProperty(const char *name) const {
    const std::string_view n(name);
    if (n == "src") return video_.src;
    if (n == "autoplay") return video_.autoplay ? "true" : "false";
    if (n == "loop") return video_.loop ? "true" : "false";
    if (n == "muted") return video_.muted ? "true" : "false";
    if (n == "fit") {
        switch (video_.fit) {
        case ImageFit::Fill: return "fill";
        case ImageFit::Contain: return "contain";
        case ImageFit::Cover: return "cover";
        case ImageFit::None: return "none";
        }
    }
    // 流读数（两模式通用）：未绑定流一律 0
    if (n == "fps") return std::format("{:.1f}", stream_ ? stream_->fps() : 0.0f);
    if (n == "droppedFrames") return std::to_string(stream_ ? stream_->droppedFrames() : 0);
    if (n == "frameWidth") return std::to_string(stream_ ? stream_->desc().width : 0);
    if (n == "frameHeight") return std::to_string(stream_ ? stream_->desc().height : 0);
    // 播放读数（文件模式；缓存自事件，UI 线程一致）
    if (n == "currentTime") return std::format("{:.3f}", player_ ? player_->currentTime() : 0.0);
    if (n == "duration") return std::format("{:.3f}", duration_);
    if (n == "paused") return paused_ ? "true" : "false";
    if (n == "buffering") return buffering_ ? "true" : "false";
    if (n == "videoWidth") return std::to_string(videoW_);
    if (n == "videoHeight") return std::to_string(videoH_);
    return View::getProperty(name);
}

PropType Video::readOnlyPropType(const char *name) const {
    const std::string_view n(name);
    if (n == "fps" || n == "currentTime" || n == "duration") return PropType::Float;
    if (n == "droppedFrames" || n == "frameWidth" || n == "frameHeight") return PropType::Int;
    if (n == "videoWidth" || n == "videoHeight") return PropType::Int;
    if (n == "paused" || n == "buffering") return PropType::Bool;
    return View::readOnlyPropType(name);
}

void Video::applyVideoProps(VideoProps p) {
    // 此前无条件 opened_=false：任意 State 变更 → 整树 reconcile → 每轮重新
    // open（接真实后端即"点任意按钮视频重头播"）。现仅 src 变化才重开/重绑
    const bool srcChanged = p.src != video_.src;
    const bool loopChanged = p.loop != video_.loop;
    const bool mutedChanged = p.muted != video_.muted;
    video_ = std::move(p);
    if (srcChanged) {
        unbindStream();     // 流名变更：释放旧绑定；文件源播放器随解绑停播
        player_.reset();
        opened_ = false;    // 下次绘制按新源重新 open
        curTime_ = duration_ = 0;
        paused_ = true;
        videoW_ = videoH_ = 0;
    } else if (player_) {
        if (loopChanged) player_->setLoop(video_.loop);       // 原位下发，不重开
        if (mutedChanged) player_->setMuted(video_.muted);
    }
    markDirty();
}

Size Video::onMeasure(Constraints c) {
    float w = props.width.value_or(480.0f);
    float h = props.height.value_or(270.0f);
    return c.constrain({w, h});
}

void Video::onDraw(Graphics &g) {
    View::onDraw(g);    // 背景 + 边框
    if (streamNameOf(video_.src).empty()) {
        // 文件源：确保播放器打开；绑定其私有流
        ensureOpened();
        bindStream(player_ ? player_->stream() : nullptr);
    } else {
        // 命名流：按名查找/等建（先声明后建流由轮询重查）——幂等
        bindStream(nullptr);
    }
    drawStreamFrame(g);
}

// ═══════════════════════════════════════════════════════════
// 流显示（stream:// 命名流 / 文件源播放器私有流 — 同一条通路）
// ═══════════════════════════════════════════════════════════
std::string_view Video::streamNameOf(std::string_view src) {
    constexpr std::string_view kScheme = "stream://";
    return src.starts_with(kScheme) ? src.substr(kScheme.size()) : std::string_view{};
}

void Video::bindStream(const std::shared_ptr<VideoStream> &target) {
    // 先注册轮询（流未就绪时由轮询周期重查/重绑）
    if (!polling_) {
        StreamPoller::get().add(this);
        polling_ = true;
    }
    if (streamNameOf(video_.src).empty()) {
        // 文件源：绑定播放器私有流（每次 open 都是同一流对象实例）
        if (stream_ != target) {
            releaseAlias();
            stream_ = target;
        }
    } else {
        const std::string name(streamNameOf(video_.src));
        if (name != boundName_) releaseAlias();    // 名字变更兜底（setProp/reconcile 已先行解绑）
        boundName_ = name;
        if (!stream_) stream_ = findVideoStream(boundName_);
    }
    if (stream_ && streamTexId_ == 0) {
        // 别名 id 同步返回、本帧即可录制引用；后端按流共享 GPU 资源（同流多 Video 只上传一份）
        streamTexId_ = TextureManager::instance().createStreamTexture(
            static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), stream_);
    }
}

void Video::releaseAlias() {
    if (streamTexId_ != 0) {
        // 别名计数 -1；归零由后端延后一帧退役（同帧被他处重绑即复活）
        TextureManager::instance().destroyTexture(
            static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), streamTexId_);
        streamTexId_ = 0;
    }
    stream_.reset();
}

void Video::unbindStream() {
    if (polling_) {
        StreamPoller::get().remove(this);
        polling_ = false;
    }
    releaseAlias();
    boundName_.clear();
    lastSeenSeq_ = 0;
    pendingDirty_ = false;
}

void Video::pollStream() {
    if (!stream_) {
        // 命名流尚未创建（或已除名后重建）：重查注册表，命中即标脏——下次绘制完成绑定
        if (!boundName_.empty() && !pendingDirty_ && findVideoStream(boundName_)) {
            pendingDirty_ = true;
            markDirty();
        }
        return;
    }
    const std::uint64_t s = stream_->seq();
    if (s != lastSeenSeq_) {
        lastSeenSeq_ = s;
        // 已标脏未绘制（被裁剪/滚出视口）不重复标脏：不可见的 Video 不产生空帧
        if (!pendingDirty_) {
            pendingDirty_ = true;
            markDirty();
        }
    }
}

void Video::drawStreamFrame(Graphics &g) {
    pendingDirty_ = false;    // 本次绘制消费掉轮询标脏
    if (!stream_ || streamTexId_ == 0 || stream_->seq() == 0) {
        g.drawRect(frame, Color(0x1a, 0x1a, 0x1a));    // 流未建 / 尚无帧：深灰占位
        return;
    }
    const FrameDesc d = stream_->desc();
    // 触碰：通知后端本帧将绘制该流（帧首据此上传新帧；未录制的流不上传）
    TextureManager::instance().touchStreamTexture(static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)),
                                                  streamTexId_);
    // fit 求解与 Image 同源（contain 留边由 View 背景填充；None 超框按圆角裁剪）
    auto sol = Image::resolveFit(video_.fit, frame, static_cast<float>(d.width), static_cast<float>(d.height));
    if (video_.fit == ImageFit::None) {
        g.clipRoundedRect(frame, props.borderRadius);
        g.drawImageUv(streamTexId_, sol.dst, sol.u0, sol.v0, sol.u1, sol.v1, props.opacity, props.borderRadius);
        g.resetClip();
    } else {
        g.drawImageUv(streamTexId_, sol.dst, sol.u0, sol.v0, sol.u1, sol.v1, props.opacity, props.borderRadius);
    }
}
