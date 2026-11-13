module;
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <X11/Xlib.h>
#endif

export module kwik.render.vulkan.context;
import kwik.core.types;

import std;

// ── 字形绘制 Push Constants ──
export struct GlyphPushConstants {
    float posX, posY; float sizeX, sizeY;
    float uvU0, uvV0; float uvU1, uvV1;
    float colorR, colorG, colorB, colorA;
    float viewportW, viewportH;
    float textContrast; float pageIndex;
    float m00, m01, m02;    // ← 矩阵（offset 64）
    float m10, m11, m12;    //    offset 76
    // 总计 88 字节
};
static_assert(sizeof(GlyphPushConstants) == 88, "GlyphPushConstants size mismatch");

// ── 图片绘制 Push Constants ──
export struct ImagePushConstants {
    float posX, posY; float sizeX, sizeY;
    float uvU0, uvV0; float uvU1, uvV1;
    float colorR, colorG, colorB, colorA;
    float viewportW, viewportH;
    float cornerRadius;
    float m00, m01, m02;    // ← 矩阵
    float m10, m11, m12;
    // 总计 84 字节
};
static_assert(sizeof(ImagePushConstants) == 84, "ImagePushConstants size mismatch");

// ── YUV 图像绘制 Push Constants（V1 推流批 2，镜像 shaders/image_yuv.slang）──
// = ImagePushConstants 84B + mode 4B（位表见 image_yuv.slang 文件头）。
// 着色器侧平铺声明（不嵌套结构——std430 会把嵌套结构补齐到 16 倍数致偏移错位）
export struct YuvPushConstants {
    ImagePushConstants base;    // 与 image 管线同布局（offset 0..83）
    std::uint32_t mode;              // offset 84
};
static_assert(sizeof(YuvPushConstants) == 88, "YuvPushConstants size mismatch");

// ── FrameToken: 每帧由 beginFrame 产出，子渲染器通过此对象获取 Vulkan 句柄 ──
export struct FrameToken {
    VkCommandBuffer commandBuffer;
    VkExtent2D extent;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
};

// ── DeviceContext: 子渲染器 upload 操作所需的最小设备句柄集 ──
export struct DeviceContext {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkCommandPool commandPool;
    VkQueue queue;
    // 宿主内存导入（VK_EXT_external_memory_host，V1 推流批 3 零拷贝）：
    // 扩展未启用时为空——调用方据此走 memcpy 拷贝路径
    PFN_vkGetMemoryHostPointerPropertiesEXT getHostPointerProps = nullptr;
    VkDeviceSize hostImportAlignment = 0;    // minImportedHostPointerAlignment（地址与尺寸均须满足）
};

