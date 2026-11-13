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
module kwik.render.vulkan_backend;
import kwik.render.vulkan.context;
import kwik.render.vulkan.rect_renderer;
import kwik.render.vulkan.glyph_renderer;
import kwik.render.vulkan.image_renderer;
import kwik.render.vulkan.clip_manager;
import kwik.render.command;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.log;
import kwik.render.video_stream;    // VideoStream（createStreamTexture 转发）

import std;

VulkanBackend::~VulkanBackend() {
    if (ctx_.device() != VK_NULL_HANDLE) { vkDeviceWaitIdle(ctx_.device()); }
}

// ================================================================
// initialize — 初始化 Context + 子渲染器
// ================================================================
bool VulkanBackend::initialize(void *native) {
    auto t0 = std::chrono::steady_clock::now();
    if (!ctx_.initialize(native)) return false;
    Log::info("[startup] vk_context_init = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());

    deviceCtx_ = DeviceContext{
        .device = ctx_.device(),
        .physicalDevice = ctx_.physicalDevice(),
        .commandPool = ctx_.commandPool(),
        .queue = ctx_.graphicsQueue(),
        .getHostPointerProps = ctx_.hostPointerPropsFn(),       // 零拷贝能力（未启用为空 → memcpy 路径）
        .hostImportAlignment = ctx_.hostImportAlignment(),
    };
    auto t1 = std::chrono::steady_clock::now();

    if (!rect_.create(ctx_.device(), ctx_.pipelineCache(), ctx_.renderPass(), ctx_.vertexBuffer(), ctx_.indexBuffer())) {
        ctx_.shutdown();
        return false;
    }
    if (!glyph_.create(ctx_.device(), ctx_.pipelineCache(), ctx_.physicalDevice(), ctx_.commandPool(), ctx_.graphicsQueue(),
                       ctx_.renderPass(), ctx_.vertexBuffer(), ctx_.indexBuffer())) {
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!image_.create(ctx_.device(), ctx_.pipelineCache(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                       ctx_.indexBuffer())) {
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!triangle_.create(ctx_.device(), ctx_.pipelineCache(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                          ctx_.indexBuffer())) {
        Log::error("TriangleRenderer init failed");
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    if (!mesh_.create(ctx_.device(), ctx_.pipelineCache(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(),
                      ctx_.indexBuffer(), image_.descSetLayout())) {
        Log::error("MeshRenderer init failed");
        glyph_.destroy();
        rect_.destroy();
        ctx_.shutdown();
        return false;
    }
    backdrop_.create(ctx_.device(), ctx_.pipelineCache(), ctx_.physicalDevice(), ctx_.renderPass(), ctx_.vertexBuffer(), ctx_.indexBuffer());
    Log::info("[startup] pipeline_create = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t1).count());

    // 帧首预处理钩子：资源通道 drain + 流纹理上传（须在主 pass 之前录制）
    ctx_.setPrePassHook([this](VkCommandBuffer cb) { prePassRecord(cb); });
    return true;
}

void VulkanBackend::shutdown() {
    ctx_.setPrePassHook(nullptr);    // 先摘钩子：shutdown 后回调不允许再触碰设备
    if (ctx_.device() != VK_NULL_HANDLE) vkDeviceWaitIdle(ctx_.device());
    image_.destroy();
    glyph_.destroy();
    rect_.destroy();
    backdrop_.destroy();
    ctx_.shutdown();
}

bool VulkanBackend::resize(int w, int h) {
    return ctx_.resize(w, h);
}

// ================================================================
// prePassRecord — 帧首预处理（VulkanContext 钩子回调：fence 已等待、
// 命令缓冲已开始、主 render pass 未开）
// ================================================================
void VulkanBackend::prePassRecord(VkCommandBuffer cb) {
    // 纹理资源通道消费：此刻 fence 等待刚完成——retire 回收的安全性前提成立
    // （在飞帧命令缓冲已结束），且本帧回放前新纹理已就绪（同帧入队同帧可用）
    image_.drainResources(deviceCtx_, ++resourceFrame_);
    // 流纹理上传：本帧被触碰且有新帧的流 → staging[帧槽] → 图像环下一槽
    // （transfer 命令只能录在 pass 外，故与 drain 同处帧首窗口）
    image_.recordStreamUploads(deviceCtx_, cb, ctx_.frameIndex(), resourceFrame_);
}

// ================================================================
// beginFrame — 获取 FrameToken + 设置裁剪初始状态
// ================================================================
bool VulkanBackend::beginFrame(const Rect &dirtyRect) {
    auto token = ctx_.beginFrame();    // 内含帧首预处理钩子（prePassRecord）
    if (!token) return false;
    currentToken_ = std::move(token);

    std::int32_t sx = std::max(0, static_cast<std::int32_t>(std::floor(dirtyRect.x)));
    std::int32_t sy = std::max(0, static_cast<std::int32_t>(std::floor(dirtyRect.y)));
    std::uint32_t sw =
        std::max(1u, static_cast<std::uint32_t>(std::ceil(dirtyRect.x + dirtyRect.width) - std::floor(dirtyRect.x)));
    std::uint32_t sh =
        std::max(1u, static_cast<std::uint32_t>(std::ceil(dirtyRect.y + dirtyRect.height) - std::floor(dirtyRect.y)));

    if (ctx_.consumeJustRecreated()) {
        Log::info("beginFrame: swapchain recreated -> force full redraw {}x{}", currentToken_->extent.width,
                  currentToken_->extent.height);
        sx = 0;
        sy = 0;
        sw = currentToken_->extent.width;
        sh = currentToken_->extent.height;
    }

    VkRect2D sc{{sx, sy}, {sw, sh}};
    vkCmdSetScissor(currentToken_->commandBuffer, 0, 1, &sc);
    clip_.beginFrame(currentToken_->extent, sc);
    // 以 ctx 帧槽分槽重置顶点写入（fence 等待周期与槽轮转同源，槽复用
    // 距 3 帧 = fence 距，CPU 重写不再与在飞帧 GPU 读竞争）
    const std::uint32_t frameSlot = ctx_.frameIndex();
    triangle_.resetOffset(frameSlot);
    mesh_.resetOffset(frameSlot);

    ctx_.accumulateDirtyRect(Rect{(float)sx, (float)sy, (float)sw, (float)sh});

    return true;
}

void VulkanBackend::endFrame() {
    // 帧末冲刷三角形合批区间（render pass 内最后提交，命令序不变）
    if (currentToken_) { triangle_.flushPending(currentToken_->commandBuffer); }
    drawCalls_ = 0;
    glyph_.uploadPendingGlyphs(deviceCtx_);
    ctx_.endFrame();
}

bool VulkanBackend::present() {
    bool ok = ctx_.present();
    currentToken_.reset();
    return ok;
}

// ================================================================
// 委托 — 全部通过 currentToken_ 获取 Vulkan 句柄，并透传矩阵
// ================================================================
void VulkanBackend::clear(const Color &c) {
    triangle_.flushPending(currentToken_->commandBuffer);
    rect_.clear(currentToken_->commandBuffer, currentToken_->extent, c);
}

void VulkanBackend::fillRect(const Rect &r, const Color &c, BlendMode mode, const Transform2D &t) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    if (mode == BlendMode::SrcCopy) {
        // 清除路径：r 已是物理坐标（clearRectArea 用 transformRect 烘焙），t 为单位矩阵，忽略。
        // 须与当前可画域求交再清：越出伤害带/裁剪栈的清除打进永不重画的区域
        // （G2D 挖空块常驻顶栏的机制环节）；负坐标 VkClearRect 按规范亦非法
        Rect area = clip_.intersectCurrent(r);
        if (area.width <= 0 || area.height <= 0) return;    // 空交集：无可清区域
        VkClearAttachment att{};
        att.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        att.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        att.colorAttachment = 0;
        VkClearRect cr{};
        cr.rect = {{static_cast<std::int32_t>(area.x), static_cast<std::int32_t>(area.y)},
                   {static_cast<std::uint32_t>(std::ceil(area.width)), static_cast<std::uint32_t>(std::ceil(area.height))}};
        cr.baseArrayLayer = 0;
        cr.layerCount = 1;
        vkCmdClearAttachments(currentToken_->commandBuffer, 1, &att, 1, &cr);
        return;
    }
    rect_.fillRect(currentToken_->commandBuffer, currentToken_->extent, r, c, t);
}

void VulkanBackend::fillRoundedRect(const Rect &r, float rad, const Color &c, const Gradient &gradient,
                                    const Transform2D &t) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    rect_.fillRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, gradient, clip_.globalAlpha(),
                          t);
}

void VulkanBackend::drawSegment(const DrawSegmentCmd &cmd) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    rect_.drawSegment(currentToken_->commandBuffer, currentToken_->extent, cmd.ax, cmd.ay, cmd.bx, cmd.by, cmd.halfW,
                      cmd.color, clip_.globalAlpha(), cmd.t);
}

void VulkanBackend::strokeRoundedRect(const Rect &r, float rad, const Color &c, float sw, const Transform2D &t) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    rect_.strokeRoundedRect(currentToken_->commandBuffer, currentToken_->extent, r, rad, c, sw, clip_.globalAlpha(), t);
}

void VulkanBackend::drawShadow(const Rect &r, float rad, const Shadow &s, const Transform2D &t) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    rect_.drawShadow(currentToken_->commandBuffer, currentToken_->extent, r, rad, s, clip_.globalAlpha(), t);
}

