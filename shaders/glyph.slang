/**
 * @brief 字形着色器 (A8 位图路径)
 *
 * Sampler2DArray 为内置 combined image sampler，
 * 绑定于 binding(0,0)，与 GLSL sampler2DArray 完全等价。
 */

// ── Push Constants ───────────────────────────────────────────
struct GlyphPushConstants {
    float2 pos;
    float2 size;
    float4 uvRect;
    float4 color;
    float2 viewportSize;
    float textContrast;
    float pageIndex;
    float m00, m01, m02;    // ← 矩阵
    float m10, m11, m12;
};

[[vk::push_constant]] ConstantBuffer<GlyphPushConstants> pc;

// ── 字形位图 Combined Sampler ───────────────────────────────
[[vk::binding(0, 0)]]
Sampler2DArray glyphAtlas;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 fragUV;
    [[vk::location(1)]] float4 fragColor;
};

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 inPosition : ATTR0) {
    VertOutput o;
    float2 local = pc.pos + inPosition * pc.size;
    float2 world = float2(pc.m00 * local.x + pc.m01 * local.y + pc.m02,
                          pc.m10 * local.x + pc.m11 * local.y + pc.m12);
    float2 ndc = world / pc.viewportSize * 2.0 - 1.0;   // glyph 无 +0.5，保持原对齐
    o.position = float4(ndc, 0.0, 1.0);
    o.fragUV = float2(lerp(pc.uvRect.x, pc.uvRect.z, inPosition.x),
                      lerp(pc.uvRect.y, pc.uvRect.w, inPosition.y));
    o.fragColor = pc.color;
    return o;
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    float alpha = glyphAtlas.Sample(float3(input.fragUV, pc.pageIndex)).r;
    float correctedAlpha = pow(alpha, 1.0 / pc.textContrast);
    return float4(pc.color.rgb, pc.color.a * correctedAlpha);
}