module;
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#endif
#include <fstream>
#include <cstring>
#include <algorithm>

#include "rect_shaders.h"
#include "glyph_shaders.h"

module kwik.render.vulkan_backend;

import std;
import kwik.core.types;
import kwik.render.font;

// ── 顶点数据 ──────────────────────────────────────────────────
static const float kQuadVertices[] = {
    0.0f, 0.0f, // bottom-left
    1.0f, 0.0f, // bottom-right
    1.0f, 1.0f, // top-right
    0.0f, 1.0f, // top-left
};
static const uint16_t kQuadIndices[] = {0, 1, 2, 0, 2, 3};

// ── Push Constants (must match GLSL layout) ──────────────────
struct PushConstants {
    float topLeftX, topLeftY;                 // offset 0
    float sizeX, sizeY;                       // offset 8
    float fillR, fillG, fillB, fillA;         // offset 16
    float radius;                             // offset 32
    float borderWidth;                        // offset 36
    float _pad0, _pad1;                       // offset 40 (align borderColor to 48)
    float borderR, borderG, borderB, borderA; // offset 48
    float opacity;                            // offset 64
    uint32_t drawMode;                        // offset 68 (0=fill, 1=stroke, 2=shadow)
    float shadowOffsetX, shadowOffsetY;       // offset 72
    float shadowBlur;                         // offset 80
    float _pad2;                              // offset 84 (align viewportSize to 88)
    float viewportW, viewportH;               // offset 88
};
static_assert(sizeof(PushConstants) == 96, "Push constants size must match shader layout");

struct GlyphPushConstants {
    float posX, posY;
    float sizeX, sizeY;
    float uvU0, uvV0;
    float uvU1, uvV1;
    float colorR, colorG, colorB, colorA;
    float viewportW, viewportH;
};
static_assert(sizeof(GlyphPushConstants) == 56, "Glyph push constants size mismatch");

// 构造函数
VulkanBackend::VulkanBackend() = default;
VulkanBackend::VulkanBackend(int w, int h) : width_(w), height_(h) {
}
VulkanBackend::~VulkanBackend() {
    shutdown();
}
bool VulkanBackend::initialize(void *nativeHandle, int width, int height) {
    width_ = width;
    height_ = height;
    return initVulkan(nativeHandle, width, height);
}
void VulkanBackend::shutdown() {
    cleanupVulkan();
}
void VulkanBackend::resize(int width, int height) {
    if (width_ == width && height_ == height) return;
    width_ = width;
    height_ = height;
    vkDeviceWaitIdle(vkDevice_);
    cleanupSwapchain();
    createSwapchain();
    createFramebuffers();
}

