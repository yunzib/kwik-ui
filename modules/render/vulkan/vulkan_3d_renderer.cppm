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
    bool create(VkDevice device, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                VkBuffer indexBuffer);

    /** @brief 销毁 pipeline、layout 和 host-visible 顶点缓存 */
    void destroy();

    /** @brief 重置每帧顶点写入偏移（由 VulkanBackend::beginFrame 调用） */
    void resetOffset() { writeOffset_ = 0; }

    /**
     * @brief 绘制 3D 网格
     * @param cmd          命令缓冲区
     * @param extent       全屏尺寸（scissor 用）
     * @param viewport     元素屏幕矩形（mesh 视口：定位+裁剪；Vulkan viewport 自动裁剪范围外图元）
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
    void drawMesh(VkCommandBuffer cmd, VkExtent2D extent, const Rect &viewport, const Vertex3D *vertices,
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
    size_t bufferCapacity_ = 0;                          ///< 缓冲容量
    size_t writeOffset_ = 0;                             ///< 当前帧内顶点写入偏移
    static constexpr size_t kDefaultCapacity = 65536;    ///< 默认 64KB
};