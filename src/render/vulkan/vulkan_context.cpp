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
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
}    // namespace

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                    void * /*userData*/) {
    if (!data) return VK_FALSE;
    switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: Log::error("[VK] {}", data->pMessage); break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: Log::warn("[VK] {}", data->pMessage); break;
    default: Log::debug("[VK] {}", data->pMessage); break;
    }
    return VK_FALSE;
}

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
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto destroyFn =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance_, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyFn) destroyFn(vkInstance_, debugMessenger_, nullptr);
    }
    if (vkInstance_ != VK_NULL_HANDLE) vkDestroyInstance(vkInstance_, nullptr);
    vkDevice_ = VK_NULL_HANDLE;
}
// ================================================================
// initialize — 核心初始化（Instance → Device → Swapchain → ...）
// ================================================================
bool VulkanContext::initialize(void *nativeHandle) {
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
// ================================================================
// resize — 重建 swapchain + canvas，返回 bool
// 短路判断改用 surface 实际尺寸（caps.currentExtent），事件参数 w/h 仅作日志参考；
// 成功重建后置位 justRecreated_，通知下一成功帧强制全量重绘。
// ================================================================
bool VulkanContext::resize(int w, int h) {
    // ── ① 查询 surface 当前真实尺寸（Windows 上 currentExtent 恒为窗口客户区）──
    VkSurfaceCapabilitiesKHR caps;
    VkResult capsResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
    if (capsResult != VK_SUCCESS) {
        Log::error("resize: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: {}", static_cast<int>(capsResult),
                   std::source_location::current());
        return false;
    }

    // ── ② 最小化/不可见（0×0）：不可重建，直接失败（调用方决定跳帧/重试）──
    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
        Log::debug("resize: surface 0x0 (minimized), skip (event {}x{})", w, h, std::source_location::current());
        return false;
    }

    // ── ③ 短路：以 surface 真实尺寸为准（而非事件参数，事件可能滞后）──
    //    currentExtent 为 UINT32_MAX 表示由 swapchain 决定（Win32 不会出现），此时退回事件尺寸比较
    const bool extentDefined = caps.currentExtent.width != UINT32_MAX;
    const uint32_t targetW = extentDefined ? caps.currentExtent.width : static_cast<uint32_t>(w);
    const uint32_t targetH = extentDefined ? caps.currentExtent.height : static_cast<uint32_t>(h);
    if (swapchainExtent_.width == targetW && swapchainExtent_.height == targetH) {
        Log::debug("resize: extent unchanged {}x{} (event {}x{}), skip", swapchainExtent_.width,
                   swapchainExtent_.height, w, h, std::source_location::current());
        return true;
    }

    const uint32_t oldW = swapchainExtent_.width;
    const uint32_t oldH = swapchainExtent_.height;

    vkDeviceWaitIdle(vkDevice_);

    // ── ④ 仅释放 + 重建 swapchain 和 canvas（不碰 sync objects / command buffers）──
    destroyCanvas();

    // 保留旧 swapchain 句柄，经 oldSwapchain 传入新建（spec 要求：同 surface 上存在
    // 未 retire 的 swapchain 时必须传 oldSwapchain，否则可能 NATIVE_WINDOW_IN_USE）
    VkSwapchainKHR oldSwapchain = swapchain_;
    swapchain_ = VK_NULL_HANDLE;
    for (auto &iv : swapchainImageViews_) {
        if (iv) vkDestroyImageView(vkDevice_, iv, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (!createSwapchain(oldSwapchain)) {    // ← 签名微调，见下
        Log::error("resize: createSwapchain failed ({}x{})", targetW, targetH, std::source_location::current());
        if (oldSwapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(vkDevice_, oldSwapchain, nullptr); }
        return false;
    }

    // 新链已建立，安全销毁旧链
    if (oldSwapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(vkDevice_, oldSwapchain, nullptr); }

    if (!createCanvasImage()) {
        Log::error("resize: createCanvasImage failed", std::source_location::current());
        return false;
    }
    if (!createCanvasFramebuffer()) {
        Log::error("resize: createCanvasFramebuffer failed", std::source_location::current());
        return false;
    }

    currentImageIndex_ = 0;
    frameIndex_ = 0;
    justRecreated_ = true;    // ← 下一成功帧强制全量重绘（canvas 已是 undefined/黑）

    Log::info("resize: recreated swapchain+canvas {}x{} → {}x{} (event {}x{})", oldW, oldH, swapchainExtent_.width,
              swapchainExtent_.height, w, h, std::source_location::current());
    return true;
}

// ================================================================
// createInstance — Vulkan 实例 + 平台 Surface
// ================================================================
bool VulkanContext::createInstance(void *nativeHandle) {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "KwiK UI";
    app.apiVersion = VK_API_VERSION_1_1;
    std::vector<const char *> ext = {VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(_WIN32)
    ext.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    ext.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
    ext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
#ifdef KWIK_ENABLE_VALIDATION
    if (hasValidation) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = validationLayers;
    }
#endif

    // 创建实例（若不支持 VK_EXT_debug_utils 则降级重试）
    VkResult instResult = vkCreateInstance(&ci, nullptr, &vkInstance_);
    if (instResult == VK_ERROR_EXTENSION_NOT_PRESENT) {
        ext.pop_back();    // 去掉 VK_EXT_debug_utils
        ci.enabledExtensionCount = (uint32_t)ext.size();
        instResult = vkCreateInstance(&ci, nullptr, &vkInstance_);
    }
    if (instResult != VK_SUCCESS) return false;

    // ── 注册 Debug Messenger ──
    auto vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkInstance_, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT && hasValidation) {
        VkDebugUtilsMessengerCreateInfoEXT dci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        dci.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                          | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dci.pfnUserCallback = debugCallback;
        vkCreateDebugUtilsMessengerEXT(vkInstance_, &dci, nullptr, &debugMessenger_);
    }

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

    // 启用 dualSrcBlend: LCD 子像素文字需要每通道独立混合因子
    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(vkPhysicalDevice_, &supported);
    VkPhysicalDeviceFeatures enabled{};
    enabled.dualSrcBlend = supported.dualSrcBlend;

    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = 1;
    di.ppEnabledExtensionNames = exts;
    di.pEnabledFeatures = &enabled;
    return vkCreateDevice(vkPhysicalDevice_, &di, nullptr, &vkDevice_) == VK_SUCCESS;
}
// ================================================================
// beginFrame — 获取 swapchain + 开始 canvas render pass
// 返回 FrameToken 或 nullopt（swapchain 不可用）
// ================================================================
std::optional<FrameToken> VulkanContext::beginFrame() {
    // ── ⓪ 上一帧报告过 SUBOPTIMAL：surface 与 swapchain 已不匹配 ──
    // 主动重建（resize 内部以 caps.currentExtent 为准，参数仅作日志），
    // 本帧跳过；重建置位 justRecreated_，下一帧强制全量重绘。
    if (suboptimalPending_) {
        suboptimalPending_ = false;
        Log::info("beginFrame: suboptimal pending -> recreate swapchain");
        resize(0, 0);
        return std::nullopt;
    }
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

    // ── swapchain out-of-date → 重建后跳过当前帧 ──
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
        if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
            return std::nullopt;    // 窗口最小化/不可见
        }
        Log::debug("beginFrame: swapchain out-of-date, {}x{} → {}x{}, recreating + skip frame", swapchainExtent_.width,
                   swapchainExtent_.height, caps.currentExtent.width, caps.currentExtent.height,
                   std::source_location::current());
        resize(static_cast<int>(caps.currentExtent.width), static_cast<int>(caps.currentExtent.height));
        // ── 已重建 swapchain，但当前帧命令仍按旧尺寸录制 → 跳过，下帧尺寸匹配 ──
        return std::nullopt;
    }

    if (r == VK_ERROR_SURFACE_LOST_KHR) {
        Log::error("beginFrame: VK_ERROR_SURFACE_LOST_KHR", std::source_location::current());
        return std::nullopt;
    }
    // VK_SUBOPTIMAL_KHR 视为成功，不触发 resize
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        Log::error("beginFrame: vkAcquireNextImageKHR failed: {}", static_cast<int>(r),
                   std::source_location::current());
        return std::nullopt;
    }

    // ── SUBOPTIMAL：图像本身可用（信号量已 signal，必须正常走完本帧消费它），──
    // ── 但 surface 与 swapchain 尺寸已不匹配，标记下一帧重建。              ──
    // ── 注意：不能像 OUT_OF_DATE 一样直接跳帧——跳帧会遗留已 signal 的     ──
    // ── imageAvailable 信号量，破坏后续 acquire/submit 同步。              ──
    if (r == VK_SUBOPTIMAL_KHR) {
        Log::info("beginFrame: acquire SUBOPTIMAL -> schedule recreate");
        suboptimalPending_ = true;
    }

    Log::info("beginFrame: acquired img={} slot={} result={}", currentImageIndex_, frameIndex_, (int)r);

    vkResetFences(vkDevice_, 1, &inFlightFences_[frameIndex_]);

    // ── ③ 开始录制 ──
    vkResetCommandBuffer(commandBuffers_[frameIndex_], 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
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
    vkCmdEndRenderPass(cb);

    // ── 合并 barrier: canvas(COLOR→TRANSFER_SRC) + swapchain(UNDEF→TRANSFER_DST) ──
    VkImageMemoryBarrier preBarriers[2]{};
    preBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // ── oldLayout/newLayout 改为 TRANSFER_SRC（render pass 已将 canvas 转换到此）──
    preBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    preBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    preBarriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    preBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    preBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[0].image = canvasImage_;
    preBarriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    preBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    preBarriers[1].srcAccessMask = 0;
    preBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    preBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    preBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[1].image = swapchainImages_[currentImageIndex_];
    preBarriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr, 2, preBarriers);

    // ── Copy（全屏）──
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    vkCmdCopyImage(cb, canvasImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImages_[currentImageIndex_],
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // ── 合并 barrier: canvas(TRANSFER_SRC→COLOR) + swapchain(TRANSFER_DST→PRESENT_SRC) ──
    VkImageMemoryBarrier postBarriers[2]{};
    postBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    postBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    postBarriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    postBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    postBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    postBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[0].image = canvasImage_;
    postBarriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    postBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    postBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarriers[1].dstAccessMask = 0;
    postBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    postBarriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    postBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[1].image = swapchainImages_[currentImageIndex_];
    postBarriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                         nullptr, 0, nullptr, 2, postBarriers);

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
        frameIndex_ = (frameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;    // ← 新增：保持槽位轮转
        return false;
    }

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinishedSemaphores_[frameIndex_];
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &currentImageIndex_;

    VkResult presentResult = vkQueuePresentKHR(vkQueue_, &pi);
    Log::info("present: result={} img={} slot={}", (int)presentResult, currentImageIndex_, frameIndex_);
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
        Log::warn("present: failed result={} (frame not shown)", (int)presentResult);
    }

    // present 侧 SUBOPTIMAL 同样安排下一帧重建（帧已正常呈现，仅是被拉伸）
    if (presentResult == VK_SUBOPTIMAL_KHR) { suboptimalPending_ = true; }

    frameIndex_ = (frameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
    return presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR;
}
// ======================================================================
// 交换链
// ======================================================================
bool VulkanContext::createSwapchain(VkSwapchainKHR oldSwapchain) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &caps);
    uint32_t fmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &fmtCount, fmts.data());
    // 使用 UNORM 而非 SRGB: SRGB 格式会导致硬件在线性空间混合, 使文字边缘偏软
    // UNORM + SRGB_NONLINEAR 色彩空间 = sRGB 空间混合, 与浏览器一致, 文字更清晰
    swapchainFormat_ = VK_FORMAT_R8G8B8A8_UNORM;
    for (auto &f : fmts) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM) {
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
    if (imgCount < 3) imgCount = 3;

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
    sci.oldSwapchain = oldSwapchain;
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