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

void treeLodInitPx(TreeLod *L, float leafM, float cardM, float focalPx, float buildR, float lodScale){
  L->H = 0.0f; L->bb = 1;
  /* R1 = wo ein Einzelblatt LOD_LEAF_PX Bildpunkte fuellt, R2 = wo ein Buschel
   * LOD_CARD_PX fuellt. px = groesse * focalPx / d  ->  d = groesse * focalPx / px.
   * focalPx <= 0 (kein Fenster bekannt) -> sinnvoller Ersatzwert, damit nichts
   * verschwindet.
   *
   * buildR (Baukante des Waldes) ist die obere Grenze fuer R1 und die UNTERE fuer
   * R2. Nutzerbefund 2026-10-09: "dein jetziger stand zeichnet tier3 nun bis zu
   * so grossen entfernungen, dass tier2 nicht mehr vorkommt. darum verschwindet
   * die baumkrone einfach, weil du tier1 noch nicht gebaut hast."
   * Gemessen an einem Baum (Bildhoehe 360 px, Blatt 0,379 m): R1 = 115,98 m,
   * R2 = 320 m (Klemme) gegen eine Baukante von 180 m - die Karten-Blende lag bei
   * 104..128 m, dahinter gab es nichts mehr. Solange es keine FERNE-Stufe im
   * Puffer gibt, muessen die Karten bis zur Baukante durchhalten, also
   * R2 >= 1,15 x buildR. */
  if(focalPx > 0.0f && leafM > 0.0f) L->R1 = leafM * focalPx / LOD_LEAF_PX;
  else                               L->R1 = 30.0f;
  if(focalPx > 0.0f && cardM > 0.0f) L->R2 = cardM * focalPx / LOD_CARD_PX;
  else                               L->R2 = 3.0f * L->R1;
  if(buildR > 0.0f){
    if(L->R1 > 0.45f * buildR) L->R1 = 0.45f * buildR;   // Stufe 2 muss vorkommen
    if(L->R2 < 1.15f * buildR) L->R2 = 1.15f * buildR;   // Karten halten bis zur Kante
  }
  if(L->R1 < 8.0f)   L->R1 = 8.0f;
  if(L->R2 < 1.5f * L->R1) L->R2 = 1.5f * L->R1;
  if(lodScale > 0.0f){ L->R1 *= lodScale; L->R2 *= lodScale; }
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

/* Komplementaere Gewichte (Summe genau 1) — Voraussetzung fuer fizzle in den
 * Shadern (lod.glsl): "n < w" und "n >= 1-w" sind disjunkt und vollstaendig. */
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
float treeLodWBranch(const TreeLod *L, float rad, float d){
  float px = 2.0f * rad * L->focalPx / (d > 0.01f ? d : 0.01f);
  return treeLodSmoothstep(LOD_PX_LO, LOD_PX_HI, px);
}
float treeLodRadCut(const TreeLod *L, float rad){
  if(L->focalPx <= 0.0f || rad <= 0.0f) return LOD_NO_CUT;
  return 2.0f * rad * L->focalPx / LOD_PX_LO;
}
float treeLodBranchCut(const TreeLod *L){
  if(!L->bb) return LOD_NO_CUT;   // Geruest blendet ohne Kacheln nie aus
  return L->R2 * LOD_MARGIN + LOD_BUILD_LAG;
}
float treeLodMeshCut(const TreeLod *L){
  if(!L->bb) return LOD_NO_CUT;
  return L->R2 * LOD_MARGIN + LOD_BUILD_LAG;
}

int treeLodTier(const TreeLod *L, float d, int prev){
  const float R1 = L->R1, R2 = L->R2;
  if(prev == 3)      return (d < R1 * 1.05f) ? 3 : (d < R2 ? 2 : 0);
  else if(prev == 2) return (d < R1 * 0.95f) ? 3 : (d < R2 * 1.05f ? 2 : 0);
  else if(prev == 0) return (d < R2 * 0.95f) ? 2 : 0;
  else               return (d < R1) ? 3 : (d < R2 ? 2 : 0);
}
