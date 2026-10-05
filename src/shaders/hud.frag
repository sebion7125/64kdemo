#version 330 core
in vec2 vUV;
out vec4 O;
uniform sampler2D uFont;
uniform vec4 uTint;
void main(){
  vec4 c = texture(uFont, vUV);
  O = vec4(c.rgb * uTint.rgb, c.a * uTint.a);
}