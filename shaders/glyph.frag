#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2DArray glyphAtlas;
layout(location = 0, index = 0) out vec4 outColor0;
layout(location = 0, index = 1) out vec4 outColor1;
layout(push_constant) uniform PushConstants {
    vec2  pos;
    vec2  size;
    vec4  uvRect;
    vec4  color;
    vec2  viewportSize;
    float textContrast;
    float pageIndex;
} pc;

void main() {
    float alpha = texture(glyphAtlas, vec3(fragUV, pc.pageIndex)).r;
    outColor0 = vec4(fragColor.rgb, fragColor.a);
    outColor1 = vec4(alpha, alpha, alpha, alpha);
}