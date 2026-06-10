module;
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xlib.h>
#include <X11/Xlib.h>
#endif
#include <cstring>
#include <algorithm>
module kwik.render.vulkan.context;
import std;
import kwik.core.types;
import kwik.core.log;
// ── 单位四边形（所有管线复用）──────────────────────────────────
namespace {
const float kQuadVertices[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
const uint16_t kQuadIndices[] = {0, 1, 2, 0, 2, 3};
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
}    // namespace
// ================================================================
// 析构 / shutdown
// ================================================================
VulkanContext::~VulkanContext() {
    shutdown();
}
void VulkanContext::shutdown() {
    if (vkDevice_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(vkDevice_);
    destroyCanvas();
    if (canvasFramebuffer_) {
        vkDestroyFramebuffer(vkDevice_, canvasFramebuffer_, nullptr);
        canvasFramebuffer_ = VK_NULL_HANDLE;
    }
    for (auto &s : imageAvailableSemaphores_)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, s, nullptr);
    imageAvailableSemaphores_.clear();
    for (auto &s : renderFinishedSemaphores_)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, s, nullptr);
    renderFinishedSemaphores_.clear();
    for (auto &f : inFlightFences_)
        if (f != VK_NULL_HANDLE) vkDestroyFence(vkDevice_, f, nullptr);
    inFlightFences_.clear();
    if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(vkDevice_, commandPool_, nullptr);
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(vkDevice_, indexBuffer_, nullptr);
        vkFreeMemory(vkDevice_, indexBufferMemory_, nullptr);
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(vkDevice_, vertexBuffer_, nullptr);
        vkFreeMemory(vkDevice_, vertexBufferMemory_, nullptr);
    }
    if (renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(vkDevice_, renderPass_, nullptr);
    cleanupSwapchain();
    if (vkSurface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
    if (vkDevice_ != VK_NULL_HANDLE) vkDestroyDevice(vkDevice_, nullptr);
    if (vkInstance_ != VK_NULL_HANDLE) vkDestroyInstance(vkInstance_, nullptr);
    vkDevice_ = VK_NULL_HANDLE;
}
// ================================================================
// initialize — 核心初始化（Instance → Device → Swapchain → ...）
// ================================================================
bool VulkanContext::initialize(void *nativeHandle, int width, int height) {
    if (!createInstance(nativeHandle)) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createLogicalDevice()) {
        shutdown();
        return false;
    }
    vkGetDeviceQueue(vkDevice_, queueFamilyIndex_, 0, &vkQueue_);
    if (!createSwapchain()) {
        shutdown();
        return false;
    }
    if (!createRenderPass()) {
        shutdown();
        return false;
    }
    if (!createCommandBuffers()) {
        shutdown();
        return false;
    }
    if (!createCanvasImage()) {
        shutdown();
        return false;
    }
    if (!createCanvasFramebuffer()) {
        shutdown();
        return false;
    }
    if (!createVertexBuffer()) {
        shutdown();
        return false;
    }
    if (!createSyncObjects()) {
        shutdown();
        return false;
    }
    return true;
}
// ================================================================
// resize — 重建 swapchain + canvas，返回 bool
// ================================================================
bool VulkanContext::resize(int w, int h) {
    if (swapchainExtent_.width == static_cast<uint32_t>(w) && swapchainExtent_.height == static_cast<uint32_t>(h))
        return true;

    vkDeviceWaitIdle(vkDevice_);
    Log::info("VulkanContext::resize: {}x{} → {}x{}", swapchainExtent_.width, swapchainExtent_.height, w, h,
              std::source_location::current());

    destroyCanvas();
    cleanupSwapchain();

    if (!createSwapchain()) {
        Log::error("resize: createSwapchain failed ({}x{})", w, h, std::source_location::current());
        return false;
    }
    // ── 重建命令缓冲和同步对象（swapchain 图像数可能变化）──
    vkFreeCommandBuffers(vkDevice_, commandPool_, (uint32_t)commandBuffers_.size(), commandBuffers_.data());
    if (!createCommandBuffers()) {
        Log::error("resize: createCommandBuffers failed", std::source_location::current());
        return false;
    }
    if (!createCanvasImage()) {
        Log::error("resize: createCanvasImage failed", std::source_location::current());
        return false;
    }
    if (!createCanvasFramebuffer()) {
        Log::error("resize: createCanvasFramebuffer failed", std::source_location::current());
        return false;
    }
    for (auto &s : imageAvailableSemaphores_)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, s, nullptr);
    for (auto &s : renderFinishedSemaphores_)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, s, nullptr);
    for (auto &f : inFlightFences_)
        if (f != VK_NULL_HANDLE) vkDestroyFence(vkDevice_, f, nullptr);
    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    inFlightFences_.clear();

    if (!createSyncObjects()) {
        Log::error("resize: createSyncObjects failed", std::source_location::current());
        return false;
    }
    frameIndex_ = 0;
    currentImageIndex_ = 0;
    return true;
}
// ================================================================
// createInstance — Vulkan 实例 + 平台 Surface
// ================================================================
bool VulkanContext::createInstance(void *nativeHandle) {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "KwiK UI";
    app.apiVersion = VK_API_VERSION_1_0;
    std::vector<const char *> ext = {VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(_WIN32)
    ext.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    ext.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)ext.size();
    ci.ppEnabledExtensionNames = ext.data();

    // 条件启用 validation layer
    const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());
    bool hasValidation = false;
    for (auto &l : available) {
        if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            hasValidation = true;
            break;
        }
    }
    if (hasValidation) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = validationLayers;
    }

    if (vkCreateInstance(&ci, nullptr, &vkInstance_) != VK_SUCCESS) return false;
