#version 150
in  vec2 in_Position;

out vec4 ex_Color;
void main(void) {
    gl_Position = vec4(in_Position.x*2.0-1.0, in_Position.y*2.0-1.0, 0.0, 1.0);
    ex_Color = vec4(1.0, 1.0, 1.0, 1.0);
}
