/**
 * @file vulkan_triangle_renderer.cppm
 * @brief Vulkan 纯色三角形网格渲染器
 *
 * 接收 FillTrianglesCmd / StrokeTrianglesCmd 中的顶点数据，
 * 上传到 GPU 并绘制。使用 push constants 传递颜色和视口信息。
 */

module;
#include <vulkan/vulkan.h>
#include <cstdint>
#include <tuple>
#include <vector>

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
    void resetOffset(uint32_t frameSlot) {
        flushPendingDestroy();
        curSlot_ = frameSlot % kFrameSlots;
        curSlotBase_ = curSlot_ * slotCapacity();
        writeOffset_ = curSlotBase_;
    }

    /**
     * @brief 绘制三角形列表
     * @param cmd      命令缓冲区
     * @param extent   视口尺寸
     * @param vertices float2 数组 (每 3 个一组 = 1 个三角形)
     * @param color    填充颜色
     * @param alpha    全局透明度
     * @brief 绘制三角形列表
     * @param sweep 非空时启用 Sweep 扫描渐变（push constant 传圆心/角度/终点色）
     *
     * 顶点数据通过 memcpy 写入内部 host-visible 顶点缓冲，
     * 避免在 render pass 内使用 vkCmdUpdateBuffer。
     * 适合单帧 ≤64KB 三角形数据，Canvas Phase 1 使用。
     */
    void drawTriangles(VkCommandBuffer cmd, VkExtent2D extent, const AAVertex *vertices, uint32_t vertexCount,
                       const Color &color, float alpha, const Transform2D &t, const SweepGrad *sweep = nullptr);
    /**
     * @brief 绘制 SDF 圆环（UberSDF 同款：CPU 仅生成 6 顶点 quad，几何/渐变/端帽全在 shader）
     * @param alpha 全局透明度（clip 衰减，与 drawTriangles 语义一致）
     */
    void drawRing(VkCommandBuffer cmd, VkExtent2D extent, const FillRingCmd &ring, float alpha);

    /** @brief 获取 pipeline layout（供外部使用） */
    VkPipelineLayout layout() const { return pipelineLayout_; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;          ///< host-visible 顶点缓冲
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;    ///< 缓冲内存
    void *mappedData_ = nullptr;                       ///< 映射指针
    size_t bufferCapacity_ = 0;                        ///< 缓冲容量（全槽合计）
    size_t writeOffset_ = 0;                           ///< 当前帧顶点写入偏移（帧内逐命令累加）
    size_t curSlotBase_ = 0;                           ///< 当前帧槽基址（resetOffset 设定）
    uint32_t curSlot_ = 0;                             ///< 当前帧槽号（扩容迁移数据用）

    static constexpr uint32_t kFrameSlots = VulkanContext::kMaxFramesInFlight;    ///< 分槽数 = 在飞帧数
    /** @brief 单帧槽容量（向下对齐 16B：槽基址作 vkCmdBindVertexBuffers 偏移
     *  须满足顶点格式对齐，整除三等分不保证 4B 对齐） */
    size_t slotCapacity() const { return (bufferCapacity_ / kFrameSlots) & ~size_t(0xF); }
    /** @brief 帧槽基址（frameSlot 为 ctx 帧槽，取模防御异常入参） */
    size_t slotBase(uint32_t frameSlot) const { return (frameSlot % kFrameSlots) * slotCapacity(); }

    /** @brief 延迟销毁队列：存扩容时被替换的旧 staging buffer。
     *  帧首 fence 等待只覆盖 3 帧前的提交——扩容时在飞的近 2 帧命令缓冲
     *  仍引用旧缓冲，倒计时 kFrameSlots 帧后（此刻引用者必然已执行完）销毁 */
    std::vector<std::tuple<VkBuffer, VkDeviceMemory, uint32_t>> pendingStaging_;

    /** @brief 扩容重建 staging 缓冲（容量 2× 递增，保留已写数据），失败返回 false */
    bool growStagingBuffer(VkDeviceSize required);

    /** @brief 销毁 pendingStaging_ 中的旧缓冲（仅可在 fence 等待完成、下一帧开始前调用） */
    void flushPendingDestroy();

    static constexpr size_t kDefaultCapacity =
        1048576;    ///< 默认 1MB（AA 顶点 7 float）——128KB 对多环复杂路径过小，超限命令被静默丢弃
};