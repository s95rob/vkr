#version 450

layout (location = 0) in vec3 vNorm;

layout (location = 0) out vec4 oFragColor;

layout(set = 0, binding = 0) uniform MaterialBuffer {
    vec4 baseColor;
} material;

vec3 ambientColor = vec3(0.7, 0.7, 0.7);
vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main() {
    vec3 lightDirection = normalize(-vec3(-0.2, -1.0, -0.3));
    float diff = max(dot(normalize(vNorm), lightDirection), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 lightingResult = (ambientColor + diffuse) * material.baseColor.rgb;

    oFragColor = vec4(lightingResult, material.baseColor.a);
}