/** @brief 可选扩展元素 Video — 插件式视频组件接口。 */
module;
#include <atomic>
#include <cstdint>
#include <functional>
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

/** @brief 打开的媒体信息（onLoaded 载荷）。 */
export struct MediaInfo {
    double duration = 0;        ///< 总时长（秒）
    uint32_t width = 0;         ///< 视频宽
    uint32_t height = 0;        ///< 视频高
    double fps = 0;             ///< 标称帧率
    bool hasAudio = false;      ///< 含音频轨（音频子系统未接入，现为只上报）
};

/**
 * @brief 媒体事件（引擎中立；UI 线程回调）
 *
 * 事件命名按媒体级而非视频级——后续音频模块复用同一事件集（音频预留）。
 */
export struct MediaEvent {
    enum class Kind : uint8_t { Loaded, TimeUpdate, End, Error, Buffering, Playing };
    Kind kind = Kind::Loaded;
    double currentTime = 0;     ///< TimeUpdate：当前播放位置（秒）
    MediaInfo info;             ///< Loaded：媒体信息
    std::string message;        ///< Error：错误描述
};

/**
 * @brief 媒体播放器抽象（引擎中立，解码/协议实现可插拔）
 *
 * 播放模型为推入私有流：实现内部解码并把帧写进自己持有的匿名 VideoStream
 * （不进命名注册表），显示端与 stream:// 推流共用同一条 GPU 通路。
 * 控制命令线程安全（内部邮箱串行化）；open 异步——结果经 onEvent
 * （Loaded/Error）送达。onEvent 在 UI 线程回调。
 */
export class MediaPlayer {
public:
    virtual ~MediaPlayer() = default;
    /** @brief 打开媒体源（异步；成功 Loaded / 失败 Error）*/
    virtual bool open(std::string url) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    /** @brief 跳转（秒；完成前 TimeUpdate 不推进）*/
    virtual void seek(double seconds) = 0;
    virtual void setLoop(bool loop) = 0;
    /** @brief 音频子系统未接入：实现仅记录状态（音频预留）*/
    virtual void setMuted(bool muted) = 0;
    /** @brief 播放器私有视频流（元素显示绑定用；无帧前 seq()==0）*/
    virtual std::shared_ptr<VideoStream> stream() = 0;
    /** @brief 当前播放位置（秒；线程安全读）*/
    virtual double currentTime() const = 0;
    /** @brief 总时长（秒；未打开为 0）*/
    virtual double duration() const = 0;
    /** @brief 是否正在播放（线程安全读）*/
    virtual bool isPaused() const = 0;

    /** @brief 媒体事件槽（UI 线程回调；元素/宿主接线）*/
    std::function<void(const MediaEvent &)> onEvent;
};

/** @brief 播放器工厂签名（宿主可注入自定义实现：平台解码器 / mpv 等）。 */
export using MediaPlayerFactory = std::function<std::unique_ptr<MediaPlayer>()>;

/** @brief 注册播放器工厂（各后端初始化时调用；后注册者生效）。 */
export void registerMediaPlayerFactory(MediaPlayerFactory f);
/** @brief 读取当前播放器工厂（未注册返回空 std::function）。 */
export const MediaPlayerFactory &mediaPlayerFactory();

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

    /** @brief 播放 (命令式 / JS play()；文件源)。 */
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

    /** @brief 媒体事件槽（引擎中立；UI 线程回调——bindings 层接 JS onLoaded 等）*/
    std::function<void(const MediaEvent &)> onMediaEvent;

    /** @brief 解码后端是否已创建（只读；行为锁/调试用——流模式恒 false） */
    bool isOpened() const { return player_ != nullptr; }

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
    std::unique_ptr<MediaPlayer> player_;    // 媒体播放器（文件源惰性创建；流模式不建）
    std::shared_ptr<std::atomic<bool>> alive_;    // 事件转发存活性标记（析构后投递的事件不再解引用）
    bool opened_ = false; /**< 文件源：open 已发起（Loaded 事件后置位） */
    // 播放状态缓存（UI 线程：事件回调更新，getProp 读取）
    double curTime_ = 0, duration_ = 0;
    bool paused_ = true, buffering_ = false;
    uint32_t videoW_ = 0, videoH_ = 0;

    // ── 流显示（stream:// 命名流，或文件源播放器私有流——同一条通路）──
    std::shared_ptr<VideoStream> stream_;      // 绑定的流（持 shared：除名后仍显示最后一帧）
    uint32_t streamTexId_ = 0;                 // 后端流纹理别名 id（0 = 未绑定）
    std::string boundName_;                    // stream:// 当前绑定/待查找的流名（文件源为空）
    uint64_t lastSeenSeq_ = 0;                 // 轮询已见的帧序号
    bool pendingDirty_ = false;                // 已标脏待绘制（未绘制前不重复标脏：不可见不空转）
    bool polling_ = false;                     // 是否已注册 StreamPoller

    /** @brief 首次使用前确保播放器已创建并 open(src)（文件源；流源直接返回）。 */
    void ensureOpened();
    /** @brief 事件入口（UI 线程）：更新状态缓存 + 转发 onMediaEvent + 按需标脏 */
    void onPlayerEvent(const MediaEvent &e);
    /** @brief 流绑定（幂等）：确保 stream_ 指向目标流 + 建流纹理别名 + 注册轮询 */
    void bindStream(const std::shared_ptr<VideoStream> &target);
    /** @brief 解绑：注销轮询 + 释放别名 id + 放开流引用（含停播播放器）*/
    void unbindStream();
    /** @brief 只释放流纹理别名 id（换绑前；保留轮询与流引用的去留由调用方定） */
    void releaseAlias();
    /** @brief 流帧绘制：fit 求解 → drawImageUv；无流/无帧时画占位（两模式共用） */
    void drawStreamFrame(Graphics &g);
};

/** @brief 自注册入口 (App 在 register_kwikui_module 前调用)。 */
export void registerVideoElement();

/** @brief FFmpeg 播放器注册（模块内部；ffmpeg/player.cpp 定义，KWIK_VIDEO_FFMPEG 编入时
 *         由 registerVideoElement 调用——宿主无需感知后端）。 */
void registerFfmpegPlayer();