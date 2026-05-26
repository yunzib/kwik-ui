#version 450
layout(location = 0) in vec2 inPosition;
layout(push_constant) uniform PushConstants {
    vec2  topLeft;
    vec2  size;
    vec4  fillColor;
    float radius;
    float borderWidth;
    float _pad0;      // std430 alignment padding
    float _pad1;
    vec4  borderColor;
    float opacity;
    uint  drawMode;   // 0=fill, 1=stroke, 2=shadow
    vec2  shadowOffset;
    float shadowBlur;
    float _pad2;      // std430 alignment padding
    vec2  viewportSize;
} pc;
layout(location = 0) out vec2  fragPos;
layout(location = 1) out flat vec4 fragFillColor;
layout(location = 2) out flat vec2 fragSize;
layout(location = 3) out flat float fragRadius;
layout(location = 4) out flat float fragBorderWidth;
layout(location = 5) out flat vec4 fragBorderColor;
layout(location = 6) out flat float fragOpacity;
layout(location = 7) out flat uint fragDrawMode;
layout(location = 8) out flat vec2 fragShadowOffset;
layout(location = 9) out flat float fragShadowBlur;
layout(location = 10) out flat vec2 fragViewportSize;
void main() {
    vec2 screenPos = pc.topLeft + inPosition * pc.size;
    vec2 ndc = (screenPos + 0.5) / pc.viewportSize * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    fragPos          = inPosition;
    fragFillColor    = pc.fillColor;
    fragSize         = pc.size;
    fragRadius       = pc.radius;
    fragBorderWidth  = pc.borderWidth;
    fragBorderColor  = pc.borderColor;
    fragOpacity      = pc.opacity;
    fragDrawMode     = pc.drawMode;
    fragShadowOffset = pc.shadowOffset;
    fragShadowBlur   = pc.shadowBlur;
    fragViewportSize = pc.viewportSize;
}