#version 450

layout (location = 0) in vec3 vNorm;
layout (location = 1) in vec2 vTexCoord;

layout (location = 0) out vec4 oFragColor;

layout(set = 0, binding = 0) uniform MaterialBuffer {
    vec4 baseColor;
} material;

layout (set = 0, binding = 1) uniform sampler2D colorTexture;

vec3 ambientColor = vec3(0.27, 0.27, 0.27);
vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main() {
    vec3 lightDirection = normalize(vec3(0.5, 0.75, 0.0));
    float diff = max(dot(normalize(vNorm), lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;
    vec4 textureSample = texture(colorTexture, vTexCoord);

    vec3 lightingResult = (ambientColor + diffuse) * (textureSample.rgb * material.baseColor.rgb);

    oFragColor = vec4(lightingResult, textureSample.a * material.baseColor.a);
}