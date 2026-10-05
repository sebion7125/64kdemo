#!/usr/bin/env python3
# GLSL-Minify fuer gen_shaders.sh: entfernt Kommentare und Leerzeilen,
# verkleinert die eingebetteten Shader-Strings im Binary (64k-Budget).
# String-Literale werden respektiert (Zustandsmaschine).
import sys, re

s = sys.stdin.read()
out = []
i, n = 0, len(s)
in_str = False
while i < n:
    c = s[i]
    if in_str:
        out.append(c)
        if c == '\\' and i + 1 < n:
            out.append(s[i + 1])
            i += 2
            continue
        if c == '"':
            in_str = False
        i += 1
        continue
    if c == '"':
        in_str = True
        out.append(c)
        i += 1
        continue
    if c == '/' and i + 1 < n and s[i + 1] == '*':
        j = s.find('*/', i + 2)
        i = n if j < 0 else j + 2
        continue
    if c == '/' and i + 1 < n and s[i + 1] == '/':
        j = s.find('\n', i)
        i = n if j < 0 else j
        continue
    out.append(c)
    i += 1
t = ''.join(out)
lines = [l.strip() for l in t.splitlines()]
lines = [l for l in lines if l]
minified = '\n'.join(lines) + '\n'
if '--embed' in sys.argv:
    # 64k-Budget: Float-Literale kuerzen (1.0 -> 1., 0.0 -> .0),
    # gilt nur fuer das eingebettete Binary, .glsl bleibt lesbar.
    m = re.sub(r'\b(\d+)\.0\b', r'\1.', minified)
    m = re.sub(r'\b0\.0\b', '.0', m)
    m = m.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
    sys.stdout.write('"' + m + '";\n')
else:
    sys.stdout.write(minified)