#version 450

layout (location = 0) in vec3 vecPosition;
layout (location = 1) in vec3 vecNormal;
layout (location = 2) in vec3 vecColor;

layout (location = 0) out vec3 outColor;

// Push constants block--must match struct on C++ side 1-to-1 or the GPU will read data incorrectly!
layout (push_constant) uniform constants {
    vec4 data;
    mat4 render_matrix;
} PushConstants;

void main() {
    gl_Position = PushConstants.render_matrix * vec4(vecPosition, 1.0);
    outColor = vecColor;
}
