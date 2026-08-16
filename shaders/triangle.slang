/**
 * @brief 纯色三角形网格着色器（解析覆盖率抗锯齿 + SDF 圆环）
 *
 * 常规三角形：距离 = λ_i × h_i（解析精确），覆盖率 = smoothstep(-0.5, 0.5, dist)。
 * gradMode: 0=纯色  1=扫描渐变(顶点按角度插值)  2=SDF圆环(像素级)
 * Push constants 128 bytes（Vulkan 最小保证），Ring 模式 quad 由后端生成。
 */

// ── Push Constants ───────────────────────────────────────────
// 布局：color(16) + viewport(8) + opacity(4) + pad(8) + 矩阵(24)
//       + 渐变/环参数(68) = 128B
struct TrianglePushConstants {
    float4 uColor;          // 纯色模式颜色；Sweep 渐变起点色；Ring 弧起点色
    float2 uViewport;
    float  uOpacity;
    float  _pad0;
    float  _pad1;
    float m00, m01, m02;    // ← 矩阵
    float m10, m11, m12;
    // ── Sweep 扫描渐变（gradMode==1）／ SDF 圆环（gradMode==2）──
    float  gradMode;        // 0=纯色  1=扫描渐变  2=SDF圆环
    float  gradCx;          // 圆心 X（本地逻辑坐标，随矩阵变换）
    float  gradCy;          // 圆心 Y
    float  gradA0;          // 起始角（弧度）：渐变起点 / 弧起点
    float  gradA1;          // 结束角（弧度）：渐变终点 / 弧终点
    float4 uGradEnd;        // 渐变终点色（Ring = 弧终点色）
    // ── SDF 圆环参数（gradMode==2 时启用）──
    float  ringMidR;        // 圆环中径（本地逻辑坐标）
    float  ringHalfW;       // 半带宽（本地逻辑坐标 = strokeWidth/2）
    float  ringCapMode;     // 0=圆头端帽  1=平头
    float  _pad3, _pad4, _pad5, _pad6, _pad7;
};

[[vk::push_constant]] ConstantBuffer<TrianglePushConstants> pc;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 bary;      // 重心坐标(u,v)；Ring 模式复用为本地坐标
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
    o.bary = aBary;
    o.h = aH;
    o.edgeMask = aMask;
    // 颜色：纯色 或 Sweep 角度渐变（顶点内 atan2 → 三角形内插值精确）。
    // 注意：SDF 圆环（gradMode==2）渐变在 fragment 逐像素计算，顶点不参与
    float4 col = pc.uColor;
    if (pc.gradMode > 0.5 && pc.gradMode < 1.5) {
        const float TAU = 6.28318530;
        float ang = atan2(aPos.y - pc.gradCy, aPos.x - pc.gradCx);
        if (ang < 0.0) ang += TAU;                               // 归一化到 [0, 2π)
        float A0 = pc.gradA0 - TAU * floor(pc.gradA0 / TAU);     // 起始角归一到 [0, 2π)
        float span = pc.gradA1 - pc.gradA0;                      // 原始跨度（含跨 2π 全圆）
        float t = (ang - A0) / span;
        if (t < 0.0) t += TAU / span;                            // 环绕修正
        col = lerp(col, pc.uGradEnd, clamp(t, 0.0, 1.0));
    }
    o.vColor = float4(col.rgb, col.a * pc.uOpacity);
    return o;
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    // ── SDF 圆环（gradMode==2）：像素级精确圆 + 渐变 + 端帽，零折线细分 ──
    if (pc.gradMode > 1.5) {
        const float TAU = 6.28318530;
        float2 p = input.bary.xy;                        // 本地逻辑坐标（quad 顶点插值）
        float2 v = p - float2(pc.gradCx, pc.gradCy);
        float len = length(v);
        float d = abs(len - pc.ringMidR) - pc.ringHalfW; // 环带 SDF（正=带外）
        float pw = max(fwidth(d), 0.75);                 // AA 过渡带（fwidth 自适应缩放；0.75 下限防过锐，同 EUI-NEO）
        float covBand = 1.0 - smoothstep(-pw, pw, d);    // 环带覆盖率

        float ang = atan2(v.y, v.x);
        if (ang < 0.0) ang += TAU;
        float A0 = pc.gradA0 - TAU * floor(pc.gradA0 / TAU);
        float span = pc.gradA1 - pc.gradA0;
        float rel = ang - A0;
        if (rel < 0.0) rel += TAU;

        // 角度范围裁剪：整圆（span≈2π）不裁剪；部分弧按起止角做软边（~1px 弧长过渡）
        float in = 1.0;
        if (span < TAU - 1e-4) {
            float aaLen = 1.0 / max(pc.ringMidR, 1.0);            // ~1px 弧长对应弧度
            float e0 = 1.0 - smoothstep(TAU - aaLen, TAU, rel);   // 起点软边（环绕尾段淡入）
            float e1 = 1.0 - smoothstep(span, span + aaLen, rel); // 终点软边
            in = min(e0, e1);
        }

        // 端帽：圆头 = 端点圆盘 SDF（半径 halfW，圆心在中径端点）；平头 = 仅角度软边收口
        float cov0 = 0.0, cov1 = 0.0;
        if (pc.ringCapMode < 0.5 && span < TAU - 1e-4) {
            float2 p0 = float2(pc.gradCx + cos(pc.gradA0) * pc.ringMidR,
                               pc.gradCy + sin(pc.gradA0) * pc.ringMidR);
            float2 p1 = float2(pc.gradCx + cos(pc.gradA1) * pc.ringMidR,
                               pc.gradCy + sin(pc.gradA1) * pc.ringMidR);
            cov0 = 1.0 - smoothstep(-pw, pw, length(p - p0) - pc.ringHalfW);
            cov1 = 1.0 - smoothstep(-pw, pw, length(p - p1) - pc.ringHalfW);
        }
        float alpha = max(covBand * in, max(cov0, cov1));   // 并集覆盖

        // 渐变：per-pixel 角度插值（起点色@a0 → 终点色@a1）；端帽处补对应端色
        float t = clamp(rel / max(span, 1e-6), 0.0, 1.0);
        float4 col = lerp(pc.uColor, pc.uGradEnd, t);
        col = lerp(col, pc.uColor, cov0);
        col = lerp(col, pc.uGradEnd, cov1);
        return float4(col.rgb, col.a * pc.uOpacity * alpha);
    }

    // ── 常规三角形：解析覆盖率抗锯齿（原有路径，零改动）──
    float3 bary = float3(input.bary, 1.0 - input.bary.x - input.bary.y);
    float3 dist = bary * input.h;
    float3 cov = smoothstep(float3(-0.5, -0.5, -0.5), float3(0.5, 0.5, 0.5), dist);
    uint m = uint(input.edgeMask);
    float3 flag = float3(float((m >> 1) & 1u), float((m >> 2) & 1u), float((m >> 0) & 1u));
    cov = lerp(float3(1.0, 1.0, 1.0), cov, flag);
    float alpha = min(min(cov.x, cov.y), cov.z);
    return float4(input.vColor.rgb, input.vColor.a * alpha);
}