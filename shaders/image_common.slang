/**
 * @brief 图像着色器公共段（image.slang / image_yuv.slang 共用）
 *
 * 顶点变换（2×3 矩阵 + 源 UV 窗口插值）与圆角 SDF 抗锯齿覆盖率。
 * push 常量由各着色器自行声明后按字段传入——不做结构嵌套：std430 会把
 * 含 float4 的嵌套结构尺寸补齐到 16 的倍数（84B → 96B），致着色器侧字段
 * 偏移与 C++ 侧（ImagePushConstants 84B + mode）错位。
 */

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 fragUV;
    [[vk::location(1)]] float4 fragColor;
    [[vk::location(2)]] float2 fragSize;
    [[vk::location(3)]] float  fragCornerRadius;
    [[vk::location(4)]] float2 fragQuad;
};

// ── 顶点变换：local = pos + quad·size；world = M·local（M 行主序 2×3）；
//    UV 在 uvRect 窗口内按 quad 插值 ──
VertOutput imageVertex(float2 inPosition, float2 pos, float2 size, float4 uvRect, float4 color,
                       float2 viewportSize, float cornerRadius, float3 mRow0, float3 mRow1) {
    VertOutput o;
    float2 local = pos + inPosition * size;
    float2 world = float2(mRow0.x * local.x + mRow0.y * local.y + mRow0.z,
                          mRow1.x * local.x + mRow1.y * local.y + mRow1.z);
    float2 ndc = (world + 0.5) / viewportSize * 2.0 - 1.0;
    o.position = float4(ndc, 0.0, 1.0);
    o.fragUV = float2(lerp(uvRect.x, uvRect.z, inPosition.x),
                      lerp(uvRect.y, uvRect.w, inPosition.y));
    o.fragColor        = color;
    o.fragSize         = size;
    o.fragCornerRadius = cornerRadius;
    o.fragQuad         = inPosition;
    return o;
}

// ── 圆角矩形 SDF (与 rect.slang 同算法) ──────────────────────
float sdRoundedRect(float2 p, float2 halfSize, float r) {
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// ── 圆角覆盖率：无圆角恒 1（零额外开销）；有圆角走 SDF + 屏幕导数抗锯齿 ──
// fragCoord 必须从 quad 内坐标重构（fragQuad=顶点 0..1 插值），
// 不能用 (fragUV-0.5)：cover/none 的 UV 是居中裁剪子窗口（如
// 0.2..0.8），(uv-0.5)*size 把 SDF 坐标系整体错位——圆角切错
// 位置（contain/fill 满幅 UV 恰好不受影响，曾是测试页正常假象）
float roundedCornerAlpha(VertOutput input) {
    if (input.fragCornerRadius <= 0.0) return 1.0;
    float2 halfSize  = input.fragSize * 0.5;
    float  cr        = min(input.fragCornerRadius, min(halfSize.x, halfSize.y));
    float2 fragCoord = (input.fragQuad - 0.5) * input.fragSize;
    float  sdf       = sdRoundedRect(fragCoord, halfSize, cr);
    float  rawAA     = length(fwidth(fragCoord));
    float  aa        = clamp(rawAA, 0.10, 0.6);
    return 1.0 - smoothstep(-aa, aa, sdf);
}
