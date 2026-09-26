module;
#include <vulkan/vulkan.h>
#include <cmath>
#include <cstddef>
#include "backdrop_shaders.h"
#include "backdrop_quad_shaders.h"
module kwik.render.vulkan.backdrop_renderer;
import kwik.render.vulkan.context;
import kwik.render.vulkan.pipeline_factory;    // 管线工厂（§四）
import kwik.render.command;
import kwik.core.types;
import std;

BackdropRenderer::~BackdropRenderer() { destroy(); }

// ================================================================
// create — 离屏 renderpass + 4 条管线经工厂创建（blurH/blurV/composite±clip）
// ================================================================
bool BackdropRenderer::create(VkDevice device, VkPipelineCache cache, VkPhysicalDevice phys, VkRenderPass mainPass,
                              VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    device_ = device;
    vertexBuffer_ = vertexBuffer;
    indexBuffer_ = indexBuffer;

    offscreenPass_ = createOffscreenPass(offscreenFormat_);
    if (offscreenPass_ == VK_NULL_HANDLE) return false;

    // ── 管线经工厂创建（清单 §四）。binding0 = 模糊/源图（blur 与 composite
    //    共用）；binding1 = 原始捕获图（composite 折射采样）。blur shader 仅声明
    //    binding0，binding1 静态未使用（合法），统一布局省一套 descSetLayout。
    //    blur 全屏覆盖无需混合（Blend::Write 全通道直写）；composite SrcOver。
    //    四条均为 2 动态态（viewport/scissor）。逐值迁移，像素不变（clip 变体
    //    静态 ref 修正除外，见下）。
    static const VkVertexInputBindingDescription vtxBind{0, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    static const VkVertexInputAttributeDescription vtxAttr{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    static const VkDescriptorSetLayoutBinding sbs[2] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    PipeDesc pd;
    pd.vertSpv = kwik::shader::kBackdropVert;  pd.vertSize = kwik::shader::kBackdropVertSize;
    pd.fragSpv = kwik::shader::kBackdropFrag;  pd.fragSize = kwik::shader::kBackdropFragSize;
    pd.bindings = {&vtxBind, 1};
    pd.attrs = {&vtxAttr, 1};
    pd.pushSize = sizeof(BlurPushConstants);
    pd.blend = PipeDesc::Blend::Write;
    pd.depthStencil = PipeDesc::DS::Off;
    pd.stencilDynStates = false;
    pd.descBindings = {sbs, 2};
    pd.renderPass = offscreenPass_;

    auto bh = makePipeline(device_, cache, pd);
    if (bh.pipeline == VK_NULL_HANDLE) return false;
    blurHPipeline_ = bh.pipeline;
    blurLayout_ = bh.layout;
    descSetLayout_ = bh.setLayout;

    pd.externalSetLayout = descSetLayout_;    // blurV 同参复用 setLayout/layout
    pd.externalLayout = blurLayout_;
    auto bv = makePipeline(device_, cache, pd);
    if (bv.pipeline == VK_NULL_HANDLE) return false;
    blurVPipeline_ = bv.pipeline;

    // composite：主 pass + QuadPushConstants（push 尺寸不同，layout 自建一份）
    pd.vertSpv = kwik::shader::kBackdropQuadVert;  pd.vertSize = kwik::shader::kBackdropQuadVertSize;
    pd.fragSpv = kwik::shader::kBackdropQuadFrag;  pd.fragSize = kwik::shader::kBackdropQuadFragSize;
    pd.pushSize = sizeof(QuadPushConstants);
    pd.blend = PipeDesc::Blend::SrcOver;
    pd.renderPass = mainPass;
    pd.externalLayout = VK_NULL_HANDLE;
    auto bc = makePipeline(device_, cache, pd);
    if (bc.pipeline == VK_NULL_HANDLE) return false;
    compositePipeline_ = bc.pipeline;
    quadLayout_ = bc.layout;

    // clip 变体：StencilTestEqual，静态 ref=1（掩码写 1，EQUAL 即"裁剪内通过"）。
    //    修正存量 bug：手写版 VkStencilOpState{} 零初始化致静态 ref=0，语义反转
    //    （裁剪内丢弃/裁剪外通过）；本管线未声明 stencil 动态态，运行时
    //    vkCmdSetStencilReference 对它无效——与 rect/glyph/image clip 变体实际的
    //    ref=1 对齐。glass.js ⑤"裁剪内玻璃"场景目视验证。
    pd.depthStencil = PipeDesc::DS::StencilTestEqual;
    pd.externalLayout = quadLayout_;    // 与 composite 同 layout
    auto bk = makePipeline(device_, cache, pd);
    if (bk.pipeline == VK_NULL_HANDLE) return false;
    compositeClipPipeline_ = bk.pipeline;

    return true;
}

// ================================================================
// createOffscreenPass — 仅 color attachment（无 stencil）
// loadOp=DONT_CARE：blur 全屏覆盖，无需加载旧内容
// finalLayout=SHADER_READ：EndRenderPass 自动过渡，无需手动 barrier
// ================================================================
VkRenderPass BackdropRenderer::createOffscreenPass(VkFormat fmt) {
    VkAttachmentDescription att{};
    att.format = fmt;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp{};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1; sp.pColorAttachments = &ref;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1; rp.pAttachments = &att;
    rp.subpassCount = 1; rp.pSubpasses = &sp;
    VkRenderPass out;
    return vkCreateRenderPass(device_, &rp, nullptr, &out) == VK_SUCCESS ? out : VK_NULL_HANDLE;
}

// ================================================================
// ensureTargets — 按 canvas/4 尺寸建 3 个 target + framebuffer + descSet
// 尺寸键 = 画布尺寸：仅 swapchain resize（帧间、vkDeviceWaitIdle 后）会变，
// 帧内多玻璃元素共用同一套 target/descSet，零中途销毁重建——
// 中途 reset 描述符池/销毁图像会使本命令缓冲已录制的 draw 失效（DEVICE_LOST）。
// ================================================================
bool BackdropRenderer::ensureTargets(VkPhysicalDevice phys, uint32_t w, uint32_t h) {
    if (w == tw_ && h == th_ && capture_.image != VK_NULL_HANDLE) return true;
    destroyTargets();
    // destroyTargets 后旧 descSet 全部失效：整体重置池从头分配（此刻设备空闲，安全）。
    if (descPool_ != VK_NULL_HANDLE) vkResetDescriptorPool(device_, descPool_, 0);

    auto mkTarget = [&](BackdropTarget &t, uint32_t tw, uint32_t th) -> bool {
        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D; ii.format = offscreenFormat_;
        ii.extent = {tw, th, 1}; ii.mipLevels = 1; ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device_, &ii, nullptr, &t.image) != VK_SUCCESS) return false;
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(device_, t.image, &mr);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mr.size,
            VulkanContext::findMemoryType(phys, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
        if (vkAllocateMemory(device_, &ai, nullptr, &t.memory) != VK_SUCCESS) return false;
        vkBindImageMemory(device_, t.image, t.memory, 0);
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = t.image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = offscreenFormat_;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCreateImageView(device_, &vi, nullptr, &t.view);
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = VK_FILTER_LINEAR; si.minFilter = VK_FILTER_LINEAR;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.maxLod = 0; si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(device_, &si, nullptr, &t.sampler);
        if (descPool_ == VK_NULL_HANDLE) {
            VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
            VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pi.poolSizeCount = 1; pi.pPoolSizes = &ps; pi.maxSets = 8;
            vkCreateDescriptorPool(device_, &pi, nullptr, &descPool_);
        }
        VkDescriptorSetAllocateInfo sai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        sai.descriptorPool = descPool_; sai.descriptorSetCount = 1;
        sai.pSetLayouts = &descSetLayout_;
        if (vkAllocateDescriptorSets(device_, &sai, &t.descSet) != VK_SUCCESS &&
            (vkResetDescriptorPool(device_, descPool_, 0),
             vkAllocateDescriptorSets(device_, &sai, &t.descSet) != VK_SUCCESS))
            return false;
        VkDescriptorImageInfo di{};
        di.sampler = t.sampler; di.imageView = t.view;
        di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = t.descSet; w.dstBinding = 0; w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w.pImageInfo = &di;
        vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
        return true;
    };
    auto mkFbo = [&](VkImageView view, VkFramebuffer *out) -> bool {
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass = offscreenPass_; fi.attachmentCount = 1; fi.pAttachments = &view;
        fi.width = w; fi.height = h; fi.layers = 1;
        return vkCreateFramebuffer(device_, &fi, nullptr, out) == VK_SUCCESS;
    };

    if (!mkTarget(capture_, w, h)) return false;
    if (!mkTarget(pingA_, w, h))  return false;
    if (!mkTarget(pingB_, w, h))  return false;
    if (!mkFbo(capture_.view, &captureFbo_)) return false;
    if (!mkFbo(pingA_.view, &pingAFbo_))     return false;
    if (!mkFbo(pingB_.view, &pingBFbo_))     return false;
    // binding1 统一指向原始捕获图：composite（绑 pingB 的 descSet）折射采样用；
    // blur shader 静态未使用 binding1，capture_/pingA_ 的写入仅为描述符合法性。
    auto writeRawBinding = [&](BackdropTarget &t) {
        VkDescriptorImageInfo di{};
        di.sampler = capture_.sampler; di.imageView = capture_.view;
        di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = t.descSet; w.dstBinding = 1; w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w.pImageInfo = &di;
        vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
    };
    writeRawBinding(capture_);
    writeRawBinding(pingA_);
    writeRawBinding(pingB_);
    tw_ = w; th_ = h;
    return true;
}

// ================================================================
// draw — ① 捕获 canvas→capture（cap 已由调用方 ∩ 画布/scissor，blit 到
//            纹理内同位置子区域）；② blurH；③ blurV。
//        目标固定 canvas/4：帧内多玻璃元素共用同一套 target/descSet，
//        无中途销毁重建（避免使已录制命令引用的对象失效）。
//        调用方已 endRenderPass，canvas 处于 TRANSFER_SRC
// ================================================================
bool BackdropRenderer::draw(VkCommandBuffer cb, VkExtent2D ctx, VkImage canvas,
                            uint32_t canvasW, uint32_t canvasH,
                            VkPhysicalDevice phys, const Rect &cap, float sigmaScreen) {
    int tw = std::max(1, static_cast<int>(canvasW) / static_cast<int>(downsample_));
    int th = std::max(1, static_cast<int>(canvasH) / static_cast<int>(downsample_));
    if (!ensureTargets(phys, static_cast<uint32_t>(tw), static_cast<uint32_t>(th)))
        return false;

    // ① canvas → capture：blit 降采样到同位置子区域（防御性 clamp 到画布；
    //    dst = src/4 取整，≤0.25 texel 误差，模糊后不可见）
    VkImageSubresourceLayers layers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    VkImageBlit blit{};
    blit.srcSubresource = layers;
    int sx0 = (int)std::max(0.0f, std::floor(cap.x));
    int sy0 = (int)std::max(0.0f, std::floor(cap.y));
    int sx1 = std::min((int)std::ceil(cap.x + cap.width),  (int)canvasW);
    int sy1 = std::min((int)std::ceil(cap.y + cap.height), (int)canvasH);
    if (sx1 <= sx0 || sy1 <= sy0) return false;
    blit.srcOffsets[0] = {sx0, sy0, 0};
    blit.srcOffsets[1] = {sx1, sy1, 1};
    blit.dstSubresource = layers;
    blit.dstOffsets[0] = {std::max(0, (int)std::lround((float)sx0 / downsample_)),
                          std::max(0, (int)std::lround((float)sy0 / downsample_)), 0};
    blit.dstOffsets[1] = {std::min((int)tw, (int)std::lround((float)sx1 / downsample_)),
                          std::min((int)th, (int)std::lround((float)sy1 / downsample_)), 1};
    // canvas: TRANSFER_SRC already; capture: UNDEFINED→TRANSFER_DST
    VkImageMemoryBarrier cb1{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    cb1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    cb1.image = capture_.image;
    cb1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    cb1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    cb1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    cb1.srcQueueFamilyIndex = cb1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &cb1);
    vkCmdBlitImage(cb, canvas, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   capture_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);
    // capture: TRANSFER_DST → SHADER_READ_ONLY（blur 采样）
    VkImageMemoryBarrier cb2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    cb2.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    cb2.image = capture_.image;
    cb2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    cb2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    cb2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    cb2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    cb2.srcQueueFamilyIndex = cb2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &cb2);

    auto beginOff = [&](VkFramebuffer fbo, uint32_t w2, uint32_t h2) {
        VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp.renderPass = offscreenPass_; rp.framebuffer = fbo;
        rp.renderArea = {{0, 0}, {w2, h2}};
        vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport vp{0, 0, (float)w2, (float)h2, 0.0f, 1.0f};
        vkCmdSetViewport(cb, 0, 1, &vp);
        VkRect2D sc{{0, 0}, {w2, h2}};
        vkCmdSetScissor(cb, 0, 1, &sc);
    };
    auto pushBlur = [&](VkPipeline pipe, VkDescriptorSet src,
                        VkFramebuffer dstFbo, bool horizontal, uint32_t w2, uint32_t h2) {
        beginOff(dstFbo, w2, h2);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, blurLayout_,
                                0, 1, &src, 0, nullptr);
        BlurPushConstants pc{};
        // radius 语义 = 屏幕空间高斯 σ（物理 px）；shader 在 /4 纹理上按 texel 取核，
        // 故换算 σ_texel = σ_screen / downsample，屏幕模糊量即 props.backdropBlur × 渲染缩放。
        pc.radius = std::max(sigmaScreen, 0.0f) / static_cast<float>(downsample_);
        pc.viewportW = (float)w2; pc.viewportH = (float)h2;
        pc.u0 = 0; pc.v0 = 0; pc.u1 = 1; pc.v1 = 1;
        pc.texelOffsetX = horizontal ? 1.0f / (float)w2 : 0.0f;
        pc.texelOffsetY = horizontal ? 0.0f : 1.0f / (float)h2;
        vkCmdPushConstants(cb, blurLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(BlurPushConstants), &pc);
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
        vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
    };

    // ② blurH：capture → pingA（横向）
    pushBlur(blurHPipeline_, capture_.descSet, pingAFbo_, true, (uint32_t)tw, (uint32_t)th);
    vkCmdEndRenderPass(cb);
    // pingA SHADER_READ 依赖 barrier（finalLayout 已自动过渡 layout）
    {
        VkImageMemoryBarrier dep{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        dep.image = pingA_.image;
        dep.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dep.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dep.srcQueueFamilyIndex = dep.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dep.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &dep);
    }

    // ③ blurV：pingA → pingB（纵向）
    pushBlur(blurVPipeline_, pingA_.descSet, pingBFbo_, false, (uint32_t)tw, (uint32_t)th);
    vkCmdEndRenderPass(cb);
    // pingB SHADER_READ 依赖 barrier
    {
        VkImageMemoryBarrier dep{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        dep.image = pingB_.image;
        dep.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dep.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dep.srcQueueFamilyIndex = dep.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        dep.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &dep);
    }

    return true;
}

