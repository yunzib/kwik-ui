// ============================================================================
// 实现: kwik.render.backend_factory
// ============================================================================
module kwik.render.backend_factory;

import kwik.render.backend;
import kwik.render.vulkan_backend;

import std;

std::unique_ptr<RenderBackend> createBackend(BackendType type) {
    switch (type) {
    case BackendType::Vulkan:
        return std::make_unique<VulkanBackend>();
    default:
        return nullptr;    // Software/OpenGL/Metal：随 §十四 多呈现目标落地
    }
}
