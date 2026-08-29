/** @brief Video 元素实现 (引擎中立, 不 import kwik.engine.*)。 */
module;
#include <stdint.h>

module kwik.ext.video;

import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.texture_manager;

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
}    // namespace

// ═══════════════════════════════════════════════════════════
// Video 实现
// ═══════════════════════════════════════════════════════════
Video::Video(ViewProps vp, VideoProps lp) : View(std::move(vp)), video_(std::move(lp)) {
    // 默认后端: 未注册工厂时回退 Null 后端 (保证无外部依赖也能跑)
    const auto &f = videoBackendFactory();
    backend_ = f ? f() : std::make_unique<NullVideoBackend>();
}

Video::~Video() {
    if (backend_) backend_->close();
    if (textureId_ != 0) TextureManager::instance().destroyTexture(textureId_);
}

void Video::setBackend(std::unique_ptr<VideoBackend> backend) {
    if (backend_) backend_->close();
    backend_ = std::move(backend);
    opened_ = false;
    markDirty();
}

void Video::ensureOpened() {
    if (opened_ || !backend_) return;
    backend_->setLoop(video_.loop);
    backend_->setMuted(video_.muted);
    opened_ = backend_->open(video_.src);
    if (video_.autoplay) play();
}

void Video::play() {
    if (!backend_) return;
    ensureOpened();
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
            video_.src = *s;
            opened_ = false;    // 换源需重新 open
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

void Video::applyVideoProps(VideoProps p) {
    video_ = std::move(p);
    opened_ = false;    // 属性变化 → 下次 draw 重新 open
    markDirty();
}

Size Video::onMeasure(Constraints c) {
    float w = props.width.value_or(480.0f);
    float h = props.height.value_or(270.0f);
    return c.constrain({w, h});
}

void Video::onDraw(Graphics &g) {
    View::onDraw(g);    // 背景 + 边框
    ensureOpened();

    // 拉取最新帧 → 上传纹理 (在渲染线程, 避免 Vulkan 线程竞态)
    VideoFrame f;
    if (backend_ && backend_->grabFrame(f) && f.width > 0 && f.height > 0) {
        // 尺寸变化 → 重建纹理
        if (textureId_ != 0 && (texW_ != f.width || texH_ != f.height)) {
            TextureManager::instance().destroyTexture(textureId_);
            textureId_ = 0;
        }
        if (textureId_ == 0) {
            textureId_ = TextureManager::instance().createTexture(f.rgba.data(), f.width, f.height);
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