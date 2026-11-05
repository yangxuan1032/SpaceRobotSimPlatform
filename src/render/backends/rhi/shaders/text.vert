#version 440

// Screen-space text quad: positions arrive in pixels, uv in [0,1].

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in float a_color;

layout(std140, binding = 0) uniform TextBlock {
  vec2 viewport_size;
  vec2 pad;
} block;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out float v_color;

void main() {
  const vec2 ndc = a_position / block.viewport_size * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
  v_uv = a_uv;
  v_color = a_color;
}