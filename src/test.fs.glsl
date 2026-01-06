#version 450

layout(location = 0) out vec4 color;

layout(set = 0, binding = 0) uniform MaterialBuffer {
    vec4 baseColor;
} material;

void main() {
    color = material.baseColor;
}