void VulkanBackend::drawGlyph(const DrawGlyphCmd &cmd) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    if (clip_.level() > 0) {
        // stencil 裁剪路径：同样透传矩阵（字形随 View 变换）
        glyph_.drawGlyphClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    } else {
        glyph_.drawGlyph(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    }
}

void VulkanBackend::drawImage(const DrawImageCmd &cmd) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    if (clip_.level() > 0) {
        image_.drawImageClipped(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    } else {
        image_.drawImage(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha(), cmd.t);
    }
}

std::uint32_t VulkanBackend::createImageTexture(const std::uint8_t *rgba, std::uint32_t w, std::uint32_t h) {
    return image_.enqueueTexture(rgba, w, h);    // 通道入队，渲染线程帧首上传
}

// 流纹理：通道入队（id 同步返回），渲染线程帧首注册并按流共享 GPU 资源
std::uint32_t VulkanBackend::createStreamTexture(std::shared_ptr<VideoStream> stream) {
    return image_.enqueueStream(std::move(stream));
}

// 本帧将绘制该流纹理：入队触碰标记，帧首据此决定是否上传新帧
void VulkanBackend::touchStreamTexture(std::uint32_t id) {
    image_.enqueueTouch(id);
}

// 17 号：异步上传回执查询（UI 线程轮询；查询即摘除该 id 回执，0=在途 1=落地 2=失败）
RenderBackend::TextureUploadStatus VulkanBackend::imageTextureResult(std::uint32_t id) {
    switch (image_.queryUploadResult(id)) {
    case 1: return RenderBackend::TextureUploadStatus::Landed;
    case 2: return RenderBackend::TextureUploadStatus::Failed;
    default: return RenderBackend::TextureUploadStatus::Pending;
    }
}

