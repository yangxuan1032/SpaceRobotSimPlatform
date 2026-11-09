#version 440

// Samples the 8-bit coverage atlas and tints it with the vertex colour.

layout(binding = 1) uniform sampler2D u_atlas;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_color;

layout(location = 0) out vec4 fragColor;

void main() {
  const float coverage = texture(u_atlas, v_uv).a;
  fragColor = vec4(v_color, v_color, v_color, coverage);
}