//  Frame 控制：
bool VulkanBackend::beginFrame() {
    vkWaitForFences(vkDevice_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    VkResult result = vkAcquireNextImageKHR(vkDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE,
                                            &currentImageIndex_);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs recreation — handled by resize() at next opportunity
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return false;
    vkResetCommandBuffer(commandBuffers_[currentImageIndex_], 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffers_[currentImageIndex_], &beginInfo);
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass_;
    rpInfo.framebuffer = framebuffers_[currentImageIndex_];
    rpInfo.renderArea.extent = swapchainExtent_;
    rpInfo.clearValueCount = 1;
    VkClearValue cv{};
    cv.color = {{clearColor_.r / 255.f, clearColor_.g / 255.f, clearColor_.b / 255.f, clearColor_.a / 255.f}};
    rpInfo.pClearValues = &cv;
    vkCmdBeginRenderPass(commandBuffers_[currentImageIndex_], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp{};
    vp.width = (float)swapchainExtent_.width;
    vp.height = (float)swapchainExtent_.height;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffers_[currentImageIndex_], 0, 1, &vp);
    VkRect2D scissor{};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(commandBuffers_[currentImageIndex_], 0, 1, &scissor);
    return true;
}
void VulkanBackend::endFrame() {
    vkCmdEndRenderPass(commandBuffers_[currentImageIndex_]);
    vkEndCommandBuffer(commandBuffers_[currentImageIndex_]);
}
void VulkanBackend::present() {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemas[] = {imageAvailableSemaphore_};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemas;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentImageIndex_];
    VkSemaphore sigSemas[] = {renderFinishedSemaphore_};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = sigSemas;
    vkResetFences(vkDevice_, 1, &inFlightFence_);
    if (vkQueueSubmit(vkQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) return;
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = sigSemas;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImageIndex_;
    vkQueuePresentKHR(vkQueue_, &presentInfo);
}

// 绘制方法
void VulkanBackend::setGlobalAlpha(float alpha) {
    globalAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
}
void VulkanBackend::pushClipRoundedRect(const Rect &, float) {
}
void VulkanBackend::resetClip() {
}
void VulkanBackend::clear(const Color &color) {
    VkClearAttachment attachment{};
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.clearValue.color = {{
        color.r / 255.f, color.b / 255.f, color.g / 255.f, color.a / 255.f // ← 注意 BGR 顺序
    }};
    VkClearRect clearRect{};
    clearRect.rect.extent = swapchainExtent_;
    clearRect.layerCount = 1;
    vkCmdClearAttachments(commandBuffers_[currentImageIndex_], 1, &attachment, 1, &clearRect);
}
void VulkanBackend::fillRect(const Rect &rect, const Color &color) {
    fillRoundedRect(rect, 0, color);
}
void VulkanBackend::fillRoundedRect(const Rect &rect, float radius, const Color &color) {
    VkCommandBuffer cmd = commandBuffers_[currentImageIndex_];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fillPipeline_);
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.fillR = color.r / 255.f;
    pc.fillG = color.g / 255.f;
    pc.fillB = color.b / 255.f;
    pc.fillA = color.a / 255.f;
    pc.radius = radius;
    pc.borderWidth = 0;
    pc.opacity = globalAlpha_;
    pc.drawMode = 0;
    pc.viewportW = (float)swapchainExtent_.width;
    pc.viewportH = (float)swapchainExtent_.height;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}
void VulkanBackend::strokeRoundedRect(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    VkCommandBuffer cmd = commandBuffers_[currentImageIndex_];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, strokePipeline_);
    PushConstants pc{};
    pc.topLeftX = rect.x;
    pc.topLeftY = rect.y;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.radius = radius;
    pc.borderWidth = strokeWidth;
    pc.borderR = color.r / 255.f;
    pc.borderG = color.g / 255.f;
    pc.borderB = color.b / 255.f;
    pc.borderA = color.a / 255.f;
    pc.opacity = globalAlpha_;
    pc.drawMode = 1;
    pc.viewportW = (float)swapchainExtent_.width;
    pc.viewportH = (float)swapchainExtent_.height;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}
void VulkanBackend::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    VkCommandBuffer cmd = commandBuffers_[currentImageIndex_];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_);
    PushConstants pc{};
    pc.topLeftX = rect.x + shadow.offsetX;
    pc.topLeftY = rect.y + shadow.offsetY;
    pc.sizeX = rect.width;
    pc.sizeY = rect.height;
    pc.fillR = shadow.color.r / 255.f;
    pc.fillG = shadow.color.g / 255.f;
    pc.fillB = shadow.color.b / 255.f;
    pc.fillA = shadow.color.a / 255.f;
    pc.radius = radius;
    pc.opacity = globalAlpha_;
    pc.drawMode = 2;
    pc.shadowOffsetX = 0.0f; // ← 顶点已偏移，fragment 不再加
    pc.shadowOffsetY = 0.0f;
    pc.shadowBlur = shadow.blurRadius;
    pc.viewportW = (float)swapchainExtent_.width;
    pc.viewportH = (float)swapchainExtent_.height;
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(PushConstants), &pc);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

// initVulkan (第7步完整体现) + cleanupVulkan：
bool VulkanBackend::initVulkan(void *nativeHandle, int width, int height) {
    // 1. Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "KwiK UI";
    appInfo.apiVersion = VK_API_VERSION_1_0;
    std::vector<const char *> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(_WIN32)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (vkCreateInstance(&createInfo, nullptr, &vkInstance_) != VK_SUCCESS) return false;
    // 2. Surface
#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    surfaceInfo.hwnd = static_cast<HWND>(nativeHandle);
    if (vkCreateWin32SurfaceKHR(vkInstance_, &surfaceInfo, nullptr, &vkSurface_) != VK_SUCCESS) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        return false;
    }
