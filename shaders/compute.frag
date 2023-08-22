#version 450

layout (binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(samplerColor, vec2(uv.s, 1.0 - uv.t));
}