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
#include <vector>

export module kwik.render.vulkan.triangle_renderer;

import kwik.core.types;
import kwik.core.path;
import kwik.render.command;

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
    bool create(VkDevice device, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                VkBuffer indexBuffer);

    /** @brief 销毁 pipeline、layout 和 host-visible 顶点缓存 */
    void destroy();

    /** @brief 重置每帧顶点写入偏移（由 VulkanBackend::beginFrame 调用）；
     *  帧开始前销毁上一帧遗留的旧 staging 缓冲（此时 fence 已等待完成，命令不再引用旧缓冲） */
    void resetOffset() {
        flushPendingDestroy();
        writeOffset_ = 0;
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
    size_t bufferCapacity_ = 0;                        ///< 缓冲容量
    size_t writeOffset_ = 0;                           ///< 当前帧内顶点写入偏移（每帧从 0 开始，逐命令累加）

    /** @brief 延迟销毁队列：存扩容时被替换的旧 staging buffer
     *  （本帧已录制的 vkCmdBindVertexBuffers 仍引用它，须等该帧命令执行完再销毁） */
    std::vector<std::pair<VkBuffer, VkDeviceMemory>> pendingStaging_;

    /** @brief 扩容重建 staging 缓冲（容量 2× 递增，保留已写数据），失败返回 false */
    bool growStagingBuffer(VkDeviceSize required);

    /** @brief 销毁 pendingStaging_ 中的旧缓冲（仅可在 fence 等待完成、下一帧开始前调用） */
    void flushPendingDestroy();

    static constexpr size_t kDefaultCapacity =
        1048576;    ///< 默认 1MB（AA 顶点 7 float）——128KB 对多环复杂路径过小，超限命令被静默丢弃
};