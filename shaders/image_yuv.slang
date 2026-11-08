/**
 * @brief YUV 图像着色器（V1 推流批 2）— 平面采样 → 范围展开 → 矩阵转 RGB
 *
 * 流纹理 YUV 格式按平面上传（R8 / RG8 / RGBA8），本着色器在片段阶段
 * 合成 RGB；顶点变换与圆角段与 image.slang 同源（image_common.slang）。
 *
 * mode 位表（与 src/render/vulkan/vulkan_image_stream.cpp yuvModeFor 同表）：
 *   bit0-1  平面形态：0 = 三平面（plane0 Y / plane1 U / plane2 V，各 R8）
 *                     1 = 双平面（plane0 Y R8 / plane1 UV 交错 RG8）
 *                     2 = 打包 4:2:2（plane0 = 同块数据按 RG8 w×h 读亮度，
 *                         plane1 = 按 RGBA8 (w/2)×h 读宏像素色度）
 *   bit2    交换 U/V（YV12 平面序 Y/V/U；NV21 交错序 VU）
 *   bit3    打包序 UYVY（亮度 .g、色度 .r/.b；否则 YUYV 亮度 .r、色度 .g/.a）
 *   bit4-5  矩阵：0 = BT.601  1 = BT.709  2 = BT.2020（非恒定亮度）
 *   bit6    full range（否则 limited：Y 16-235 / C 16-240）
 */
#include "image_common.slang"

// ── Push Constants（= ImagePushConstants 84B + mode 4B，平铺不嵌套）──
struct YuvPushConstants {
    float2 pos;
    float2 size;
    float4 uvRect;
    float4 color;
    float2 viewportSize;
    float cornerRadius;
    float m00, m01, m02;    // 矩阵（行主序 2×3）
    float m10, m11, m12;
    uint mode;              // 见文件头位表
};

[[vk::push_constant]] ConstantBuffer<YuvPushConstants> pc;

// ── 平面采样器（双平面格式 binding 2 由宿主重复绑 plane0 占位——静态使用须有效）──
[[vk::binding(0, 0)]] Sampler2D plane0;
[[vk::binding(1, 0)]] Sampler2D plane1;
[[vk::binding(2, 0)]] Sampler2D plane2;

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float2 inPosition : ATTR0) {
    return imageVertex(inPosition, pc.pos, pc.size, pc.uvRect, pc.color, pc.viewportSize, pc.cornerRadius,
                       float3(pc.m00, pc.m01, pc.m02), float3(pc.m10, pc.m11, pc.m12));
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    const float2 uv = input.fragUV;
    const uint kind = pc.mode & 3u;
    float y, u, v;
    if (kind == 0u) {                      // 三平面 I420 / YV12
        y = plane0.Sample(uv).r;
        u = plane1.Sample(uv).r;
        v = plane2.Sample(uv).r;
    } else if (kind == 1u) {               // 双平面 NV12 / NV21
        y = plane0.Sample(uv).r;
        float2 c = plane1.Sample(uv).rg;
        u = c.x;
        v = c.y;
    } else {                               // 打包 4:2:2 YUYV / UYVY
        float2 l = plane0.Sample(uv).rg;
        float4 c = plane1.Sample(uv);
        if ((pc.mode & 8u) != 0u) { y = l.y; u = c.x; v = c.z; }    // UYVY: U Y0 V Y1
        else                      { y = l.x; u = c.y; v = c.w; }    // YUYV: Y0 U Y1 V
    }
    if ((pc.mode & 4u) != 0u) {            // YV12 / NV21：U、V 互换
        float t = u;
        u = v;
        v = t;
    }

    // ── 范围展开：Y → [0,1]，Cb/Cr → [-0.5,0.5] ──
    if ((pc.mode & 64u) != 0u) {
        u -= 128.0 / 255.0;
        v -= 128.0 / 255.0;
    } else {
        y = (y - 16.0 / 255.0) * (255.0 / 219.0);
        u = (u - 128.0 / 255.0) * (255.0 / 224.0);
        v = (v - 128.0 / 255.0) * (255.0 / 224.0);
    }

    // ── Y'CbCr → R'G'B'（系数 = 2(1-Kr) / 2Kb(1-Kb)/Kg / 2Kr(1-Kr)/Kg / 2(1-Kb)）──
    const uint matrix = (pc.mode >> 4) & 3u;
    float3 rgb;
    if (matrix == 0u)      rgb = float3(y + 1.402 * v,  y - 0.344136 * u - 0.714136 * v, y + 1.772 * u);     // BT.601
    else if (matrix == 1u) rgb = float3(y + 1.5748 * v, y - 0.187324 * u - 0.468124 * v, y + 1.8556 * u);    // BT.709
    else                   rgb = float3(y + 1.4746 * v, y - 0.164553 * u - 0.571353 * v, y + 1.8814 * u);    // BT.2020
    rgb = saturate(rgb);

    // 不透明（YUV 无 alpha）；输出约定与 image.slang 一致：(rgb, 覆盖率) × opacity
    return float4(rgb, roundedCornerAlpha(input)) * input.fragColor.a;
}
