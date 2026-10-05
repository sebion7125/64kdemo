#!/usr/bin/env python3
# GLSL-Minify fuer gen_shaders.sh: entfernt Kommentare und Leerzeilen,
# verkleinert die eingebetteten Shader-Strings im Binary (64k-Budget).
# String-Literale werden respektiert (Zustandsmaschine).
import sys

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
sys.stdout.write('\n'.join(lines) + '\n')