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

typedef struct {
  float H;         // Wuchshoehe des Baums
  float R1;        // Einzelblaetter -> Karten
  float R2;        // Karten/Geruest -> Ganzbaum-Darstellung
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
float treeLodBranchCut(const TreeLod *L);    // Aeste im Geruest
float treeLodMeshCut(const TreeLod *L);      // feines Geruest-Netz

/* Detailstufe. prev = Stufe des letzten Builds (-1 = unbekannt); die
 * Rueckstellung erfolgt frueher als die Hochstellung (Hysterese), damit ein
 * Baum an der Grenze nicht zwischen zwei Builds hin- und herspringt. */
int   treeLodTier(const TreeLod *L, float d, int prev);

#endif /* TREE_LOD_H */
