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

    // 根据文字亮度动态 gamma 校正覆盖值:
    //   亮色文字 (白字/亮彩色字): pow(cov, 1/2.2) 提升近满覆盖值, 使文字更纯净
    //   暗色文字 (黑字白底): 不调整, 保持 sRGB 混合的锐利度
    //   用 max(r,g,b) 而非 luma 判断亮度: 纯红/绿/蓝等彩色字也应获得完整 gamma 校正
    //   mix 按亮度平滑插值, 无跳变 — 与 DirectWrite 策略一致
    float text_lightness = max(fragColor.r, max(fragColor.g, fragColor.b));
    vec3 adj_subpixel = mix(subpixel, pow(subpixel, vec3(1.0 / 2.2)), text_lightness);
    float avg_coverage = dot(adj_subpixel, vec3(0.3333));

    // dual-source blending (UNORM 渲染目标, sRGB 空间混合):
    //   混合公式: final = src0 * src1 + dst * (1 - src1)
    //   即: final_R = text_R * r_cov + bg_R * (1 - r_cov)  — 每通道独立
    outColor0 = vec4(fragColor.rgb, fragColor.a);
    outColor1 = vec4(adj_subpixel, avg_coverage);
}