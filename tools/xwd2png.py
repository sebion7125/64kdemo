#!/usr/bin/env python3
# Minimaler XWD -> PNG Konverter (nur stdlib), fuer 32-bit TrueColor
import sys, struct, zlib

def read32(b, off, bo):
    return struct.unpack_from(bo + 'I', b, off)[0]

def main():
    data = open(sys.argv[1], 'rb').read()
    out = sys.argv[2]
    magic = struct.unpack_from('<I', data, 0)[0]
    assert magic == 0x00494200, "kein XWD"
    bo = '<'  # x86 Header
    # xwd-Header: magic@0 w@4 h@8 x@12 y@16 bpp@20 fmt@24 depth@28
    #   pixbo@32 bpl@36 unit@40 bitord@44 pad@48 pad@52 databo@56
    #   visual@60 rm@64 gm@68 bm@72 bpp2@76  |  Pixel ab @80
    w = read32(data, 4, bo)
    h = read32(data, 8, bo)
    bytes_per_line = read32(data, 40, bo)
    data_bo = read32(data, 56, bo)
    visual = read32(data, 60, bo)
    rm = read32(data, 64, bo)
    gm = read32(data, 68, bo)
    bm = read32(data, 72, bo)
    bpp = read32(data, 76, bo)
    off = 80
    bo2 = '<' if data_bo == 0x0123 else '>'
    def shift_of(mask):
        s = 0
        while not (mask >> s) & 1:
            s += 1
        return s
    rs, gs, bs = shift_of(rm), shift_of(gm), shift_of(bm)
    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            v = struct.unpack_from(bo2 + 'I', data, off + (x * bpp))[0]
            row += bytes(((v >> rs) & 0xff, (v >> gs) & 0xff, (v >> bs) & 0xff))
        rows.append(bytes(row))
    raw = b''.join(b'\x00' + r for r in rows)
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 6))
    png += chunk(b'IEND', b'')
    open(out, 'wb').write(png)
    print(f"{w}x{h} -> {out}")

if __name__ == '__main__':
    main()