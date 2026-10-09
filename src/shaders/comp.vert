#version 300 es
// Composite-Pass: volles Dreieck ohne Vertex-Buffer (gl_VertexID 0..2 ->
// (-1,-1) (3,-1) (-1,3)), ueberdeckt den ganzen Bildschirm.
void main(){
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
