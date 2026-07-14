#version 450
/**
 * @brief 纯色三角形网格顶点着色器
 *
 * 输入: float2 位置 (NDC 由 push constants 中的视口尺寸换算)
 * Push constants: { color (vec4), opacity (float), viewport (vec2) }
 */

layout(location = 0) in vec2 aPos;

layout(push_constant) uniform PushConstants {
    vec4  uColor;
    vec2  uViewport;
    float uOpacity;
    float uPad0;
    float uPad1;
} pc;

layout(location = 0) out vec4 vColor;

void main() {
    // 屏幕空间 → NDC
    float px = (aPos.x / pc.uViewport.x) * 2.0 - 1.0;
    float py = (aPos.y / pc.uViewport.y) * 2.0 - 1.0;
    gl_Position = vec4(px, py, 0.0, 1.0);
    vColor = vec4(pc.uColor.rgb, pc.uColor.a * pc.uOpacity);
}