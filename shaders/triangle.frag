#version 450
/**
 * @brief 纯色三角形网格片段着色器
 */

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vColor;
}