#elif defined(__linux__)
    VkXlibSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.dpy = XOpenDisplay(nullptr);
    surfaceInfo.window = reinterpret_cast<Window>(nativeHandle);
    if (vkCreateXlibSurfaceKHR(vkInstance_, &surfaceInfo, nullptr, &vkSurface_) != VK_SUCCESS) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        return false;
    }
#endif
    // 3. Physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        cleanupVulkan();
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, devices.data());
    vkPhysicalDevice_ = devices[0];
    // 4. Queue family
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &familyCount, families.data());
    queueFamilyIndex_ = 0;
    bool found = false;
    for (uint32_t i = 0; i < familyCount; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_, i, vkSurface_, &present);
            if (present) {
                queueFamilyIndex_ = i;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        cleanupVulkan();
        return false;
    }
    // 5. Device
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    const char *devExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo devInfo{};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    devInfo.enabledExtensionCount = 1;
    devInfo.ppEnabledExtensionNames = devExts;
    if (vkCreateDevice(vkPhysicalDevice_, &devInfo, nullptr, &vkDevice_) != VK_SUCCESS) {
        cleanupVulkan();
        return false;
    }
    vkGetDeviceQueue(vkDevice_, queueFamilyIndex_, 0, &vkQueue_);
    // 7. Swapchain + Render Pass + Pipeline + Buffers + Sync
    if (!createSwapchain()) {
        cleanupVulkan();
        return false;
    }
    if (!createRenderPass()) {
        cleanupVulkan();
        return false;
    }
    if (!createFramebuffers()) {
        cleanupVulkan();
        return false;
    }
    if (!createPipeline()) {
        cleanupVulkan();
        return false;
    }
    if (!createGlyphPipeline()) {
        cleanupVulkan();
        return false;
    }
    if (!createGlyphAtlas()) {
        cleanupVulkan();
        return false;
    }
    if (!createVertexBuffer()) {
        cleanupVulkan();
        return false;
    }
    if (!createCommandBuffers()) {
        cleanupVulkan();
        return false;
    }
    if (!createSyncObjects()) {
        cleanupVulkan();
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}
void VulkanBackend::cleanupVulkan() {
    if (vkDevice_ != VK_NULL_HANDLE) vkDeviceWaitIdle(vkDevice_);
    if (inFlightFence_ != VK_NULL_HANDLE) vkDestroyFence(vkDevice_, inFlightFence_, nullptr);
    if (renderFinishedSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, renderFinishedSemaphore_, nullptr);
    if (imageAvailableSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, imageAvailableSemaphore_, nullptr);
    if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(vkDevice_, commandPool_, nullptr);
    if (indexBufferMemory_ != VK_NULL_HANDLE) vkDestroyBuffer(vkDevice_, indexBuffer_, nullptr);
    if (vertexBufferMemory_ != VK_NULL_HANDLE) vkDestroyBuffer(vkDevice_, vertexBuffer_, nullptr);
    if (indexBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(vkDevice_, indexBufferMemory_, nullptr);
    if (vertexBufferMemory_ != VK_NULL_HANDLE) vkFreeMemory(vkDevice_, vertexBufferMemory_, nullptr);
    if (fillPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(vkDevice_, fillPipeline_, nullptr);
    if (strokePipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(vkDevice_, strokePipeline_, nullptr);
    if (shadowPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(vkDevice_, shadowPipeline_, nullptr);
    if (glyphPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(vkDevice_, glyphPipeline_, nullptr);
    if (glyphPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(vkDevice_, glyphPipelineLayout_, nullptr);
    if (glyphDescSet_ != VK_NULL_HANDLE) vkFreeDescriptorSets(vkDevice_, glyphDescPool_, 1, &glyphDescSet_);
    if (glyphDescPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(vkDevice_, glyphDescPool_, nullptr);
    if (glyphDescSetLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vkDevice_, glyphDescSetLayout_, nullptr);
    if (glyphAtlasSampler_ != VK_NULL_HANDLE) vkDestroySampler(vkDevice_, glyphAtlasSampler_, nullptr);
    if (glyphAtlasView_ != VK_NULL_HANDLE) vkDestroyImageView(vkDevice_, glyphAtlasView_, nullptr);
    if (glyphAtlasImage_ != VK_NULL_HANDLE) vkDestroyImage(vkDevice_, glyphAtlasImage_, nullptr);
    if (glyphAtlasMemory_ != VK_NULL_HANDLE) vkFreeMemory(vkDevice_, glyphAtlasMemory_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(vkDevice_, pipelineLayout_, nullptr);
    if (renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(vkDevice_, renderPass_, nullptr);
    cleanupSwapchain();
    if (vkSurface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
    if (vkInstance_ != VK_NULL_HANDLE) vkDestroyInstance(vkInstance_, nullptr);
}

// swapchain 创建/清理：
bool VulkanBackend::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, formats.data());
    swapchainFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    for (auto &f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB) {
            swapchainFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
            break;
        }
    }
    uint32_t presentCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &presentCount, presentModes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
    for (auto &m : presentModes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    if (caps.currentExtent.width != UINT32_MAX) {
        swapchainExtent_ = caps.currentExtent;
    } else {
        swapchainExtent_.width = std::clamp((uint32_t)width_, caps.minImageExtent.width, caps.maxImageExtent.width);
        swapchainExtent_.height = std::clamp((uint32_t)height_, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = vkSurface_;
    sci.minImageCount = imageCount;
    sci.imageFormat = swapchainFormat_;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = swapchainExtent_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = presentMode;
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(vkDevice_, &sci, nullptr, &swapchain_) != VK_SUCCESS) return false;
    uint32_t count;
    vkGetSwapchainImagesKHR(vkDevice_, swapchain_, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(vkDevice_, swapchain_, &count, swapchainImages_.data());
    swapchainImageViews_.resize(count);
    for (size_t i = 0; i < count; i++) {
        VkImageViewCreateInfo iv{};
        iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv.image = swapchainImages_[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = swapchainFormat_;
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.levelCount = 1;
        iv.subresourceRange.layerCount = 1;
        if (vkCreateImageView(vkDevice_, &iv, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) return false;
    }
    return true;
}
void VulkanBackend::cleanupSwapchain() {
    for (auto &fb : framebuffers_) vkDestroyFramebuffer(vkDevice_, fb, nullptr);
    framebuffers_.clear();
    for (auto &iv : swapchainImageViews_) vkDestroyImageView(vkDevice_, iv, nullptr);
    swapchainImageViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(vkDevice_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    swapchainImages_.clear();
}

//  RenderPass / Framebuffers / Pipeline：
bool VulkanBackend::createRenderPass() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format = swapchainFormat_;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference attRef{};
    attRef.attachment = 0;
    attRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attRef;
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAtt;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    return vkCreateRenderPass(vkDevice_, &rpInfo, nullptr, &renderPass_) == VK_SUCCESS;
}
bool VulkanBackend::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass_;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &swapchainImageViews_[i];
        fbInfo.width = swapchainExtent_.width;
        fbInfo.height = swapchainExtent_.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(vkDevice_, &fbInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) return false;
    }
    return true;
}

VkShaderModule VulkanBackend::createShaderModule(const std::uint8_t *spv, std::size_t size) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode = reinterpret_cast<const std::uint32_t *>(spv);
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(vkDevice_, &ci, nullptr, &mod);
    return mod;
}

bool VulkanBackend::createPipeline() {
    VkShaderModule vertMod = createShaderModule(kwik::shader::kRectVert, kwik::shader::kRectVertSize);
    VkShaderModule fragMod = createShaderModule(kwik::shader::kRectFrag, kwik::shader::kRectFragSize);
    if (!vertMod || !fragMod) return false;
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertMod;
    vertStage.pName = "main";
    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragMod;
    fragStage.pName = "main";
    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};
    // Vertex input
    VkVertexInputBindingDescription vtxBind{};
    vtxBind.binding = 0;
    vtxBind.stride = 2 * sizeof(float);
    vtxBind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vtxAttr{};
    vtxAttr.binding = 0;
    vtxAttr.location = 0;
    vtxAttr.format = VK_FORMAT_R32G32_SFLOAT;
    vtxAttr.offset = 0;
    VkPipelineVertexInputStateCreateInfo vtxInput{};
    vtxInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vtxInput.vertexBindingDescriptionCount = 1;
    vtxInput.pVertexBindingDescriptions = &vtxBind;
    vtxInput.vertexAttributeDescriptionCount = 1;
    vtxInput.pVertexAttributeDescriptions = &vtxAttr;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vpState{};
    vpState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpState.viewportCount = 1;
    vpState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable = VK_TRUE;
    blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
    blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAtt.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAtt;
    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;
    // Push constants: VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(vkDevice_, &plInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(vkDevice_, fragMod, nullptr);
        vkDestroyShaderModule(vkDevice_, vertMod, nullptr);
        return false;
    }
    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;
    pipeInfo.pVertexInputState = &vtxInput;
    pipeInfo.pInputAssemblyState = &inputAssembly;
    pipeInfo.pViewportState = &vpState;
    pipeInfo.pRasterizationState = &raster;
    pipeInfo.pMultisampleState = &msaa;
    pipeInfo.pColorBlendState = &blend;
    pipeInfo.pDynamicState = &dyn;
    pipeInfo.layout = pipelineLayout_;
    pipeInfo.renderPass = renderPass_;
    pipeInfo.subpass = 0;
    // Create all three pipelines
    if (vkCreateGraphicsPipelines(vkDevice_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &fillPipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(vkDevice_, fragMod, nullptr);
        vkDestroyShaderModule(vkDevice_, vertMod, nullptr);
        return false;
    }
    // strokePipeline_ uses same config (differentiated by drawMode push constant)
    if (vkCreateGraphicsPipelines(vkDevice_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &strokePipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(vkDevice_, fragMod, nullptr);
        vkDestroyShaderModule(vkDevice_, vertMod, nullptr);
        return false;
    }
    // shadowPipeline_ — same shader, different drawMode
    if (vkCreateGraphicsPipelines(vkDevice_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &shadowPipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(vkDevice_, fragMod, nullptr);
        vkDestroyShaderModule(vkDevice_, vertMod, nullptr);
        return false;
    }
    vkDestroyShaderModule(vkDevice_, fragMod, nullptr);
    vkDestroyShaderModule(vkDevice_, vertMod, nullptr);
    return true;
}

//  Vertex/Index buffer, Command buffers, Sync：
uint32_t VulkanBackend::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return 0;
}
bool VulkanBackend::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                 VkBuffer &buffer, VkDeviceMemory &memory) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(vkDevice_, &bi, nullptr, &buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(vkDevice_, buffer, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
    if (vkAllocateMemory(vkDevice_, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(vkDevice_, buffer, memory, 0);
    return true;
}
void VulkanBackend::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = commandPool_;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(vkDevice_, &ai, &cmd);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(vkQueue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkQueue_);
    vkFreeCommandBuffers(vkDevice_, commandPool_, 1, &cmd);
}
bool VulkanBackend::createVertexBuffer() {
    VkDeviceSize vbSize = sizeof(kQuadVertices);
    VkDeviceSize ibSize = sizeof(kQuadIndices);
    VkBuffer stagingVB, stagingIB;
    VkDeviceMemory stagingVBMem, stagingIBMem;
    if (!createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingVB,
                      stagingVBMem))
        return false;
    if (!createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingIB,
                      stagingIBMem)) {
        vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
        vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
        return false;
    }
    void *data;
    vkMapMemory(vkDevice_, stagingVBMem, 0, vbSize, 0, &data);
    memcpy(data, kQuadVertices, vbSize);
    vkUnmapMemory(vkDevice_, stagingVBMem);
    vkMapMemory(vkDevice_, stagingIBMem, 0, ibSize, 0, &data);
    memcpy(data, kQuadIndices, ibSize);
    vkUnmapMemory(vkDevice_, stagingIBMem);
    if (!createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_)
        || !createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_)) {
        return false;
    }
    // Command pool must exist before copyBuffer is called, so we create it here first
    {
        VkCommandPoolCreateInfo cpInfo{};
        cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpInfo.queueFamilyIndex = queueFamilyIndex_;
        vkCreateCommandPool(vkDevice_, &cpInfo, nullptr, &commandPool_);
    }
    copyBuffer(stagingVB, vertexBuffer_, vbSize);
    copyBuffer(stagingIB, indexBuffer_, ibSize);
    vkDestroyCommandPool(vkDevice_, commandPool_, nullptr);
    commandPool_ = VK_NULL_HANDLE;
    vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
    vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
    vkDestroyBuffer(vkDevice_, stagingIB, nullptr);
    vkFreeMemory(vkDevice_, stagingIBMem, nullptr);
    return true;
}
bool VulkanBackend::createCommandBuffers() {
    VkCommandPoolCreateInfo cpInfo{};
    cpInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpInfo.queueFamilyIndex = queueFamilyIndex_;
    cpInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(vkDevice_, &cpInfo, nullptr, &commandPool_) != VK_SUCCESS) return false;
    commandBuffers_.resize(swapchainImageViews_.size());
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)commandBuffers_.size();
    return vkAllocateCommandBuffers(vkDevice_, &ai, commandBuffers_.data()) == VK_SUCCESS;
}
bool VulkanBackend::createSyncObjects() {
    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(vkDevice_, &si, nullptr, &imageAvailableSemaphore_) == VK_SUCCESS
           && vkCreateSemaphore(vkDevice_, &si, nullptr, &renderFinishedSemaphore_) == VK_SUCCESS
           && vkCreateFence(vkDevice_, &fi, nullptr, &inFlightFence_) == VK_SUCCESS;
}

bool VulkanBackend::createGlyphPipeline() {
    VkShaderModule vertMod = createShaderModule(kwik::shader::kRectVert, kwik::shader::kRectVertSize);
    // 需要使用 glyph shaders，但当前 spv_to_header 生成的名字是 kRectVert/kRectFrag
    // 实际使用时改为 glyph 版本：
    VkShaderModule glyphVert = createShaderModule(kwik::shader::kGlyphVert, kwik::shader::kGlyphVertSize);
    VkShaderModule glyphFrag = createShaderModule(kwik::shader::kGlyphFrag, kwik::shader::kGlyphFragSize);

    if (!glyphVert || !glyphFrag) return false;
    // Descriptor set layout
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &samplerBinding;
    if (vkCreateDescriptorSetLayout(vkDevice_, &dslInfo, nullptr, &glyphDescSetLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(vkDevice_, glyphFrag, nullptr);
        vkDestroyShaderModule(vkDevice_, glyphVert, nullptr);
        return false;
    }
    // Pipeline layout with push constants + descriptor set
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(GlyphPushConstants);
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &glyphDescSetLayout_;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(vkDevice_, &plInfo, nullptr, &glyphPipelineLayout_) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(vkDevice_, glyphDescSetLayout_, nullptr);
        vkDestroyShaderModule(vkDevice_, glyphFrag, nullptr);
        vkDestroyShaderModule(vkDevice_, glyphVert, nullptr);
        return false;
    }
    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, glyphVert,
         "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, glyphFrag,
         "main"},
    };
    VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    VkPipelineVertexInputStateCreateInfo vtxInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vtxInput.vertexBindingDescriptionCount = 1;
    vtxInput.pVertexBindingDescriptions = &vtxBind;
    vtxInput.vertexAttributeDescriptionCount = 1;
    vtxInput.pVertexAttributeDescriptions = &vtxAttr;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAtt{};
    blendAtt.blendEnable = VK_TRUE;
    blendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAtt.colorBlendOp = VK_BLEND_OP_ADD;
    blendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAtt.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAtt;
    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;
    VkGraphicsPipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;
    pipeInfo.pVertexInputState = &vtxInput;
    pipeInfo.pInputAssemblyState = &ia;
    pipeInfo.pViewportState = &vp;
    pipeInfo.pRasterizationState = &rs;
    pipeInfo.pMultisampleState = &ms;
    pipeInfo.pColorBlendState = &blend;
    pipeInfo.pDynamicState = &dyn;
    pipeInfo.layout = glyphPipelineLayout_;
    pipeInfo.renderPass = renderPass_;
    pipeInfo.subpass = 0;
    VkResult r = vkCreateGraphicsPipelines(vkDevice_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &glyphPipeline_);
    vkDestroyShaderModule(vkDevice_, glyphFrag, nullptr);
    vkDestroyShaderModule(vkDevice_, glyphVert, nullptr);
    return r == VK_SUCCESS;
}
bool VulkanBackend::createGlyphAtlas() {
    uint32_t atlasW = 1024, atlasH = 1024;
    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8_UNORM;
    imgInfo.extent = {atlasW, atlasH, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vkDevice_, &imgInfo, nullptr, &glyphAtlasImage_) != VK_SUCCESS) return false;
    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(vkDevice_, glyphAtlasImage_, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vkDevice_, &allocInfo, nullptr, &glyphAtlasMemory_) != VK_SUCCESS) return false;
    vkBindImageMemory(vkDevice_, glyphAtlasImage_, glyphAtlasMemory_, 0);
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = glyphAtlasImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(vkDevice_, &viewInfo, nullptr, &glyphAtlasView_) != VK_SUCCESS) return false;
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(vkDevice_, &samplerInfo, nullptr, &glyphAtlasSampler_) != VK_SUCCESS) return false;
    // Descriptor pool + set
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(vkDevice_, &poolInfo, nullptr, &glyphDescPool_) != VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo setAlloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAlloc.descriptorPool = glyphDescPool_;
    setAlloc.descriptorSetCount = 1;
    setAlloc.pSetLayouts = &glyphDescSetLayout_;
    if (vkAllocateDescriptorSets(vkDevice_, &setAlloc, &glyphDescSet_) != VK_SUCCESS) return false;
    VkDescriptorImageInfo imgDescInfo{glyphAtlasSampler_, glyphAtlasView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = glyphDescSet_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgDescInfo;
    vkUpdateDescriptorSets(vkDevice_, 1, &write, 0, nullptr);
    return true;
}
void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    auto &fm = FontManager::instance();
    if (fm.atlasDirty()) {
        uploadGlyphAtlas(fm.atlasData(), fm.atlasWidth(), fm.atlasHeight());
        fm.clearAtlasDirty();
    }
    VkCommandBuffer cmdbuf = commandBuffers_[currentImageIndex_];
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphPipeline_);
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphPipelineLayout_, 0, 1, &glyphDescSet_, 0,
                            nullptr);
    GlyphPushConstants pc{};
    pc.posX = cmd.x;
    pc.posY = cmd.y;
    pc.sizeX = cmd.width;
    pc.sizeY = cmd.height;
    pc.uvU0 = cmd.uvLeft;
    pc.uvV0 = cmd.uvTop;
    pc.uvU1 = cmd.uvRight;
    pc.uvV1 = cmd.uvBottom;
    pc.colorR = cmd.color.r / 255.f;
    pc.colorG = cmd.color.g / 255.f;
    pc.colorB = cmd.color.b / 255.f;
    pc.colorA = cmd.color.a / 255.f;
    pc.viewportW = (float)swapchainExtent_.width;
    pc.viewportH = (float)swapchainExtent_.height;
    vkCmdPushConstants(cmdbuf, glyphPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(GlyphPushConstants), &pc);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vertexBuffer_, &offset);
    vkCmdBindIndexBuffer(cmdbuf, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdbuf, 6, 1, 0, 0, 0);
}

