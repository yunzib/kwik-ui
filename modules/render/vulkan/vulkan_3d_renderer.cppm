/**
 * @file vulkan_3d_renderer.cppm
 * @brief Vulkan 3D 网格渲染器 (G3D 组件)
 *
 * 接收 DrawMeshCmd 的 Vertex3D 顶点数据 (位置 + 法线 + UV),
 * 上传到 GPU 并以深度测试管线绘制。
 *
 * Pipeline 配置（纯色/贴图两条，顶点输入与 push 布局相同）:
 *   - 顶点输入: float3 pos + float3 normal + float2 uv (stride 32)
 *   - 图元: TRIANGLE_LIST
 *   - Push constants 96B: { mvp[16], color(vec4), lightDir(vec3)+pad }
 *   - 深度: test + write, LESS_OR_EQUAL (depth 每帧 CLEAR 为 1.0)
 *   - 剔除: 关闭 (v1 简化; 封闭网格由深度测试自行遮挡背面)
 *   - 混合: SRC_ALPHA / ONE_MINUS_SRC_ALPHA
 *   - 贴图变体: set0 = 单采样器（复用 ImageRenderer 纹理注册表的
 *     descSet 与 setLayout，纹理生命周期全归 ImageRenderer）
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
     * @param device            Vulkan 设备
     * @param physDevice        Vulkan 物理设备
     * @param renderPass        渲染通道 (必须含 D24S8 depth attachment)
     * @param vertexBuffer      预分配的顶点缓冲区 (未使用, 保持接口统一)
     * @param indexBuffer       预分配的索引缓冲区 (未使用)
     * @param textureSetLayout  贴图管线 set0 布局（ImageRenderer 主管线
     *                         setLayout，外部复用保证 descSet 同源兼容；
     *                         空句柄 = 跳过贴图管线创建，仅纯色路径）
     * @return 成功返回 true
     */
    bool create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass,
                VkBuffer vertexBuffer, VkBuffer indexBuffer, VkDescriptorSetLayout textureSetLayout);

    /** @brief 销毁 pipeline、layout 和 host-visible 顶点缓存 */
    void destroy();

    /** @brief 开始一帧顶点写入（由 VulkanBackend::beginFrame 以 ctx 帧槽调用）：
     *  staging 按 in-flight 帧分槽，写入区间 = [slotBase, slotBase+slotCapacity)，
     *  帧首 fence 等待已保证上一次使用本槽的帧（3 帧前）GPU 执行完毕，CPU
     *  重写不与在飞帧 GPU 读竞争（原每帧从 0 覆写，复用距 1 帧 < fence 距
     *  3 帧，高负载下顶点数据花屏）
     *
     *  上帧溢出（drawMesh 槽不足丢弃）→ 帧边界按需倍增重建（见 growStaging）。
     *  非 3D 应用永不溢出 → 永不扩容，内存占用维持初始 192KB 不变 */
    void resetOffset(std::uint32_t frameSlot) {
        if (overflowed_ && bufferCapacity_ < kMaxCapacity) growStaging();
        curSlotBase_ = (frameSlot % kFrameSlots) * slotCapacity();
        writeOffset_ = curSlotBase_;
        overflowed_ = false;    // 溢出告警按帧节流（同帧只报首个超限网格）
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
     * @param textureSet   材质贴图 descSet（ImageRenderer 纹理注册表按
     *                     textureId 查得；空句柄 = 纯色管线）
     *
     * 顶点数据通过 memcpy 写入内部 host-visible 顶点缓冲,
     * 避免在 render pass 内使用 vkCmdUpdateBuffer。
     * 正高度 viewport: clip→NDC 的 y 翻转由投影矩阵承担
     * (见 G3D 组件构造投影时的 scale(1,-1,1))。
     */
    void drawMesh(VkCommandBuffer cmd, const Rect &viewport, VkRect2D scissor, const Vertex3D *vertices,
                  std::uint32_t vertexCount, const float mvp[16], const Color &color, const float lightDir[3],
                  VkDescriptorSet textureSet);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout texPipelineLayout_ = VK_NULL_HANDLE;    ///< 贴图变体（set0 单采样器）
    VkPipeline texPipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_ = VK_NULL_HANDLE;
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;            ///< host-visible 顶点缓冲
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;      ///< 缓冲内存
    void *mappedData_ = nullptr;                         ///< 映射指针
    std::size_t bufferCapacity_ = 0;                          ///< 缓冲容量（全槽合计）
    std::size_t writeOffset_ = 0;                             ///< 当前帧顶点写入偏移
    std::size_t curSlotBase_ = 0;                             ///< 当前帧槽基址（resetOffset 设定）

    static constexpr std::uint32_t kFrameSlots = VulkanContext::kMaxFramesInFlight;    ///< 分槽数 = 在飞帧数
    /** @brief 单帧槽容量（向下对齐 16B：槽基址作 vkCmdBindVertexBuffers 偏移
     *  须满足顶点格式对齐，整除三等分不保证 4B 对齐） */
    std::size_t slotCapacity() const { return (bufferCapacity_ / kFrameSlots) & ~std::size_t(0xF); }

    /// 默认 3×64KB：分槽后单帧仍保 64KB（G3D 网格顶点 32B/个，≈2048 顶点/帧）。
    /// 超限不静默——drawMesh 告警 + 帧首按需倍增（非 3D 应用永不触发，零增量）
    static constexpr std::size_t kDefaultCapacity = 196608;
    /// 扩容上限（全槽合计）：约 32 万顶点/帧封顶，防异常模型无界吃内存
    static constexpr std::size_t kMaxCapacity = std::size_t{32} << 20;
    bool overflowed_ = false;    ///< 上帧槽不足标志（resetOffset 消费并触发扩容）

    /** @brief 帧边界按需倍增 staging（resetOffset 调用，渲染线程）
     *  重建释放旧缓冲——在飞帧可能仍读旧内存，须先全设备等闲（罕见事件：
     *  每档容量整个生命周期最多触发一次） */
    void growStaging();

    /** @brief 释放 staging 缓冲（destroy / growStaging 共用，不动管线对象） */
    void freeStaging();
};