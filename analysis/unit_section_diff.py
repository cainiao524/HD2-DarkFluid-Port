"""Byte-level diff of the unit sections that change between version 1 and 10800438, as u32 words.

  python -I unit_section_diff.py <name or hash16> <section> [...]
Sections: LightList, LODGroupList, StreamInfo, MeshInfo, CustomizationInfo, Ending.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
from unit_diff import header, OFFS  # noqa: E402  (reuses its loaded DataDirs)
import unit_diff as ud

sys.stdout.reconfigure(encoding="utf-8")
name, sections = sys.argv[1], sys.argv[2:]
k = (hd.TYPE["unit"], name if hd.HEX16.match(name) else hd.h64(name))
a, b = ud.old.read(*k), ud.cur.read(*k)
ha, hb = header(a), header(b)


def span(h, data, nm):
    off = h[nm]
    nxt = sorted(v for n_, v in h.items() if n_ in OFFS and v and v > off)
    return off, (nxt[0] if nxt else len(data))


for nm in sections:
    oa, ea = span(ha, a, nm)
    ob, eb = span(hb, b, nm)
    sa, sb = a[oa:ea], b[ob:eb]
    print(f"== {nm}: old {len(sa)} B at {oa:#x}, new {len(sb)} B at {ob:#x}")
    wa = struct.unpack_from(f"<{len(sa)//4}I", sa)
    wb = struct.unpack_from(f"<{len(sb)//4}I", sb)
    for i in range(max(len(wa), len(wb))):
        x = wa[i] if i < len(wa) else None
        y = wb[i] if i < len(wb) else None
        if x != y:
            print(f"   +{4*i:#06x}  old {'' if x is None else f'{x:#010x}':>10}  new {'' if y is None else f'{y:#010x}':>10}")
