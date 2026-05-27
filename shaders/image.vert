#version 450
// 与 glyph.vert 完全相同的 PushConstants 布局，可复用 GlyphPushConstants
layout(location = 0) in vec2 inPosition;
layout(push_constant) uniform PushConstants {
    vec2  pos;
    vec2  size;
    vec4  uvRect;       // 整图: 0,0,1,1
    vec4  color;        // rgba, alpha 通道承载 opacity
    vec2  viewportSize;
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