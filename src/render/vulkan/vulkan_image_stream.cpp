/**
 * @file vulkan_image_stream.cpp
 * @brief ImageRenderer 流纹理实现（V1 推流）— 资源组 / 上传录制 / 绘制
 *
 * 与 vulkan_image_renderer.cpp 同属 kwik.render.vulkan.image_renderer 模块
 * （一模块多实现单元，同 kwik.ext.video 的 video.cpp + bindings.cpp 惯例）。
 * 本文件全部函数仅在渲染线程执行（streams_/streamAliases_ 无锁访问前提）。
 * 格式支持：RGBA8/BGRA8 直采样（image 管线）；YUV 六格式按 TexturePlan 分平面
 * 上传、image_yuv 管线片段阶段转色（批 2）。
 * 拷贝源（批 3）：设备支持 VK_EXT_external_memory_host 时池槽内存块直接导入为
 * 拷贝源缓冲（零 CPU 拷贝，固定持有到该帧 fence）；否则 memcpy 进 staging。
 */
module;
#include <vulkan/vulkan.h>
#include <cstring>
module kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.context;
import kwik.render.command;
import kwik.render.video_stream;
import kwik.core.types;
import kwik.core.log;    // 零拷贝回退告警
import std;

VkFormat ImageRenderer::vkFormatFor(TexelFormat f) {
    switch (f) {
    case TexelFormat::R8: return VK_FORMAT_R8_UNORM;
    case TexelFormat::RG8: return VK_FORMAT_R8G8_UNORM;
    case TexelFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
    case TexelFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
    }
    return VK_FORMAT_UNDEFINED;
}

namespace {
/**
 * @brief YUV 着色器 mode 位（与 shaders/image_yuv.slang 文件头位表同表）
 *
 * bit0-1 平面形态（0 三平面 / 1 双平面 / 2 打包 4:2:2）、bit2 交换 U/V、
 * bit3 打包序 UYVY、bit4-5 矩阵（YuvMatrix 枚举序 601/709/2020）、bit6 full range
 */
uint32_t yuvModeFor(const FrameDesc &d) {
    uint32_t m = 0;
    switch (d.format) {
    case PixelFormat::I420: m = 0u; break;
    case PixelFormat::YV12: m = 0u | 4u; break;    // 平面序 Y/V/U
    case PixelFormat::NV12: m = 1u; break;
    case PixelFormat::NV21: m = 1u | 4u; break;    // 交错序 VU
    case PixelFormat::YUYV: m = 2u; break;
    case PixelFormat::UYVY: m = 2u | 8u; break;
    default: break;                                // RGB 不走 YUV 管线
    }
    m |= uint32_t(d.matrix) << 4;
    if (d.fullRange) m |= 64u;
    return m;
}
}    // namespace

// 描述符分配：复用纹理池的懒建/扩容/旧池回填纪律（绝不 reset——会把全部
// 存活 descSet 一并作废，见 createTextureWithId 内同款注释）。YUV 三采样器
// 布局与 image 单采样器布局共用同一组池（池按描述符总数记账，混用合法）
VkDescriptorSet ImageRenderer::allocDescSet(const DeviceContext &dc, VkDescriptorSetLayout layout,
                                            VkDescriptorPool &outPool) {
    if (descPools_.empty()) {
        VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;
        pi.maxSets = 256;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(dc.device, &pi, nullptr, &pool) != VK_SUCCESS) return VK_NULL_HANDLE;
        descPools_.push_back(pool);
    }
    VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    sa.descriptorSetCount = 1;
    sa.pSetLayouts = &layout;
    auto tryAlloc = [&](VkDescriptorPool pool, VkDescriptorSet &out) {
        sa.descriptorPool = pool;
        return vkAllocateDescriptorSets(dc.device, &sa, &out);
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult r = tryAlloc(descPools_.back(), set);
    for (size_t i = 0; r != VK_SUCCESS && i + 1 < descPools_.size(); ++i) r = tryAlloc(descPools_[i], set);
    if (r != VK_SUCCESS) {
        uint32_t cap = std::min<uint32_t>(256u << (descPools_.size() - 1), 4096);
        VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cap};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;
        pi.maxSets = cap;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(dc.device, &pi, nullptr, &pool) == VK_SUCCESS) {
            descPools_.push_back(pool);
            r = tryAlloc(pool, set);
        }
    }
    outPool = sa.descriptorPool;
    return r == VK_SUCCESS ? set : VK_NULL_HANDLE;
}

