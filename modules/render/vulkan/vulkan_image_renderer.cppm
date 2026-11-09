module;
#include <vulkan/vulkan.h>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
export module kwik.render.vulkan.image_renderer;
import kwik.core.types;
import kwik.render.command;
import kwik.render.vulkan.context;
import kwik.render.video_stream;    // VideoStream / FrameDesc / FrameLayout（流纹理）

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
    /** @brief 入队流纹理绑定（UI 线程调用）：id 同步返回；渲染线程帧首按
     *         VideoStream* 共享 GPU 资源并引用计数（同流多显示端各自持 id） */
    uint32_t enqueueStream(std::shared_ptr<VideoStream> stream);
    /** @brief 入队流纹理触碰（UI 线程录制该 id 绘制命令时调用）：标记本帧
     *         将绘制——未被触碰的流帧首不上传新帧（不可见不占带宽） */
    void enqueueTouch(uint32_t id);
    /** @brief 消费挂起的创建/销毁/流绑定作业（渲染线程调用，须在帧首 fence 等待之后）
     *         @param frameNo 单调递增帧号——retire 队列回收帧距的基准 */
    void drainResources(const DeviceContext &dc, uint64_t frameNo);
    /** @brief 流纹理上传（渲染线程帧首、主 render pass 之前调用——transfer
     *         命令唯一合法窗口）：被触碰且有新帧的流 → memcpy 进 staging[帧槽]
     *         → 拷入图像环下一槽（fence 证明该槽已无在飞帧引用，免执行依赖） */
    void recordStreamUploads(const DeviceContext &dc, VkCommandBuffer cb, uint32_t frameSlot, uint64_t frameNo);
    /** @brief 查询某 id 的异步上传结果（UI 线程调用，查询即摘除该 id 回执）
     *         @return 0=在途 1=已落地（源像素可释放） 2=上传失败（可重试） */
    int queryUploadResult(uint32_t id);
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
    // YUV 管线（V1 批 2）：三平面 combined sampler + push 88B；裁剪/非裁剪共用
    // 一条（深度模板态同 image 主管线 StencilTestEqual，无裁剪时比较掩码 0 恒通过）
    VkPipeline yuvPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout yuvPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout yuvDescSetLayout_ = VK_NULL_HANDLE;
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

    // ── 流纹理（V1 推流）：按 VideoStream* 共享 GPU 资源，别名 id 引用计数 ──
    struct StreamSlot {                          // 图像环一槽（槽距 = kMaxFramesInFlight）
        VkDeviceMemory memory = VK_NULL_HANDLE;  // 本槽全部平面共用一块（按对齐子分配）
        std::vector<VkImage> images;             // 采样平面图像（按 TexturePlan：RGB 1 张 / YUV 2~3 张）
        std::vector<VkImageView> views;
        VkDescriptorSet descSet = VK_NULL_HANDLE;    // 建槽时写一次，此后只读（免在飞更新）
        VkDescriptorPool pool = VK_NULL_HANDLE;      // descSet 所属池（逐 set 释放路由）
    };
    struct StreamStage {                         // 每在飞帧一份常驻 staging（fence 保证复用安全）
        VkBuffer buffer = VK_NULL_HANDLE;        // 拷贝路径才建（零拷贝路径懒建，导入失败时回退用）
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void *mapped = nullptr;
        VkDeviceSize capacity = 0;
    };
    struct StreamResources {                     // GPU 资源组（格式/尺寸变更时整组换新，旧组延迟释放）
        std::vector<StreamSlot> slots;           // kMaxFramesInFlight 槽图像环
        std::vector<StreamStage> stages;         // kMaxFramesInFlight 份 staging（各槽可空）
    };
    // 零拷贝（批 3）：池槽内存块经 VK_EXT_external_memory_host 导入为拷贝源缓冲。
    // 一块一个导入（块大小恒定，按块对象地址缓存复用）；块退役即延迟释放
    struct ImportedBlock {
        std::shared_ptr<MemoryBlock> block;      // 续命：导入期间宿主内存不得释放
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };
    struct StreamTexture {
        std::shared_ptr<VideoStream> stream;     // 数据源（持 shared：退役帧距内不悬空）
        FrameDesc desc;                          // 当前 GPU 布局（格式/尺寸变则整组重建；矩阵/range 随每次上传更新）
        FrameLayout layout;                      // 与 desc 对应的帧内存布局（staging/拷贝依据）
        TexturePlan plan;                        // 与 desc 对应的 GPU 采样平面规划（图像/拷贝/管线依据）
        StreamResources res;                     // 图像环 + staging + 每槽描述符集
        uint32_t ringIndex = 0;                  // 最近写入的槽（上传时先 +1 再写；绘制即采样此槽）
        uint64_t uploadedSeq = 0;                // 已上传帧序号（同帧不重复上传）
        bool hasUpload = false;                  // 首帧上传前绘制跳过（元素侧占位）
        uint64_t touchedFrame = 0;               // 最近被录制触碰的帧号（不可见跳过上传）
        int32_t refCount = 0;                    // 别名计数（归零延后一帧退役）
        uint64_t retireAt = 0;                   // 退役执行帧号（0 = 未挂起；复活即取消）
        // ── 零拷贝（批 3）──
        std::unordered_map<const MemoryBlock *, ImportedBlock> imports;    // 块地址 → 导入对象
        std::vector<PinGuard> heldPins;          // 每在飞帧槽一个：零拷贝帧的固定持有到该槽 fence 再等到
        uint32_t reservedPins = 0;               // 已向流登记的长时持有数（退役时撤销）
        uint32_t importFailures = 0;             // 导入失败计数（达阈值对该流永久回退 memcpy）
        bool zeroCopyUploads = false;            // 最近一次上传走零拷贝（读数/日志用）
    };
    std::unordered_map<VideoStream *, std::unique_ptr<StreamTexture>> streams_;    // 仅渲染线程
    std::unordered_map<uint32_t, VideoStream *> streamAliases_;    // 别名 id → 流（仅渲染线程）
    VkSampler streamSampler_ = VK_NULL_HANDLE;   // 流专用采样器（线性 + clamp，无 mip；全流共享）
    bool zeroCopyLogged_ = false;                // 首次零拷贝上传已记日志（每渲染器一次，确认实际路径）

    // ── 资源通道：UI 线程生产（enqueue）↔ 渲染线程消费（drain） ──
    struct PendingCreate {
        std::vector<uint8_t> pixels;    // 像素拷贝（入队即脱离调用方内存）
        uint32_t id;
        uint32_t width;
        uint32_t height;
    };
    struct PendingStream {              // 流绑定作业（id 已在 UI 线程预分配）
        uint32_t id;
        std::shared_ptr<VideoStream> stream;
    };
    struct RetiredTexture {
        TextureData tex;
        uint64_t frameNo;    // 退役帧号；帧距达到 kMaxFramesInFlight 后回收
    };
    std::mutex jobsMutex_;                      // 生产/消费交接锁（仅护交接，执行段无锁）
    std::vector<PendingCreate> pendingCreates_; // 待创建（UI 线程 push / 渲染线程 drain）
    std::vector<uint32_t> pendingDestroys_;     // 待销毁（同上；流别名 id 混用此队列）
    std::vector<PendingStream> pendingStreams_; // 待绑定流（同上）
    std::vector<uint32_t> pendingTouches_;      // 本帧被触碰的流别名 id（同上）
    std::deque<RetiredTexture> retired_;        // 退役中：帧距未满不释放（仅渲染线程）
    std::deque<std::pair<uint64_t, std::function<void()>>> deferredFrees_;    // 流资源延迟释放（帧距满执行，仅渲染线程）
    std::atomic<uint32_t> nextId_{1};           // id 分配（UI 线程无锁递增；流别名共用此空间）

    // ── 上传回执（渲染线程在 drainResources 写入，UI 线程执行 query）──
    // 查询即摘除。渲染线程不入 textures_ 的失败 id 也在此留痕，调用方
    // 得以复用源像素重新入队（此前失败 id 凭空落地、源像素已被释放——
    // 永久白块不可重试的根源）
    std::vector<uint32_t> landedIds_;     // 创建成功回执（源像素可释放）
    std::vector<uint32_t> failedCreates_; // 创建失败回执（调用方重试）

    /** @brief 设备端创建+上传（drain 内执行，id 由入队时预分配）
     *         @return id 成功；0 失败（id 作废，drawImage 查不到即跳过） */
    uint32_t createTextureWithId(const DeviceContext &dc, uint32_t id, const uint8_t *rgba, uint32_t w, uint32_t h);
    /** @brief 立即释放一套纹理资源（渲染线程；调用方保证无在飞帧引用） */
    void freeTexture(const TextureData &t);

    // ── 流纹理内部（仅渲染线程）──
    /** @brief 纹素格式 → VkFormat（R8/RG8/RGBA8/BGRA8 均为规范强制支持采样 + 线性过滤） */
    static VkFormat vkFormatFor(TexelFormat f);
    /** @brief 创建/重建 StreamTexture 的 GPU 资源组（图像环 + staging + 每槽描述符集）
     *         @return false = 资源创建失败（流保持未就绪，下帧重试） */
    bool createStreamResources(const DeviceContext &dc, StreamTexture &st);
    /** @brief 立即释放一组流资源（调用方保证无在飞帧引用） */
    void freeStreamResources(const StreamResources &r);
    /** @brief 把整组流资源推入延迟释放队列（帧距满后在 drainResources 执行） */
    void deferFreeResources(StreamResources &&r, uint64_t frameNo);
    /** @brief 流整体退役：资源组 + 导入块 + 固定持有一并延迟释放，撤销持有登记 */
    void retireStream(StreamTexture &st, uint64_t frameNo);
    /** @brief 懒建帧槽 staging（拷贝路径/零拷贝回退时调用） @return 失败 false */
    bool ensureStage(const DeviceContext &dc, StreamTexture &st, uint32_t frameSlot);
    /** @brief 取（或首次导入）块对应的拷贝源缓冲 @return 失败返回 VK_NULL_HANDLE（调用方回退 memcpy） */
    VkBuffer importBlock(const DeviceContext &dc, StreamTexture &st, const std::shared_ptr<MemoryBlock> &block);
    /** @brief 释放已退役块的导入对象（延迟到在飞帧引用结束） */
    void pruneImports(StreamTexture &st, uint64_t frameNo);
    /** @brief 从描述符池分配一个 descSet（复用纹理池的懒建/扩容/旧池回填逻辑）
     *         @param layout image（单采样器）或 yuv（三采样器）布局 */
    VkDescriptorSet allocDescSet(const DeviceContext &dc, VkDescriptorSetLayout layout, VkDescriptorPool &outPool);
    /** @brief 流纹理绘制（drawImage/drawImageClipped 按别名表分派到此；RGB 复用
     *         image 管线、YUV 走 yuv 管线，均采样最近写入的环槽） */
    void drawStream(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha,
                    const Transform2D &t, bool clipped);
};