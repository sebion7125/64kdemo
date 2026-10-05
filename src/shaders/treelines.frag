#version 330 core
precision highp float;
in float vFade;
uniform vec4 uColor;
out vec4 O;
void main(){
  O = vec4(uColor.rgb, uColor.a * vFade);
}