// void VulkanBackend::uploadGlyphAtlas(const uint8_t *data, uint32_t width, uint32_t height) {
//     VkDeviceSize size = (VkDeviceSize)width * height;
//     VkBuffer staging;
//     VkDeviceMemory stagingMem;
//     if (!createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
//                       stagingMem)) {
//         std::print("uploadGlyphAtlas: createBuffer failed\n");
//         return;
//     }
//     void *mapped;
//     vkMapMemory(vkDevice_, stagingMem, 0, size, 0, &mapped);
//     std::memcpy(mapped, data, size);
//     vkUnmapMemory(vkDevice_, stagingMem);
//     // Transition layout and copy
//     VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
//     ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//     ai.commandPool = commandPool_;
//     ai.commandBufferCount = 1;
//     VkCommandBuffer cmd;
//     vkAllocateCommandBuffers(vkDevice_, &ai, &cmd);
//     VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
//     bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//     vkBeginCommandBuffer(cmd, &bi);
//     VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
//     barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//     barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.image = glyphAtlasImage_;
//     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     barrier.subresourceRange.levelCount = 1;
//     barrier.subresourceRange.layerCount = 1;
//     barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
//                          nullptr, 1, &barrier);
//     VkBufferImageCopy region{};
//     region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     region.imageSubresource.layerCount = 1;
//     region.imageExtent = {width, height, 1};
//     vkCmdCopyBufferToImage(cmd, staging, glyphAtlasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
//     barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//     barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
//     0,
//                          nullptr, 1, &barrier);
//     vkEndCommandBuffer(cmd);
//     VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
//     si.commandBufferCount = 1;
//     si.pCommandBuffers = &cmd;
//     vkQueueSubmit(vkQueue_, 1, &si, VK_NULL_HANDLE);
//     vkQueueWaitIdle(vkQueue_);
//     vkFreeCommandBuffers(vkDevice_, commandPool_, 1, &cmd);
//     vkDestroyBuffer(vkDevice_, staging, nullptr);
//     vkFreeMemory(vkDevice_, stagingMem, nullptr);
// }

