module;
#include <vulkan/vulkan.h>
#include <cstring>
#include <cmath>
#include <cstddef>
#include "image_shaders.h"
module kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.context;
import kwik.render.vulkan.pipeline_factory;    // 管线工厂（§四）
import kwik.render.command;
import kwik.core.types;
import std;

ImageRenderer::~ImageRenderer() {
    destroy();
}
// ================================================================
// destroy
// ================================================================
void ImageRenderer::destroy() {
    if (device_ == VK_NULL_HANDLE) return;
    for (auto &[id, t] : textures_) {
        vkDestroySampler(device_, t.sampler, nullptr);
        vkDestroyImageView(device_, t.view, nullptr);
        vkDestroyImage(device_, t.image, nullptr);
        vkFreeMemory(device_, t.memory, nullptr);
    }
    textures_.clear();
    if (imageDescPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, imageDescPool_, nullptr);
        imageDescPool_ = VK_NULL_HANDLE;
    }
    if (imagePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, imagePipeline_, nullptr);
        imagePipeline_ = VK_NULL_HANDLE;
    }
    if (imageClipPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, imageClipPipeline_, nullptr);
        imageClipPipeline_ = VK_NULL_HANDLE;
    }
    if (imageClipPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, imageClipPipelineLayout_, nullptr);
        imageClipPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (imagePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, imagePipelineLayout_, nullptr);
        imagePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (imageDescSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, imageDescSetLayout_, nullptr);
        imageDescSetLayout_ = VK_NULL_HANDLE;
    }
}
// ================================================================
// create — 管线 + descriptor set layout
// ================================================================
bool ImageRenderer::create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice physDevice, VkRenderPass renderPass, VkBuffer vertexBuffer,
                           VkBuffer indexBuffer) {
    device_ = device;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;
    // ── 管线经工厂创建（清单 §四）。主/clip 手写版 depth-stencil 逐值相同，
    //    统一 StencilTestEqual，逐值迁移像素不变。clip 复用 setLayout。
    static const VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    static const VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    static const VkDescriptorSetLayoutBinding dslBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    PipeDesc pd;
    pd.vertSpv = kwik::shader::kImageVert;  pd.vertSize = kwik::shader::kImageVertSize;
    pd.fragSpv = kwik::shader::kImageFrag;  pd.fragSize = kwik::shader::kImageFragSize;
    pd.bindings = {&vtxBind, 1};
    pd.attrs = {&vtxAttr, 1};
    pd.pushSize = sizeof(ImagePushConstants);
    pd.blend = PipeDesc::Blend::SrcOver;
    pd.depthStencil = PipeDesc::DS::StencilTestEqual;
    pd.stencilDynStates = true;
    pd.descBindings = {&dslBinding, 1};
    pd.renderPass = renderPass;

    auto mainPipe = makePipeline(device_, cache, pd);
    if (mainPipe.pipeline == VK_NULL_HANDLE) return false;
    imagePipeline_ = mainPipe.pipeline;
    imagePipelineLayout_ = mainPipe.layout;
    imageDescSetLayout_ = mainPipe.setLayout;

    pd.externalSetLayout = imageDescSetLayout_;    // clip 变体复用 setLayout
    auto clipPipe = makePipeline(device_, cache, pd);
    if (clipPipe.pipeline == VK_NULL_HANDLE) return false;
    imageClipPipeline_ = clipPipe.pipeline;
    imageClipPipelineLayout_ = clipPipe.layout;

    return true;
}
// ================================================================
// createTexture — 上传 RGBA + mipmap 生成
// ================================================================
uint32_t ImageRenderer::createTexture(const DeviceContext &dc, const uint8_t *rgba, uint32_t width, uint32_t height) {
    if (!rgba || width == 0 || height == 0) return 0;
    VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;
    // ── Staging buffer ──
    VkBuffer staging;
    VkDeviceMemory stagingMem;
    if (!VulkanContext::createBuffer(dc.device, dc.physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     staging, stagingMem))
        return 0;
    {
        void *mapped;
        vkMapMemory(dc.device, stagingMem, 0, imageSize, 0, &mapped);
        std::memcpy(mapped, rgba, (size_t)imageSize);
        vkUnmapMemory(dc.device, stagingMem);
    }
    uint32_t mipLevels = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
    // ── Device-local VkImage ──
    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width, height, 1};
    imgInfo.mipLevels = mipLevels;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureData tex;
    tex.width = width;
    tex.height = height;
    if (vkCreateImage(dc.device, &imgInfo, nullptr, &tex.image) != VK_SUCCESS) {
        vkDestroyBuffer(dc.device, staging, nullptr);
        vkFreeMemory(dc.device, stagingMem, nullptr);
        return 0;
    }
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dc.device, tex.image, &mr);
    VkMemoryAllocateInfo ai{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
        VulkanContext::findMemoryType(dc.physicalDevice, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if (vkAllocateMemory(dc.device, &ai, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyImage(dc.device, tex.image, nullptr);
        vkDestroyBuffer(dc.device, staging, nullptr);
        vkFreeMemory(dc.device, stagingMem, nullptr);
        return 0;
    }
    vkBindImageMemory(dc.device, tex.image, tex.memory, 0);
    // ── 一次性命令 — copy + mipmap（§四收口） ──
    VulkanContext::runOneOff(dc.device, dc.commandPool, dc.queue, [&](VkCommandBuffer cmd) {
        // UNDEFINED → TRANSFER_DST (level 0)
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = tex.image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);
        // Copy staging → image (level 0)
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(cmd, staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        if (mipLevels > 1) {
            VkImageMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            preBarrier.srcAccessMask = 0;
            preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            preBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            preBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            preBarrier.image = tex.image;
            preBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 1, mipLevels - 1, 0, 1};
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &preBarrier);
            barrier.subresourceRange.levelCount = 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                 nullptr, 1, &barrier);
            int32_t mipW = (int32_t)width, mipH = (int32_t)height;
            for (uint32_t i = 1; i < mipLevels; i++) {
                VkImageBlit blit{};
                blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
                blit.srcOffsets[1] = {mipW, mipH, 1};
                blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
                blit.dstOffsets[1] = {mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1};
                vkCmdBlitImage(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
                VkImageMemoryBarrier mb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                mb.image = tex.image;
                mb.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1};
                mb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                mb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                mb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                                     0, nullptr, 1, &mb);
                if (mipW > 1) mipW /= 2;
                if (mipH > 1) mipH /= 2;
            }
            VkImageMemoryBarrier tb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            tb.image = tex.image;
            tb.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
            tb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            tb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            tb.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            tb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &tb);
        } else {
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &barrier);
        }
    });
    vkDestroyBuffer(dc.device, staging, nullptr);
    vkFreeMemory(dc.device, stagingMem, nullptr);
    // ── ImageView ──
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = tex.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
    if (vkCreateImageView(dc.device, &vi, nullptr, &tex.view) != VK_SUCCESS) {
        vkDestroyImage(dc.device, tex.image, nullptr);
        vkFreeMemory(dc.device, tex.memory, nullptr);
        return 0;
    }
    // ── Sampler ──
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = (float)mipLevels;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(dc.device, &samplerInfo, nullptr, &tex.sampler);
    // ── Descriptor pool (lazy) ──
    if (imageDescPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256};
        VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        // destroyTexture 走 vkFreeDescriptorSets 逐个释放，池必须带 FREE 标志
        // （与 glyph 渲染器一致；缺标志时 vkFreeDescriptorSets 为规格违规）
        pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pi.poolSizeCount = 1;
        pi.pPoolSizes = &ps;
        pi.maxSets = 256;
        vkCreateDescriptorPool(dc.device, &pi, nullptr, &imageDescPool_);
    }
    // ── Descriptor set ──
    VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    sa.descriptorPool = imageDescPool_;
    sa.descriptorSetCount = 1;
    sa.pSetLayouts = &imageDescSetLayout_;
    VkResult dsr = vkAllocateDescriptorSets(dc.device, &sa, &tex.descSet);
    if (dsr == VK_ERROR_OUT_OF_POOL_MEMORY) {
        vkResetDescriptorPool(dc.device, imageDescPool_, 0);
        dsr = vkAllocateDescriptorSets(dc.device, &sa, &tex.descSet);
    }
    if (dsr != VK_SUCCESS) {
        vkDestroySampler(dc.device, tex.sampler, nullptr);
        vkDestroyImageView(dc.device, tex.view, nullptr);
        vkDestroyImage(dc.device, tex.image, nullptr);
        vkFreeMemory(dc.device, tex.memory, nullptr);
        return 0;
    }

    VkDescriptorImageInfo di{};
    di.sampler = tex.sampler;
    di.imageView = tex.view;
    di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = tex.descSet;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &di;
    vkUpdateDescriptorSets(dc.device, 1, &w, 0, nullptr);
    uint32_t id = nextId_++;
    textures_[id] = tex;
    return id;
}
// ================================================================
// destroyTexture
// ================================================================
void ImageRenderer::destroyTexture(uint32_t id) {
    auto it = textures_.find(id);
    if (it == textures_.end()) return;
    auto &t = it->second;
    // 纹理销毁为低频路径（image 重载/树重建/shutdown）：等 GPU 排空再释放，
    // 否则最多 3 个在途帧的命令缓冲仍绑定该描述符集/采样器 → 规格违规
    vkDeviceWaitIdle(device_);
    vkFreeDescriptorSets(device_, imageDescPool_, 1, &t.descSet);
    vkDestroySampler(device_, t.sampler, nullptr);
    vkDestroyImageView(device_, t.view, nullptr);
    vkDestroyImage(device_, t.image, nullptr);
    vkFreeMemory(device_, t.memory, nullptr);
    textures_.erase(it);
}
// ================================================================
// drawImage
// ================================================================
void ImageRenderer::drawImage(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha,
                              const Transform2D &t) {
    auto it = textures_.find(cmd.textureId);
    if (it == textures_.end()) return;
    auto &tex = it->second;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipelineLayout_, 0, 1, &tex.descSet, 0, nullptr);
    ImagePushConstants pc{};
    pc.posX = cmd.rect.x;
    pc.posY = cmd.rect.y;
    pc.sizeX = cmd.rect.width;
    pc.sizeY = cmd.rect.height;
    pc.uvU0 = 0.0f;
    pc.uvV0 = 0.0f;
    pc.uvU1 = 1.0f;
    pc.uvV1 = 1.0f;
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
    vkCmdPushConstants(cb, imagePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(ImagePushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}
// ================================================================
// drawImageClipped — stencil 测试版
// ================================================================
void ImageRenderer::drawImageClipped(VkCommandBuffer cb, VkExtent2D extent, const DrawImageCmd &cmd, float globalAlpha,
                                     const Transform2D &t) {
    auto it = textures_.find(cmd.textureId);
    if (it == textures_.end()) return;
    auto &tex = it->second;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, imageClipPipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipelineLayout_, 0, 1, &tex.descSet, 0, nullptr);
    ImagePushConstants pc{};
    pc.posX = cmd.rect.x;
    pc.posY = cmd.rect.y;
    pc.sizeX = cmd.rect.width;
    pc.sizeY = cmd.rect.height;
    pc.uvU0 = 0.0f;
    pc.uvV0 = 0.0f;
    pc.uvU1 = 1.0f;
    pc.uvV1 = 1.0f;
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
    vkCmdPushConstants(cb, imagePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(ImagePushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}