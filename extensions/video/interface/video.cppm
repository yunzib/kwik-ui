/** @brief 可选扩展元素 Video — 插件式视频组件接口。 */
module;
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

export module kwik.ext.video;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.video_stream;    // VideoStream（stream:// 流模式绑定）
import kwik.element.typed_prop;     // PropType（只读读数类型声明）

import std;

/** @brief Video 特有属性。 */
export struct VideoProps {
    std::string src;       /**< 视频源：文件路径 / "stream://名字"（宿主 C++ 推流） */
    bool autoplay = false; /**< 是否自动播放 */
    bool loop = false;     /**< 是否循环 */
    bool muted = false;    /**< 是否静音 */
    ImageFit fit = ImageFit::Contain; /**< 画面适配（默认 contain，同 HTML video；Image 默认 cover） */
};

/** @brief 解码后的一帧 (RGBA 像素缓冲)。 */
export struct VideoFrame {
    std::vector<uint8_t> rgba; /**< RGBA8 像素, 长度 = width*height*4 */
    uint32_t width = 0;
    uint32_t height = 0;
};

/**
 * @brief 视频后端抽象 (引擎中立, 平台/解码器可插拔)
 *
 * FFmpeg / mpv / 平台原生各自实现此接口, 经 registerVideoBackendFactory
 * 注入, Video 组件据此创建后端。
 *
 * 采用拉取模型: 渲染线程在 onDraw 时调 grabFrame 取最新帧并上传纹理,
 * 避免解码线程直接触碰 Vulkan (见 application.cpp 预加载纹理注释)。
 */
export class VideoBackend {
public:
    virtual ~VideoBackend() = default;
    /** @brief 打开视频源 */
    virtual bool open(const std::string &src) = 0;
    /** @brief 播放 */
    virtual void play() = 0;
    /** @brief 暂停 */
    virtual void pause() = 0;
    /** @brief 跳转 @param seconds 秒 */
    virtual void seek(float seconds) = 0;
    /** @brief 当前时间 (秒) */
    virtual float position() const = 0;
    /** @brief 总时长 (秒) */
    virtual float duration() const = 0;
    /** @brief 拉取最新帧, 有变化返回 true */
    virtual bool grabFrame(VideoFrame &out) = 0;
    /** @brief 设置循环 */
    virtual void setLoop(bool loop) = 0;
    /** @brief 设置静音 */
    virtual void setMuted(bool muted) = 0;
    /** @brief 释放资源 */
    virtual void close() = 0;
};

/** @brief 视频后端工厂签名 (同 LazyListSourceFactory 钩子模式)。 */
export using VideoBackendFactory = std::function<std::unique_ptr<VideoBackend>()>;

/** @brief 注册后端工厂 (由各平台后端初始化时调用)。 */
export void registerVideoBackendFactory(VideoBackendFactory f);
/** @brief 读取当前后端工厂 (未注册返回空 std::function)。 */
export const VideoBackendFactory &videoBackendFactory();

/**
 * @brief Video 元素 — 插件式视频组件。
 *
 * type() 返回 ElementType::Video (调试/事件兜底);
 * typeName() 覆写为 "Video" 供 reconcile 字符串匹配。
 */
export class Video : public View {
public:
    using View::View;

    /** @brief 构建 Video。 @param vp 通用 View 属性 @param lp Video 特有属性 */
    explicit Video(ViewProps vp, VideoProps lp);
    ~Video() override;

    ElementType type() const override {
        static ElementType id = registerExtensionType("Video");    // 注册式扩展类型 (运行时分配 id)
        return id;
    }
    

    /** @brief 注入后端 (注册层经工厂创建后调用)。 */
    void setBackend(std::unique_ptr<VideoBackend> backend);

    /** @brief 播放 (命令式 / JS play())。 */
    void play();
    /** @brief 暂停。 */
    void pause();
    /** @brief 跳转 @param t 秒 */
    void seek(float t);

    /** @brief 属性增量更新 (src/autoplay/loop/muted/fit)。 */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    /** @brief reconcile 复用旧 View 时的专有属性写入 (reconcileProps 钩子)；
     *         仅 src 变化才重开/重绑——其余属性原位生效（loop/muted 直接下发后端） */
    void applyVideoProps(VideoProps p);

    /** @brief 当前专有属性（只读；行为锁/调试用） */
    const VideoProps &videoProps() const { return video_; }

    /**
     * @brief 属性读取：专有属性（src/fit/autoplay/loop/muted）+ 流模式只读读数
     *        fps（生产者近期提交帧率）/ droppedFrames（池满丢帧累计）/
     *        frameWidth / frameHeight（最新帧尺寸）——未绑定流时读数为 0
     */
    std::string getProperty(const char *name) const override;
    /** @brief 只读读数的类型声明（getProp 返回 JS 原生 number） */
    PropType readOnlyPropType(const char *name) const override;

    /** @brief 解码后端是否已 open（只读；行为锁/调试用——流模式恒 false） */
    bool isOpened() const { return opened_; }

    /**
     * @brief 解析 "stream://名字" 源
     * @param src 视频源串
     * @return 流名；非流源（文件路径等）或空名返回空串
     */
    static std::string_view streamNameOf(std::string_view src);

    /** @brief StreamPoller 回调（UI 线程，4ms 周期）：流 seq 变化 → 标脏一次；
     *         流尚未创建 → 重查注册表（先声明后建流顺序无关） */
    void pollStream();

protected:
    Size onMeasure(Constraints c) override;
    void onDraw(Graphics &g) override;

private:
    VideoProps video_;
    std::unique_ptr<VideoBackend> backend_;    // 解码后端（文件源惰性创建；流模式不建）
    uint32_t textureId_ = 0; /**< GPU 纹理句柄 (0=未上传) */
    uint32_t texW_ = 0, texH_ = 0;
    bool opened_ = false; /**< 后端是否已 open */
    bool playing_ = false;
    float position_ = 0.0f;

    // ── 流模式（src = "stream://名字"）──
    std::shared_ptr<VideoStream> stream_;      // 绑定的命名流（持 shared：除名后仍显示最后一帧）
    uint32_t streamTexId_ = 0;                 // 后端流纹理别名 id（0 = 未绑定）
    std::string boundName_;                    // 当前绑定/待查找的流名
    uint64_t lastSeenSeq_ = 0;                 // 轮询已见的帧序号
    bool pendingDirty_ = false;                // 已标脏待绘制（未绘制前不重复标脏：不可见不空转）
    bool polling_ = false;                     // 是否已注册 StreamPoller

    /** @brief 首次使用后端前确保 open(src)（文件源；流源直接返回）。 */
    void ensureOpened();
    /** @brief 流绑定（幂等）：按名查找 + 建流纹理别名 + 注册轮询 */
    void bindStream();
    /** @brief 解绑：注销轮询 + 释放别名 id + 放开流引用 */
    void unbindStream();
    /** @brief 流模式绘制：fit 求解 → drawImageUv；无流/无帧时画占位 */
    void drawStreamFrame(Graphics &g);
};

/** @brief 自注册入口 (App 在 register_kwikui_module 前调用)。 */
export void registerVideoElement();