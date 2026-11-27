"""Across unit pairs with unchanged meshes: which MeshInfo words differ after conversion, by position within each mesh
record, and the old -> new value pairs seen there. Also the LOD group flag pairs.

  python -I meshinfo_map.py [limit]
"""
import collections, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import convert_unit as cu

sys.stdout.reconfigure(encoding="utf-8")
OLD, CUR = r"F:\hd2_old\20240601\data", r"F:\Game\A\HD2\HellForge\game\data"
old, cur = hd.DataDir(OLD), hd.DataDir(CUR)
U = hd.TYPE["unit"]
limit = int(sys.argv[1]) if len(sys.argv) > 1 else 3000


def span(h, d, sec):
    off = h[sec]
    later = sorted(v for k_, v in h.items() if k_ in cu.OFFSET_FIELDS and v and v > off)
    return off, d[off:(later[0] if later else len(d))]


pos = collections.Counter()
pairs = collections.defaultdict(collections.Counter)
lod_flags = collections.Counter()
n_units = 0
for (t, n), es in old.index.items():
    if t != U or (t, n) not in cur.index or n_units >= limit:
        continue
    e = es[0]
    if e.sizes[2] and not os.path.exists(os.path.join(OLD, e.archive + ".gpu_resources")):
        continue
    a = old.read(t, n, 0, e)
    if not a or len(a) < 0x80 or struct.unpack_from("<I", a, 0x2C)[0] != 1:
        continue
    if e.sizes[2] and old.read(t, n, 2, e) != cur.read(t, n, 2):
        continue
    b = cur.read(t, n)
    try:
        c = cu.convert(a)
    except Exception:
        continue
    n_units += 1
    hc, hb = cu.read_header(c), cu.read_header(b)
    # LOD flags, group by group
    ha = cu.read_header(a)
    for h, d, tag in ((ha, a, "old"), (hb, b, "new")):
        pass
    sa, la = span(ha, a, "LODGroupList")
    sb, lb = span(hb, b, "LODGroupList")
    ca, cb = struct.unpack_from("<I", la, 0)[0], struct.unpack_from("<I", lb, 0)[0]
    if ca == cb:
        for i in range(ca):
            ga = struct.unpack_from("<I", la, 4 + 4 * i)[0]
            gb = struct.unpack_from("<I", lb, 4 + 4 * i)[0]
            lod_flags[(struct.unpack_from("<I", la, ga)[0], struct.unpack_from("<I", lb, gb)[0])] += 1
    if not hc["MeshInfo"] or not hb["MeshInfo"]:
        continue
    oc, mc = span(hc, c, "MeshInfo")
    ob, mb = span(hb, b, "MeshInfo")
    if len(mc) != len(mb) or mc == mb:
        continue
    count = struct.unpack_from("<I", mb, 0)[0]
    offs = struct.unpack_from(f"<{count}I", mb, 4)
    for i in range(0, len(mc) - 3, 4):
        x, y = struct.unpack_from("<I", mc, i)[0], struct.unpack_from("<I", mb, i)[0]
        if x != y:
            rec = max([o for o in offs if o <= i], default=0)
            rel = i - rec
            pos[rel] += 1
            pairs[rel][(x, y)] += 1
print("units compared:", n_units)
print("LOD flags old->new:", {f"{k[0]:#x}->{k[1]:#x}": v for k, v in lod_flags.most_common(12)})
for rel, cnt in pos.most_common(8):
    print(f"mesh record +{rel:#x}: {cnt} differences;", {f"{k[0]:#x}->{k[1]:#x}": v for k, v in pairs[rel].most_common(8)})
