#version 450

layout (location = 0) out vec3 outColor;

void main() {
    // Const array of positions for the triangle.
    const vec3 positions[3] = vec3[3](
        vec3( 1., 1., 0.),
        vec3(-1., 1., 0.),
        vec3( 0.,-1., 0.)
    );

    // Const array of colors for the triangle.
    const vec3 colors[3] = vec3[3](
        vec3(1., 0., 0.),
        vec3(0., 1., 0.),
        vec3(0., 0., 1.)
    );

    // Output the position of each vertex
    gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
    outColor = colors[gl_VertexIndex];
}