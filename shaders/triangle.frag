#version 450

// Declare we will write a vec4 at location 0
layout (location = 0) out vec4 outFragColor;

void main() {
    outFragColor = vec4(1., 0. ,0., 1.); // Red
}