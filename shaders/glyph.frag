#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0, index = 0) out vec4 outColor0;   // src0: 文字颜色 (sRGB 空间, 非预乘)
layout(location = 0, index = 1) out vec4 outColor1;   // src1: 子像素覆盖值
layout(push_constant) uniform PushConstants {
    vec2  pos;
    vec2  size;
    vec4  uvRect;
    vec4  color;
    vec2  viewportSize;
    float textContrast;
} pc;

void main() {
    // LCD 子像素渲染: RGB 三通道分别存储子像素覆盖值
    vec3 subpixel = texture(glyphAtlas, fragUV).rgb;
    float avg_coverage = dot(subpixel, vec3(0.3333));

    // dual-source blending (UNORM 渲染目标, sRGB 空间混合):
    //   混合公式: final = src0 * src1 + dst * (1 - src1)
    //   即: final_R = text_R * r_cov + bg_R * (1 - r_cov)  — 每通道独立, sRGB 空间
    //   与浏览器一致: sRGB 空间混合使文字边缘更锐利
    outColor0 = vec4(fragColor.rgb, fragColor.a);
    outColor1 = vec4(subpixel, avg_coverage);
}