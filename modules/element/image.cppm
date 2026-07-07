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
export class Image : public View {
public:
    Image() = default;
    explicit Image(ViewProps vp, ImageProps ip = {}) : View(std::move(vp)), imageProps_(std::move(ip)) {
        loadImage();
    }
    ~Image() override;

    ElementType type() const override {
        return ElementType::Image;
    }

    const ImageProps &imageProps() const {
        return imageProps_;
    }
    int imageWidth() const {
        return decodedWidth_;
    }
    int imageHeight() const {
        return decodedHeight_;
    }
    bool isLoaded() const {
        return loaded_;
    }
    bool pixelsEmpty() const {
        return pixels_.empty();
    }
    void uploadTexture();
    const std::string &error() const {
        return errorMsg_;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    ImageProps imageProps_;
    std::vector<uint8_t> pixels_;    // 解码后的 RGBA 像素数据
    int decodedWidth_ = 0;
    int decodedHeight_ = 0;
    bool loaded_ = false;
    std::string errorMsg_;
    uint32_t textureId_ = 0;    // GPU 纹理句柄 (0=未上传)
    void loadImage();
    void loadFromFile(const std::string &path);
    void loadFromBuffer();
    void loadFromSvg(const std::string &path);    // SVG 矢量解码 (nanosvg)
};