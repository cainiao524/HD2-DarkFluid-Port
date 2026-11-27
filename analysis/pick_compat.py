"""Candidates for the old-format test: resources every bug mission loads, present in both builds with different bytes,
whose old main/stream/gpu data are all downloaded.

  python -I pick_compat.py
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

sys.stdout.reconfigure(encoding="utf-8")
R = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002"
names = hd.load_names([R + r"\filediver\hashes\hashes.txt", R + r"\filediver\hashes\cracked.txt"])
OLD, CUR = r"F:\hd2_old\20240601\data", r"F:\Game\A\HD2\HellForge\game\data"
old, cur = hd.DataDir(OLD), hd.DataDir(CUR)


def have_all(e):
    return all(e.sizes[p] == 0 or os.path.exists(os.path.join(OLD, e.archive + ext)) for p, ext in ((1, ".stream"), (2, ".gpu_resources")))


def show(label, typ, name):
    k = (hd.TYPE[typ], hd.h64(name) if "/" in name else name)
    o, c = old.index.get(k), cur.index.get(k)
    if not o or not c:
        print(f"{label}: {name} old {bool(o)} cur {bool(c)}"); return
    e = next((x for x in o if have_all(x)), None)
    same = [old.read(*k, p, e or o[0]) == cur.read(*k, p) for p in range(3)] if e else None
    print(f"{label}: {name} old sizes {o[0].sizes} cur {c[0].sizes} data downloaded {e is not None} same(main,stream,gpu) {same} archive {(e or o[0]).archive}")


# Extraction stamp levels (every bug mission has an extraction).
for i in range(1, 6):
    pkg = f"packages/generated/stamps/bug_extraction_0{i}"
    h = hd.h64(pkg)
    lv = [n for (t, n) in old.index if t == hd.TYPE["level"] and any(e.archive == h for e in old.index[(t, n)])]
    for n in lv:
        show(f"extraction {i} level", "level", n)
for u in ["content/env_bugs/assets/bug_rocks/bug_rock_medium_01", "content/env_bugs/assets/bug_rocks/bug_rock_wall_01",
          "content/env_bugs/assets/bug_rocks/bug_rock_flat_01", "content/env_bugs/assets/bug_rocks/bug_rock_cliff_face_01",
          "content/env_bugs/assets/bug_hive/spore_light_01_objective"]:
    show("unit", "unit", u)
