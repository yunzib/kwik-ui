#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D imageTex;
layout(location = 2) in flat vec2 fragSize;           
layout(location = 3) in flat float fragCornerRadius;  
layout(location = 0) out vec4 outColor;

// ── 圆角 SDF (与 rect.frag 同算法) ─────────────────
float sdRoundedRect(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec4 texColor = texture(imageTex, fragUV);

    // 若无圆角 (cornerRadius <= 0) → 直接输出，零额外开销
    if (fragCornerRadius <= 0.0) {
        outColor = vec4(texColor.rgb, texColor.a) * fragColor.a;
        return;
    }

    // ── SDF 圆角裁剪 ───────────────────────────────
    // fragUV 范围 [0,1], 映射到以矩形中心为原点的坐标系
    vec2  halfSize  = fragSize * 0.5;
    float cr        = min(fragCornerRadius, min(halfSize.x, halfSize.y));
    vec2  fragCoord = (fragUV - 0.5) * fragSize;      // 相对中心的像素坐标
    float sdf       = sdRoundedRect(fragCoord, halfSize, cr);
    // fwidth 自适应抗锯齿
    float rawAA = length(fwidth(fragCoord));
    float aa    = clamp(rawAA, 0.10, 0.6);
    float alpha = 1.0 - smoothstep(-aa, aa, sdf);

    outColor = vec4(texColor.rgb, texColor.a * alpha) * fragColor.a;
}