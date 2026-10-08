/* tree_lod.c — kanonischer LOD-Kern, Spiegelbild von src/shaders/lod.glsl.
 * Siehe tree_lod.h fuer den Zweck. Aenderungen hier MITSAMMT lod.glsl aendern;
 * tools/test_tree_lod.c prueft beide Seiten. */
#include "tree_lod.h"

float treeLodSmoothstep(float a, float b, float x){
  if(b <= a) return x < a ? 0.0f : 1.0f;
  float t = (x - a) / (b - a);
  if(t < 0.0f) t = 0.0f; else if(t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

void treeLodInit(TreeLod *L, float H, float lodScale){
  L->H = H; L->bb = 1;
  /* Schaltweiten: R1 = wo die Einzelblaetter aufhoeren zu decken, R2 = wo die
   * Ganzbaum-Darstellung uebernimmt. 5x/15x Wuchshoehe, absolut begrenzt —
   * gemessen an einem Baum (Laubpixel ueber der Entfernung): ein 6-cm-Blatt ist
   * ab ~50 m unter einem Pixel, die Einzelblaetter decken die Krone also nur bis
   * etwa 5x Wuchshoehe. */
  L->R1 = 5.0f * H;
  L->R2 = 15.0f * H;
  if(L->R1 < 20.0f)  L->R1 = 20.0f;  else if(L->R1 > 60.0f)  L->R1 = 60.0f;
  if(L->R2 < 40.0f)  L->R2 = 40.0f;  else if(L->R2 > 120.0f) L->R2 = 120.0f;
  if(lodScale > 0.0f){ L->R1 *= lodScale; L->R2 *= lodScale; }
}

float treeLodWLeaf(const TreeLod *L, float d){
  return 1.0f - treeLodSmoothstep(L->R1 * LOD_BAND_LO, L->R1 * LOD_BAND_HI, d);
}
float treeLodWBillboard(const TreeLod *L, float d){
  if(!L->bb) return 0.0f;
  return treeLodSmoothstep(L->R2 * LOD_BAND_LO, L->R2 * LOD_BAND_HI, d);
}
float treeLodWCard(const TreeLod *L, float d){
  float in  = treeLodSmoothstep(L->R1 * LOD_BAND_LO, L->R1 * LOD_BAND_HI, d);
  float out = L->bb ? (1.0f - treeLodSmoothstep(L->R2 * LOD_BAND_LO, L->R2 * LOD_BAND_HI, d)) : 1.0f;
  return in * out;
}
float treeLodWMesh(const TreeLod *L, float d){
  if(!L->bb) return 1.0f;
  return 1.0f - treeLodSmoothstep(L->R2 * LOD_BAND_LO, L->R2 * LOD_BAND_HI, d);
}

float treeLodLeafCut(const TreeLod *L){
  return L->R1 * LOD_MARGIN + LOD_BUILD_LAG;
}
float treeLodBranchCut(const TreeLod *L){
  return L->R2 * LOD_MARGIN + LOD_BUILD_LAG;
}
float treeLodMeshCut(const TreeLod *L){
  return L->R2 * LOD_MARGIN + LOD_BUILD_LAG;
}

int treeLodTier(const TreeLod *L, float d, int prev){
  const float R1 = L->R1, R2 = L->R2;
  if(prev == 3)      return (d < R1 * 1.05f) ? 3 : (d < R2 ? 2 : 0);
  else if(prev == 2) return (d < R1 * 0.95f) ? 3 : (d < R2 * 1.05f ? 2 : 0);
  else if(prev == 0) return (d < R2 * 0.95f) ? 2 : 0;
  else               return (d < R1) ? 3 : (d < R2 ? 2 : 0);
}
