/**
 * @brief 纯色三角形网格着色器
 *
 * 输入 float2 屏幕空间位置，由 push constants 视口尺寸转换到 NDC。
 * Push constants 36 bytes: color(16) + viewport(8) + opacity(4) + pad(8)
 */

// ── Push Constants ───────────────────────────────────────────
struct TrianglePushConstants {
    float4 uColor;
    float2 uViewport;
    float  uOpacity;
    float  _pad0;
    float  _pad1;
};

[[vk::push_constant]] ConstantBuffer<TrianglePushConstants> pc;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float4 vColor;
};

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 aPos : ATTR0) {
    VertOutput o;
    float px = (aPos.x / pc.uViewport.x) * 2.0 - 1.0;
    float py = (aPos.y / pc.uViewport.y) * 2.0 - 1.0;
    o.position = float4(px, py, 0.0, 1.0);
    o.vColor = float4(pc.uColor.rgb, pc.uColor.a * pc.uOpacity);
    return o;
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    return input.vColor;
}