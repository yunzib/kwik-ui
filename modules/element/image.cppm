module;
#include <memory>
#include <vector>
#include <string>
export module kwik.element.image;
import kwik.render.texture_manager;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import std;
/**
 * @brief Image 控件
 *
 * 基于独立 GPU 纹理的图像渲染组件。
 * 支持本地文件路径 (File source) 和内存像素缓冲区 (Buffer source)。
 * 加载为同步 (stb_image 解码), 纹理在首次 onDraw 时上传至 Vulkan 后端。
 *
 * JS 使用示例:
 *   Image({ src: "assets/logo.png", fit: "contain", width: 200, height: 100 })
 *   Image({ data: rawBuffer, width: 256, height: 256 })
 */
/**
 * @brief ImageFit 求解结果（20 号 fit 语义）
 *
 * dst：视频目标落笔矩形；uv0..uv1：源纹理裁剪窗口（cover 裁长轴 /
 * none 超框 1:1 裁剪；contain/fill 为满幅窗口）。fill 的 dst==frame。
 */
export struct ImageFitSolution {
    Rect dst;
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
};

export class Image : public View {
public:
    Image() = default;
    explicit Image(ViewProps vp, ImageProps ip = {}) : View(std::move(vp)), imageProps_(std::move(ip)) { loadImage(); }
    ~Image() override;

    /** @brief reconcile 属性覆盖：src/data 变更才重新加载解码（避免整树重建触发重复解码） */
    void applyImageProps(ImageProps ip);

    ElementType type() const override { return ElementType::Image; }

    const ImageProps &imageProps() const { return imageProps_; }
    int imageWidth() const { return decodedWidth_; }
    int imageHeight() const { return decodedHeight_; }
    bool isLoaded() const { return loaded_; }
    bool pixelsEmpty() const { return pixels_.empty(); }
    void uploadTexture();
    const std::string &error() const { return errorMsg_; }

    /**
     * @brief ImageFit 求解（纯函数，核心测试可达）：按 fit 模式把
     *        iw×ih 图像入 frame 容器——返回绝对目标矩形与源 UV 窗口。
     *        图像或容器有零边时退回 Fill 等价解（dst=frame 满幅窗口）。
     */
    static ImageFitSolution resolveFit(ImageFit fit, Rect frame, float iw, float ih);

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    void onLayout() override;

private:
    ImageProps imageProps_;
    std::vector<uint8_t> pixels_;    // 解码后的 RGBA 像素数据
    int decodedWidth_ = 0;
    int decodedHeight_ = 0;
    bool loaded_ = false;
    bool svgLoaded_ = false;          // SVG 源（显式尺寸变更重光栅化判据，20 号）
    std::string errorMsg_;
    uint32_t textureId_ = 0;    // GPU 纹理句柄 (0=未上传)
    bool uploadConfirmed_ = false;    // 17 号：当前 id 已确认落地（落地才释放 pixels_，失败重试需源数据）
    int uploadRetries_ = 0;           // 17 号：异步上传失败重试计数（超限弃传，防每帧刷写）
    bool errorPending_ = false;       // 17 号：装载失败待报（构造期 onError 槽未接线时挂起，首帧补发）
    static constexpr int kMaxUploadRetries = 3;
    void loadImage();
    void loadFromFile(const std::string &path);
    void loadFromBuffer();
    void loadFromSvg(const std::string &path);    // SVG 矢量解码 (nanosvg)
    void confirmUpload();    // 17 号：拉取异步上传回执（落地释放像素 / 失败重试或弃传）
    void failUpload(const std::string &msg);    // 17 号：上传终态失败收场（弃传 + 置白 + onError）
    void fireError();    // 17 号：onError 出口（槽未接线时转挂起，接线后首帧补发一次）
};