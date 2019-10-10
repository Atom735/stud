#version 150
in  vec2 in_Position;

out vec4 ex_Color;
void main(void) {
    gl_Position = vec4(in_Position.x, in_Position.y, 0.0, 1.0);
    ex_Color = vec4(in_Position.x, in_Position.y, 0.0, 1.0);
}
