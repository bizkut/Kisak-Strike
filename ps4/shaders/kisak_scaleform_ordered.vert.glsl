#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 inpos;
layout(location = 1) in vec4 incol;
layout(location = 2) in vec2 inuv;
layout(location = 3) in float inmode;

layout(location = 0) out vec4 outcol;
layout(location = 1) out vec2 outuv;
layout(location = 2) out float outmode;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(inpos.xy, 0.0, 1.0);
    outcol = incol;
    outuv = inuv;
    outmode = inmode;
}
