/**
 * @file vulkan_image_stream.cpp
 * @brief ImageRenderer 流纹理实现（V1 推流）— 资源组 / 上传录制 / 绘制
 *
 * 与 vulkan_image_renderer.cpp 同属 kwik.render.vulkan.image_renderer 模块
 * （一模块多实现单元，同 kwik.ext.video 的 video.cpp + bindings.cpp 惯例）。
 * 本文件全部函数仅在渲染线程执行（streams_/streamAliases_ 无锁访问前提）。
 */
module;
#include <vulkan/vulkan.h>
#include <cstring>
module kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.context;
import kwik.render.command;
import kwik.render.video_stream;
import kwik.core.types;
import std;

VkFormat ImageRenderer::vkFormatFor(PixelFormat f) {
    switch (f) {
    case PixelFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::BGRA8: return VK_FORMAT_B8G8R8A8_UNORM;
    default: return VK_FORMAT_UNDEFINED;    // YUV 批 2（image_yuv 管线）接入
    }
}

// 描述符分配：复用纹理池的懒建/扩容/旧池回填纪律（绝不 reset——会把全部
// 存活 descSet 一并作废，见 createTextureWithId 内同款注释）
VkDescriptorSet ImageRenderer::allocDescSet(const DeviceContext &dc, VkDescriptorPool &outPool) {
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
    sa.pSetLayouts = &imageDescSetLayout_;
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
    VkFormat fmt = vkFormatFor(st.desc.format);
    if (fmt == VK_FORMAT_UNDEFINED || st.desc.width == 0 || st.desc.height == 0) return false;
    st.layout = FrameLayout::forDesc(st.desc);
    if (st.layout.planeCount == 0) return false;
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
    // ② 图像环：每槽一块 DEVICE_LOCAL 内存按对齐子分配绑定全部平面
    for (uint32_t s = 0; s < N; ++s) {
        auto &slot = r.slots[s];
        slot.images.assign(st.layout.planeCount, VK_NULL_HANDLE);
        slot.views.assign(st.layout.planeCount, VK_NULL_HANDLE);
        std::vector<VkMemoryRequirements> reqs(st.layout.planeCount);
        for (uint32_t p = 0; p < st.layout.planeCount; ++p) {
            VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ii.imageType = VK_IMAGE_TYPE_2D;
            ii.format = fmt;
            ii.extent = {st.layout.planes[p].width, st.layout.planes[p].height, 1};
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
        std::vector<VkDeviceSize> bindOff(st.layout.planeCount, 0);
        VkDeviceSize total = 0;
        for (uint32_t p = 0; p < st.layout.planeCount; ++p) {
            bindOff[p] = (total + reqs[p].alignment - 1) & ~(VkDeviceSize(reqs[p].alignment) - 1);
            total = bindOff[p] + reqs[p].size;
        }
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = total;
        ai.memoryTypeIndex =
            VulkanContext::findMemoryType(dc.physicalDevice, reqs[0].memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(dc.device, &ai, nullptr, &slot.memory) != VK_SUCCESS) {
            freeStreamResources(r);
            return false;
        }
        for (uint32_t p = 0; p < st.layout.planeCount; ++p) {
            if (vkBindImageMemory(dc.device, slot.images[p], slot.memory, bindOff[p]) != VK_SUCCESS) {
                freeStreamResources(r);
                return false;
            }
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image = slot.images[p];
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = fmt;
            vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (vkCreateImageView(dc.device, &vi, nullptr, &slot.views[p]) != VK_SUCCESS) {
                freeStreamResources(r);
                return false;
            }
        }
    }
    // ③ staging：每在飞帧一份常驻映射（HOST_COHERENT）——写入按帧槽轮转，
    //    复用安全由 fence 等待保证（与顶点分槽同源）
    for (uint32_t s = 0; s < N; ++s) {
        auto &stage = r.stages[s];
        if (!VulkanContext::createBuffer(dc.device, dc.physicalDevice, st.layout.totalBytes,
                                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         stage.buffer, stage.memory)) {
            freeStreamResources(r);
            return false;
        }
        if (vkMapMemory(dc.device, stage.memory, 0, st.layout.totalBytes, 0, &stage.mapped) != VK_SUCCESS) {
            stage.mapped = nullptr;
            freeStreamResources(r);
            return false;
        }
        stage.capacity = st.layout.totalBytes;
    }
    // ④ 每槽描述符集：写一次此后只读（plane0 视图 + 共享采样器）——
    //    绝不更新在飞帧正在采样的 set（规范禁止）
    for (uint32_t s = 0; s < N; ++s) {
        auto &slot = r.slots[s];
        slot.descSet = allocDescSet(dc, slot.pool);
        if (slot.descSet == VK_NULL_HANDLE) {
            freeStreamResources(r);
            return false;
        }
        VkDescriptorImageInfo di{};
        di.sampler = streamSampler_;
        di.imageView = slot.views[0];
        di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = slot.descSet;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &di;
        vkUpdateDescriptorSets(dc.device, 1, &w, 0, nullptr);
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

void ImageRenderer::recordStreamUploads(const DeviceContext &dc, VkCommandBuffer cb, uint32_t frameSlot,
                                        uint64_t frameNo) {
    if (streams_.empty()) return;

    // ① 逐流取最新帧：memcpy 进本帧 staging（host 写先于 submit，HOST_COHERENT
    //    + 同线程提交顺序保证可见）——只收集 barrier/copy 描述，统一批量录制
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
        if (frameNo > st.touchedFrame + 2) continue;     // 不可见（近期未录制绘制）跳过上传
        if (!st.stream) continue;
        PinGuard pin = st.stream->pinLatest();
        if (!pin) continue;                              // 尚无任何帧
        const FrameDesc &d = pin.desc();
        if (vkFormatFor(d.format) == VK_FORMAT_UNDEFINED) continue;    // 批 1 仅 RGB 直通
        // GPU 资源未建 / 格式或尺寸变更 → 整组重建（旧组延迟释放）
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

        StreamStage &stage = st.res.stages[frameSlot];
        if (stage.capacity < st.layout.totalBytes) continue;    // 防御（重建组必配对）
        auto planes = pin.planes();
        for (uint32_t p = 0; p < st.layout.planeCount && p < planes.size(); ++p) {
            const auto &pl = st.layout.planes[p];
            uint8_t *dst = static_cast<uint8_t *>(stage.mapped) + pl.offset;
            uint32_t rowBytes = pl.width * pl.bytesPerTexel;
            for (uint32_t y = 0; y < pl.height; ++y)
                std::memcpy(dst + size_t(pl.stride) * y, planes[p].data + size_t(planes[p].stride) * y, rowBytes);
        }

        // 环推进 + 屏障/拷贝描述收集——槽复用安全由 fence 证明：第 n 次上传在
        // 帧 U_n，该槽下次覆写最早在 U_(n+3) ≥ U_(n+1)+3 帧，而其最后一次采样
        // 在 ≤ U_(n+1)-1 帧（该帧 fence 已等）。故 oldLayout=UNDEFINED 弃旧内容、
        // src 阶段 TOP_OF_PIPE 即合法，与在飞帧无执行依赖（GPU 无同步气泡）
        st.ringIndex = (st.ringIndex + 1) % uint32_t(st.res.slots.size());
        StreamSlot &slot = st.res.slots[st.ringIndex];
        for (uint32_t p = 0; p < st.layout.planeCount; ++p) {
            const auto &pl = st.layout.planes[p];
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = slot.images[p];
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;    // 槽复用即整体覆写，弃旧内容
            b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            preBarriers.push_back(b);
            b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            postBarriers.push_back(b);

            VkBufferImageCopy region{};
            region.bufferOffset = pl.offset;
            region.bufferRowLength = pl.stride / pl.bytesPerTexel;    // 行距按 texel 计（对齐 stride 时 ≠ width）
            region.bufferImageHeight = 0;                             // 0 = 行紧随行长排布
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {pl.width, pl.height, 1};
            copies.push_back({stage.buffer, slot.images[p], region});
        }
        st.uploadedSeq = pin.seq();
        st.hasUpload = true;
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
    VkPipelineLayout layout = clipped ? imageClipPipelineLayout_ : imagePipelineLayout_;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, clipped ? imageClipPipeline_ : imagePipeline_);
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
    vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(ImagePushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}
