#version 450 core

in vec2 v2f_texCoords;

out vec4 out_color;

uniform sampler2D u_tex;
uniform vec4 u_color;

void main() {
    out_color = u_color;
    out_color.a *= texture(u_tex, v2f_texCoords).r;
}