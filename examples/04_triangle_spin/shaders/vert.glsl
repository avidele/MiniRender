#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightColor; // 虽然这里没用，但 UBO 结构要匹配 C++
} ubo;

layout(location = 0) out vec3 fragColor;

void main() {
    // 使用 UBO 中的 MVP 矩阵计算最终位置
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    // 可选：如果你想让点更大，可以设置 gl_PointSize
    // gl_PointSize = 5.0; // 例如，5 像素大小
}