bool ImageRenderer::createStreamResources(const DeviceContext &dc, StreamTexture &st) {
    if (st.desc.width == 0 || st.desc.height == 0) return false;
    st.layout = FrameLayout::forDesc(st.desc);
    st.plan = TexturePlan::forDesc(st.desc);
    if (st.layout.planeCount == 0 || st.plan.count == 0) return false;
    const uint32_t P = st.plan.count;                        // GPU 采样平面数（RGB 1 / YUV 2~3）
    const uint32_t N = VulkanContext::kMaxFramesInFlight;    // 环槽距 = 在飞帧数

    // ① 全流共享采样器（线性 + clamp、单 mip——流纹理按帧覆写，不产 mip 链）
    if (streamSampler_ == VK_NULL_HANDLE) {
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = VK_FILTER_LINEAR;
        si.minFilter = VK_FILTER_LINEAR;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(dc.device, &si, nullptr, &streamSampler_) != VK_SUCCESS) {
            streamSampler_ = VK_NULL_HANDLE;
            return false;
        }
    }

    StreamResources r;
    r.slots.resize(N);
    r.stages.resize(N);
    // ② 图像环：每槽一块 DEVICE_LOCAL 内存按对齐子分配绑定全部采样平面。
    //    打包 4:2:2 的两张视图共享同一块内存（Y 视图与宏像素色度视图各占一段
    //    绑定偏移——帧内存同一平面按不同纹素宽解释，两视图不重叠）
    for (uint32_t s = 0; s < N; ++s) {
        auto &slot = r.slots[s];
        slot.images.assign(P, VK_NULL_HANDLE);
        slot.views.assign(P, VK_NULL_HANDLE);
        std::vector<VkFormat> fmts(P, VK_FORMAT_UNDEFINED);
        std::vector<VkMemoryRequirements> reqs(P);
        for (uint32_t p = 0; p < P; ++p) {
            const auto &pl = st.plan.planes[p];
            VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = fmts[p] = vkFormatFor(pl.format);
            ii.extent = {pl.width, pl.height, 1};
            ii.mipLevels = 1;
            ii.arrayLayers = 1;
            ii.samples = VK_SAMPLE_COUNT_1_BIT;
            ii.tiling = VK_IMAGE_TILING_OPTIMAL;
            ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (vkCreateImage(dc.device, &ii, nullptr, &slot.images[p]) != VK_SUCCESS) {
                freeStreamResources(r);
                return false;
            }
            vkGetImageMemoryRequirements(dc.device, slot.images[p], &reqs[p]);
        }
        std::vector<VkDeviceSize> bindOff(P, 0);
        VkDeviceSize total = 0;
        for (uint32_t p = 0; p < P; ++p) {
            bindOff[p] = (total + reqs[p].alignment - 1) & ~(VkDeviceSize(reqs[p].alignment) - 1);
            total = bindOff[p] + reqs[p].size;
        }
        // 内存类型取全部平面的交集（R8/RG8/RGBA8 常同型；交集为空即无法共用一块，
        // 按创建失败收场——findMemoryType 无匹配时回退 0，不能据返回值判失败）
        uint32_t typeBits = reqs[0].memoryTypeBits;
        for (uint32_t p = 1; p < P; ++p) typeBits &= reqs[p].memoryTypeBits;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = total;
        ai.memoryTypeIndex = VulkanContext::findMemoryType(dc.physicalDevice, typeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (typeBits == 0 || vkAllocateMemory(dc.device, &ai, nullptr, &slot.memory) != VK_SUCCESS) {
            freeStreamResources(r);
            return false;
        }
        for (uint32_t p = 0; p < P; ++p) {
            if (vkBindImageMemory(dc.device, slot.images[p], slot.memory, bindOff[p]) != VK_SUCCESS) {
                freeStreamResources(r);
                return false;
            }
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image = slot.images[p];
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = fmts[p];
            vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (vkCreateImageView(dc.device, &vi, nullptr, &slot.views[p]) != VK_SUCCESS) {
                freeStreamResources(r);
                return false;
            }
        }
    }
    // ③ staging 不在此建：拷贝路径按帧槽懒建（ensureStage）；零拷贝路径直接以
    //    导入的池槽内存块为拷贝源，全程不需要 staging（省 3 份帧大小内存）
    // ④ 每槽描述符集：写一次此后只读——绝不更新在飞帧正在采样的 set（规范
    //    禁止）。YUV 三绑定缺的平面用 plane0 占位（着色器按 mode 只取用到的
    //    绑定，占位仅满足"静态使用须有效"）；RGB 单绑定走 image 布局
    for (uint32_t s = 0; s < N; ++s) {
        auto &slot = r.slots[s];
        const bool yuv = st.plan.yuv;
        slot.descSet = allocDescSet(dc, yuv ? yuvDescSetLayout_ : imageDescSetLayout_, slot.pool);
        if (slot.descSet == VK_NULL_HANDLE) {
            freeStreamResources(r);
            return false;
        }
        VkDescriptorImageInfo di[3];
        VkWriteDescriptorSet w[3];
        for (uint32_t b = 0; b < (yuv ? 3u : 1u); ++b) {
            di[b] = {streamSampler_, slot.views[b < P ? b : 0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            w[b] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w[b].dstSet = slot.descSet;
            w[b].dstBinding = b;
            w[b].descriptorCount = 1;
            w[b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w[b].pImageInfo = &di[b];
        }
        vkUpdateDescriptorSets(dc.device, yuv ? 3 : 1, w, 0, nullptr);
    }

    st.res = std::move(r);
    st.ringIndex = 0;
    st.uploadedSeq = 0;       // 强制下一帧上传当前最新帧
    st.hasUpload = false;
    return true;
}

void ImageRenderer::freeStreamResources(const StreamResources &r) {
    for (auto &slot : r.slots) {
        if (slot.descSet != VK_NULL_HANDLE && slot.pool != VK_NULL_HANDLE)
            vkFreeDescriptorSets(device_, slot.pool, 1, &slot.descSet);
        for (auto v : slot.views)
            if (v != VK_NULL_HANDLE) vkDestroyImageView(device_, v, nullptr);
        for (auto i : slot.images)
            if (i != VK_NULL_HANDLE) vkDestroyImage(device_, i, nullptr);
        if (slot.memory != VK_NULL_HANDLE) vkFreeMemory(device_, slot.memory, nullptr);
    }
    for (auto &stage : r.stages) {
        if (stage.mapped) vkUnmapMemory(device_, stage.memory);
        if (stage.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device_, stage.buffer, nullptr);
        if (stage.memory != VK_NULL_HANDLE) vkFreeMemory(device_, stage.memory, nullptr);
    }
}

void ImageRenderer::deferFreeResources(StreamResources &&r, uint64_t frameNo) {
    // 帧距满后执行（退役/重建两路共用；在飞帧引用保护同 retire 队列口径）
    deferredFrees_.push_back({frameNo, [this, r = std::move(r)]() { freeStreamResources(r); }});
}

// ── 拷贝路径 staging 懒建：每在飞帧一份常驻映射（HOST_COHERENT）——写入按帧槽
//    轮转，复用安全由 fence 等待保证（与顶点分槽同源）──
bool ImageRenderer::ensureStage(const DeviceContext &dc, StreamTexture &st, uint32_t frameSlot) {
    if (st.res.stages.size() <= frameSlot) st.res.stages.resize(VulkanContext::kMaxFramesInFlight);
    StreamStage &stage = st.res.stages[frameSlot];
    if (stage.buffer != VK_NULL_HANDLE) return stage.capacity >= st.layout.totalBytes;
    if (!VulkanContext::createBuffer(dc.device, dc.physicalDevice, st.layout.totalBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stage.buffer, stage.memory))
        return false;
    if (vkMapMemory(dc.device, stage.memory, 0, st.layout.totalBytes, 0, &stage.mapped) != VK_SUCCESS) {
        vkDestroyBuffer(dc.device, stage.buffer, nullptr);
        vkFreeMemory(dc.device, stage.memory, nullptr);
        stage = StreamStage{};
        return false;
    }
    stage.capacity = st.layout.totalBytes;
    return true;
}

// ── 零拷贝：池槽内存块 → VK_EXT_external_memory_host 导入的拷贝源缓冲 ──
// 块大小恒定（换分辨率即换块），故一块一个导入、按块对象地址缓存复用；
// 块的 shared_ptr 存入缓存续命——导入存续期间宿主内存不得释放（规范要求）
VkBuffer ImageRenderer::importBlock(const DeviceContext &dc, StreamTexture &st,
                                    const std::shared_ptr<MemoryBlock> &block) {
    if (!block || !block->data() || !dc.getHostPointerProps || dc.hostImportAlignment == 0) return VK_NULL_HANDLE;
    if (auto it = st.imports.find(block.get()); it != st.imports.end()) return it->second.buffer;
    // 地址与尺寸须为 minImportedHostPointerAlignment 整数倍（块按 64KiB 分配，主流驱动 4KiB 恒满足）
    const auto addr = reinterpret_cast<uintptr_t>(block->data());
    if (addr % dc.hostImportAlignment != 0 || block->size() % dc.hostImportAlignment != 0) return VK_NULL_HANDLE;
    VkMemoryHostPointerPropertiesEXT hp{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    if (dc.getHostPointerProps(dc.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, block->data(), &hp)
        != VK_SUCCESS)
        return VK_NULL_HANDLE;

    ImportedBlock ib;
    ib.block = block;
    VkExternalMemoryBufferCreateInfo eb{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
    eb.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.pNext = &eb;
    bi.size = block->size();
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dc.device, &bi, nullptr, &ib.buffer) != VK_SUCCESS) return VK_NULL_HANDLE;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(dc.device, ib.buffer, &mr);
    // 内存类型：导入指针允许的类型 ∩ 缓冲要求，且须 HOST_COHERENT——生产者 CPU
    // 写入经队列提交的隐式宿主依赖对 GPU 可见（与 staging 路径同一前提，免 flush）
    const uint32_t bits = hp.memoryTypeBits & mr.memoryTypeBits;
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(dc.physicalDevice, &mp);
    const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
            type = i;
            break;
        }
    if (type == UINT32_MAX || mr.size > block->size()) {
        vkDestroyBuffer(dc.device, ib.buffer, nullptr);
        return VK_NULL_HANDLE;
    }
    VkImportMemoryHostPointerInfoEXT imp{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT};
    imp.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
    imp.pHostPointer = block->data();
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.pNext = &imp;
    ai.allocationSize = block->size();
    ai.memoryTypeIndex = type;
    if (vkAllocateMemory(dc.device, &ai, nullptr, &ib.memory) != VK_SUCCESS) {
        vkDestroyBuffer(dc.device, ib.buffer, nullptr);
        return VK_NULL_HANDLE;
    }
    if (vkBindBufferMemory(dc.device, ib.buffer, ib.memory, 0) != VK_SUCCESS) {
        vkDestroyBuffer(dc.device, ib.buffer, nullptr);
        vkFreeMemory(dc.device, ib.memory, nullptr);
        return VK_NULL_HANDLE;
    }
    const VkBuffer buf = ib.buffer;
    st.imports.emplace(block.get(), std::move(ib));
    return buf;
}

// ── 释放已退役块的导入对象：块退役 = 池已换块，本流再不会从它拷贝；但此前
//    帧的拷贝可能仍在飞——延迟到帧距满再销毁缓冲/内存，最后才放开块（宿主
//    内存释放必须晚于导入对象销毁）──
void ImageRenderer::pruneImports(StreamTexture &st, uint64_t frameNo) {
    for (auto it = st.imports.begin(); it != st.imports.end();) {
        if (it->second.block->retired()) {
            // lambda 持块 shared_ptr：先执行销毁 GPU 对象，lambda 析构时才释放块
            deferredFrees_.push_back({frameNo, [this, ib = it->second]() {
                                          vkDestroyBuffer(device_, ib.buffer, nullptr);
                                          vkFreeMemory(device_, ib.memory, nullptr);
                                      }});
            it = st.imports.erase(it);
        } else {
            ++it;
        }
    }
}

// ── 流整体退役：GPU 资源组、全部导入块、跨帧固定一并延迟到帧距满释放；
//    长时持有登记即时撤销（只影响池后续扩容，已启用槽不回收）──
void ImageRenderer::retireStream(StreamTexture &st, uint64_t frameNo) {
    deferFreeResources(std::move(st.res), frameNo);
    for (auto &[key, ib] : st.imports) {
        deferredFrees_.push_back({frameNo, [this, ib]() {
                                      vkDestroyBuffer(device_, ib.buffer, nullptr);
                                      vkFreeMemory(device_, ib.memory, nullptr);
                                  }});
    }
    st.imports.clear();
    // 固定须持有到在飞帧读完：转入延迟队列（std::function 须可复制 → shared_ptr 包装）
    auto pins = std::make_shared<std::vector<PinGuard>>(std::move(st.heldPins));
    deferredFrees_.push_back({frameNo, [pins]() {}});    // 帧距满后 lambda 析构 → 解除固定
    if (st.reservedPins != 0 && st.stream) st.stream->releaseConsumerPins(st.reservedPins);
    st.reservedPins = 0;
}

void ImageRenderer::recordStreamUploads(const DeviceContext &dc, VkCommandBuffer cb, uint32_t frameSlot,
                                        uint64_t frameNo) {
    if (streams_.empty()) return;
    const bool zeroCopyAvail = dc.getHostPointerProps != nullptr;
    constexpr uint32_t kImportFailLimit = 3;    // 同一流连续导入失败达此数 → 永久回退 memcpy

    // ① 逐流取最新帧，决定拷贝源：零拷贝 = 池槽内存块导入的缓冲（无 CPU 拷贝）；
    //    拷贝路径 = memcpy 进本帧 staging。两者均为主机可见一致性内存，CPU 写
    //    先于 submit、同线程提交的隐式宿主依赖保证 GPU 可见——只收集 barrier/
    //    copy 描述，统一批量录制
    struct CopyItem {
        VkBuffer buf;
        VkImage img;
        VkBufferImageCopy region;
    };
    std::vector<VkImageMemoryBarrier> preBarriers, postBarriers;
    std::vector<CopyItem> copies;
    for (auto &[key, up] : streams_) {
        StreamTexture &st = *up;
        if (st.retireAt != 0) continue;                  // 挂起退役不再投喂
        if (!st.stream) continue;
        // 本帧槽 fence 已等：该槽上次零拷贝帧 GPU 已读完 → 解除其固定（还池）。
        // 须先于可见性判断——不可见的流也得及时放手，否则占槽致生产者丢帧
        if (st.heldPins.size() > frameSlot) st.heldPins[frameSlot] = PinGuard{};
        pruneImports(st, frameNo);                       // 池已换块的旧导入延迟释放
        if (frameNo > st.touchedFrame + 2) continue;     // 不可见（近期未录制绘制）跳过上传
        PinGuard pin = st.stream->pinLatest();
        if (!pin) continue;                              // 尚无任何帧
        const FrameDesc &d = pin.desc();
        // GPU 资源未建 / 格式或尺寸变更 → 整组重建（旧组延迟释放；RGB↔YUV 切换
        // 同时换描述符布局与绘制管线）
        bool needRebuild = st.res.slots.empty() || d.format != st.desc.format || d.width != st.desc.width
                           || d.height != st.desc.height;
        if (needRebuild) {
            if (!st.res.slots.empty()) deferFreeResources(std::move(st.res), frameNo);
            st.desc = d;
            if (!createStreamResources(dc, st)) {
                st.res = StreamResources{};    // 失败保持未就绪，下帧重试
                continue;
            }
        }
        if (pin.seq() == st.uploadedSeq) continue;       // 无新帧（屏 60Hz < 流帧率时多数帧跳过）

        // 拷贝源选择：零拷贝优先。池按构造把平面放在 块首 + FrameLayout 偏移
        // （VideoStream::acquire），与 staging 布局一致——两路拷贝区域偏移同源
        VkBuffer srcBuf = VK_NULL_HANDLE;
        bool zeroCopy = false;
        if (zeroCopyAvail && st.importFailures < kImportFailLimit && pin.block()) {
            srcBuf = importBlock(dc, st, pin.block());
            if (srcBuf != VK_NULL_HANDLE) {
                zeroCopy = true;
                if (!zeroCopyLogged_) {    // 每渲染器一次：日志可确认实际走零拷贝（非仅扩展已启用）
                    zeroCopyLogged_ = true;
                    Log::info("[video] stream upload path = zero-copy (host memory import)");
                }
            } else if (++st.importFailures == kImportFailLimit) {
                Log::warn("流纹理零拷贝导入连续失败，该流回退 memcpy 拷贝路径");
            }
        }
        if (!zeroCopy) {
            if (!ensureStage(dc, st, frameSlot)) continue;    // staging 建失败：本帧跳过，下帧重试
            StreamStage &stage = st.res.stages[frameSlot];
            auto planes = pin.planes();
            for (uint32_t p = 0; p < st.layout.planeCount && p < planes.size(); ++p) {
                const auto &pl = st.layout.planes[p];
                uint8_t *dst = static_cast<uint8_t *>(stage.mapped) + pl.offset;
                uint32_t rowBytes = pl.width * pl.bytesPerTexel;
                for (uint32_t y = 0; y < pl.height; ++y)
                    std::memcpy(dst + size_t(pl.stride) * y, planes[p].data + size_t(planes[p].stride) * y, rowBytes);
            }
            srcBuf = stage.buffer;
        }

        // 环推进 + 屏障/拷贝描述收集——槽复用安全由 fence 证明：第 n 次上传在
        // 帧 U_n，该槽下次覆写最早在 U_(n+3) ≥ U_(n+1)+3 帧，而其最后一次采样
        // 在 ≤ U_(n+1)-1 帧（该帧 fence 已等）。故 oldLayout=UNDEFINED 弃旧内容、
        // src 阶段 TOP_OF_PIPE 即合法，与在飞帧无执行依赖（GPU 无同步气泡）
        st.ringIndex = (st.ringIndex + 1) % uint32_t(st.res.slots.size());
        StreamSlot &slot = st.res.slots[st.ringIndex];
        // 逐 GPU 采样平面：拷贝源 = 源缓冲中第 srcPlane 平面，行长按该视图
        // 纹素字节换算（打包 4:2:2 同一源平面拷两次：RG8 亮度视图 + RGBA8 色度视图）
        for (uint32_t p = 0; p < st.plan.count; ++p) {
            const auto &tp = st.plan.planes[p];
            const auto &src = st.layout.planes[tp.srcPlane];
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = slot.images[p];
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;    // 槽复用即整体覆写，弃旧内容
            b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            // 布局转换须对随后的拷贝写入可见（缺此掩码即写后写冒险：同步校验
            // SYNC-HAZARD 报"拷贝写与先前布局转换无同步"）；src 侧无需等待——
            // fence 已证明该槽无在飞读（见上方推理）
            b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            preBarriers.push_back(b);
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            postBarriers.push_back(b);

            VkBufferImageCopy region{};
            region.bufferOffset = src.offset;
            region.bufferRowLength = src.stride / tp.texelBytes;    // 行距按本视图纹素计（对齐 stride 时 ≠ width）
            region.bufferImageHeight = 0;                           // 0 = 行紧随行长排布
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {tp.width, tp.height, 1};
            copies.push_back({srcBuf, slot.images[p], region});
        }
        st.uploadedSeq = pin.seq();
        st.hasUpload = true;
        st.zeroCopyUploads = zeroCopy;
        // 矩阵/range 不触发重建（只影响 push 常量），随上传帧更新——绘制采样的
        // 正是本次上传的槽，二者同帧对应
        st.desc.matrix = d.matrix;
        st.desc.fullRange = d.fullRange;
        if (zeroCopy) {
            // GPU 在本帧直接读池槽内存：固定须持有到本帧槽 fence 再等到（kMaxFramesInFlight
            // 帧后同一 frameSlot 回到此处才放手）。首次走零拷贝向流登记长时持有，
            // 池按需扩容——生产者在 GPU 持有期间仍有槽可写
            if (st.heldPins.empty()) st.heldPins.resize(VulkanContext::kMaxFramesInFlight);
            if (st.reservedPins == 0) {
                st.reservedPins = VulkanContext::kMaxFramesInFlight;
                st.stream->reserveConsumerPins(st.reservedPins);
            }
            st.heldPins[frameSlot] = std::move(pin);
        }
    }

    // ② 批量录制：前屏障（全部流合并）→ 拷贝 → 后屏障——屏障次数与流数无关
    if (copies.empty()) return;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, uint32_t(preBarriers.size()), preBarriers.data());
    for (const auto &c : copies)
        vkCmdCopyBufferToImage(cb, c.buf, c.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c.region);
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, uint32_t(postBarriers.size()), postBarriers.data());
}

