module;
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <X11/Xlib.h>
#endif

export module kwik.render.vulkan_backend;

import kwik.render.backend;
import kwik.core.types;
import std;

/**
 * @brief Vulkan渲染后端实现
 */
export class VulkanBackend : public RenderBackend {
public:
    VulkanBackend();
    VulkanBackend(int width, int height);
    ~VulkanBackend() override;

    bool initialize(void *nativeHandle, int width, int height) override;
    void shutdown() override;
    void resize(int width, int height) override;
    bool beginFrame() override;
    void endFrame() override;
    void present() override;
    void setGlobalAlpha(float alpha) override;
    void pushClipRoundedRect(const Rect &rect, float radius) override;
    void resetClip() override;
    void clear(const Color &color) override;
    void fillRect(const Rect &rect, const Color &color) override;
    void fillRoundedRect(const Rect &rect, float radius, const Color &color) override;
    void strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) override;
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow) override;
    BackendType getType() const override {
        return BackendType::Vulkan;
    }
    int getWidth() const override {
        return width_;
    }
    int getHeight() const override {
        return height_;
    }

private:
    bool initVulkan(void *nativeHandle, int width, int height);
    void cleanupVulkan();
    int width_ = 0;
    int height_ = 0;
    void *nativeHandle_ = nullptr;
    float globalAlpha_ = 1.0f;

    // Vulkan句柄
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice vkDevice_ = VK_NULL_HANDLE;
    VkQueue vkQueue_ = VK_NULL_HANDLE;

#if defined(__linux__)
    Display *x11Display_ = nullptr;
#endif
};
