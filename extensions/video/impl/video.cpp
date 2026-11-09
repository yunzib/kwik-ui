/** @brief Video 元素实现 (引擎中立, 不 import kwik.engine.*)。 */
module;
#include <stdint.h>

module kwik.ext.video;

import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.texture_manager;
import kwik.render.backend;         // RenderBackend（纹理按本树后端路由）
import kwik.render.video_stream;    // VideoStream / findVideoStream（流模式）
import kwik.element.image;          // Image::resolveFit（fit 求解与 Image 同源）
import kwik.core.timer;             // CoreTimer（StreamPoller 4ms 轮询）

import std;

// ═══════════════════════════════════════════════════════════
// 后端工厂钩子 (Meyer's Singleton, 与 lazyListSourceFactory 同构)
// ═══════════════════════════════════════════════════════════
namespace {
VideoBackendFactory &backendFactory() {
    static VideoBackendFactory f;
    return f;
}
}    // namespace

void registerVideoBackendFactory(VideoBackendFactory f) {
    backendFactory() = std::move(f);
}
const VideoBackendFactory &videoBackendFactory() {
    return backendFactory();
}

// ═══════════════════════════════════════════════════════════
// Null 后端 — 无外部依赖, 生成静态灰度帧用于验证管线
// ═══════════════════════════════════════════════════════════
namespace {
class NullVideoBackend final : public VideoBackend {
public:
    bool open(const std::string &) override { opened_ = true; return true; }
    void play() override { playing_ = true; }
    void pause() override { playing_ = false; }
    void seek(float s) override { position_ = s; }
    float position() const override { return position_; }
    float duration() const override { return duration_; }
    bool grabFrame(VideoFrame &out) override {
        if (!opened_ || frameEmitted_) return false;
        // 生成 480x270 深灰帧, 中间一条水平亮带以示"有画面"
        constexpr uint32_t w = 480, h = 270;
        out.width = w;
        out.height = h;
        out.rgba.resize(size_t(w) * h * 4);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                size_t i = (size_t(y) * w + x) * 4;
                uint8_t v = (y > h / 2 - 8 && y < h / 2 + 8) ? 90 : 26;
                out.rgba[i] = v;
                out.rgba[i + 1] = v;
                out.rgba[i + 2] = v;
                out.rgba[i + 3] = 255;
            }
        frameEmitted_ = true;
        return true;
    }
    void setLoop(bool) override {}
    void setMuted(bool) override {}
    void close() override { opened_ = false; }

private:
    bool opened_ = false, frameEmitted_ = false, playing_ = false;
    float position_ = 0.0f, duration_ = 0.0f;
};

// ═══════════════════════════════════════════════════════════
// StreamPoller — 全部流模式 Video 共用一个 4ms 定时器
// （与主循环空闲休眠同粒度，新帧显示延迟 ≤ 4ms）。逐个读原子 seq，变了
// 才标脏；未找到流的同时重查注册表。首个注册启动，末个注销停止。
// 仅 UI 线程访问（CoreTimer 在主循环 tick 内回调）
// ═══════════════════════════════════════════════════════════
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
    // 解码后端惰性创建（ensureOpened）：流模式（stream://）全程不建，
    // 文件源首次绘制/play 时按工厂创建（未注册回退 Null 后端）
}

Video::~Video() {
    unbindStream();    // 注销轮询 + 释放流纹理别名（析构前 this 仍有效）
    if (backend_) backend_->close();
    if (textureId_ != 0) {
        TextureManager::instance().destroyTexture(
            static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), textureId_);
    }
}

void Video::setBackend(std::unique_ptr<VideoBackend> backend) {
    if (backend_) backend_->close();
    backend_ = std::move(backend);
    opened_ = false;
    markDirty();
}

void Video::ensureOpened() {
    if (opened_ || !streamNameOf(video_.src).empty()) return;    // 流源不走解码后端
    if (!backend_) {
        const auto &f = videoBackendFactory();
        backend_ = f ? f() : std::make_unique<NullVideoBackend>();
    }
    backend_->setLoop(video_.loop);
    backend_->setMuted(video_.muted);
    opened_ = backend_->open(video_.src);
    // autoplay 直调后端（不经 play()：open 失败时 play→ensureOpened→play 会无限递归）
    if (opened_ && video_.autoplay) {
        backend_->play();
        playing_ = true;
    }
}

void Video::play() {
    ensureOpened();    // 惰性后端：先建再播
    if (!backend_) return;
    backend_->play();
    playing_ = true;
    markDirty();
}

void Video::pause() {
    if (!backend_) return;
    backend_->pause();
    playing_ = false;
    markDirty();
}

void Video::seek(float t) {
    if (!backend_) return;
    backend_->seek(t);
    position_ = t;
    markDirty();
}

