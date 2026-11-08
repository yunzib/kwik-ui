/**
 * @brief 图片着色器
 *
 * Sampler2D 为内置 combined image sampler，
 * 可选圆角裁剪使用 SDF 实现（顶点变换与圆角段见 image_common.slang，
 * 与 image_yuv.slang 共用）。
 */
#include "image_common.slang"

// ── Push Constants ───────────────────────────────────────────
struct ImagePushConstants {
    float2 pos;
    float2 size;
    float4 uvRect;
    float4 color;
    float2 viewportSize;
    float cornerRadius;
    float m00, m01, m02;    // 矩阵（行主序 2×3）
    float m10, m11, m12;
};

[[vk::push_constant]] ConstantBuffer<ImagePushConstants> pc;

// ── 图片 Combined Sampler ───────────────────────────────────
[[vk::binding(0, 0)]]
Sampler2D imageTex;

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 inPosition : ATTR0) {
    return imageVertex(inPosition, pc.pos, pc.size, pc.uvRect, pc.color, pc.viewportSize, pc.cornerRadius,
                       float3(pc.m00, pc.m01, pc.m02), float3(pc.m10, pc.m11, pc.m12));
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    float4 texColor = imageTex.Sample(input.fragUV);
    // 无圆角时覆盖率恒 1（等价原直出 float4(rgb, a) * opacity）
    return float4(texColor.rgb, texColor.a * roundedCornerAlpha(input)) * input.fragColor.a;
}