// ================================================================
// composite — 主 pass 运行中调用：绘制元素 rect（未外扩），UV=世界坐标/视口
//             （纹理与画布同构），圆角 SDF + 折射
// ================================================================
void BackdropRenderer::composite(VkCommandBuffer cb, VkExtent2D ctx,
                                 const BackdropBlurCmd &cmd, bool clipped) {
    if (pingB_.view == VK_NULL_HANDLE) return;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      clipped ? compositeClipPipeline_ : compositePipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, quadLayout_,
                            0, 1, &pingB_.descSet, 0, nullptr);
    QuadPushConstants pc{};
    pc.posX = cmd.rect.x; pc.posY = cmd.rect.y;
    pc.sizeX = cmd.rect.width; pc.sizeY = cmd.rect.height;
    pc.cornerRadius = cmd.cornerRadius;
    pc.refraction = cmd.refraction;
    pc.specular = cmd.specular;
    pc.m00 = cmd.t.m00; pc.m01 = cmd.t.m01; pc.m02 = cmd.t.m02;
    pc.m10 = cmd.t.m10; pc.m11 = cmd.t.m11; pc.m12 = cmd.t.m12;
    pc.viewportW = (float)ctx.width; pc.viewportH = (float)ctx.height;
    vkCmdPushConstants(cb, quadLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(QuadPushConstants), &pc);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer_, &off);
    vkCmdBindIndexBuffer(cb, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}

