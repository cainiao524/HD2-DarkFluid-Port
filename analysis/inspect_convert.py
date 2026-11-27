"""For one unit: section sizes of converted-old vs current, and the LOD groups' layout, to find the rule a case misses.

  python -I inspect_convert.py <hash16 or name>
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import convert_unit as cu

sys.stdout.reconfigure(encoding="utf-8")
old, cur = hd.DataDir(r"F:\hd2_old\20240601\data"), hd.DataDir(r"F:\Game\A\HD2\HellForge\game\data")
n = sys.argv[1]
k = (hd.TYPE["unit"], n if hd.HEX16.match(n) else hd.h64(n))
a, b = old.read(*k), cur.read(*k)
ha, hb = cu.read_header(a), cu.read_header(b)


def spans(h, total):
    secs = sorted((v, k_) for k_, v in h.items() if k_ in cu.OFFSET_FIELDS and v)
    return {k_: (v, (secs[i + 1][0] if i + 1 < len(secs) else total) - v) for i, (v, k_) in enumerate(secs)}


sa, sb = spans(ha, len(a)), spans(hb, len(b))
print(f"main {len(a)} -> {len(b)}; gpu same: {old.read(*k, 2) == cur.read(*k, 2)}")
for name in cu.OFFSET_FIELDS:
    if name in sa or name in sb:
        print(f"  {name:20s} old {sa.get(name)}  new {sb.get(name)}")
s = ha["LODGroupList"]
count = struct.unpack_from("<I", a, s)[0]
print("old LOD groups:", count)
for i in range(count):
    g = struct.unpack_from("<I", a, s + 4 + 4 * i)[0]
    flags, name, z, one, nn = struct.unpack_from("<IIIII", a, s + g)
    offs = struct.unpack_from(f"<{nn + 1}I", a, s + g + 20)
    nxt = struct.unpack_from("<I", a, s + 4 + 4 * (i + 1))[0] if i + 1 < count else sa["LODGroupList"][1]
    print(f"  group {i} at {g:#x} flags {flags:#x} name {name:08x} {z} {one} n {nn} offs {[hex(x) for x in offs]} next {nxt:#x} "
          f"-> len {nxt - g:#x}, expected {offs[nn] + 40:#x}")
