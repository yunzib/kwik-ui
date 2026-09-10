/** @brief 液态玻璃：合成（模糊图贴元素框 + 圆角 SDF + 液态玻璃边缘折射/高光）
 *  blur/raw 纹理为固定 canvas/4 尺寸，blit 到与画布同位置的子区域，
 *  故 UV = 世界像素 / 视口尺寸（viewport == canvas extent）。 */
struct QuadPushConstants {
    float posX; float posY; float sizeX; float sizeY;   // 元素逻辑框（未外扩）
    float cornerRadius; float refraction; float specular; float _pad0;
    float m00; float m01; float m02; float m10; float m11; float m12;
    float viewportW; float viewportH;
};
[[vk::push_constant]] ConstantBuffer<QuadPushConstants> pc;
[[vk::binding(0, 0)]] Sampler2D blurTex;   // pingB：分离高斯结果（覆盖捕获盒）
[[vk::binding(1, 0)]] Sampler2D rawTex;    // capture：未模糊降采样图（折射锐利细节）

float sdRoundedRect(float2 p, float2 halfSize, float r) {
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

[shader("vertex")]
float4 vertexMain(float2 inPosition : ATTR0,
                  out float2 fragPos : TEXCOORD0,
                  out float2 worldPos : TEXCOORD1) : SV_Position {
    float2 local = float2(pc.posX, pc.posY) + inPosition * float2(pc.sizeX, pc.sizeY);
    float2 world = float2(pc.m00*local.x + pc.m01*local.y + pc.m02,
                          pc.m10*local.x + pc.m11*local.y + pc.m12);
    float2 ndc = (world + 0.5) / float2(pc.viewportW, pc.viewportH) * 2.0 - 1.0;
    fragPos = inPosition;                       // 0..1（同 rect.slang 约定，片元再乘 size）
    worldPos = world;
    return float4(ndc, 0.0, 1.0);
}

[shader("fragment")]
float4 fragmentMain(float2 fragPos : TEXCOORD0, float2 worldPos : TEXCOORD1) : SV_Target0 {
    float2 size = float2(pc.sizeX, pc.sizeY);
    float2 pos = fragPos * size;                // 0..1 → 元素本地像素（同 rect.slang: fragPos*fragSize）
    float2 center = size * 0.5;
    float cr = min(pc.cornerRadius, min(size.x, size.y) * 0.5);
    float2 p = pos - center;
    float sdf = sdRoundedRect(p, size * 0.5, cr);
    float aa = clamp(length(fwidth(pos)), 0.10, 0.6);
    float alpha = 1.0 - smoothstep(-aa, aa, sdf);

    float2 viewport = float2(pc.viewportW, pc.viewportH);
    float2 uv = worldPos / viewport;           // 世界 px → 纹理 UV（纹理与画布同构，1/4 采样）
    float4 c = blurTex.Sample(uv);

    // ── 液态玻璃边缘：SDF 解析梯度 → 边缘带高度场 → 折射 + 高光 ──
    if (pc.refraction > 0.0 || pc.specular > 0.0) {
        float band = max(min(min(size.x, size.y) * 0.5, 24.0), 1.0);   // 边缘带宽（本地 px）
        float2 innerHalf = max(size * 0.5 - cr, 0.0);
        float2 cp = clamp(p, -innerHalf, innerHalf);
        float2 dir = p - cp;                     // 内盒指向片元 = 边缘法线方向（本地）
        float dl = length(dir);
        dir = dl > 1e-4 ? dir / dl : float2(0.0, 0.0);
        float rim = saturate(1.0 + sdf / band);  // 1 在边缘 → 0 深入内部

        // 本地折射偏移 → 世界 px（引擎变换仅平移/旋转/等比缩放，线性部分直接作用）
        float2 off = dir * rim * pc.refraction;
        float2 worldR = worldPos + float2(pc.m00*off.x + pc.m01*off.y,
                                          pc.m10*off.x + pc.m11*off.y);
        float2 uvR = worldR / viewport;
        float4 raw = rawTex.Sample(uvR);
        c.rgb = lerp(c.rgb, raw.rgb, rim * rim * 0.75);   // 越靠边缘透出越锐利的折射内容

        // 高光：外圈钟形 + 左上受光偏置（y-down：dir=(0,-1) 为顶边）
        float bias = 0.65 + 0.35 * dot(dir, float2(-0.6, -0.8));
        c.rgb += pc.specular * rim * rim * rim * bias;
    }
    return float4(c.rgb, alpha);
}
