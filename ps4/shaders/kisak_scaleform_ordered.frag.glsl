#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 incol;
layout(location = 1) in vec2 inuv;
layout(location = 2) in float inmode;

layout(set = 0, binding = 0) uniform sampler2D gradient_atlas;

layout(location = 0) out vec4 outcol;

void main() {
    vec4 gradient = texture(gradient_atlas, inuv);
    outcol = mix(incol, gradient, clamp(inmode, 0.0, 1.0));
}
