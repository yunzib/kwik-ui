/**
 * @file vulkan_triangle_renderer.cppm
 * @brief Vulkan 纯色三角形网格渲染器
 *
 * 接收 FillTrianglesCmd / StrokeTrianglesCmd 中的顶点数据，
 * 上传到 GPU 并绘制。使用 push constants 传递颜色和视口信息。
 */

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

export module kwik.render.vulkan.triangle_renderer;

import kwik.core.types;
import kwik.core.path;
import kwik.render.command;
import kwik.render.vulkan.context;    // kMaxFramesInFlight（分槽唯一事实源）

import std;

/**
 * @brief 三角形网格渲染器
 *
 * Pipeline 配置:
 *   - 顶点输入: float2 (R32G32_SFLOAT)
 *   - 图元: TRIANGLE_LIST
 *   - Push constants: { color(vec4), opacity(float), pad, viewport(vec2)
 *   - 混合: SRC_ALPHA / ONE_MINUS_SRC_ALPHA
 */
export class TriangleRenderer {
public:
    TriangleRenderer() = default;
    ~TriangleRenderer();

    TriangleRenderer(const TriangleRenderer &) = delete;
    TriangleRenderer &operator=(const TriangleRenderer &) = delete;

    /**
     * @brief 创建 pipeline、pipeline layout 和 host-visible 顶点缓存
     * @param device       Vulkan 设备
     * @param physDevice   Vulkan 物理设备
     * @param renderPass   渲染通道
     * @param vertexBuffer 预分配的顶点缓冲区（当前仅 rect 使用，triangle 使用自有缓存）
     * @param indexBuffer  预分配的索引缓冲区（暂不使用）
     * @return 成功返回 true
     */
    bool create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                VkBuffer indexBuffer);

    /** @brief 销毁 pipeline、layout 和 host-visible 顶点缓存 */
    void destroy();

    /** @brief 开始一帧顶点写入（由 VulkanBackend::beginFrame 以 ctx 帧槽调用）：
     *  staging 按 in-flight 帧分槽，写入区间 = [slotBase, slotBase+slotCapacity)，
     *  帧首 fence 等待已保证上一次使用本槽的帧（3 帧前）GPU 执行完毕，CPU
     *  重写不与在飞帧 GPU 读竞争（原每帧从 0 覆写，复用距 1 帧 < fence 距
     *  3 帧，高负载下花屏）；同时回收到期的扩容旧缓冲 */
    void resetOffset(std::uint32_t frameSlot) {
        flushPendingDestroy();
        curSlot_ = frameSlot % kFrameSlots;
        curSlotBase_ = curSlot_ * slotCapacity();
        writeOffset_ = curSlotBase_;
        hasPending_ = false;    // 跨帧不保留未提交合批（新帧新命令缓冲）
    }

    /**
     * @brief 绘制三角形列表（同 push constants 的连续命令自动合批为一次
     *        vkCmdDraw——bind/pushConstants/viewport 仅在换参数时重发）
     * @param cmd      命令缓冲区
     * @param extent   视口尺寸
     * @param vertices float2 数组 (每 3 个一组 = 1 个三角形)
     * @param color    填充颜色
     * @param alpha    全局透明度
     * @param sweep    非空时启用 Sweep 扫描渐变（push constant 传圆心/角度/终点色）
     *
     * 顶点数据通过 memcpy 写入内部 host-visible 顶点缓冲，
     * 避免在 render pass 内使用 vkCmdUpdateBuffer。
     * 合批语义：pending 区间 [pendingOffset_, pendingCount_) 在
     *   ① push constants 变化 ② 非三角形绘制插入（backend 于其它绘制前调
     *   flushPending）③ render pass 结束 三种时机统一冲刷——绘制次序与
     *   逐命令立即绘制完全一致（命令序 = 混合序），像素零差异。
     */
    void drawTriangles(VkCommandBuffer cmd, VkExtent2D extent, const AAVertex *vertices, std::uint32_t vertexCount,
                       const Color &color, float alpha, const Transform2D &t, const SweepGrad *sweep = nullptr);
    /**
     * @brief 绘制 SDF 圆环（UberSDF 同款：CPU 仅生成 6 顶点 quad，几何/渐变/端帽全在 shader）
     * @param alpha 全局透明度（clip 衰减，与 drawTriangles 语义一致）
     */
    void drawRing(VkCommandBuffer cmd, VkExtent2D extent, const FillRingCmd &ring, float alpha);

    /** @brief 冲刷未提交的合批区间（backend 在任何非三角形绘制前与帧末调用；
     *  无 pending 时零操作） */
    void flushPending(VkCommandBuffer cmd);

    /** @brief 获取 pipeline layout（供外部使用） */
    VkPipelineLayout layout() const { return pipelineLayout_; }