#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    si.hinstance = GetModuleHandle(nullptr);
    si.hwnd = static_cast<HWND>(nativeHandle);
    if (vkCreateWin32SurfaceKHR(vkInstance_, &si, nullptr, &vkSurface_) != VK_SUCCESS) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        return false;
    }
#elif defined(__linux__)
    VkXlibSurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
    si.dpy = XOpenDisplay(nullptr);
    si.window = reinterpret_cast<Window>(nativeHandle);
    if (vkCreateXlibSurfaceKHR(vkInstance_, &si, nullptr, &vkSurface_) != VK_SUCCESS) {
        vkDestroyInstance(vkInstance_, nullptr);
        vkInstance_ = VK_NULL_HANDLE;
        return false;
    }
#endif
    return true;
}
// ================================================================
// pickPhysicalDevice — 选取第一个满足条件的 GPU
// ================================================================
bool VulkanContext::pickPhysicalDevice() {
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(vkInstance_, &n, nullptr);
    if (n == 0) return false;
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(vkInstance_, &n, devs.data());
    vkPhysicalDevice_ = devs[0];
    uint32_t fn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &fn, nullptr);
    std::vector<VkQueueFamilyProperties> fams(fn);
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &fn, fams.data());
    for (uint32_t i = 0; i < fn; i++) {
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice_, i, vkSurface_, &present);
            if (present) {
                queueFamilyIndex_ = i;
                return true;
            }
        }
    }
    return false;
}
// ================================================================
// createLogicalDevice
// ================================================================
bool VulkanContext::createLogicalDevice() {
    float pri = 1.0f;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, queueFamilyIndex_, 1, &pri};
    const char *exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = 1;
    di.ppEnabledExtensionNames = exts;
    return vkCreateDevice(vkPhysicalDevice_, &di, nullptr, &vkDevice_) == VK_SUCCESS;
}
// ================================================================
// beginFrame — 获取 swapchain + 开始 canvas render pass
// 返回 FrameToken 或 nullopt（swapchain 不可用）
// ================================================================
std::optional<FrameToken> VulkanContext::beginFrame() {
    // ── ① 等待当前帧槽完成 ──
    VkResult fenceWait = vkWaitForFences(vkDevice_, 1, &inFlightFences_[frameIndex_], VK_TRUE, UINT64_MAX);
    if (fenceWait != VK_SUCCESS) {
        Log::error("beginFrame: vkWaitForFences failed: {}", static_cast<int>(fenceWait),
                   std::source_location::current());
        return std::nullopt;
    }

    // ── ② 获取 swapchain 图像 ──
    VkResult r = vkAcquireNextImageKHR(vkDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[frameIndex_],
                                       VK_NULL_HANDLE, &currentImageIndex_);

    // ── @fix: swapchain out-of-date 时重建后重试 ──
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
        if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
            Log::warn("beginFrame: surface not visible after out-of-date", std::source_location::current());
            return std::nullopt;
        }
        Log::info("beginFrame: swapchain out-of-date, recreating ({}x{})", caps.currentExtent.width,
                  caps.currentExtent.height, std::source_location::current());
        if (!resize(static_cast<int>(caps.currentExtent.width), static_cast<int>(caps.currentExtent.height))) {
            return std::nullopt;
        }
        // 重建后 fence 为 VK_FENCE_CREATE_SIGNALED，无需 wait / reset
        r = vkAcquireNextImageKHR(vkDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[frameIndex_],
                                  VK_NULL_HANDLE, &currentImageIndex_);
    }

    if (r == VK_ERROR_SURFACE_LOST_KHR) {
        Log::error("beginFrame: VK_ERROR_SURFACE_LOST_KHR", std::source_location::current());
        return std::nullopt;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        Log::error("beginFrame: vkAcquireNextImageKHR failed: {}", static_cast<int>(r),
                   std::source_location::current());
        return std::nullopt;
    }

    vkResetFences(vkDevice_, 1, &inFlightFences_[frameIndex_]);

    // ── ③ 开始录制 ──
    vkResetCommandBuffer(commandBuffers_[frameIndex_], 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffers_[frameIndex_], &bi);

    // ── ④ 开始 canvas render pass（LOAD_OP_LOAD，全屏 renderArea）──
    VkRenderPassBeginInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpInfo.renderPass = renderPass_;
    rpInfo.framebuffer = canvasFramebuffer_;
    rpInfo.renderArea = {{0, 0}, swapchainExtent_};
    VkClearValue cvs[2];
    cvs[0].color = {{0.96f, 0.96f, 0.96f, 1.0f}};
    cvs[1].depthStencil = {1.0f, 0};
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = cvs;
    vkCmdBeginRenderPass(commandBuffers_[frameIndex_], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ── ⑤ Viewport / Scissor 默认全屏 ──
    VkViewport vp{0,    0,   static_cast<float>(swapchainExtent_.width), static_cast<float>(swapchainExtent_.height),
                  0.0f, 1.0f};
    vkCmdSetViewport(commandBuffers_[frameIndex_], 0, 1, &vp);
    VkRect2D sc{{0, 0}, swapchainExtent_};
    vkCmdSetScissor(commandBuffers_[frameIndex_], 0, 1, &sc);

    // ── ⑥ Stencil 初始状态 ──
    vkCmdSetStencilReference(commandBuffers_[frameIndex_], VK_STENCIL_FACE_FRONT_AND_BACK, 0);
    vkCmdSetStencilCompareMask(commandBuffers_[frameIndex_], VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
    vkCmdSetStencilWriteMask(commandBuffers_[frameIndex_], VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);

    return FrameToken{
        .commandBuffer = commandBuffers_[frameIndex_],
        .extent = swapchainExtent_,
        .vertexBuffer = vertexBuffer_,
        .indexBuffer = indexBuffer_,
    };
}
// ================================================================
// endFrame — 结束 render pass + blit canvas → swapchain + barrier
// ================================================================
void VulkanContext::endFrame() {
    VkCommandBuffer cb = commandBuffers_[frameIndex_];

    // ── 结束 canvas render pass（canvas → TRANSFER_SRC_OPTIMAL）──
    vkCmdEndRenderPass(cb);

    // ── Swapchain barrier: UNDEFINED → TRANSFER_DST_OPTIMAL ──
    VkImageMemoryBarrier swapBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    swapBarrier.srcAccessMask = 0;
    swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapBarrier.image = swapchainImages_[currentImageIndex_];
    swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &swapBarrier);

    // ── Blit: canvas → swapchain（全屏）──
    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    VkExtent3D ext = {swapchainExtent_.width, swapchainExtent_.height, 1};
    blitRegion.srcOffsets[1] = {static_cast<int32_t>(ext.width), static_cast<int32_t>(ext.height), 1};
    blitRegion.dstOffsets[1] = {static_cast<int32_t>(ext.width), static_cast<int32_t>(ext.height), 1};
    vkCmdBlitImage(cb, canvasImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImages_[currentImageIndex_],
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

    // ── Swapchain barrier: TRANSFER_DST → PRESENT_SRC ──
    swapBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.dstAccessMask = 0;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &swapBarrier);

    // ── Canvas barrier: TRANSFER_SRC → COLOR_ATTACHMENT ──
    VkImageMemoryBarrier canvasBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    canvasBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    canvasBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    canvasBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    canvasBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    canvasBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    canvasBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    canvasBarrier.image = canvasImage_;
    canvasBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &canvasBarrier);

    vkEndCommandBuffer(cb);
}
// ================================================================
// present — 提交 + 呈现，内含 out-of-date 自愈
// ================================================================
bool VulkanContext::present() {
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &imageAvailableSemaphores_[frameIndex_];
    si.pWaitDstStageMask = stages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &commandBuffers_[frameIndex_];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinishedSemaphores_[frameIndex_];

    VkResult submitResult = vkQueueSubmit(vkQueue_, 1, &si, inFlightFences_[frameIndex_]);
    if (submitResult != VK_SUCCESS) {
        Log::error("present: vkQueueSubmit failed: {}", static_cast<int>(submitResult),
                   std::source_location::current());
    }

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinishedSemaphores_[frameIndex_];
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &currentImageIndex_;

    VkResult presentResult = vkQueuePresentKHR(vkQueue_, &pi);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
        if (caps.currentExtent.width != 0 && caps.currentExtent.height != 0) {
            uint32_t newW =
                (caps.currentExtent.width == UINT32_MAX) ? swapchainExtent_.width : caps.currentExtent.width;
            uint32_t newH =
                (caps.currentExtent.height == UINT32_MAX) ? swapchainExtent_.height : caps.currentExtent.height;
            resize(static_cast<int>(newW), static_cast<int>(newH));
        }
    }

    frameIndex_ = (frameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
    return presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR;
}
// ======================================================================
// 交换链
// ======================================================================
bool VulkanContext::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
    uint32_t fmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &fmtCount, fmts.data());
    swapchainFormat_ = VK_FORMAT_R8G8B8A8_UNORM;
    for (auto &f : fmts) {
        if (f.format == VK_FORMAT_R8G8B8A8_SRGB) {
            swapchainFormat_ = f.format;
            break;
        }
    }
    uint32_t pmCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> pms(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &pmCount, pms.data());
    VkPresentModeKHR pm = VK_PRESENT_MODE_FIFO_KHR;
    for (auto &m : pms) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            pm = m;
            break;
        }
    }
    if (caps.currentExtent.width != UINT32_MAX)
        swapchainExtent_ = caps.currentExtent;
    else
        swapchainExtent_ = {
            std::clamp((uint32_t)swapchainExtent_.width, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp((uint32_t)swapchainExtent_.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount) imgCount = caps.maxImageCount;
    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = vkSurface_;
    sci.minImageCount = imgCount;
    sci.imageFormat = swapchainFormat_;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = swapchainExtent_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = pm;
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(vkDevice_, &sci, nullptr, &swapchain_) != VK_SUCCESS) return false;
    uint32_t n;
    vkGetSwapchainImagesKHR(vkDevice_, swapchain_, &n, nullptr);
    swapchainImages_.resize(n);
    vkGetSwapchainImagesKHR(vkDevice_, swapchain_, &n, swapchainImages_.data());
    swapchainImageViews_.resize(n);
    for (uint32_t i = 0; i < n; i++) {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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
void VulkanContext::cleanupSwapchain() {
    for (auto &iv : swapchainImageViews_)
        if (iv) vkDestroyImageView(vkDevice_, iv, nullptr);
    swapchainImageViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vkDevice_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();
}
// ================================================================
// RenderPass — Canvas 颜色（1x, LOAD_OP_LOAD）+ Stencil（1x, CLEAR）
// ================================================================
bool VulkanContext::createRenderPass() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format = swapchainFormat_;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VkAttachmentDescription stencilAtt{};
    stencilAtt.format = VK_FORMAT_D24_UNORM_S8_UINT;
    stencilAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    stencilAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    stencilAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencilAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference stencilRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &stencilRef;

    VkAttachmentDescription atts[] = {colorAtt, stencilAtt};
    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = atts;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    return vkCreateRenderPass(vkDevice_, &rpInfo, nullptr, &renderPass_) == VK_SUCCESS;
}
// ================================================================
// createCanvasFramebuffer — 单份持久 canvas
// ================================================================
bool VulkanContext::createCanvasFramebuffer() {
    VkImageView atts[] = {canvasView_, canvasStencilView_};
    VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb.renderPass = renderPass_;
    fb.attachmentCount = 2;
    fb.pAttachments = atts;
    fb.width = swapchainExtent_.width;
    fb.height = swapchainExtent_.height;
    fb.layers = 1;
    return vkCreateFramebuffer(vkDevice_, &fb, nullptr, &canvasFramebuffer_) == VK_SUCCESS;
}
// ================================================================
// 顶点 / 索引缓冲
// ================================================================
bool VulkanContext::createVertexBuffer() {
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
    std::memcpy(data, kQuadVertices, vbSize);
    vkUnmapMemory(vkDevice_, stagingVBMem);
    vkMapMemory(vkDevice_, stagingIBMem, 0, ibSize, 0, &data);
    std::memcpy(data, kQuadIndices, ibSize);
    vkUnmapMemory(vkDevice_, stagingIBMem);
    if (!createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_)
        || !createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_)) {
        vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
        vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
        vkDestroyBuffer(vkDevice_, stagingIB, nullptr);
        vkFreeMemory(vkDevice_, stagingIBMem, nullptr);
        return false;
    }
    copyBuffer(stagingVB, vertexBuffer_, vbSize);
    copyBuffer(stagingIB, indexBuffer_, ibSize);
    vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
    vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
    vkDestroyBuffer(vkDevice_, stagingIB, nullptr);
    vkFreeMemory(vkDevice_, stagingIBMem, nullptr);
    return true;
}
// ================================================================
// createCommandBuffers — 按 MAX_FRAMES_IN_FLIGHT 创建
// ================================================================
bool VulkanContext::createCommandBuffers() {
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vkDevice_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cp.queueFamilyIndex = queueFamilyIndex_;
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(vkDevice_, &cp, nullptr, &commandPool_) != VK_SUCCESS) return false;
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    return vkAllocateCommandBuffers(vkDevice_, &ai, commandBuffers_.data()) == VK_SUCCESS;
}
// ================================================================
// createSyncObjects — 按 MAX_FRAMES_IN_FLIGHT 创建
// ================================================================
bool VulkanContext::createSyncObjects() {
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(vkDevice_, &si, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(vkDevice_, &si, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(vkDevice_, &fi, nullptr, &inFlightFences_[i]) != VK_SUCCESS) return false;
    }
    return true;
}
// ================================================================
// 工具函数（VulkanContext 成员 + 静态）
// ================================================================
uint32_t VulkanContext::findMemoryType(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((typeFilter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    return 0;
}
bool VulkanContext::createBuffer(VkDevice device, VkPhysicalDevice physDevice, VkDeviceSize size,
                                 VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buffer,
                                 VkDeviceMemory &memory) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE};
    if (vkCreateBuffer(device, &bi, nullptr, &buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, buffer, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
                            findMemoryType(physDevice, mr.memoryTypeBits, props)};
    if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(device, buffer, memory, 0);
    return true;
}
bool VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                 VkBuffer &buffer, VkDeviceMemory &memory) {
    return createBuffer(vkDevice_, vkPhysicalDevice_, size, usage, props, buffer, memory);
}
bool VulkanContext::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(vkDevice_, &ai, &cmd) != VK_SUCCESS) return false;
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd};
    vkQueueSubmit(vkQueue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkQueue_);
    vkFreeCommandBuffers(vkDevice_, commandPool_, 1, &cmd);
    return true;
}
VkShaderModule VulkanContext::createShaderModule(VkDevice device, const std::uint8_t *spv, std::size_t size) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, size,
                                reinterpret_cast<const uint32_t *>(spv)};
    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &ci, nullptr, &mod);
    return mod;
}
// ================================================================
// createCanvasImage — 创建持久画布 + Stencil（单份，不随 swapchain 轮转）
// ================================================================
bool VulkanContext::createCanvasImage() {
    VkExtent2D ext = swapchainExtent_;

    // ── 颜色画布（COLOR_ATTACHMENT | TRANSFER_SRC | TRANSFER_DST）──
    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = swapchainFormat_;
    imgInfo.extent = {ext.width, ext.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vkDevice_, &imgInfo, nullptr, &canvasImage_) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(vkDevice_, canvasImage_, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
                            findMemoryType(vkPhysicalDevice_, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if (vkAllocateMemory(vkDevice_, &ai, nullptr, &canvasMemory_) != VK_SUCCESS) return false;
    vkBindImageMemory(vkDevice_, canvasImage_, canvasMemory_, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = canvasImage_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = swapchainFormat_;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(vkDevice_, &vi, nullptr, &canvasView_) != VK_SUCCESS) return false;

    // ── Stencil 附件（D24S8, 1x, 单份）──
    VkImageCreateInfo sImg{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    sImg.imageType = VK_IMAGE_TYPE_2D;
    sImg.format = VK_FORMAT_D24_UNORM_S8_UINT;
    sImg.extent = {ext.width, ext.height, 1};
    sImg.mipLevels = 1;
    sImg.arrayLayers = 1;
    sImg.samples = VK_SAMPLE_COUNT_1_BIT;
    sImg.tiling = VK_IMAGE_TILING_OPTIMAL;
    sImg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    sImg.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vkDevice_, &sImg, nullptr, &canvasStencilImage_) != VK_SUCCESS) return false;
    VkMemoryRequirements smr;
    vkGetImageMemoryRequirements(vkDevice_, canvasStencilImage_, &smr);
    VkMemoryAllocateInfo sAlloc{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, smr.size,
        findMemoryType(vkPhysicalDevice_, smr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if (vkAllocateMemory(vkDevice_, &sAlloc, nullptr, &canvasStencilMemory_) != VK_SUCCESS) return false;
    vkBindImageMemory(vkDevice_, canvasStencilImage_, canvasStencilMemory_, 0);
    VkImageViewCreateInfo sView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    sView.image = canvasStencilImage_;
    sView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    sView.format = VK_FORMAT_D24_UNORM_S8_UINT;
    sView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(vkDevice_, &sView, nullptr, &canvasStencilView_) != VK_SUCCESS) return false;

    // ── 首帧初始化：vkCmdClearColorImage 清除 canvas ──
    {
        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool_,
                                         VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
        VkCommandBuffer initCb;
        vkAllocateCommandBuffers(vkDevice_, &cbai, &initCb);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(initCb, &bi);

        VkImageMemoryBarrier bar{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        bar.srcAccessMask = 0;
        bar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.image = canvasImage_;
        bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(initCb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &bar);

        VkClearColorValue clearColor{};
        clearColor.float32[0] = 0.96f;
        clearColor.float32[1] = 0.96f;
        clearColor.float32[2] = 0.96f;
        clearColor.float32[3] = 1.0f;
        VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(initCb, canvasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);

        bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(initCb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &bar);

        // 初始化 Stencil Image 布局
        VkImageMemoryBarrier stencilBar{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        stencilBar.srcAccessMask = 0;
        stencilBar.dstAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        stencilBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        stencilBar.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        stencilBar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        stencilBar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        stencilBar.image = canvasStencilImage_;
        stencilBar.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(initCb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &stencilBar);

        vkEndCommandBuffer(initCb);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &initCb;
        vkQueueSubmit(vkQueue_, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(vkQueue_);
        vkFreeCommandBuffers(vkDevice_, commandPool_, 1, &initCb);
    }
    return true;
}
// ================================================================
// destroyCanvas
// ================================================================
void VulkanContext::destroyCanvas() {
    if (canvasFramebuffer_) {
        vkDestroyFramebuffer(vkDevice_, canvasFramebuffer_, nullptr);
        canvasFramebuffer_ = VK_NULL_HANDLE;
    }
    if (canvasStencilView_) {
        vkDestroyImageView(vkDevice_, canvasStencilView_, nullptr);
        canvasStencilView_ = VK_NULL_HANDLE;
    }
    if (canvasStencilImage_) {
        vkDestroyImage(vkDevice_, canvasStencilImage_, nullptr);
        canvasStencilImage_ = VK_NULL_HANDLE;
    }
    if (canvasStencilMemory_) {
        vkFreeMemory(vkDevice_, canvasStencilMemory_, nullptr);
        canvasStencilMemory_ = VK_NULL_HANDLE;
    }
    if (canvasView_) {
        vkDestroyImageView(vkDevice_, canvasView_, nullptr);
        canvasView_ = VK_NULL_HANDLE;
    }
    if (canvasImage_) {
        vkDestroyImage(vkDevice_, canvasImage_, nullptr);
        canvasImage_ = VK_NULL_HANDLE;
    }
    if (canvasMemory_) {
        vkFreeMemory(vkDevice_, canvasMemory_, nullptr);
        canvasMemory_ = VK_NULL_HANDLE;
    }
}
// ── accessor 实现 ──
VkDevice VulkanContext::device() const {
    return vkDevice_;
}
VkQueue VulkanContext::graphicsQueue() const {
    return vkQueue_;
}
VkCommandPool VulkanContext::commandPool() const {
    return commandPool_;
}
VkRenderPass VulkanContext::renderPass() const {
    return renderPass_;
}
VkPhysicalDevice VulkanContext::physicalDevice() const {
    return vkPhysicalDevice_;
}
VkBuffer VulkanContext::vertexBuffer() const {
    return vertexBuffer_;
}
VkBuffer VulkanContext::indexBuffer() const {
    return indexBuffer_;
}