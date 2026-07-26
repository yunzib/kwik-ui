#version 450
layout(location = 0) in vec2 fragUV;
layout(binding = 0) uniform sampler2DArray glyphAtlas;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec2  pos;
    vec2  size;
    vec4  uvRect;
    vec4  color;         // ← 直接从 push constant 取颜色
    vec2  viewportSize;
    float textContrast;
    float pageIndex;
} pc;

void main() {
    float alpha = texture(glyphAtlas, vec3(fragUV, pc.pageIndex)).r;
    float correctedAlpha = pow(alpha, 1.0 / pc.textContrast);
    outColor = vec4(pc.color.rgb, pc.color.a * correctedAlpha);
}