JS Text({...}) → Vulkan 屏幕渲染 完整流程
┌─────────────────────────────────────────────────────────────────────────────┐
│ ① JS 层：组件声明                                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│ text.js:                                                                    │
│   import { Text } from 'kwikui';                                           │
│   Text({ text: "Hello", fontSize: 24, color: "#e94560" })                  │
│          │                                                                  │
│          ▼                                                                  │
│ ② C 绑定层：构建组件描述符                                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│ src/engine/bindings.cpp → js_text()                                        │
│                                                                             │
│   JSValue js_text(ctx, this_val, argc, argv) {                             │
│       JSValue props = argv[0];  // { text:"Hello", fontSize:24, ... }      │
│       return makeElement(ctx, "Text", props, JS_UNDEFINED);                │
│       // → { type: "Text", props: {...}, children: undefined }             │
│   }                                                                         │
│                                                                             │
│   注册: JS_CFUNC_DEF("Text", 1, js_text) → 导出到 "kwikui" 模块             │
│          │                                                                  │
│          ▼                                                                  │
│ ③ 解析层：JS 对象 → C++ ViewProps                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│ src/bridge/element_parser.cpp → parseNode()                                │
│                                                                             │
│   1. typeVal = jsVal.getProperty("type")  → "Text"                         │
│   2. props   = parseViewProps(jsVal.getProperty("props"))                  │
│                           │                                                 │
│      src/bridge/props_parser.cpp → parseViewProps():                       │
│        "text"     → result.text = "Hello"                                  │
│        "fontSize" → result.fontSize = 24.0f                                │
│        "color"    → result.textColor = Color(0xe9,0x45,0x60,0xff)          │
│                                                                             │
│   3. 查注册表: registry["Text"] → creator(props)                           │
│      src/bridge/element_parser.cpp:63                                      │
│        return std::make_unique<Text>(std::move(props));                    │
│          │                                                                  │
│          ▼                                                                  │
│ ④ 布局层：测量 & 定位                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│ text_example.cpp → 主线程:                                                 │
│   tree->measure(Constraints::loose(800, 600))                              │
│          │                                                                  │
│   src/element/view.cpp → View::measure() → Text::onMeasure()              │
│   src/element/text.cpp:                                                    │
│                                                                             │
│     ① FontManager::instance().loadFont(默认字体路径)                        │
│        src/render/font.cpp:                                                │
│          FT_New_Face()       → FreeType 加载字体                            │
│          hb_ft_font_create_referenced() → HarfBuzz 创建字体 handle           │
│                                                                             │
│     ② fm.shapeText("Hello", 24)                                           │
│        src/render/font.cpp:                                                │
│          FT_Set_Pixel_Sizes(24)                                            │
│          hb_font_set_scale(24*64)                                          │
│          hb_buffer_add_utf8("Hello")                                       │
│          hb_shape()              → HarfBuzz 文本整形                        │
│          遍历 hb_glyph_info_t / hb_glyph_position_t                         │
│          → std::vector<ShapedGlyph> (每个字形: x, y, advanceX, w, h)        │
│                                                                             │
│     ③ 遍历 glyphs: 累加 advanceX → totalWidth                              │
│        getMetrics(24).lineHeight → maxHeight                               │
│                                                                             │
│     ④ constraints.constrain({totalWidth, lineHeight}) → {w, h}            │
│                                                                             │
│   tree->layout(Rect(0,0,800,600))                                         │
│     src/element/view.cpp → View::onLayout()                               │
│       递归遍历子节点,计算每个 child 的 frame (x,y,w,h)                       │
│       child->layout(Rect{px, py, childW, childH})                         │
│          │                                                                  │
│          ▼                                                                  │
│ ⑤ 绘制层：生成渲染命令                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│ 主线程渲染循环:                                                              │
│   Graphics canvas(&cmdBuffer);                                              │
│   canvas.beginFrame();                                                      │
│   tree->draw(canvas);                                                      │
│          │                                                                  │
│   src/element/view.cpp → View::draw() → View::onDraw()                    │
│     绘制背景/边框/阴影/圆角 → FillRoundedRectCmd 等                         │
│     遍历 children → child->draw(canvas)                                    │
│                                                                             │
│   src/element/text.cpp → Text::onDraw():                                  │
│     ① graphics.save()                                                      │
│     ② graphics.translate(frame.x, frame.y)  → 记录变换状态                 │
│     ③ fm.loadFont() (早期返回，字体已加载)                                   │
│     ④ graphics.drawText(font, "Hello", 24, 0, 0, textColor)              │
│        src/render/graphics.cpp:                                            │
│          fm.shapeText("Hello", 24)                                         │
│          for each glyph:                                                   │
│            info = fm.getGlyphInfo(glyphIndex, 24)                          │
│                → renderGlyph()                                             │
│                   FT_Load_Glyph() + FT_Render_Glyph(SDF)                   │
│                   复制 SDF 位图到 atlasData_ (1024x1024)                    │
│                   atlasDirty_ = true                                       │
│            u,v = atlas坐标 / 1024                                          │
│            tx,ty = (x+gx)*sx+tx, (y+gy)*sy+ty  ← 应用 transform 状态      │
│            addCommand(DrawGlyphCmd{glyphIndex, tx, ty, w,h, u0,v0,u1,v1,c})│
│              → 写入命令缓冲区                                               │
│     ⑤ graphics.restore()                                                   │
│                                                                             │
│   canvas.endFrame() → EndFrameCmd                                           │
│   renderThread.commandQueue().submit() → 交换缓冲区                          │
│          │                                                                  │
│          ▼                                                                  │
│ ⑥ 渲染线程：命令执行                                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│ src/render/render_thread.cpp → threadMain():                               │
│   commandQueue_.acquire() → 获取待处理命令                                   │
│   processCommands(buffer):                                                  │
│     backend_->beginFrame()  → Vulkan: vkAcquireNextImage + beginRenderPass │
│     executeCommand(cmd) → std::visit:                                       │
│       ClearCmd         → backend_->clear()                                  │
│       FillRoundedRectCmd → backend_->fillRoundedRect()                     │
│       DrawGlyphCmd     → backend_->drawGlyph(arg)                          │
│             │                                                               │
│             ▼                                                               │
│   src/render/vulkan_backend.cpp → VulkanBackend::drawGlyph():              │
│     ① if (atlasDirty) {                                                     │
│          uploadGlyphAtlas(atlasData, 1024, 1024)                            │
│            创建 staging buffer                                              │
│            memcpy(atlasData → staging)                                      │
│            图像布局转换: UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY        │
│            vkCmdCopyBufferToImage(staging → glyphAtlasImage)                │
│            vkQueueSubmit + vkQueueWaitIdle                                  │
│          clearAtlasDirty()                                                  │
│        }                                                                    │
│                                                                             │
│     ② vkCmdBindPipeline(glyphPipeline_)  → glyph.vert + glyph.frag         │
│     ③ vkCmdBindDescriptorSets(glyphDescSet_) → 绑定 glyphAtlas 纹理 sampler │
│     ④ GlyphPushConstants pc { pos, size, uvRect, color, viewport }         │
│        vkCmdPushConstants(pc)                                               │
│     ⑤ vkCmdBindVertexBuffers(quad) + vkCmdBindIndexBuffer(quad indices)    │
│     ⑥ vkCmdDrawIndexed(6)  → 2 三角形 = 1 四边形                           │
│                                                                             │
│     backend_->endFrame() → vkCmdEndRenderPass                               │
│     backend_->present()  → vkQueueSubmit + vkQueuePresentKHR               │
│                                   │                                         │
│                                   ▼                                         │
│ ⑦ GPU 管线                                                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│ glyph.vert (顶点着色器):                                                     │
│   screenPos = pc.pos + inPosition * pc.size  // 四边形定位                  │
│   ndc = screenPos / viewport * 2 - 1         // NDC 变换                    │
│   fragUV = mix(uvRect.xy, uvRect.zw, inPos)  // 图集 UV                     │
│   fragColor = pc.color                       // 透传到片段着色器             │
│                                                                             │
│ glyph.frag (片段着色器):                                                     │
│   distance = texture(glyphAtlas, fragUV).r    // 采样 SDF 值 (0~1)          │
│   width = fwidth(distance)                    // 屏幕导数 = 抗锯齿宽度       │
│   alpha = smoothstep(0.5-w, 0.5+w, distance)  // SDF → 平滑 alpha           │
│   outColor = fragColor * alpha                // RGBA 输出                  │
│                                                                             │
│   → Vulkan 将结果写入 swapchain image → vkQueuePresentKHR → 屏幕显示 ✅     │
└─────────────────────────────────────────────────────────────────────────────┘