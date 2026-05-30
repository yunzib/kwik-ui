#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0) out vec4 outColor;
void main() {
    float distance = texture(glyphAtlas, fragUV).r;
    // ── SDF 自适应边缘宽度 ────────────────────────────────
    // fwidth 基于屏幕像素梯度, 对缩放不敏感 — 在高低 DPI 下宽度恒定
    float pixelEdge = length(fwidth(fragUV)) * 2048.0; // 转换到图集坐标
    // 在 2048 图集上: 1 像素边缘 ≈ 0.000488 纹理单位
    // fwidth(fragUV) ≈ 1/屏幕像素, 故 pixelEdge ≈ 2048/屏幕像素 ≈ 1 图集像素
    float edge = clamp(pixelEdge * 1.2, 0.015, 0.15); // 1.2px 基础 + 上下限
    // ── 锐化边缘: 用更窄的 smoothstep 跨距 ────────────────
    // 标准 SDF: smoothstep(0.5-edge, 0.5+edge, d) → 跨距 2*edge
    // 优化: 使用 0.52 中点补偿 FreeType 的 R8_UNORM 量化误差
    float alpha = smoothstep(0.52 - edge, 0.52 + edge, distance);
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}