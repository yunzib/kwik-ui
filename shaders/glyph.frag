#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0) out vec4 outColor;
void main() {
    float distance = texture(glyphAtlas, fragUV).r;
    float width = clamp(fwidth(distance) * 0.6, 0.05, 0.35);
    float alpha = smoothstep(0.5 - width, 0.5 + width, distance);
    // 输出未预乘的 RGBA — 与混合状态的 VK_BLEND_FACTOR_SRC_ALPHA 匹配
    // 修复前 outColor = fragColor * alpha 产生 α² 衰减，亮底文字边缘发虚
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}