void VulkanBackend::destroyImageTexture(std::uint32_t id) {
    image_.enqueueDestroy(id);    // 通道入队，渲染线程延迟释放（不阻塞 UI 线程）
}

void VulkanBackend::pushClipRoundedRect(const Rect &r, float rad, const Transform2D &t, const Rect &clipRect) {
    triangle_.flushPending(currentToken_->commandBuffer);    // stencil 写入也是绘制（画序）
    if (clip_.level() == 0) {
        vkCmdSetStencilReference(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
        vkCmdSetStencilWriteMask(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        rect_.writeStencilMask(currentToken_->commandBuffer, currentToken_->extent, r, rad,
                               t);    // stencil 仍用逻辑+矩阵
        vkCmdSetStencilCompareMask(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
        vkCmdSetStencilReference(currentToken_->commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 1);
    }
    clip_.pushClipRoundedRect(currentToken_->commandBuffer, clipRect, rad);    // ← scissor 用物理 AABB
    pushKinds_.push_back(PushKind::Clip);
}

void VulkanBackend::popState() {
    // 裁剪域变化也是动态态边界：clip 域内录制的 pending 三角形必须先按旧
    // scissor 提交，否则冲刷时拿到恢复后的 scissor 逃逸裁剪
    triangle_.flushPending(currentToken_->commandBuffer);
    if (pushKinds_.empty()) return;
    PushKind kind = pushKinds_.back();
    pushKinds_.pop_back();
    if (kind == PushKind::Clip) {
        if (currentToken_) {
            clip_.resetClip(currentToken_->commandBuffer);
            if (clip_.level() == 0) { rect_.disableStencilTest(currentToken_->commandBuffer); }
        }
    }
}

void VulkanBackend::fillTriangles(const FillTrianglesCmd &cmd, const AAVertex *vertices, const SweepGrad *sweep) {
    drawCalls_++;
    triangle_.drawTriangles(currentToken_->commandBuffer, currentToken_->extent, vertices, cmd.vertexCount, cmd.color,
                            clip_.globalAlpha(), cmd.t, sweep);
}

void VulkanBackend::fillRing(const FillRingCmd &cmd) {
    drawCalls_++;
    triangle_.drawRing(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.globalAlpha());
}

void VulkanBackend::drawMesh(const DrawMeshCmd &cmd, const Vertex3D *vertices) {
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    // mesh 视口定位几何；scissor 收口 = viewport ∩ 当前可画域（裁剪栈/伤害带）
    // ——元素滚出滚动视口即空剪不画（原恒全屏，docsite 滚动后 3D 盖到顶栏）
    VkRect2D sc = clip_.toScissor(clip_.intersectCurrent(cmd.viewport));
    // 材质贴图：textureId → 纹理注册表 descSet（未落地/流纹理查不到 → 纯色回退）
    VkDescriptorSet texSet =
        (cmd.textureId != 0) ? image_.textureDescSet(cmd.textureId) : VK_NULL_HANDLE;
    mesh_.drawMesh(currentToken_->commandBuffer, cmd.viewport, sc, vertices, cmd.vertexCount,
                   cmd.mvp, cmd.color, cmd.lightDir, texSet);
    // mesh 改了 viewport/scissor 动态态——按回放流当前裁剪态恢复（原硬编码
    // 回全屏，紧随其后的 SrcCopy 清屏/2D 绘制会逃逸裁剪栈）
    clip_.reapplyState(currentToken_->commandBuffer);
}


// ================================================================
// backdropBlur — 液态玻璃 effect 的 Vulkan 实现（语义契约见 RenderBackend）。
// 手段：中断主 pass（canvas → TRANSFER_SRC）→ 捕获/离屏模糊 → 恢复主 pass
// （LOAD_OP_LOAD 保留画布+模板）→ 合成。软件/GL 后端可用各自手段等价实现，
// 上层只见语义命令（§四 收口）。
// ================================================================
void VulkanBackend::backdropBlur(const BackdropBlurCmd &cmd) {
    if (!currentToken_) return;
    triangle_.flushPending(currentToken_->commandBuffer);
    drawCalls_++;
    // 捕获盒 ∩ scissor ∩ 画布，取整对齐：blit 子区域与画布同构映射，
    // 离屏/出界元素不错位（composite UV = 世界坐标/视口，与 cap 解耦）。
    Rect cap = cmd.captureBox;
    if (auto sc = clip_.currentScissor()) cap = cap.intersection(*sc);
    cap = cap.intersection(
        Rect{0, 0, static_cast<float>(currentToken_->extent.width), static_cast<float>(currentToken_->extent.height)});
    cap = Rect{std::round(cap.x), std::round(cap.y), std::round(cap.width), std::round(cap.height)};
    if (cap.isEmpty()) return;
    // ① 中断主 pass（canvas → TRANSFER_SRC）
    ctx_.endRenderPass();
    // ② 捕获 + 离屏模糊（capture→pingA→pingB）；σ 换算到屏幕 px 并封顶
    float sigma = std::clamp(cmd.radius * cmd.scale, 0.0f, 64.0f);
    bool ok = backdrop_.draw(currentToken_->commandBuffer, currentToken_->extent, ctx_.canvasImage(),
                             currentToken_->extent.width, currentToken_->extent.height, ctx_.physicalDevice(),
                             cap, sigma);
    // ③ 恢复主 pass（LOAD_OP_LOAD 保留画布 + 模板跨中断保留）+ 复位裁剪动态状态
    ctx_.beginMainRenderPass();
    clip_.reapplyState(currentToken_->commandBuffer);
    // ④ 合成（元素框 + 圆角 + 可选折射/高光；stencil 裁剪用 clip 变体）
    //    合成整块写元素矩形——伤害带若只盖住一部分，带外子级被剔除不重画
    //    （合成覆盖子内容）且带内外捕获时点不同（接缝）。带完整性由
    //    RenderThread 的 expandDamageForBackdrop 保证：带相交玻璃必扩到整块。
    if (ok) backdrop_.composite(currentToken_->commandBuffer, currentToken_->extent, cmd, clip_.level() > 0);
    // ⑤ 合成管线为静态模板状态，绑定会使后续动态模板状态管线的 ref/mask 失效
    //    （如 clip 内继续绘制文本），重设一次恢复
    clip_.reapplyState(currentToken_->commandBuffer);
}