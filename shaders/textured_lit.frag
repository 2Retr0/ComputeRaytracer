#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 normalWorldSpace;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 fogColor;          // w=exponent
    vec4 fogDistances;      // x=min; y=max; z,w=unused.
    vec4 ambientColor;
    vec4 sunlightDirection; // w= power
    vec4 sunlightColor;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D tex;

void main() {
    vec3 color = texture(tex, texCoord).xyz;

    float ambientIntensity = sceneData.sunlightDirection.w;
    vec3 sunlightDirection = sceneData.sunlightDirection.xyz;
    float diffuseIntensity = min(ambientIntensity + max(dot(normalWorldSpace, sunlightDirection), 0), 1);

    outFragColor = vec4(color * diffuseIntensity, 1.0f);
}