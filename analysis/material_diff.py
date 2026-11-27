"""Material resources, 2024 (0x102) vs now (0x120): header words of a few size-changed pairs, to tell layout changes from
re-authored parameters. Also: which materials the Dark Fluid stamps need, and whether their parent template exists now.

  python -I material_diff.py
"""
import collections, json, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

sys.stdout.reconfigure(encoding="utf-8")
old, cur = hd.DataDir(r"F:\hd2_old\20240601\data"), hd.DataDir(r"F:\Game\A\HD2\HellForge\game\data")
M = hd.TYPE["material"]
shown = collections.Counter()
for (t, n) in list(old.index):
    if t != M or (t, n) not in cur.index:
        continue
    a, b = old.read(t, n), cur.read(t, n)
    if not a or not b or len(a) == len(b):
        continue
    d = len(b) - len(a)
    if shown[d] >= 1 or len(shown) >= 4:
        continue
    shown[d] += 1
    print(f"== {n}: {len(a)} -> {len(b)}")
    print("   old", " ".join(f"{x:08x}" for x in struct.unpack_from("<20I", a, 0)))
    print("   new", " ".join(f"{x:08x}" for x in struct.unpack_from("<20I", b, 0)))
need = json.load(open(r"F:\hd2_old\20240601-diff\missing.json", encoding="utf-8"))
print("\nmissing materials of the Dark Fluid set:")
for r in need:
    if r["type"] != "material":
        continue
    a = old.read(M, r["name_hash"])
    parent = struct.unpack_from("<Q", a, 0x08)[0] if a else None
    print(f"   {r['name_hash']} size {len(a)} head {struct.unpack_from('<I', a, 0)[0]:#x} word2-3 {parent:016x} "
          f"parent-as-material now: {(M, f'{parent:016x}') in cur.index}")
