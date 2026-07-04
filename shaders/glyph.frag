#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0) out vec4 outColor;
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
    vec3 linear_color = pow(fragColor.rgb, vec3(2.2));
     // 每个颜色通道乘以对应的子像素覆盖
    float avg_coverage = dot(subpixel, vec3(0.3333));
    outColor = vec4(linear_color * subpixel, fragColor.a * avg_coverage);
}