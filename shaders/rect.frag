#version 450
layout(location = 0) in vec2  fragPos;
layout(location = 1) in flat vec4 fragFillColor;
layout(location = 2) in flat vec2 fragSize;
layout(location = 3) in flat float fragRadius;
layout(location = 4) in flat float fragBorderWidth;
layout(location = 5) in flat vec4 fragBorderColor;
layout(location = 6) in flat float fragOpacity;
layout(location = 7) in flat uint fragDrawMode;
layout(location = 8) in flat vec2 fragShadowOffset;
layout(location = 9) in flat float fragShadowBlur;
layout(location = 10) in flat vec2 fragViewportSize;
layout(location = 0) out vec4 outColor;
float sdRoundedRect(vec2 p, vec2 halfSize, float r) {
    vec2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
void main() {
    vec2  center   = fragSize * 0.5;
    float cr       = min(fragRadius, min(fragSize.x, fragSize.y) * 0.5);
    float sdf      = sdRoundedRect(fragPos * fragSize - center, fragSize * 0.5, cr);
    // 1 pixel in NDC → pixelSize for anti-aliasing
    float rawAA = length(fwidth(fragPos * fragSize));
    float aa = clamp(rawAA, 0.15, 0.4);
    if (fragDrawMode == 2u) {
        // ── Shadow ──────────────────────────────────────────────
        vec2  shadowCenter = fragSize * 0.5 + fragShadowOffset;
        float sr = cr + fragShadowBlur * 0.5;
        sr = min(sr, min(fragSize.x, fragSize.y) * 0.5);
        float shadowSDF = sdRoundedRect(fragPos * fragSize - shadowCenter, fragSize * 0.5, sr);
        float shadowAlpha = 1.0 - smoothstep(-aa, aa, shadowSDF);
        shadowAlpha *= fragOpacity;
        outColor = vec4(fragFillColor.bgr, fragFillColor.a * shadowAlpha);
    } else if (fragDrawMode == 1u) {
        // ── Stroke (border) ─────────────────────────────────────
        vec2  innerHalf  = vec2(0.5) - fragBorderWidth / fragSize;
        vec2  innerSize  = fragSize * innerHalf;
        float innerR     = max(cr - fragBorderWidth, 0.0);
        float innerSDF   = sdRoundedRect(fragPos * fragSize - center, innerSize, innerR);
        float outerAlpha = 1.0 - smoothstep(-aa, aa, sdf);
        float innerAlpha = 1.0 - smoothstep(-aa, aa, innerSDF);
        float borderAlpha = outerAlpha - innerAlpha;
        borderAlpha *= fragOpacity;
        outColor = vec4(fragBorderColor.bgr, fragBorderColor.a * borderAlpha);
    } else {
        // ── Fill ────────────────────────────────────────────────
        float alpha = 1.0 - smoothstep(-aa, aa, sdf);
        alpha *= fragOpacity;
        outColor = vec4(fragFillColor.bgr, fragFillColor.a * alpha);
    }
}