export class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    /** @brief 在飞帧上限（三缓冲槽位数）；延迟释放类队列的安全帧距以此为准 */
    static constexpr std::uint32_t kMaxFramesInFlight = 3;

    // ── 生命周期 ──
    bool initialize(void *nativeHandle);
    void shutdown();
    bool resize(int width, int height);    // 返回 bool

    // ── 帧控制 ──
    std::optional<FrameToken> beginFrame();
    void endFrame();
    bool present();
    void accumulateDirtyRect(const Rect &dirtyRect);   // 累积脏区 + 首帧回退

    /** @brief 帧首预处理钩子（V1 推流）：beginFrame 内、fence 等待与命令缓冲
     *         开始之后、主 render pass 之前回调——录 transfer 类命令的唯一
     *         合法窗口（pass 内不可录 transfer）。置空清除。 */
    void setPrePassHook(std::function<void(VkCommandBuffer)> hook) { prePassHook_ = std::move(hook); }

    /** @brief 宿主内存导入是否可用（扩展受支持且未被 KWIK_VK_NO_HOST_IMPORT=1 禁用） */
    bool hostImportEnabled() const { return getHostPointerProps_ != nullptr; }
    /** @brief vkGetMemoryHostPointerPropertiesEXT（未启用为空） */
    PFN_vkGetMemoryHostPointerPropertiesEXT hostPointerPropsFn() const { return getHostPointerProps_; }
    /** @brief 导入地址/尺寸对齐要求（未启用为 0） */
    VkDeviceSize hostImportAlignment() const { return hostImportAlignment_; }

    // ── 资源访问（仅子渲染器 setup / upload 时用） ──
    VkDevice device() const;
    VkQueue graphicsQueue() const;
    VkCommandPool commandPool() const;
    VkRenderPass renderPass() const;
    VkPhysicalDevice physicalDevice() const;
    VkPipelineCache pipelineCache() const;    // 管线工厂共用（§四）
    VkBuffer vertexBuffer() const;    // 仅初始化时用
    VkBuffer indexBuffer() const;     // 仅初始化时用

    // ── 工具 ──
    static std::uint32_t findMemoryType(VkPhysicalDevice physDevice, std::uint32_t typeFilter, VkMemoryPropertyFlags props);
    static bool createBuffer(VkDevice device, VkPhysicalDevice physDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags props, VkBuffer &buffer, VkDeviceMemory &memory);
    bool copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    static VkShaderModule createShaderModule(VkDevice device, const std::uint8_t *spv, std::size_t size);
    /** @brief 一次性命令缓冲骨架：alloc→begin→record→end→submit→wait→free。
     *         清单 §四样板收口（原 5 处手写：纹理上传/图集过渡/画布初始清屏等）。 */
    static bool runOneOff(VkDevice device, VkCommandPool pool, VkQueue queue,
                          const std::function<void(VkCommandBuffer)> &record);
    // ── 便捷版（使用内部 device/physicalDevice） ──
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer &buffer,
                      VkDeviceMemory &memory);

    bool consumeJustRecreated() {
        bool v = justRecreated_;
        justRecreated_ = false;
        return v;
    }

    /** @brief 立即结束当前主 render pass（canvas → TRANSFER_SRC）。液态玻璃帧中途中断用。 */
    void endRenderPass();

    /** @brief 重启主 render pass（TRANSFER_SRC→COLOR_ATTACHMENT barrier + LOAD_OP_LOAD 保留画布 + 复位动态状态） */
    void beginMainRenderPass();

    /** @brief 当前主色画布 / 宽 / 高（backdrop 捕获用） */
    VkImage canvasImage() const { return canvasImage_; }
    std::uint32_t canvasWidth() const { return swapchainExtent_.width; }
    std::uint32_t canvasHeight() const { return swapchainExtent_.height; }

    /** @brief 当前帧槽（0..kMaxFramesInFlight-1，present 后轮转；resize 中
     *  waitIdle 后归零）——staging 分槽渲染器据此对齐 fence 等待周期 */
    std::uint32_t frameIndex() const { return frameIndex_; }

private:
    VkFormat depthStencilFormat_ = VK_FORMAT_D24_UNORM_S8_UINT;
    std::function<void(VkCommandBuffer)> prePassHook_;    ///< 帧首预处理（见 setPrePassHook；渲染线程调用）
    PFN_vkGetMemoryHostPointerPropertiesEXT getHostPointerProps_ = nullptr;    ///< 宿主内存导入（未启用为空）
    VkDeviceSize hostImportAlignment_ = 0;                                     ///< 导入对齐要求
    VkInstance vkInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice vkDevice_ = VK_NULL_HANDLE;
    VkQueue vkQueue_ = VK_NULL_HANDLE;
    std::uint32_t queueFamilyIndex_ = 0;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent2D swapchainExtent_ = {800, 600};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;    ///< 管线工厂共用（§四；进程内加速派生管线创建）
    std::vector<VkCommandBuffer> commandBuffers_;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    std::uint32_t frameIndex_ = 0;
    std::uint32_t currentImageIndex_ = 0;
    VkImage canvasImage_ = VK_NULL_HANDLE;
    VkDeviceMemory canvasMemory_ = VK_NULL_HANDLE;
    VkImageView canvasView_ = VK_NULL_HANDLE;
    VkImage canvasStencilImage_ = VK_NULL_HANDLE;
    VkDeviceMemory canvasStencilMemory_ = VK_NULL_HANDLE;
    VkImageView canvasStencilView_ = VK_NULL_HANDLE;
    VkFramebuffer canvasFramebuffer_ = VK_NULL_HANDLE;

    bool justRecreated_ = false;        ///< swapchain/canvas 刚重建，首个成功帧需全量重绘
    bool suboptimalPending_ = false;    ///< acquire/present 报告过 SUBOPTIMAL，下帧主动重建
                                        ///< （部分驱动尺寸不匹配时只报 SUBOPTIMAL 不报 OUT_OF_DATE，
                                        ///<   若无视会导致画面被驱动拉伸——本次拉伸 bug 的防御位）

    std::vector<Rect> accumulatedDirtyRects_;             // per-SC-image 脏区并集
    std::vector<VkImageLayout> swapchainImageLayouts_;    // per-SC-image layout 追踪

    bool createInstance(void *nativeHandle);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createCanvasImage();
    void destroyCanvas();
    bool createSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void cleanupSwapchain();
    bool createRenderPass();
    bool createCanvasFramebuffer();
    bool createVertexBuffer();
    bool createCommandBuffers();
    bool createSyncObjects();
#if defined(__linux__)
    Display *x11Display_ = nullptr;
#endif
};