bool Video::setPropertyTyped(const char *name, const TypedProp &value) {
    std::string_view n(name);
    if (n == "src") {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (*s != video_.src) {
                unbindStream();     // 流名变更：释放旧绑定，下次绘制按新名重绑
                opened_ = false;    // 文件源：下次绘制按新源重新 open
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
            if (backend_) backend_->setLoop(video_.loop);
            return true;
        }
        return false;
    }
    if (n == "muted") {
        if (auto b = typedToBool(value)) {
            video_.muted = *b;
            if (backend_) backend_->setMuted(video_.muted);
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

// ── 属性读取：专有属性 + 流模式只读读数（getProp 命令式读出）──
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
    // 只读读数：未绑定流（文件模式 / 流未建）一律 0
    if (n == "fps") return std::format("{:.1f}", stream_ ? stream_->fps() : 0.0f);
    if (n == "droppedFrames") return std::to_string(stream_ ? stream_->droppedFrames() : 0);
    if (n == "frameWidth") return std::to_string(stream_ ? stream_->desc().width : 0);
    if (n == "frameHeight") return std::to_string(stream_ ? stream_->desc().height : 0);
    return View::getProperty(name);
}

PropType Video::readOnlyPropType(const char *name) const {
    const std::string_view n(name);
    if (n == "fps") return PropType::Float;
    if (n == "droppedFrames" || n == "frameWidth" || n == "frameHeight") return PropType::Int;
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
        unbindStream();     // 流名变更：释放旧绑定
        opened_ = false;    // 文件源：下次绘制按新源重新 open
    } else if (backend_) {
        if (loopChanged) backend_->setLoop(video_.loop);       // 原位下发，不重开
        if (mutedChanged) backend_->setMuted(video_.muted);
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
    if (!streamNameOf(video_.src).empty()) {    // 流模式：宿主 C++ 推帧（V1）
        drawStreamFrame(g);
        return;
    }
    ensureOpened();

    // 拉取最新帧 → 上传纹理 (在渲染线程, 避免 Vulkan 线程竞态)
    VideoFrame f;
    if (backend_ && backend_->grabFrame(f) && f.width > 0 && f.height > 0) {
        // 尺寸变化 → 重建纹理
        auto *be = static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend));
        if (textureId_ != 0 && (texW_ != f.width || texH_ != f.height)) {
            TextureManager::instance().destroyTexture(be, textureId_);
            textureId_ = 0;
        }
        if (textureId_ == 0) {
            textureId_ = TextureManager::instance().createTexture(be, f.rgba.data(), f.width, f.height);
            texW_ = f.width;
            texH_ = f.height;
        }
    }

    if (textureId_ != 0) {
        g.drawImage(textureId_, frame, props.opacity, props.borderRadius);
    } else {
        // 无帧占位: 深灰底
        g.drawRect(frame, Color(0x1a, 0x1a, 0x1a));
    }
}

// ═══════════════════════════════════════════════════════════
// 流模式（src = "stream://名字"，V1 推流）
// ═══════════════════════════════════════════════════════════
std::string_view Video::streamNameOf(std::string_view src) {
    constexpr std::string_view kScheme = "stream://";
    return src.starts_with(kScheme) ? src.substr(kScheme.size()) : std::string_view{};
}

void Video::bindStream() {
    std::string_view name = streamNameOf(video_.src);
    if (name != boundName_) unbindStream();    // 名字变更兜底（setProp/reconcile 已先行解绑）
    boundName_ = std::string(name);
    // 先注册轮询再查流：流尚未创建时由轮询周期重查（先声明后建流顺序无关）
    if (!polling_) {
        StreamPoller::get().add(this);
        polling_ = true;
    }
    if (!stream_) stream_ = findVideoStream(boundName_);
    if (stream_ && streamTexId_ == 0) {
        // 别名 id 同步返回、本帧即可录制引用；后端按流共享 GPU 资源（同流多 Video 只上传一份）
        streamTexId_ = TextureManager::instance().createStreamTexture(
            static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), stream_);
    }
}

void Video::unbindStream() {
    if (polling_) {
        StreamPoller::get().remove(this);
        polling_ = false;
    }
    if (streamTexId_ != 0) {
        // 别名计数 -1；归零由后端延后一帧退役（同帧被他处重绑即复活）
        TextureManager::instance().destroyTexture(
            static_cast<RenderBackend *>(treeService(View::kSvcRenderBackend)), streamTexId_);
        streamTexId_ = 0;
    }
    stream_.reset();
    boundName_.clear();
    lastSeenSeq_ = 0;
    pendingDirty_ = false;
}

void Video::pollStream() {
    if (!stream_) {
        // 流尚未创建（或已除名后重建）：重查注册表，命中即标脏——下次绘制完成绑定
        if (!pendingDirty_ && findVideoStream(boundName_)) {
            pendingDirty_ = true;
            markDirty();
        }
        return;
    }
    const uint64_t s = stream_->seq();
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
    bindStream();             // 幂等：首次绘制完成绑定（此时 parent 链已通、树服务可达）
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