private:
    /**
     * @brief Push constants 布局（必须与 triangle.slang 对齐，128B）
     * 自匿名命名空间迁入类内（合批需持有 pending 快照成员）
     */
    struct PushConstants {
        float r, g, b, a;      // color      (offset 0)
        float viewportW;       // viewport   (offset 16)
        float viewportH;       //            (offset 20)
        float opacity;         //            (offset 24)
        float _pad0, _pad1;    // pad        (offset 28)
        float m00, m01, m02, m10, m11, m12;   // 矩阵（36B→60B）
        // ── Sweep 渐变 / SDF 圆环（gradMode==1/2 时启用）──
        float gradMode;        // 0=纯色 1=扫描渐变 2=SDF圆环 (offset 60)
        float gradCx, gradCy;  // 圆心（本地逻辑坐标）    (offset 64)
        float gradA0, gradA1;  // 起止角（弧度）          (offset 72)
        float gc1r, gc1g, gc1b, gc1a;   // 渐变终点色    (offset 80)
        // ── SDF 圆环参数（gradMode==2 时启用）──
        float ringMidR;        // 圆环中径（本地逻辑坐标） (offset 96)
        float ringHalfW;       // 半带宽（本地逻辑坐标）   (offset 100)
        float ringCapMode;     // 0=圆头 1=平头           (offset 104)
        float _pad3, _pad4, _pad5, _pad6, _pad7;         // pad  → 128B
    };

    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;          ///< host-visible 顶点缓冲
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;    ///< 缓冲内存
    void *mappedData_ = nullptr;                       ///< 映射指针
    std::size_t bufferCapacity_ = 0;                        ///< 缓冲容量（全槽合计）
    std::size_t writeOffset_ = 0;                           ///< 当前帧顶点写入偏移（帧内逐命令累加）
    std::size_t curSlotBase_ = 0;                           ///< 当前帧槽基址（resetOffset 设定）
    std::uint32_t curSlot_ = 0;                             ///< 当前帧槽号（扩容迁移数据用）

    // ── 合批状态（同 push constants 连续三角形命令并成一次 vkCmdDraw）──
    bool hasPending_ = false;          ///< 有未提交合批区间
    PushConstants pendingPc_{};        ///< pending 区间的 push constants 快照
    VkExtent2D pendingExtent_{};       ///< pending 视口（pc 已含，冗余存防 memcmp 假同）
    VkDeviceSize pendingOffset_ = 0;   ///< pending 区间在 staging 中的起始偏移
    std::uint32_t pendingCount_ = 0;        ///< pending 区间顶点数
    /// push constants 逐字节相等（两处填充均已 memset 清零 pad，可 memcmp）
    static bool pcEqual(const PushConstants &a, const PushConstants &b) {
        return std::memcmp(&a, &b, sizeof(PushConstants)) == 0;
    }

    static constexpr std::uint32_t kFrameSlots = VulkanContext::kMaxFramesInFlight;    ///< 分槽数 = 在飞帧数
    /** @brief 单帧槽容量（向下对齐 16B：槽基址作 vkCmdBindVertexBuffers 偏移
     *  须满足顶点格式对齐，整除三等分不保证 4B 对齐） */
    std::size_t slotCapacity() const { return (bufferCapacity_ / kFrameSlots) & ~std::size_t(0xF); }
    /** @brief 帧槽基址（frameSlot 为 ctx 帧槽，取模防御异常入参） */
    std::size_t slotBase(std::uint32_t frameSlot) const { return (frameSlot % kFrameSlots) * slotCapacity(); }

    /** @brief 延迟销毁队列：存扩容时被替换的旧 staging buffer。
     *  帧首 fence 等待只覆盖 3 帧前的提交——扩容时在飞的近 2 帧命令缓冲
     *  仍引用旧缓冲，倒计时 kFrameSlots 帧后（此刻引用者必然已执行完）销毁 */
    std::vector<std::tuple<VkBuffer, VkDeviceMemory, std::uint32_t>> pendingStaging_;

    /** @brief 扩容重建 staging 缓冲（容量 2× 递增，保留已写数据），失败返回 false */
    bool growStagingBuffer(VkDeviceSize required);

    /** @brief 销毁 pendingStaging_ 中的旧缓冲（仅可在 fence 等待完成、下一帧开始前调用） */
    void flushPendingDestroy();

    static constexpr std::size_t kDefaultCapacity =
        1048576;    ///< 默认 1MB（AA 顶点 7 float）——128KB 对多环复杂路径过小，超限命令被静默丢弃
};