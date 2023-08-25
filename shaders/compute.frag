#version 450

layout (binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

// Push constants block (UNUSED)--must match struct on C++ side 1-to-1 or the GPU will read data incorrectly!
layout (push_constant) uniform CameraPushConstants {
    float iteration;
} constants;

void main() {
    vec4 color = texture(samplerColor, vec2(uv.s, 1.0 - uv.t));
    color.xyz = color.xyz / (color.w);

    outFragColor = color;
}