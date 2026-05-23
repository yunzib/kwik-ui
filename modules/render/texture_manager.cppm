module;
#include <cstdint>
#include <unordered_map>
#include <vector>
export module kwik.render.texture_manager;
import kwik.render.backend;
import kwik.render.command;
import std;
/**
 * @brief GPU 纹理管理器（单例）
 *
 * 管理图像纹理的创建、销毁和生命周期。
 * 遵循 FontManager 的单例模式，通过 instance() 访问。
 *
 * 使用流程:
 *   Application::init() → TextureManager::instance().setBackend(backend);
 *   Image::onDraw()    → uint32_t id = TextureManager::instance().createTexture(rgba, w, h);
 *   Image::~Image()    → TextureManager::instance().destroyTexture(id);
 */
export class TextureManager {
public:
    static TextureManager &instance() {
        static TextureManager mgr;
        return mgr;
    }
    /**
     * @brief 设置渲染后端（由 Application::init 调用一次）
     */
    void setBackend(RenderBackend *backend) {
        backend_ = backend;
    }
    /**
     * @brief 创建图像纹理并上传 RGBA 像素到 GPU
     * @return 非零纹理句柄，0 表示失败
     */
    uint32_t createTexture(const uint8_t *rgba, uint32_t width, uint32_t height) {
        if (!backend_ || !rgba || width == 0 || height == 0) return 0;
        uint32_t id = backend_->createImageTexture(rgba, width, height);
        if (id != 0) {
            activeIds_.push_back(id);
        }
        return id;
    }
    /**
     * @brief 销毁图像纹理
     */
    void destroyTexture(uint32_t id) {
        if (!backend_ || id == 0) return;
        backend_->destroyImageTexture(id);
    }
    /**
     * @brief 销毁所有纹理（shutdown 时调用）
     */
    void destroyAll() {
        if (!backend_) return;
        for (uint32_t id : activeIds_) {
            backend_->destroyImageTexture(id);
        }
        activeIds_.clear();
    }
private:
    TextureManager() = default;
    RenderBackend *backend_ = nullptr;
    std::vector<uint32_t> activeIds_;
};