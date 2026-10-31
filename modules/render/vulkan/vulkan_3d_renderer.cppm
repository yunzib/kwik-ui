/**
 * @file vulkan_3d_renderer.cppm
 * @brief Vulkan 3D 网格渲染器 (G3D 组件)
 *
 * 接收 DrawMeshCmd 的 Vertex3D 顶点数据 (位置 + 法线),
 * 上传到 GPU 并以深度测试管线绘制。
 *
 * Pipeline 配置:
 *   - 顶点输入: float3 pos + float3 normal (stride 24)
 *   - 图元: TRIANGLE_LIST
 *   - Push constants 96B: { mvp[16], color(vec4), lightDir(vec3)+pad }
 *   - 深度: test + write, LESS_OR_EQUAL (depth 每帧 CLEAR 为 1.0)
 *   - 剔除: 关闭 (v1 简化; 封闭网格由深度测试自行遮挡背面)
 *   - 混合: SRC_ALPHA / ONE_MINUS_SRC_ALPHA
 */

module;
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstddef>

export module kwik.render.vulkan.mesh_renderer;

import kwik.core.types;
import kwik.render.command;
import kwik.render.vulkan.context;    // kMaxFramesInFlight（分槽唯一事实源）

import std;

/**
 * @brief 3D 网格渲染器
 *
 * 与 TriangleRenderer 同构: 自有 host-visible 顶点缓冲,
 * render pass 内 memcpy 上传顶点数据。
 */
export class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer &) = delete;
    MeshRenderer &operator=(const MeshRenderer &) = delete;

    /**
     * @brief 创建 pipeline、pipeline layout 和 host-visible 顶点缓存
     * @param device       Vulkan 设备
     * @param physDevice   Vulkan 物理设备
     * @param renderPass   渲染通道 (必须含 D24S8 depth attachment)
     * @param vertexBuffer 预分配的顶点缓冲区 (未使用, 保持接口统一)
     * @param indexBuffer  预分配的索引缓冲区 (未使用)
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
     *  3 帧，高负载下顶点数据花屏） */
    void resetOffset(uint32_t frameSlot) {
        curSlotBase_ = (frameSlot % kFrameSlots) * slotCapacity();
        writeOffset_ = curSlotBase_;
    }

    /**
     * @brief 绘制 3D 网格
     * @param cmd          命令缓冲区
     * @param viewport     元素屏幕矩形（mesh 视口：NDC→屏幕映射定位几何）
     * @param scissor      像素裁剪矩形（后端已收口 = viewport ∩ 当前可画域；
     *                     元素滚出滚动视口/伤害带时空剪不画——几何定位仍由
     *                     viewport 承担，缩 viewport 会拉伸画面）
     * @param vertices     Vertex3D 数组 (每 3 个一组 = 1 个三角形)
     * @param vertexCount  顶点总数 (3 的倍数)
     * @param mvp          模型-视图-投影矩阵 (列主序, 16 floats)
     * @param color        基础颜色
     * @param lightDir     方向光方向 (对象空间, 3 floats)
     *
     * 顶点数据通过 memcpy 写入内部 host-visible 顶点缓冲,
     * 避免在 render pass 内使用 vkCmdUpdateBuffer。
     * 正高度 viewport: clip→NDC 的 y 翻转由投影矩阵承担
     * (见 G3D 组件构造投影时的 scale(1,-1,1))。
     */
    void drawMesh(VkCommandBuffer cmd, const Rect &viewport, VkRect2D scissor, const Vertex3D *vertices,
                  uint32_t vertexCount, const float mvp[16], const Color &color, const float lightDir[3]);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;            ///< host-visible 顶点缓冲
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;      ///< 缓冲内存
    void *mappedData_ = nullptr;                         ///< 映射指针
    size_t bufferCapacity_ = 0;                          ///< 缓冲容量（全槽合计）
    size_t writeOffset_ = 0;                             ///< 当前帧顶点写入偏移
    size_t curSlotBase_ = 0;                             ///< 当前帧槽基址（resetOffset 设定）

    static constexpr uint32_t kFrameSlots = VulkanContext::kMaxFramesInFlight;    ///< 分槽数 = 在飞帧数
    /** @brief 单帧槽容量（向下对齐 16B：槽基址作 vkCmdBindVertexBuffers 偏移
     *  须满足顶点格式对齐，整除三等分不保证 4B 对齐） */
    size_t slotCapacity() const { return (bufferCapacity_ / kFrameSlots) & ~size_t(0xF); }

    /// 默认 3×64KB：分槽后单帧仍保 64KB（G3D 网格顶点 24B/个，≈2700 顶点/帧）
    static constexpr size_t kDefaultCapacity = 196608;
};