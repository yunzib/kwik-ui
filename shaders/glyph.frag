#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D glyphAtlas;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    layout(offset = 60) float pxRange;
    layout(offset = 64) float atlasSizeW;
    layout(offset = 68) float atlasSizeH;
} pc;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange(vec2 uv) {
    vec2 atlasSize = vec2(pc.atlasSizeW, pc.atlasSizeH);
    vec2 unitRange = pc.pxRange / atlasSize;
    vec2 screenTexSize = vec2(1.0) / fwidth(uv);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 msd = texture(glyphAtlas, fragUV).rgb;
    float sd = 1.0 - median(msd.r, msd.g, msd.b);
    float spd = screenPxRange(fragUV) * (sd - 0.5);
    float alpha = clamp(spd + 0.5, 0.0, 1.0);
    float ca = 1.0 - pow(1.0 - fragColor.a * alpha, 2.2);
    outColor = vec4(fragColor.rgb, ca);
}