void ImageRenderer::drawStream(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha,
                               const Transform2D &t, bool clipped) {
    auto sa = streamAliases_.find(cmd.textureId);
    if (sa == streamAliases_.end()) return;
    auto it = streams_.find(sa->second);
    if (it == streams_.end()) return;
    StreamTexture &st = *it->second;
    if (!st.hasUpload || st.res.slots.empty()) return;    // 首帧上传前由元素侧占位
    // 采样最近写入槽：上传时先推进 ringIndex 再写，故 ringIndex 即最近写槽
    // （该槽经后屏障已转 SHADER_READ_ONLY；未写过的槽仍为 UNDEFINED，不可采样）
    StreamSlot &slot = st.res.slots[st.ringIndex];
    // 管线：RGB 复用 image 主/clip 管线；YUV 走 yuv 管线（裁剪/非裁剪共用一条）
    const bool yuv = st.plan.yuv;
    VkPipelineLayout layout = yuv ? yuvPipelineLayout_ : (clipped ? imageClipPipelineLayout_ : imagePipelineLayout_);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      yuv ? yuvPipeline_ : (clipped ? imageClipPipeline_ : imagePipeline_));
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &slot.descSet, 0, nullptr);
    ImagePushConstants pc{};
    pc.posX = cmd.rect.x;
    pc.posY = cmd.rect.y;
    pc.sizeX = cmd.rect.width;
    pc.sizeY = cmd.rect.height;
    pc.uvU0 = cmd.uvU0;
    pc.uvV0 = cmd.uvV0;
    pc.uvU1 = cmd.uvU1;
    pc.uvV1 = cmd.uvV1;
    pc.colorR = 1.0f;
    pc.colorG = 1.0f;
    pc.colorB = 1.0f;
    pc.colorA = cmd.opacity * globalAlpha;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.cornerRadius = cmd.cornerRadius;
    pc.m00 = t.m00;
    pc.m01 = t.m01;
    pc.m02 = t.m02;
    pc.m10 = t.m10;
    pc.m11 = t.m11;
    pc.m12 = t.m12;
    if (yuv) {
        const YuvPushConstants ypc{pc, yuvModeFor(st.desc)};    // 84B image 段 + mode 字
        vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(YuvPushConstants), &ypc);
    } else {
        vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(ImagePushConstants), &pc);
    }
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}
