#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat vec4 fragColor;
layout(binding = 0) uniform sampler2D imageTex;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 texColor = texture(imageTex, fragUV);
    outColor = texColor * fragColor;  // fragColor.a 携带 opacity
}