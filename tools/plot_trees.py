#!/usr/bin/env python3
"""2D-Plot der Baumpositionen aus /tmp/trees.txt (debug_trees.c).

Nutzung:  python3 tools/plot_trees.py [input] [output]
Default:  /tmp/trees.txt -> /tmp/trees_plot.png
"""
import sys
import re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

inp = sys.argv[1] if len(sys.argv) > 1 else "/tmp/trees.txt"
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/trees_plot.png"

pat = re.compile(r"^\s*(-?\d+\.\d+)\s+(-?\d+\.\d+)")
pts = []
with open(inp) as f:
    for line in f:
        m = pat.match(line)
        if m:
            pts.append((float(m.group(1)), float(m.group(2))))

xs = [p[0] for p in pts]
zs = [p[1] for p in pts]
G = 9.0  # Zellgröße (fragment.glsl)

fig, axes = plt.subplots(2, 2, figsize=(14, 12))

# 1) Scatter + Zellgitter
ax = axes[0][0]
ax.scatter(xs, zs, s=8, c="darkgreen", alpha=0.7)
lo = -15 * G - G / 2
hi = 15 * G + G / 2
gi = int(G)
for v in range(int(lo // G) * gi, int(hi // G) * gi + 1, gi):
    ax.axvline(v, color="lightgray", lw=0.4, zorder=0)
    ax.axhline(v, color="lightgray", lw=0.4, zorder=0)
ax.set_aspect("equal")
ax.set_title(f"Baumpositionen (n={len(pts)}), Gitter = {G:g}m-Zellen")
ax.set_xlabel("x")
ax.set_ylabel("z")

# 2) Nur Punkte in Zell-Mitte-Region (Offset-Bereich pro Zelle)
ax = axes[0][1]
ax.scatter(xs, zs, s=8, c="darkgreen", alpha=0.7)
ax.set_aspect("equal")
ax.set_xlim(-40, 40)
ax.set_ylim(-40, 40)
ax.set_title("Zoom auf Zentrumsbereich")
ax.set_xlabel("x")
ax.set_ylabel("z")

# 3) Histogramm x
ax = axes[1][0]
ax.hist(xs, bins=80, color="steelblue")
ax.set_title("Verteilung x")
ax.set_xlabel("x")

# 4) Histogramm z
ax = axes[1][1]
ax.hist(zs, bins=80, color="steelblue")
ax.set_title("Verteilung z")
ax.set_xlabel("z")

fig.suptitle("64kDemo: 2D-Baumpositionen (aus /tmp/trees.txt)")
fig.tight_layout()
fig.savefig(out, dpi=120)
print(f"gespeichert: {out}  ({len(pts)} Bäume)")