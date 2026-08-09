/**
 * @file mesh.slang
 * @brief 3D 网格着色器 (G3D 组件)
 *
 * 输入: float3 位置 + float3 法线 (均对象空间)
 * Push constants 96 bytes (必须与 vulkan_3d_renderer.cpp 的 PushConstants 对齐):
 *   uMvp      (offset 0)  float4x4  模型-视图-投影 (列主序)
 *   uColor    (offset 64) float4    基础颜色
 *   uLightDir (offset 80) float4    方向光 (对象空间, 归一化; w 未用)
 *
 * 光照策略: 兰伯特漫反射 + 环境光, 在对象空间计算。
 * 法线与光源同处对象空间 → 顶点无需法线变换, 节省一个矩阵。
 * 光源随物体旋转的"贴纸感"由 G3D 组件 CPU 端用模型旋转转置
 * 预旋转光源方向消除 (见 src/element/g3d.cpp)。
 */

// ── Push Constants ───────────────────────────────────────────
struct MeshPushConstants {
    float4x4 uMvp;       // offset 0,  列主序 64B
    float4   uColor;     // offset 64
    float4   uLightDir;  // offset 80 (xyz 有效, w 未用)
};
[[vk::push_constant]] ConstantBuffer<MeshPushConstants> pc;

// ── Vertex → Fragment 传参 ─────────────────────────────────
struct VertOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 vNormal : NORMAL0;
    [[vk::location(1)]] float4 vColor : COLOR0;
};

// ── 顶点着色器 ──────────────────────────────────────────────
[shader("vertex")]
VertOutput vertexMain(float3 aPos : ATTR0, float3 aNormal : ATTR1) {
    VertOutput o;
    // HLSL 语义: 行向量 × 列主序矩阵 = 正确模型变换
    o.position = mul(pc.uMvp, float4(aPos, 1.0));   // 矩阵在前，列向量在后 → mvp·p
    o.vNormal = aNormal;
    o.vColor = pc.uColor;
    return o;
}

// ── 片段着色器 ──────────────────────────────────────────────
[shader("fragment")]
float4 fragmentMain(VertOutput input) : SV_Target0 {
    float3 n = normalize(input.vNormal);
    float3 l = normalize(pc.uLightDir.xyz);
    float diff = max(dot(n, l), 0.0);          // 兰伯特漫反射
    const float ambient = 0.25;                // 环境光补偿 (避免全黑)
    float3 c = input.vColor.rgb * (ambient + (1.0 - ambient) * diff);
    return float4(c, input.vColor.a);
}