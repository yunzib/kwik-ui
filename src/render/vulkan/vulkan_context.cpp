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
} // namespace
// ================================================================
// 析构 / shutdown
// ================================================================
VulkanContext::~VulkanContext() {
    shutdown();
}
void VulkanContext::shutdown() {
    if (vkDevice_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(vkDevice_);
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
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(vkPhysicalDevice_, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts;
    if (counts & VK_SAMPLE_COUNT_4_BIT)
        msaaSamples_ = VK_SAMPLE_COUNT_4_BIT;
    else if (counts & VK_SAMPLE_COUNT_2_BIT)
        msaaSamples_ = VK_SAMPLE_COUNT_2_BIT;
    else
        msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;
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
    if (!createFramebuffers()) {
        shutdown();
        return false;
    }
    if (!createCommandBuffers()) {
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
    cleanupSwapchain();
    if (!createSwapchain()) return false;
    if (!createFramebuffers()) return false;
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
// beginFrame — 获取交换链图像 + 开始渲染通道录制
// 返回 true: 成功; false: 交换链失效需重建
// ================================================================
bool VulkanContext::beginFrame() {
    VkResult fenceWait = vkWaitForFences(vkDevice_, 1, &inFlightFences_[frameIndex_], VK_TRUE, 1'000'000'000);
    if (fenceWait == VK_TIMEOUT) return false;
    VkResult r = vkAcquireNextImageKHR(vkDevice_, swapchain_, 1'000'000'000, imageAvailableSemaphores_[frameIndex_],
                                       VK_NULL_HANDLE, &currentImageIndex_);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_ERROR_SURFACE_LOST_KHR) return false;
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return false;
    vkResetFences(vkDevice_, 1, &inFlightFences_[frameIndex_]);
    vkResetCommandBuffer(commandBuffers_[currentImageIndex_], 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffers_[currentImageIndex_], &bi);
    VkRenderPassBeginInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpInfo.renderPass = renderPass_;
    rpInfo.framebuffer = framebuffers_[currentImageIndex_];
    rpInfo.renderArea.extent = swapchainExtent_;
    VkClearValue cvs[3];
    cvs[0].color = {{0.96f, 0.96f, 0.96f, 1.0f}}; // 颜色附件清屏
    cvs[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};    // resolve 附件
    cvs[2].depthStencil = {1.0f, 0};              // stencil 清 0
    rpInfo.clearValueCount = 3;                   // 2→3
    rpInfo.pClearValues = cvs;
    vkCmdBeginRenderPass(commandBuffers_[currentImageIndex_], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp{0, 0, (float)swapchainExtent_.width, (float)swapchainExtent_.height, 0.0f, 1.0f};
    vkCmdSetViewport(commandBuffers_[currentImageIndex_], 0, 1, &vp);
    VkRect2D scissor{{0, 0}, swapchainExtent_};
    vkCmdSetScissor(commandBuffers_[currentImageIndex_], 0, 1, &scissor);

    // ── 初始化 stencil 动态状态: compareMask=0 → EQUAL 恒成立 ──
    // (stencilRef & 0x00) == (bufferStencil & 0x00) → 0==0 → always pass
    VkCommandBuffer cb = commandBuffers_[currentImageIndex_];
    vkCmdSetStencilReference(cb, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
    vkCmdSetStencilCompareMask(cb, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
    vkCmdSetStencilWriteMask(cb, VK_STENCIL_FACE_FRONT_AND_BACK, 0x00);
    return true;
}

void VulkanContext::endFrame() {
    vkCmdEndRenderPass(commandBuffers_[currentImageIndex_]);
    vkEndCommandBuffer(commandBuffers_[currentImageIndex_]);
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
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    // MSAA 颜色缓冲
    msaaImages_.resize(n);
    msaaMemories_.resize(n);
    msaaViews_.resize(n);
    for (uint32_t i = 0; i < n; i++) {
        VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = swapchainFormat_;
        imgInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = msaaSamples_;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(vkDevice_, &imgInfo, nullptr, &msaaImages_[i]) != VK_SUCCESS) return false;
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(vkDevice_, msaaImages_[i], &mr);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
                                findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
        if (vkAllocateMemory(vkDevice_, &ai, nullptr, &msaaMemories_[i]) != VK_SUCCESS) return false;
        vkBindImageMemory(vkDevice_, msaaImages_[i], msaaMemories_[i], 0);
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = msaaImages_[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = swapchainFormat_;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(vkDevice_, &vi, nullptr, &msaaViews_[i]) != VK_SUCCESS) return false;
    }

    // ── Stencil 附件 (D24_UNORM_S8_UINT, 每 swapchain 图像一份) ──
    stencilImages_.resize(n);
    stencilMemories_.resize(n);
    stencilViews_.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        VkImageCreateInfo sImg{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        sImg.imageType = VK_IMAGE_TYPE_2D;
        sImg.format = VK_FORMAT_D24_UNORM_S8_UINT;
        sImg.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        sImg.mipLevels = 1;
        sImg.arrayLayers = 1;
        sImg.samples = msaaSamples_; // ─ 与 MSAA 相同采样率
        sImg.tiling = VK_IMAGE_TILING_OPTIMAL;
        sImg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        sImg.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(vkDevice_, &sImg, nullptr, &stencilImages_[i]) != VK_SUCCESS) return false;
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(vkDevice_, stencilImages_[i], &mr);
        VkMemoryAllocateInfo sAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
                                    findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
        if (vkAllocateMemory(vkDevice_, &sAlloc, nullptr, &stencilMemories_[i]) != VK_SUCCESS) return false;
        vkBindImageMemory(vkDevice_, stencilImages_[i], stencilMemories_[i], 0);
        VkImageViewCreateInfo sView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        sView.image = stencilImages_[i];
        sView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        sView.format = VK_FORMAT_D24_UNORM_S8_UINT;
        sView.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(vkDevice_, &sView, nullptr, &stencilViews_[i]) != VK_SUCCESS) return false;
    }
    return true;
}
void VulkanContext::cleanupSwapchain() {
    for (auto &fb : framebuffers_)
        if (fb) vkDestroyFramebuffer(vkDevice_, fb, nullptr);
    framebuffers_.clear();
    for (auto &iv : swapchainImageViews_)
        if (iv) vkDestroyImageView(vkDevice_, iv, nullptr);
    swapchainImageViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vkDevice_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    swapchainImages_.clear();
    for (auto &v : msaaViews_)
        if (v) vkDestroyImageView(vkDevice_, v, nullptr);
    msaaViews_.clear();
    for (auto &img : msaaImages_)
        if (img) vkDestroyImage(vkDevice_, img, nullptr);
    msaaImages_.clear();
    for (auto &m : msaaMemories_)
        if (m) vkFreeMemory(vkDevice_, m, nullptr);
    msaaMemories_.clear();
    // ── Stencil 清理 ──────────────────────────────────────
    for (auto &v : stencilViews_)
        if (v) vkDestroyImageView(vkDevice_, v, nullptr);
    stencilViews_.clear();
    for (auto &img : stencilImages_)
        if (img) vkDestroyImage(vkDevice_, img, nullptr);
    stencilImages_.clear();
    for (auto &m : stencilMemories_)
        if (m) vkFreeMemory(vkDevice_, m, nullptr);
    stencilMemories_.clear();
}
// ======================================================================
// 渲染通道 — MSAA color + Resolve
// ======================================================================
bool VulkanContext::createRenderPass() {
    VkAttachmentDescription msaaAtt{};
    msaaAtt.format = swapchainFormat_;
    msaaAtt.samples = msaaSamples_;
    msaaAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    msaaAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    msaaAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    msaaAtt.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentDescription resolveAtt{};
    resolveAtt.format = swapchainFormat_;
    resolveAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    resolveAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolveAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolveAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // ── Stencil 附件 (MSAA 分辨率, 仅关注 stencil 分量) ──
    VkAttachmentDescription stencilAtt{};
    stencilAtt.format = VK_FORMAT_D24_UNORM_S8_UINT;
    stencilAtt.samples = msaaSamples_;
    stencilAtt.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 每帧清 0
    stencilAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencilAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolveRef{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference stencilRef{2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pResolveAttachments = &resolveRef;
    subpass.pDepthStencilAttachment = &stencilRef;                      // ← 绑定
    VkAttachmentDescription atts[] = {msaaAtt, resolveAtt, stencilAtt}; // 2→3
    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 3; // 2→3
    rpInfo.pAttachments = atts;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    return vkCreateRenderPass(vkDevice_, &rpInfo, nullptr, &renderPass_) == VK_SUCCESS;
}

bool VulkanContext::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView atts[] = {msaaViews_[i], swapchainImageViews_[i], stencilViews_[i]}; // 2→3
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass = renderPass_;
        fb.attachmentCount = 3; // 2→3
        fb.pAttachments = atts;
        fb.width = swapchainExtent_.width;
        fb.height = swapchainExtent_.height;
        fb.layers = 1;
        if (vkCreateFramebuffer(vkDevice_, &fb, nullptr, &framebuffers_[i]) != VK_SUCCESS) return false;
    }
    return true;
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
