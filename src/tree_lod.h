/* tree_lod.h — kanonischer LOD-Kern der Baeume (Strategie B).
 *
 * Alles, was aus der Entfernung zu einem Baum eine Entscheidung macht
 * (Schaltweiten, Schichtgewichte, Existenz-Schnitte, Detailstufe), lebt hier als
 * REINE Funktion: kein GL, keine Zeit, keine globalen Puffer. Die Engine ruft
 * dieselben Funktionen, und tools/test_tree_lod.c faehrt synthetische
 * Kamerapfade und prueft die Invarianten in Millisekunden — ohne Engine, ohne
 * Screenshot.
 *
 * Die GLSL-Seite ist src/shaders/lod.glsl; sie wird von leaf.vert, crown.vert
 * und tree.vert per #include eingebunden, damit nicht drei Shader drei
 * verschiedene Baelen bekommen (das war die Ursache der Spruenge). Der
 * Wirtstest prueft zusaetzlich, dass beide Seiten dieselben Bandzahlen
 * benutzen.
 */
#ifndef TREE_LOD_H
#define TREE_LOD_H

/* Alle Baelen laufen relativ zum Schaltradius R, und zwar EXAKT diese Breite.
 * Eine Schicht, die ausblaendet, und die Nachfolgerin, die einblaendet, teilen
 * sich dasselbe Band: Gewichte sind komplementaer, Summe immer 1. */
#define LOD_BAND_LO 0.90f
#define LOD_BAND_HI 1.10f

/* Nachlauf des Builds: ein Rebuild erfolgt erst ab 12 m Kameraweg, und die
 * Uploads laufen ueber mehrere Frames. Existenz-Schnitte muessen diesen
 * Nachlauf plus das aeusserste Bandende abdecken. */
#define LOD_BUILD_LAG 20.0f

/* Rand ueber das aeusserste Bandende hinaus, damit Geometrie nie innerhalb
 * eines Blendenbands verschwindet. */
#define LOD_MARGIN 1.15f

/* Kein Schnitt: die Geometrie reicht bis an die Bau-Grenze. */
#define LOD_NO_CUT 1e9f

/* Ast-Aufloesung nach BILDGROESSE statt nach Entfernung.
 * Ein Ast mit Radius r hat die Dicke 2r; projiziert sind das
 *   px = 2*r*focalPx/d
 * Pixel. focalPx = vertikale Brennweite in Pixeln (hier 1,7 * Bildhoehe, weil
 * build_mvp mit fy = 3.4 = 2*1,7 NDC rechnet). Unter LOD_PX_LO ist der Ast
 * subpixelig, ueber LOD_PX_HI voll aufloesbar. Das Band ist damit von selbst
 * breit (r = 1 cm bei 1080p: 37 m bis 98 m) — gleichmaessigeres, langsameres
 * Ausblenden ohne geratene Bandkonstanten. */
#define LOD_PX_LO 0.6f
#define LOD_PX_HI 1.6f

typedef struct {
  float H;         // Wuchshoehe des Baums
  float R1;        // Einzelblaetter -> Karten
  float R2;        // Karten/Geruest -> Ganzbaum-Darstellung
  int   bb;        // 1 = Ganzbaum-Kacheln liegen im Puffer (treeBillboard0).
                   // Ohne Kacheln duerfen Karten und Geruest bei R2 NICHT
                   // ausblenden — sonst hat ein ferner Baum keine Darstellung.
                   // Das ist keine Shader-Spezialitaet: ohne dieses Feld
                   // rechnete der Ein-Baum-Test "Billboard 1,000 / Karten 0,000"
                   // fuer einen Baum, der im Bild sehr wohl stand.
  float focalPx;  // Brennweite in Pixeln (1,7 * Bildhoehe) — Ast-LOD nach Bildgroesse
} TreeLod;

/* Schaltweiten aus der Wuchshoehe; lodScale != 0 ist die Messbank
 * (DEMO_LODRSCALE), die beide Radien gleichermassen skaliert. */
void  treeLodInit(TreeLod *L, float H, float lodScale);

/* GLSL-kompatibles smoothstep (a<b; a==b -> harte Kante). */
float treeLodSmoothstep(float a, float b, float x);

/* Schichtgewichte in Abhaengigkeit vom kanonischen Abstand d
 * (waagerechter Abstand Kamera -> Lotpunkt des Baums). */
float treeLodWLeaf(const TreeLod *L, float d);        // Einzelblaetter
float treeLodWCard(const TreeLod *L, float d);        // Kronen-Karten
float treeLodWBillboard(const TreeLod *L, float d);   // Ganzbaum-Kachel
float treeLodWMesh(const TreeLod *L, float d);        // Geruest (Stamm/Aeste)

/* Existenz-Schnitte des Builds: bis hierhin muss die Geometrie im Puffer sein. */
float treeLodLeafCut(const TreeLod *L);      // Einzelblatt-Instanzen
/* Aeste / feines Netz. Regel: ein Schnitt darf NIE in einem Bereich liegen,
 * in dem die Schicht Gewicht hat — sonst verschwindet (oder erscheint)
 * Geometrie sichtbar, und beim naechsten Build wird das zu einem Sprung mit der
 * Periode des Rebuild-Abstands (im Nutzer-Log gemessen: alle 17,0 m ein Bau,
 * dabei Geruest 45 <-> 10 164 Vertices, Ueberblendung mischt zwei verschiedene
 * Gerueste). Ohne Ganzbaum-Kacheln blendet das Geruest nie aus (Gewicht 1 bis
 * zur Bau-Grenze) -> dann darf es gar nicht geschnitten werden. */
/* Gewicht eines Asts mit Radius rad bei raumlicher Entfernung d:
 * 0 = subpixelig, 1 = aufloesbar. lod.glsl hat dasselbe (lodBranchW). */
float treeLodWBranch(const TreeLod *L, float rad, float d);
/* Bis hierhin hat ein Ast dieses Radius noch Bildgroesse > LOD_PX_LO. JENSEITS
 * ist sein Gewicht 0, er darf also im Puffer fehlen — die Existenz-Regel faellt
 * damit mit der Sichtbarkeitsregel zusammen, und ein Satz-Wechsel kann nichts
 * mehr sichtbar veraendern. */
float treeLodRadCut(const TreeLod *L, float rad);
float treeLodBranchCut(const TreeLod *L);
float treeLodMeshCut(const TreeLod *L);

/* Detailstufe. prev = Stufe des letzten Builds (-1 = unbekannt); die
 * Rueckstellung erfolgt frueher als die Hochstellung (Hysterese), damit ein
 * Baum an der Grenze nicht zwischen zwei Builds hin- und herspringt. */
int   treeLodTier(const TreeLod *L, float d, int prev);

#endif /* TREE_LOD_H */
