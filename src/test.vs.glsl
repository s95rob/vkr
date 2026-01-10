#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) out vec3 oNorm;
layout (location = 1) out vec2 oTexCoord;

layout (push_constant) uniform constants {
    mat4 modelMatrix;
    mat4 viewProjectionMatrix;
} PushConstants;

void main() {
    gl_Position = PushConstants.viewProjectionMatrix * PushConstants.modelMatrix * vec4(aPos, 1.0);
    oNorm = aNorm;
    oTexCoord = aTexCoord;
}