#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
uniform vec3 uCamPos;
out float vFade;
void main(){
  gl_Position = uMVP * vec4(aPos, 1.0);
  float d = distance(aPos, uCamPos);
  vFade = clamp(1.6 - d * 0.004, 0.0, 1.0);
}