#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 texCoord;
layout (location = 2) out vec3 normalWorldSpace;

// Binding 0 within descriptor set at slot 0.
layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
} cameraData;

struct ObjectData {
    mat4 model;
};

// --- All Object Matrices ---
// We need to use the std140 layout description to make the array match how arrays work in cpp. It enforces some rules
// about how the memory is laid out, and its alignment.
layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    // Array is not sized; you can only have unsized arrays in storage buffers. This will let the shader scale to
    // whatever buffer size we have.
    ObjectData objects[];
} objectBuffer;

// Push constants block (UNUSED)--must match struct on C++ side 1-to-1 or the GPU will read data incorrectly!
layout (push_constant) uniform constants {
    vec4 data;
    mat4 renderMatrix;
} PushConstants;

void main() {
    // All the draw commands in Vulkan request "first instance" and "instance count"; because we are not doing
    // instanced rendering, the instance count is always 1. We can still change the "first instance" parameter giving
    // us a way to send a single integer to the shader without setting up push constants/descriptors.
    mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
    mat4 transformMatrix = cameraData.viewProjection * modelMatrix;
    gl_Position = transformMatrix * vec4(vPosition, 1.0);

    normalWorldSpace = normalize(mat3(modelMatrix) * vNormal);

    outColor = vColor;
    texCoord = vTexCoord;
}
