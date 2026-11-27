"""Side-by-side u32/f32 dump of a unit's LOD group list in both builds.

  python -I lod_dump.py <name or hash16> [...]
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import unit_diff as ud

sys.stdout.reconfigure(encoding="utf-8")


def lod(h, data):
    off = h["LODGroupList"]
    nxt = sorted(v for n_, v in h.items() if n_ in ud.OFFS and v and v > off)
    return data[off:(nxt[0] if nxt else len(data))]


def fmt(w):
    f = struct.unpack("<f", struct.pack("<I", w))[0]
    return f"{w:08x}" + (f" {f:9.4g}" if 1e-6 < abs(f) < 1e7 else " " * 10)


for n in sys.argv[1:]:
    k = (hd.TYPE["unit"], n if hd.HEX16.match(n) else hd.h64(n))
    a, b = ud.old.read(*k), ud.cur.read(*k)
    la, lb = lod(ud.header(a), a), lod(ud.header(b), b)
    wa, wb = struct.unpack(f"<{len(la)//4}I", la), struct.unpack(f"<{len(lb)//4}I", lb)
    print(f"== {n}: old {len(la)} new {len(lb)}")
    for i in range(max(len(wa), len(wb))):
        x = fmt(wa[i]) if i < len(wa) else ""
        y = fmt(wb[i]) if i < len(wb) else ""
        print(f"  +{4*i:#05x}  {x:<20} {y:<20}{'' if x[:8] == y[:8] else '  *'}")
