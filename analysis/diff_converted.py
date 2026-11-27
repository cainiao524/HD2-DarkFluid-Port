"""Word diff of one section: converted 2024 unit vs today's unit.

  python -I diff_converted.py <hash16> <section> [max lines]
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import convert_unit as cu

sys.stdout.reconfigure(encoding="utf-8")
old, cur = hd.DataDir(r"F:\hd2_old\20240601\data"), hd.DataDir(r"F:\Game\A\HD2\HellForge\game\data")
n, sec = sys.argv[1], sys.argv[2]
limit = int(sys.argv[3]) if len(sys.argv) > 3 else 24
k = (hd.TYPE["unit"], n)
c, b = cu.convert(old.read(*k)), cur.read(*k)
hc, hb = cu.read_header(c), cu.read_header(b)


def span(h, d):
    off = h[sec]
    later = sorted(v for k_, v in h.items() if k_ in cu.OFFSET_FIELDS and v and v > off)
    return d[off:(later[0] if later else len(d))]


x, y = span(hc, c), span(hb, b)
print(f"{sec}: converted {len(x)} B, current {len(y)} B")
wx, wy = struct.unpack(f"<{len(x)//4}I", x[:len(x)//4*4]), struct.unpack(f"<{len(y)//4}I", y[:len(y)//4*4])
shown = 0
for i in range(max(len(wx), len(wy))):
    p, q = (wx[i] if i < len(wx) else None), (wy[i] if i < len(wy) else None)
    if p != q:
        fp = struct.unpack("<f", struct.pack("<I", p))[0] if p is not None else None
        fq = struct.unpack("<f", struct.pack("<I", q))[0] if q is not None else None
        print(f"  +{4*i:#06x}  {p if p is None else f'{p:#010x}'} ({fp:.4g})  {q if q is None else f'{q:#010x}'} ({fq:.4g})"
              if fp is not None and fq is not None else f"  +{4*i:#06x}  {p}  {q}")
        shown += 1
        if shown >= limit:
            break
