/** @brief 液态玻璃：分离高斯模糊（blurH/blurV 复用 vertexMain+fragmentMain） */
struct BlurPushConstants {
    float texelOffsetX; float texelOffsetY; float radius; float _pad;
    float u0; float v0; float u1; float v1;
    float viewportW; float viewportH;
};
[[vk::push_constant]] ConstantBuffer<BlurPushConstants> pc;
[[vk::binding(0,0)]] Sampler2D srcTex;

[shader("vertex")]
float4 vertexMain(float2 inPosition : ATTR0, out float2 fragUV : TEXCOORD0) : SV_Position {
    float2 ndc = inPosition * 2.0 - 1.0;
    fragUV = pc.u0 + inPosition * (pc.u1 - pc.u0);   // 目标内 UV
    return float4(ndc, 0.0, 1.0);
}
[shader("fragment")]
float4 fragmentMain(float2 fragUV : TEXCOORD0) : SV_Target0 {
    int R = int(ceil(pc.radius * 3.0));
    float4 acc = 0; float ws = 0.0;
    for (int i = -R; i <= R; ++i) {
        float w = exp(-float(i*i) / (2.0*pc.radius*pc.radius));
        acc += srcTex.Sample(fragUV + float2(float(i)*pc.texelOffsetX, float(i)*pc.texelOffsetY)) * w;
        ws += w;
    }
    return acc / ws;
}