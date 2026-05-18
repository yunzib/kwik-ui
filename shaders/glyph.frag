#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0) out vec4 outColor;
void main() {
    float distance = texture(glyphAtlas, fragUV).r;
    float width = fwidth(distance) * 0.6;   // ← 40% sharper edge
    float alpha = smoothstep(0.5 - width, 0.5 + width, distance);
    outColor = fragColor * alpha;
}