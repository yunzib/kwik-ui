/**
 * @brief 图片着色器
 *
 * Sampler2D 为内置 combined image sampler，
 * 可选圆角裁剪使用 SDF 实现。
 */

// ── Push Constants ───────────────────────────────────────────
struct ImagePushConstants {
    float2 pos;
    float2 size;
    float4 uvRect;
    float4 color;
    float2 viewportSize;
    float cornerRadius;
};

[[vk::push_constant]] ConstantBuffer<ImagePushConstants> pc;

// ── 图片 Combined Sampler ───────────────────────────────────
[[vk::binding(0, 0)]]
Sampler2D imageTex;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 fragUV;
    [[vk::location(1)]] float4 fragColor;
    [[vk::location(2)]] float2 fragSize;
    [[vk::location(3)]] float  fragCornerRadius;
};

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 inPosition : ATTR0) {
    VertOutput o;
    float2 screenPos = pc.pos + inPosition * pc.size;
    float2 ndc = (screenPos + 0.5) / pc.viewportSize * 2.0 - 1.0;
    o.position = float4(ndc, 0.0, 1.0);
    o.fragUV = float2(
        lerp(pc.uvRect.x, pc.uvRect.z, inPosition.x),
        lerp(pc.uvRect.y, pc.uvRect.w, inPosition.y)
    );
    o.fragColor        = pc.color;
    o.fragSize         = pc.size;
    o.fragCornerRadius = pc.cornerRadius;
    return o;
}

// ── 圆角矩形 SDF (与 rect.slang 同算法) ──────────────────────
float sdRoundedRect(float2 p, float2 halfSize, float r) {
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    float4 texColor = imageTex.Sample(input.fragUV);

    // 无圆角 → 直接输出，零额外开销
    if (input.fragCornerRadius <= 0.0) {
        return float4(texColor.rgb, texColor.a) * input.fragColor.a;
    }

    // ── SDF 圆角裁剪 ──────────────────────────────────────────
    float2 halfSize  = input.fragSize * 0.5;
    float  cr        = min(input.fragCornerRadius, min(halfSize.x, halfSize.y));
    float2 fragCoord = (input.fragUV - 0.5) * input.fragSize;
    float  sdf       = sdRoundedRect(fragCoord, halfSize, cr);
    float  rawAA     = length(fwidth(fragCoord));
    float  aa        = clamp(rawAA, 0.10, 0.6);
    float  alpha     = 1.0 - smoothstep(-aa, aa, sdf);

    return float4(texColor.rgb, texColor.a * alpha) * input.fragColor.a;
}