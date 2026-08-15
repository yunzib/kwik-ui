/**
 * @brief 纯色三角形网格着色器（解析覆盖率抗锯齿）
 *
 * 输入屏幕空间位置 + 重心坐标 + 三条边高 + edgeMask。
 * 距离 = λ_i × h_i（解析精确），覆盖率 = smoothstep(-0.5, 0.5, dist)（边上=0.5）。
 * 只对轮廓边(edgeMask bit=1)做 AA，内部共享边覆盖率恒为 1。
 * Push constants 36 bytes: color(16) + viewport(8) + opacity(4) + pad(8)
 */

// ── Push Constants ───────────────────────────────────────────
struct TrianglePushConstants {
    float4 uColor;
    float2 uViewport;
    float  uOpacity;
    float  _pad0;
    float  _pad1;
    float m00, m01, m02;    // ← 矩阵
    float m10, m11, m12;
};

[[vk::push_constant]] ConstantBuffer<TrianglePushConstants> pc;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 bary;
    [[vk::location(1)]] float3 h;
    [[vk::location(2)]] float edgeMask;
    [[vk::location(3)]] float4 vColor;
};

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 aPos : ATTR0, float2 aBary : ATTR1, float3 aH : ATTR2, float aMask : ATTR3) {
    VertOutput o;
    float2 world = float2(pc.m00 * aPos.x + pc.m01 * aPos.y + pc.m02,
                          pc.m10 * aPos.x + pc.m11 * aPos.y + pc.m12);
    float px = (world.x / pc.uViewport.x) * 2.0 - 1.0;
    float py = (world.y / pc.uViewport.y) * 2.0 - 1.0;
    o.position = float4(px, py, 0.0, 1.0);
    // bary/h/edgeMask/vColor 输出不变
    o.bary = aBary;
    o.h = aH;
    o.edgeMask = aMask;
    o.vColor = float4(pc.uColor.rgb, pc.uColor.a * pc.uOpacity);
    return o;
}

// ── 片段着色器：解析覆盖率抗锯齿 ────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    float3 bary = float3(input.bary, 1.0 - input.bary.x - input.bary.y);
    // 解析精确距离：λ_i × h_i（像素单位，正=内侧，负=外侧）
    float3 dist = bary * input.h;
    // 正确覆盖率：边上 dist=0 → 0.5；±0.5px 过渡
    float3 cov = smoothstep(float3(-0.5, -0.5, -0.5), float3(0.5, 0.5, 0.5), dist);
    // edgeMask bit0=(p0,p1)↔λ2，bit1=(p1,p2)↔λ0，bit2=(p2,p0)↔λ1；内部边恒 1
    uint m = uint(input.edgeMask);
    float3 flag = float3(float((m >> 1) & 1u), float((m >> 2) & 1u), float((m >> 0) & 1u));
    cov = lerp(float3(1.0, 1.0, 1.0), cov, flag);
    float alpha = min(min(cov.x, cov.y), cov.z);
    return float4(input.vColor.rgb, input.vColor.a * alpha);
}