/**
 * @brief 圆角矩形着色器
 *
 * 支持 Fill(0) / Stroke(1) / Shadow(2) 三种绘制模式。
 * Push constants 布局 96 bytes，与 C++ PushConstants 对齐。
 */

// ── Push Constants ───────────────────────────────────────────
struct RectPushConstants {
    float2 topLeft;
    float2 size;
    float4 fillColor;
    float radius;
    float borderWidth;
    float _pad0;
    float _pad1;
    float4 borderColor;
    float opacity;
    uint  drawMode;        // 0=fill 1=stroke 2=shadow 3=segment 4=linear渐变 5=radial渐变
    float2 shadowOffset;   // 渐变复用：linear=起点 / radial=中心（相对 rect 左上）
    float shadowBlur;
    float _pad2;
    float2 viewportSize;
    float m00, m01, m02;
    float m10, m11, m12;
    float2 gradientVec;    // ← 渐变：linear=终点-起点向量 / radial=(半径,0)
};

[[vk::push_constant]] ConstantBuffer<RectPushConstants> pc;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 fragPos;
    [[vk::location(1)]] float4 fragFillColor;
    [[vk::location(2)]] float2 fragSize;
    [[vk::location(3)]] float  fragRadius;
    [[vk::location(4)]] float  fragBorderWidth;
    [[vk::location(5)]] float4 fragBorderColor;
    [[vk::location(6)]] float  fragOpacity;
    [[vk::location(7)]] uint   fragDrawMode;
    [[vk::location(8)]] float2 fragShadowOffset;
    [[vk::location(9)]] float  fragShadowBlur;
    [[vk::location(10)]] float2 fragViewportSize;
    [[vk::location(11)]] float2 fragGradientVec;
};

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 inPosition : ATTR0) {
    VertOutput o;
    float2 local = pc.topLeft + inPosition * pc.size;
    // 矩阵变换：world = M * local
    float2 world = float2(pc.m00 * local.x + pc.m01 * local.y + pc.m02,
                          pc.m10 * local.x + pc.m11 * local.y + pc.m12);
    float2 ndc = (world + 0.5) / pc.viewportSize * 2.0 - 1.0;
    o.position = float4(ndc, 0.0, 1.0);
    o.fragPos          = inPosition;
    o.fragFillColor    = pc.fillColor;
    o.fragSize         = pc.size;
    o.fragRadius       = pc.radius;
    o.fragBorderWidth  = pc.borderWidth;
    o.fragBorderColor  = pc.borderColor;
    o.fragOpacity      = pc.opacity;
    o.fragDrawMode     = pc.drawMode;
    o.fragShadowOffset = pc.shadowOffset;
    o.fragGradientVec = pc.gradientVec;
    o.fragShadowBlur   = pc.shadowBlur;
    o.fragViewportSize = pc.viewportSize;
    return o;
}

// ── 圆角矩形 SDF ───────────────────────────────────────────
float sdRoundedRect(float2 p, float2 halfSize, float r) {
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// 点到线段的精确欧几里得距离（与 EUI-NEO polygon.frag 同款）
float sdSegment(float2 p, float2 a, float2 b) {
    float2 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
    return length(p - (a + ab * t));
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    float2 center  = input.fragSize * 0.5;
    float  cr      = min(input.fragRadius, min(input.fragSize.x, input.fragSize.y) * 0.5);
    float2 pos     = input.fragPos * input.fragSize;
    float  sdf     = sdRoundedRect(pos - center, input.fragSize * 0.5, cr);

    // 1 pixel NDC → 自适应抗锯齿
    float rawAA = length(fwidth(pos));
    float aa    = clamp(rawAA, 0.10, 0.6);

    if (input.fragDrawMode == 3u) {
        // ── 胶囊（线段 + round cap）：fragBorderColor 复用为端点 a/b ──
        float2 a = input.fragBorderColor.xy;
        float2 b = input.fragBorderColor.zw;
        float sdfSeg = sdSegment(pos, a, b) - input.fragRadius;
        float alpha = (1.0 - smoothstep(-aa, aa, sdfSeg)) * input.fragOpacity;
        return float4(input.fragFillColor.rgb, input.fragFillColor.a * alpha);
    }

    if (input.fragDrawMode == 4u) {
        // ── 线性渐变：pos 在 gradientVec 上的投影比例 t → color0↔color1 插值 ──
        float2 d = input.fragGradientVec;
        float t = clamp(dot(pos - input.fragShadowOffset, d) / max(dot(d, d), 1e-6), 0.0, 1.0);
        float4 col = lerp(input.fragFillColor, input.fragBorderColor, t);
        float alpha = (1.0 - smoothstep(-aa, aa, sdf)) * input.fragOpacity;
        return float4(col.rgb, col.a * alpha);
    }
    if (input.fragDrawMode == 5u) {
        // ── 径向渐变：中心距离 / 半径 → t → 插值 ──
        float2 c = input.fragShadowOffset;
        float r = max(input.fragGradientVec.x, 1e-4);
        float t = clamp(length(pos - c) / r, 0.0, 1.0);
        float4 col = lerp(input.fragFillColor, input.fragBorderColor, t);
        float alpha = (1.0 - smoothstep(-aa, aa, sdf)) * input.fragOpacity;
        return float4(col.rgb, col.a * alpha);
    }

    if (input.fragDrawMode == 2u) {
        // ── 阴影 ──────────────────────────────────────────────
        float2 shadowCenter = input.fragSize * 0.5 + input.fragShadowOffset;
        float  sr = min(cr + input.fragShadowBlur * 0.5,
                        min(input.fragSize.x, input.fragSize.y) * 0.5);
        float shadowSDF = sdRoundedRect(pos - shadowCenter, input.fragSize * 0.5, sr);
        float alpha = (1.0 - smoothstep(-aa, aa, shadowSDF)) * input.fragOpacity;
        return float4(input.fragFillColor.rgb, input.fragFillColor.a * alpha);
    } else if (input.fragDrawMode == 1u) {
        // ── 描边 ──────────────────────────────────────────────
        float2 innerHalf  = float2(0.5) - input.fragBorderWidth / input.fragSize;
        float2 innerSize  = input.fragSize * innerHalf;
        float  innerR     = max(cr - input.fragBorderWidth, 0.0);
        float  innerSDF   = sdRoundedRect(pos - center, innerSize, innerR);
        float  outerAlpha = 1.0 - smoothstep(-aa, aa, sdf);
        float  innerAlpha = 1.0 - smoothstep(-aa, aa, innerSDF);
        float  borderAlpha = (outerAlpha - innerAlpha) * input.fragOpacity;
        return float4(input.fragBorderColor.rgb, input.fragBorderColor.a * borderAlpha);
    } else {
        // ── 填充 ──────────────────────────────────────────────
        float alpha = (1.0 - smoothstep(-aa, aa, sdf)) * input.fragOpacity;
        return float4(input.fragFillColor.rgb, input.fragFillColor.a * alpha);
    }
}