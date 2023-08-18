#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;

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

void main() {
    outFragColor = vec4(inColor + sceneData.ambientColor.xyz, 1.0f);
}