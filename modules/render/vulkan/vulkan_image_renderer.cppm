module;
#include <vulkan/vulkan.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>
export module kwik.render.vulkan.image_renderer;
import kwik.core.types;
import kwik.render.command;
import kwik.render.vulkan.context;

export class ImageRenderer {
public:
    ImageRenderer() = default;
    ~ImageRenderer();
    bool create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass,
                VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void destroy();

    /** @brief 入队纹理创建（UI 线程调用）：像素拷贝进作业，id 同步返回；
     *         实际 GPU 上传由渲染线程在 drainResources 执行，调用返回不代表上传完成 */
    uint32_t enqueueTexture(const uint8_t *rgba, uint32_t w, uint32_t h);
    /** @brief 入队纹理销毁（UI 线程调用）：id 即失效；实际释放由渲染线程延迟执行
     *         （retire 队列保证在飞帧引用安全），UI 线程不阻塞等待 GPU */
    void enqueueDestroy(uint32_t id);
    /** @brief 消费挂起的创建/销毁作业（渲染线程调用，须在帧首 fence 等待之后）
     *         @param frameNo 单调递增帧号——retire 队列回收帧距的基准 */
    void drainResources(const DeviceContext &dc, uint64_t frameNo);
    void drawImage(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha,
                   const Transform2D &t);
    void drawImageClipped(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha,
                          const Transform2D &t);
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkPipeline imagePipeline_ = VK_NULL_HANDLE;
    VkPipeline imageClipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout imagePipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout imageClipPipelineLayout_ = VK_NULL_HANDLE;    // clip 变体（工厂各建等价 layout）
    VkDescriptorSetLayout imageDescSetLayout_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> descPools_;    // 描述符池（首池 256，池满扩容新池倍增——
                                                 // 旧池上已分配 descSet 全部保持有效）
    struct TextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descSet = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;    // descSet 所属池（逐 set 释放路由）
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::unordered_map<uint32_t, TextureData> textures_;    // 仅渲染线程访问（资源编组后）

    // ── 资源通道：UI 线程生产（enqueue）↔ 渲染线程消费（drain） ──
    struct PendingCreate {
        std::vector<uint8_t> pixels;    // 像素拷贝（入队即脱离调用方内存）
        uint32_t id;
        uint32_t width;
        uint32_t height;
    };
    struct RetiredTexture {
        TextureData tex;
        uint64_t frameNo;    // 退役帧号；帧距达到 kMaxFramesInFlight 后回收
    };
    std::mutex jobsMutex_;                      // 生产/消费交接锁（仅护交接，执行段无锁）
    std::vector<PendingCreate> pendingCreates_; // 待创建（UI 线程 push / 渲染线程 drain）
    std::vector<uint32_t> pendingDestroys_;     // 待销毁（同上）
    std::deque<RetiredTexture> retired_;        // 退役中：帧距未满不释放（仅渲染线程）
    std::atomic<uint32_t> nextId_{1};           // id 分配（UI 线程无锁递增）

    /** @brief 设备端创建+上传（drain 内执行，id 由入队时预分配）
     *         @return id 成功；0 失败（id 作废，drawImage 查不到即跳过） */
    uint32_t createTextureWithId(const DeviceContext &dc, uint32_t id, const uint8_t *rgba, uint32_t w, uint32_t h);
    /** @brief 立即释放一套纹理资源（渲染线程；调用方保证无在飞帧引用） */
    void freeTexture(const TextureData &t);
};