module;
#include <vulkan/vulkan.h>
#include <cstring>
#include <cstddef>
#include <print>
#include "glyph_shaders.h"
module kwik.render.vulkan.glyph_renderer;
import kwik.render.vulkan.context;
import kwik.render.command;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.render.text.cache;
import std;

GlyphRenderer::~GlyphRenderer() {
    destroy();
}
// ================================================================
// destroy
// ================================================================
void GlyphRenderer::destroy() {
    if (device_ == VK_NULL_HANDLE) return;
    if (glyphDescPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, glyphDescPool_, nullptr);
        glyphDescPool_ = VK_NULL_HANDLE;
    }
    if (glyphDescSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, glyphDescSetLayout_, nullptr);
        glyphDescSetLayout_ = VK_NULL_HANDLE;
    }
    if (glyphAtlasSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, glyphAtlasSampler_, nullptr);
        glyphAtlasSampler_ = VK_NULL_HANDLE;
    }
    if (glyphAtlasView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, glyphAtlasView_, nullptr);
        glyphAtlasView_ = VK_NULL_HANDLE;
    }
    if (glyphAtlasImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, glyphAtlasImage_, nullptr);
        glyphAtlasImage_ = VK_NULL_HANDLE;
    }
    if (glyphAtlasMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, glyphAtlasMemory_, nullptr);
        glyphAtlasMemory_ = VK_NULL_HANDLE;
    }
    if (glyphPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, glyphPipeline_, nullptr);
        glyphPipeline_ = VK_NULL_HANDLE;
    }
    if (glyphClipPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, glyphClipPipeline_, nullptr);
        glyphClipPipeline_ = VK_NULL_HANDLE;
    }
    if (glyphPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, glyphPipelineLayout_, nullptr);
        glyphPipelineLayout_ = VK_NULL_HANDLE;
    }
}

