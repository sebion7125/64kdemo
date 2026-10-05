#version 330 core
in vec2 vUV;
out vec4 O;
uniform sampler2D uFont;
uniform vec4 uTint;
void main(){
  vec4 c = texture(uFont, vUV);
  // Farbe aus dem Tint (Font-Textur hat nur Alpha, RGB=0);
  // alpha = Textur-Maske * Tint-Alpha.
  O = vec4(uTint.rgb, c.a * uTint.a);
}