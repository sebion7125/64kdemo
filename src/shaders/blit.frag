#version 330 core
in vec2 vUV;
out vec4 O;
uniform sampler2D uTex;
void main(){
  O = texture(uTex, vUV);
}