module;
#include <cstdint>
#include <unordered_map>
#include <vector>
export module kwik.render.texture_manager;
import kwik.render.backend;
import kwik.render.command;
import std;
/**
 * @brief GPU 纹理管理器（单例，按渲染后端分域）
 *
 * 多呈现（清单 §十四）：每棵 UI 树一个 RenderThread/backend，纹理按
 * backend 分域路由——create/destroy 显式携带目标 backend（组件经根节点
 * 服务槽 kSvcRenderBackend 上行获取本树 backend，见 Image::uploadTexture）。
 *
 * 使用流程:
 *   KwikRuntime::init() → instance().registerBackend(rt.backend());
 *   Image::uploadTexture() → instance().createTexture(backend, rgba, w, h);
 *   Image::~Image()      → instance().destroyTexture(backend, id);
 *   KwikRuntime::teardown() → instance().destroyAll();   // 遍历全部域
 */
export class TextureManager {
public:
    static TextureManager &instance() {
        static TextureManager mgr;
        return mgr;
    }
    /**
     * @brief 注册渲染后端域（每棵 UI 树的 KwikRuntime::init 调用；
     *        重复注册同一 backend 幂等）
     */
    void registerBackend(RenderBackend *backend) {
        if (backend) domains_.try_emplace(backend);
    }

    /**
     * @brief 创建图像纹理并上传 RGBA 像素到 GPU（目标 backend 域内记账）
     * @return 非零纹理句柄，0 表示失败
     */
    uint32_t createTexture(RenderBackend *backend, const uint8_t *rgba, uint32_t width, uint32_t height) {
        if (!backend || !rgba || width == 0 || height == 0) return 0;
        auto it = domains_.find(backend);
        if (it == domains_.end()) return 0;    // 未注册域（防御）
        uint32_t id = backend->createImageTexture(rgba, width, height);
        if (id != 0) {
            it->second.push_back(id);
        }
        return id;
    }

    /**
     * @brief 销毁图像纹理（按所属 backend 域路由）
     */
    void destroyTexture(RenderBackend *backend, uint32_t id) {
        if (!backend || id == 0) return;
        backend->destroyImageTexture(id);
        if (auto it = domains_.find(backend); it != domains_.end()) {
            auto &ids = it->second;
            ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        }
    }

    /**
     * @brief 销毁所有域的全部纹理（进程退出时调用）
     */
    void destroyAll() {
        for (auto &[backend, ids] : domains_) {
            for (uint32_t id : ids) backend->destroyImageTexture(id);
            ids.clear();
        }
    }
private:
    TextureManager() = default;
    std::unordered_map<RenderBackend *, std::vector<uint32_t>> domains_;    // backend → 活跃纹理 id
};
