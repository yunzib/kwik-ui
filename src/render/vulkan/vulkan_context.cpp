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
// ── 单位四边形 (所有管线复用) ──────────────────────────────────
namespace {
const float kQuadVertices[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
const uint16_t kQuadIndices[] = {0, 1, 2, 0, 2, 3};
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
// initialize — 核心初始化 (Instance → Device → Swapchain → ...)
// ================================================================
bool VulkanContext::initialize(void *nativeHandle, int width, int height) {
    width_ = width;
    height_ = height;
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

bool VulkanContext::resize(int w, int h) {
    if (width_ == w && height_ == h) return true;
    width_ = w;
    height_ = h;
    vkDeviceWaitIdle(vkDevice_);
    destroyCanvas();
    cleanupSwapchain();
    if (!createSwapchain()) return false;
    if (!createCanvasImage()) return false;
    if (!createCanvasFramebuffer()) return false;
    // ── 重建命令缓冲和同步对象 (swapchain 图像数可能变化) ──
    vkFreeCommandBuffers(vkDevice_, commandPool_, (uint32_t)commandBuffers_.size(), commandBuffers_.data());
    if (!createCommandBuffers()) return false;
    for (auto &s : imageAvailableSemaphores_)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, s, nullptr);
    for (auto &s : renderFinishedSemaphores_)
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(vkDevice_, s, nullptr);
    for (auto &f : inFlightFences_)
        if (f != VK_NULL_HANDLE) vkDestroyFence(vkDevice_, f, nullptr);
    if (!createSyncObjects()) return false;
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

    // 改为条件启用：
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
// pickPhysicalDevice — 选取第一个 GPU
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
// beginFrame — 获取 swapchain + 开始 canvas render pass + 设 scissor
// dirtyRect: 脏区域 (逻辑坐标, 已由调用方乘以 dpi 转换)
// ================================================================
bool VulkanContext::beginFrame(const Rect &dirtyRect) {
    // ① 等待 fence + 获取 swapchain 图像
    VkResult fenceWait = vkWaitForFences(vkDevice_, 1, &inFlightFences_[frameIndex_], VK_TRUE, 1'000'000'000);
    if (fenceWait == VK_TIMEOUT) return false;
    VkResult r = vkAcquireNextImageKHR(vkDevice_, swapchain_, 1'000'000'000, imageAvailableSemaphores_[frameIndex_],
                                       VK_NULL_HANDLE, &currentImageIndex_);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_ERROR_SURFACE_LOST_KHR) return false;
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return false;
    vkResetFences(vkDevice_, 1, &inFlightFences_[frameIndex_]);

    // ② 开始录制
    vkResetCommandBuffer(commandBuffers_[currentImageIndex_], 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffers_[currentImageIndex_], &bi);

    // ③ 开始 canvas render pass (LOAD_OP_LOAD, 全屏 renderArea)
    VkRenderPassBeginInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpInfo.renderPass = renderPass_;
    rpInfo.framebuffer = canvasFramebuffer_;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapchainExtent_;
    VkClearValue cvs[2];
    cvs[0].color = {{0.96f, 0.96f, 0.96f, 1.0f}};    // 备用
    cvs[1].depthStencil = {1.0f, 0};
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = cvs;
    vkCmdBeginRenderPass(commandBuffers_[currentImageIndex_], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ④ viewport 全屏, scissor 脏区域
    VkViewport vp{0.0f, 0.0f, static_cast<float>(swapchainExtent_.width), static_cast<float>(swapchainExtent_.height),
                  0.0f, 1.0f};
    vkCmdSetViewport(commandBuffers_[currentImageIndex_], 0, 1, &vp);
    int32_t sx = std::max(0, static_cast<int32_t>(dirtyRect.x));
    int32_t sy = std::max(0, static_cast<int32_t>(dirtyRect.y));
    uint32_t sw = std::max(1u, static_cast<uint32_t>(std::ceil(dirtyRect.width)));
    uint32_t sh = std::max(1u, static_cast<uint32_t>(std::ceil(dirtyRect.height)));
    VkRect2D sc{{sx, sy}, {sw, sh}};
    vkCmdSetScissor(commandBuffers_[currentImageIndex_], 0, 1, &sc);

    // ⑤ 初始化 stencil 状态
    VkCommandBuffer cb = commandBuffers_[currentImageIndex_];
    vkCmdSetStencilReference(cb, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
    vkCmdSetStencilCompareMask(cb, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
    vkCmdSetStencilWriteMask(cb, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
    return true;
}

// Vulkan 规范 (Vulkan 1.4.310, §8.3): vkCmdBlitImage 要求源图像在 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL、
// 目标在 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL。VK_ACCESS_TRANSFER_READ_BIT 和 VK_ACCESS_TRANSFER_WRITE_BIT
// 分别在源和目标阶段使用。 VK_PIPELINE_STAGE_TRANSFER_BIT 是 blit 的执行管线阶段。
void VulkanContext::endFrame() {
    VkCommandBuffer cb = commandBuffers_[currentImageIndex_];

    // ① 结束 canvas render pass (canvas → TRANSFER_SRC_OPTIMAL)
    vkCmdEndRenderPass(cb);

    // ② Swapchain barrier: UNDEFINED → TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier swapBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    swapBarrier.srcAccessMask = 0;
    swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapBarrier.image = swapchainImages_[currentImageIndex_];
    swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,    // render pass 保证完成
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

    // ③ Blit: canvas → swapchain (全屏)
    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    VkExtent3D ext = {swapchainExtent_.width, swapchainExtent_.height, 1};
    blitRegion.srcOffsets[1] = {static_cast<int32_t>(ext.width), static_cast<int32_t>(ext.height), 1};
    blitRegion.dstOffsets[1] = {static_cast<int32_t>(ext.width), static_cast<int32_t>(ext.height), 1};
    vkCmdBlitImage(cb, canvasImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImages_[currentImageIndex_],
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

    // ④ Swapchain barrier: TRANSFER_DST → PRESENT_SRC
    swapBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.dstAccessMask = 0;
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &swapBarrier);

    // ⑤ Canvas barrier: TRANSFER_SRC → COLOR_ATTACHMENT (下一帧 render pass 初始 layout)
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
void VulkanContext::present() {
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &imageAvailableSemaphores_[frameIndex_];
    si.pWaitDstStageMask = stages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &commandBuffers_[currentImageIndex_];
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinishedSemaphores_[frameIndex_];
    vkQueueSubmit(vkQueue_, 1, &si, inFlightFences_[frameIndex_]);
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinishedSemaphores_[frameIndex_];
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &currentImageIndex_;
    vkQueuePresentKHR(vkQueue_, &pi);
    frameIndex_ = (frameIndex_ + 1) % (uint32_t)imageAvailableSemaphores_.size();
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
        swapchainExtent_ = {std::clamp((uint32_t)width_, caps.minImageExtent.width, caps.maxImageExtent.width),
                            std::clamp((uint32_t)height_, caps.minImageExtent.height, caps.maxImageExtent.height)};
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
    // (MSAA + stencil 多缓冲已移至 createCanvasImage，单份持久)
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
// ======================================================================
// 渲染通道 — Canvas 颜色 (1x, LOAD_OP_LOAD) + Stencil (1x, CLEAR)
// ======================================================================
bool VulkanContext::createRenderPass() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format = swapchainFormat_;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;    // ─ 保留 canvas 内容 ─
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;    // 后续 blit 用

    VkAttachmentDescription stencilAtt{};
    stencilAtt.format = VK_FORMAT_D24_UNORM_S8_UINT;
    stencilAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    stencilAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // ─ 每帧清 0 ─
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
    // ─ 无 resolve 附件 ─

    VkAttachmentDescription atts[] = {colorAtt, stencilAtt};
    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = atts;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    return vkCreateRenderPass(vkDevice_, &rpInfo, nullptr, &renderPass_) == VK_SUCCESS;
}

// ======================================================================
// createCanvasFramebuffer — 单份 (持久 canvas)
// ======================================================================
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

// ======================================================================
// 顶点 / 索引缓冲
// ======================================================================
bool VulkanContext::createVertexBuffer() {
    VkDeviceSize vbSize = sizeof(kQuadVertices);
    VkDeviceSize ibSize = sizeof(kQuadIndices);
    VkBuffer stagingVB, stagingIB;
    VkDeviceMemory stagingVBMem, stagingIBMem;
    // ── Vertex Staging ──
    if (!createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingVB,
                      stagingVBMem))
        return false;
    // ── Index Staging ──
    if (!createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingIB,
                      stagingIBMem)) {
        vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
        vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
        return false;
    }
    // ── Upload vertex data ──
    void *data;
    vkMapMemory(vkDevice_, stagingVBMem, 0, vbSize, 0, &data);
    std::memcpy(data, kQuadVertices, vbSize);
    vkUnmapMemory(vkDevice_, stagingVBMem);
    // ── Upload index data ──
    vkMapMemory(vkDevice_, stagingIBMem, 0, ibSize, 0, &data);
    std::memcpy(data, kQuadIndices, ibSize);
    vkUnmapMemory(vkDevice_, stagingIBMem);
    // ── GPU-local vertex/index buffers ──
    if (!createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_)
        || !createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_)) {
        // 清理 staging
        vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
        vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
        vkDestroyBuffer(vkDevice_, stagingIB, nullptr);
        vkFreeMemory(vkDevice_, stagingIBMem, nullptr);
        return false;
    }
    // ── Copy staging → GPU local ──
    copyBuffer(stagingVB, vertexBuffer_, vbSize);
    copyBuffer(stagingIB, indexBuffer_, ibSize);
    // ── Cleanup staging ──
    vkDestroyBuffer(vkDevice_, stagingVB, nullptr);
    vkFreeMemory(vkDevice_, stagingVBMem, nullptr);
    vkDestroyBuffer(vkDevice_, stagingIB, nullptr);
    vkFreeMemory(vkDevice_, stagingIBMem, nullptr);
    return true;
}

bool VulkanContext::createCommandBuffers() {
    // ── 先销毁旧池 (resize 场景下避免泄漏) ──
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vkDevice_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }

    VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cp.queueFamilyIndex = queueFamilyIndex_;
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(vkDevice_, &cp, nullptr, &commandPool_) != VK_SUCCESS) return false;

    commandBuffers_.resize(swapchainImageViews_.size());
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)commandBuffers_.size();
    return vkAllocateCommandBuffers(vkDevice_, &ai, commandBuffers_.data()) == VK_SUCCESS;
}
// 为每张图像创建独立信号量
bool VulkanContext::createSyncObjects() {
    uint32_t n = (uint32_t)swapchainImages_.size();
    imageAvailableSemaphores_.resize(n);
    renderFinishedSemaphores_.resize(n);
    inFlightFences_.resize(n);
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < n; i++) {
        if (vkCreateSemaphore(vkDevice_, &si, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(vkDevice_, &si, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(vkDevice_, &fi, nullptr, &inFlightFences_[i]) != VK_SUCCESS) return false;
    }
    return true;
}
// ======================================================================
// 工具函数
// ======================================================================
uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((typeFilter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    return 0;
}
bool VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                 VkBuffer &buffer, VkDeviceMemory &memory) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE};
    if (vkCreateBuffer(vkDevice_, &bi, nullptr, &buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(vkDevice_, buffer, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
                            findMemoryType(mr.memoryTypeBits, props)};
    if (vkAllocateMemory(vkDevice_, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(vkDevice_, buffer, memory, 0);
    return true;
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
// createCanvasImage — 创建持久画布 + Stencil (单份, 不随 swapchain 轮转)
// ================================================================
bool VulkanContext::createCanvasImage() {
    VkExtent2D ext = swapchainExtent_;

    // ① 颜色画布 (COLOR_ATTACHMENT | TRANSFER_SRC)
    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = swapchainFormat_;
    imgInfo.extent = {ext.width, ext.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;    // ─ 无 MSAA ─
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vkDevice_, &imgInfo, nullptr, &canvasImage_) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(vkDevice_, canvasImage_, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
                            findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if (vkAllocateMemory(vkDevice_, &ai, nullptr, &canvasMemory_) != VK_SUCCESS) return false;
    vkBindImageMemory(vkDevice_, canvasImage_, canvasMemory_, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = canvasImage_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = swapchainFormat_;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(vkDevice_, &vi, nullptr, &canvasView_) != VK_SUCCESS) return false;

    // ② Stencil 附件 (D24S8, 1x, 单份)
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
    VkMemoryAllocateInfo sAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, smr.size,
                                findMemoryType(smr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if (vkAllocateMemory(vkDevice_, &sAlloc, nullptr, &canvasStencilMemory_) != VK_SUCCESS) return false;
    vkBindImageMemory(vkDevice_, canvasStencilImage_, canvasStencilMemory_, 0);
    VkImageViewCreateInfo sView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    sView.image = canvasStencilImage_;
    sView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    sView.format = VK_FORMAT_D24_UNORM_S8_UINT;
    sView.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(vkDevice_, &sView, nullptr, &canvasStencilView_) != VK_SUCCESS) return false;

    // ③ 首帧初始化: 用 vkCmdClearColorImage 清除 canvas (无临时 RP/FB)
    {
        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, commandPool_,
                                         VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
        VkCommandBuffer initCb;
        vkAllocateCommandBuffers(vkDevice_, &cbai, &initCb);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(initCb, &bi);

        // barrier: UNDEFINED → TRANSFER_DST_OPTIMAL
        VkImageMemoryBarrier bar{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        bar.srcAccessMask    = 0;
        bar.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        bar.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.image            = canvasImage_;
        bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(initCb,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &bar);

        // clear canvas to background color
        VkClearColorValue clearColor{};
        clearColor.float32[0] = 0.96f;
        clearColor.float32[1] = 0.96f;
        clearColor.float32[2] = 0.96f;
        clearColor.float32[3] = 1.0f;
        VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(initCb, canvasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clearColor, 1, &clearRange);

        // barrier: TRANSFER_DST_OPTIMAL → COLOR_ATTACHMENT_OPTIMAL
        bar.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        bar.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(initCb,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &bar);

        vkEndCommandBuffer(initCb);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &initCb;
        vkQueueSubmit(vkQueue_, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(vkQueue_);
        vkFreeCommandBuffers(vkDevice_, commandPool_, 1, &initCb);
    }
    return true;
}

// ================================================================
// destroyCanvas — 销毁 canvas 颜色 + Stencil + Framebuffer
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