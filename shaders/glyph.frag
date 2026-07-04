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
    float coverage = texture(glyphAtlas, fragUV).r;
    if (pc.textContrast != 1.0)
        coverage = 1.0 - pow(1.0 - coverage, pc.textContrast);
    vec3 linear_color = pow(fragColor.rgb, vec3(2.2));
    outColor = vec4(linear_color * coverage, fragColor.a * coverage);
}