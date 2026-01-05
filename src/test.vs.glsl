#version 450

layout (location = 0) in vec3 aPos;

layout (push_constant) uniform constants {
    mat4 mvp;
} PushConstants;

void main() {
    gl_Position = PushConstants.mvp * vec4(aPos, 1.0);
}