// ================================================================
// destroyTargets — 只释放 target/fbo（保留 pipeline/pass/descPool）
// ================================================================
void BackdropRenderer::destroyTargets() {
    if (device_ == VK_NULL_HANDLE) return;
    auto freeTarget = [](VkDevice d, BackdropTarget &t) {
        if (t.sampler) vkDestroySampler(d, t.sampler, nullptr);
        if (t.view)    vkDestroyImageView(d, t.view, nullptr);
        if (t.image)   vkDestroyImage(d, t.image, nullptr);
        if (t.memory)  vkFreeMemory(d, t.memory, nullptr);
        t = {};
    };
    if (captureFbo_) vkDestroyFramebuffer(device_, captureFbo_, nullptr);
    if (pingAFbo_)   vkDestroyFramebuffer(device_, pingAFbo_, nullptr);
    if (pingBFbo_)   vkDestroyFramebuffer(device_, pingBFbo_, nullptr);
    captureFbo_ = pingAFbo_ = pingBFbo_ = VK_NULL_HANDLE;
    freeTarget(device_, capture_);
    freeTarget(device_, pingA_);
    freeTarget(device_, pingB_);
    tw_ = th_ = 0;
}

// ================================================================
// destroy — 完整释放（shutdown 时调用）
// ================================================================
void BackdropRenderer::destroy() {
    if (device_ == VK_NULL_HANDLE) return;
    destroyTargets();
    if (descPool_)       { vkDestroyDescriptorPool(device_, descPool_, nullptr);       descPool_ = VK_NULL_HANDLE; }
    if (descSetLayout_)  { vkDestroyDescriptorSetLayout(device_, descSetLayout_, nullptr); descSetLayout_ = VK_NULL_HANDLE; }
    if (blurHPipeline_)  { vkDestroyPipeline(device_, blurHPipeline_, nullptr);  blurHPipeline_ = VK_NULL_HANDLE; }
    if (blurVPipeline_)  { vkDestroyPipeline(device_, blurVPipeline_, nullptr);  blurVPipeline_ = VK_NULL_HANDLE; }
    if (compositePipeline_)     { vkDestroyPipeline(device_, compositePipeline_, nullptr);     compositePipeline_ = VK_NULL_HANDLE; }
    if (compositeClipPipeline_) { vkDestroyPipeline(device_, compositeClipPipeline_, nullptr); compositeClipPipeline_ = VK_NULL_HANDLE; }
    if (blurLayout_) { vkDestroyPipelineLayout(device_, blurLayout_, nullptr); blurLayout_ = VK_NULL_HANDLE; }
    if (quadLayout_) { vkDestroyPipelineLayout(device_, quadLayout_, nullptr); quadLayout_ = VK_NULL_HANDLE; }
    if (offscreenPass_) { vkDestroyRenderPass(device_, offscreenPass_, nullptr); offscreenPass_ = VK_NULL_HANDLE; }
}