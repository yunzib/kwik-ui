#version 450
layout(location = 0) in vec2 inPosition;
layout(push_constant) uniform PushConstants {
    vec2  pos;
    vec2  size;
    vec4  uvRect;
    vec4  color;
    vec2  viewportSize;
    float cornerRadius;
    float pxRange;
    vec2  atlasSize;
} pc;
layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat vec4 fragColor;
void main() {
    vec2 screenPos = pc.pos + inPosition * pc.size;
    vec2 ndc = (screenPos + 0.5) / pc.viewportSize * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    fragUV = vec2(
        mix(pc.uvRect.x, pc.uvRect.z, inPosition.x),
        mix(pc.uvRect.y, pc.uvRect.w, inPosition.y)
    );
    fragColor = pc.color;
}