// ================================================================
// create — glyph 管线 + 1024x1024 R8_UNORM 图集
// ================================================================
bool GlyphRenderer::create(VkDevice device, VkPhysicalDevice physDevice, VkCommandPool cmdPool, VkQueue queue,
                           VkRenderPass renderPass, VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    device_ = device;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;
    VkShaderModule glyphVert =
        VulkanContext::createShaderModule(device_, kwik::shader::kGlyphVert, kwik::shader::kGlyphVertSize);
    VkShaderModule glyphFrag =
        VulkanContext::createShaderModule(device_, kwik::shader::kGlyphFrag, kwik::shader::kGlyphFragSize);
    if (!glyphVert || !glyphFrag) return false;
    // Descriptor set layout
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 1;
    dsl.pBindings = &samplerBinding;
    if (vkCreateDescriptorSetLayout(device_, &dsl, nullptr, &glyphDescSetLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, glyphFrag, nullptr);
        vkDestroyShaderModule(device_, glyphVert, nullptr);
        return false;
    }
    // Pipeline layout
    VkPushConstantRange pcRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                sizeof(GlyphPushConstants)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &glyphDescSetLayout_;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(device_, &pl, nullptr, &glyphPipelineLayout_) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device_, glyphDescSetLayout_, nullptr);
        vkDestroyShaderModule(device_, glyphFrag, nullptr);
        vkDestroyShaderModule(device_, glyphVert, nullptr);
        return false;
    }
    // 管线 stages / 输入 / 视口 / 光栅化 / 混合
    VkPipelineShaderStageCreateInfo stages[] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, glyphVert,
         "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, glyphFrag,
         "main"},
    };
    VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    VkPipelineVertexInputStateCreateInfo vtxIn{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vtxIn.vertexBindingDescriptionCount = 1;
    vtxIn.pVertexBindingDescriptions = &vtxBind;
    vtxIn.vertexAttributeDescriptionCount = 1;
    vtxIn.pVertexAttributeDescriptions = &vtxAttr;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba{};
    ba.blendEnable = VK_TRUE;
    ba.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.colorBlendOp = VK_BLEND_OP_ADD;
    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.alphaBlendOp = VK_BLEND_OP_ADD;
    ba.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &ba;
    VkDynamicState dynStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,           VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,  VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 5;
    dyn.pDynamicStates = dynStates;
    VkStencilOpState stencilNoWrite{};
    stencilNoWrite.failOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.passOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilNoWrite.compareOp = VK_COMPARE_OP_EQUAL;
    stencilNoWrite.compareMask = 0xFF;
    stencilNoWrite.writeMask = 0x00;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.stencilTestEnable = VK_TRUE;
    ds.front = stencilNoWrite;
    ds.back = stencilNoWrite;
    VkGraphicsPipelineCreateInfo pipeInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;
    pipeInfo.pVertexInputState = &vtxIn;
    pipeInfo.pInputAssemblyState = &ia;
    pipeInfo.pViewportState = &vp;
    pipeInfo.pRasterizationState = &rs;
    pipeInfo.pMultisampleState = &ms;
    pipeInfo.pColorBlendState = &blend;
    pipeInfo.pDynamicState = &dyn;
    pipeInfo.layout = glyphPipelineLayout_;
    pipeInfo.renderPass = renderPass;
    pipeInfo.subpass = 0;
    pipeInfo.pDepthStencilState = &ds;
    bool ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &glyphPipeline_) == VK_SUCCESS;
    if (!ok) {
        vkDestroyPipelineLayout(device_, glyphPipelineLayout_, nullptr);
        vkDestroyShaderModule(device_, glyphFrag, nullptr);
        vkDestroyShaderModule(device_, glyphVert, nullptr);
        return false;
    }
    // ── Stencil 测试变体管线 ─────────────────────────────
    {
        VkDynamicState clipDynStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,           VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,  VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        };
        VkPipelineDynamicStateCreateInfo clipDyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        clipDyn.dynamicStateCount = 5;
        clipDyn.pDynamicStates = clipDynStates;
        VkStencilOpState stencilTest{};
        stencilTest.failOp = VK_STENCIL_OP_KEEP;
        stencilTest.passOp = VK_STENCIL_OP_KEEP;
        stencilTest.depthFailOp = VK_STENCIL_OP_KEEP;
        stencilTest.compareOp = VK_COMPARE_OP_EQUAL;
        stencilTest.compareMask = 0xFF;
        stencilTest.writeMask = 0x00;
        VkPipelineDepthStencilStateCreateInfo dsClip{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dsClip.stencilTestEnable = VK_TRUE;
        dsClip.front = stencilTest;
        dsClip.back = stencilTest;
        pipeInfo.pDynamicState = &clipDyn;
        pipeInfo.pDepthStencilState = &dsClip;
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &glyphClipPipeline_);
    }
    vkDestroyShaderModule(device_, glyphFrag, nullptr);
    vkDestroyShaderModule(device_, glyphVert, nullptr);
    if (!ok) {
        vkDestroyPipelineLayout(device_, glyphPipelineLayout_, nullptr);
        return false;
    }
    // ── Glyph atlas 1024x1024 R8_UNORM (FreeType A8 位图) ───────────
    uint32_t atlasW = TextCache::kAtlasSize, atlasH = TextCache::kAtlasSize;
    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {atlasW, atlasH, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device_, &imgInfo, nullptr, &glyphAtlasImage_) != VK_SUCCESS) {
        destroy();
        return false;
    }
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device_, glyphAtlasImage_, &mr);
    VkMemoryAllocateInfo ai{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
        VulkanContext::findMemoryType(physDevice, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if (vkAllocateMemory(device_, &ai, nullptr, &glyphAtlasMemory_) != VK_SUCCESS) {
        destroy();
        return false;
    }

    vkBindImageMemory(device_, glyphAtlasImage_, glyphAtlasMemory_, 0);

    // ── 初始 layout 过渡: UNDEFINED → SHADER_READ_ONLY_OPTIMAL ──
    {
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool = cmdPool;
        cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cai.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &cai, &cmd);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);
        VkImageMemoryBarrier initBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        initBarrier.image = glyphAtlasImage_;
        initBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        initBarrier.subresourceRange.levelCount = 1;
        initBarrier.subresourceRange.layerCount = 1;
        initBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        initBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        initBarrier.srcAccessMask = 0;
        initBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &initBarrier);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device_, cmdPool, 1, &cmd);
    }
    atlasLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = glyphAtlasImage_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device_, &vi, nullptr, &glyphAtlasView_) != VK_SUCCESS) {
        destroy();
        return false;
    }
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(device_, &si, nullptr, &glyphAtlasSampler_) != VK_SUCCESS) {
        destroy();
        return false;
    }
    // Descriptor pool
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    pi.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &pi, nullptr, &glyphDescPool_) != VK_SUCCESS) {
        destroy();
        return false;
    }
    // Descriptor set
    VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    sa.descriptorPool = glyphDescPool_;
    sa.descriptorSetCount = 1;
    sa.pSetLayouts = &glyphDescSetLayout_;
    if (vkAllocateDescriptorSets(device_, &sa, &glyphDescSet_) != VK_SUCCESS) {
        destroy();
        return false;
    }
    VkDescriptorImageInfo di{glyphAtlasSampler_, glyphAtlasView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = glyphDescSet_;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &di;
    vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
    return true;
}
// ================================================================
// drawGlyph
// ================================================================
void GlyphRenderer::drawGlyph(VkCommandBuffer cb, VkExtent2D extent, const DrawGlyphCmd &cmd, float globalAlpha) {
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphPipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphPipelineLayout_, 0, 1, &glyphDescSet_, 0,
                            nullptr);
    GlyphPushConstants pc{};
    pc.posX = cmd.x;
    pc.posY = cmd.y;
    pc.sizeX = cmd.width;
    pc.sizeY = cmd.height;
    pc.uvU0 = cmd.uvLeft;
    pc.uvV0 = cmd.uvTop;
    pc.uvU1 = cmd.uvRight;
    pc.uvV1 = cmd.uvBottom;
    pc.colorR = cmd.color.r / 255.f;
    pc.colorG = cmd.color.g / 255.f;
    pc.colorB = cmd.color.b / 255.f;
    pc.colorA = cmd.color.a / 255.f * globalAlpha;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.textContrast = 1.0f;
    vkCmdPushConstants(cb, glyphPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(GlyphPushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}

// ============================================================================
// uploadPendingGlyphs — 通过 TextAtlas + TextService 逐 glyph 上传
// ============================================================================
void GlyphRenderer::uploadPendingGlyphs(const DeviceContext &dc) {
    auto jobs = TextRenderPipeline::instance().consumeUploads();
    if (jobs.empty()) return;

    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = dc.commandPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(dc.device, &cai, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = glyphAtlasImage_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = atlasLayout_;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask =
        (atlasLayout_ == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ? VK_ACCESS_SHADER_READ_BIT : VkAccessFlags(0);
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    VkPipelineStageFlags srcStage = (atlasLayout_ == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) ?
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT :
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 计算总大小并分配单个 staging buffer
    VkDeviceSize totalSize = 0;
    for (auto &job : jobs) {
        if (!job.pixelData.empty()) totalSize += job.pixelData.size();
    }
    if (totalSize == 0) return;

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    if (!VulkanContext::createBuffer(dc.device, dc.physicalDevice, totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     staging, stagingMem))
        return;

    void *map;
    vkMapMemory(dc.device, stagingMem, 0, totalSize, 0, &map);

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(jobs.size());
    size_t offset = 0;

    for (auto &job : jobs) {
        auto &px = job.pixelData;
        if (px.empty()) continue;

        std::memcpy(static_cast<uint8_t *>(map) + offset, px.data(), px.size());

        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {(int32_t)job.x, (int32_t)job.y, 0};
        region.imageExtent = {job.w, job.h, 1};
        regions.push_back(region);

        offset += px.size();
    }
    vkUnmapMemory(dc.device, stagingMem);

    vkCmdCopyBufferToImage(cmd, staging, glyphAtlasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           (uint32_t)regions.size(), regions.data());

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    atlasLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(dc.queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(dc.queue);

    vkDestroyBuffer(dc.device, staging, nullptr);
    vkFreeMemory(dc.device, stagingMem, nullptr);
    vkFreeCommandBuffers(dc.device, dc.commandPool, 1, &cmd);
}

// ================================================================
// drawGlyphClipped — stencil 测试版
// ================================================================
void GlyphRenderer::drawGlyphClipped(VkCommandBuffer cb, VkExtent2D extent, const DrawGlyphCmd &cmd,
                                     float globalAlpha) {
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphClipPipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, glyphPipelineLayout_, 0, 1, &glyphDescSet_, 0,
                            nullptr);
    GlyphPushConstants pc{};
    pc.posX = cmd.x;
    pc.posY = cmd.y;
    pc.sizeX = cmd.width;
    pc.sizeY = cmd.height;
    pc.uvU0 = cmd.uvLeft;
    pc.uvV0 = cmd.uvTop;
    pc.uvU1 = cmd.uvRight;
    pc.uvV1 = cmd.uvBottom;
    pc.colorR = cmd.color.r / 255.f;
    pc.colorG = cmd.color.g / 255.f;
    pc.colorB = cmd.color.b / 255.f;
    pc.colorA = cmd.color.a / 255.f * globalAlpha;
    pc.viewportW = static_cast<float>(extent.width);
    pc.viewportH = static_cast<float>(extent.height);
    pc.textContrast = 1.0f;
    vkCmdPushConstants(cb, glyphPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(GlyphPushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}