/**
 * @brief 增量上传字形图集到 GPU
 * @param data   图集原始数据 (1024 x 1024 R8)
 * @param width  图集宽度
 * @param height 图集高度
 *
 * 优化: 仅上传脏区域 (dirtyMin ~ dirtyMax 之间的行),
 * 避免每帧全量 1MB 上传
 */
void VulkanBackend::uploadGlyphAtlas(const uint8_t *data, uint32_t width, uint32_t height) {
    auto &fm = FontManager::instance();
    uint32_t dirtyMin = fm.atlasDirtyMinRow();
    uint32_t dirtyMax = fm.atlasDirtyMaxRow();
    // 合法性检查 (初始化后 dirtyMin = atlasSize > dirtyMax = 0 时为脏假阳性)
    if (dirtyMin >= dirtyMax || dirtyMin >= height) {
        dirtyMin = 0;
        dirtyMax = height;
    }
    uint32_t dirtyHeight = dirtyMax - dirtyMin;
    VkDeviceSize size = (VkDeviceSize)width * dirtyHeight;
    // ① 创建临时 staging buffer (仅脏区域大小)
    VkBuffer staging;
    VkDeviceMemory stagingMem;
    if (!createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
                      stagingMem)) {
        std::print("uploadGlyphAtlas: createBuffer failed\n");
        return;
    }
    // ② 仅拷贝脏行到 staging
    void *mapped;
    vkMapMemory(vkDevice_, stagingMem, 0, size, 0, &mapped);
    std::memcpy(mapped, data + dirtyMin * width, size);
    vkUnmapMemory(vkDevice_, stagingMem);
    // ③ 分配一次性提交的 command buffer
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = commandPool_;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(vkDevice_, &ai, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    // ④ 初次使用: UNDEFINED → TRANSFER_DST
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = glyphAtlasImage_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    // ⑤ 仅拷贝脏区域
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, (int32_t)dirtyMin, 0};
    region.imageExtent = {width, dirtyHeight, 1};
    vkCmdCopyBufferToImage(cmd, staging, glyphAtlasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    // ⑥ TRANSFER_DST → SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    // ⑦ 提交 + 等待完成
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(vkQueue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkQueue_);
    // ⑧ 清理
    vkFreeCommandBuffers(vkDevice_, commandPool_, 1, &cmd);
    vkDestroyBuffer(vkDevice_, staging, nullptr);
    vkFreeMemory(vkDevice_, stagingMem, nullptr);
}