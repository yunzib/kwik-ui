#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0) out vec4 outColor;

// ── A8 位图路径: 直接采样单通道 alpha，无需 MSDF 解码 ──
void main() {
    float alpha = texture(glyphAtlas, fragUV).r;      // R8_UNORM → .r = alpha
    float ca = 1.0 - pow(1.0 - fragColor.a * alpha, 2.2);
    outColor = vec4(fragColor.rgb, ca);
}