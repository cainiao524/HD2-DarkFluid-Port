import json, sys, collections
sys.argv = [sys.argv[0]] + sys.argv[1:]
from query_index import mm, names, types, idx, present  # noqa
pk = ["packages/content/bug_tcs_01", "packages/content/bug_tcs_02", "packages/content/bug_tcs_03_backpack",
      "packages/content/bug_tcs_03_thumper", "packages/content/bug_tcs_04", "packages/generated/loadout/tcs_03_thumper",
      "packages/content/black_hole", "packages/content/gen_prospecting_drill", "packages/content/gen_icbm"]
for p in pk:
    h = f"{mm(p.encode()):016x}"
    files = idx.get(h)
    print("\n==", p, h, "archive" if files else "NO ARCHIVE", len(files or []))
    if files:
        c = collections.Counter(types.get(t, t) for t, n, s in files)
        print("  ", dict(c))
        for t, n, s in files:
            if n in names and types.get(t) in ("unit", "level", "package", "prefab", "entity"):
                print("    ", types.get(t), names[n])
        print("   unnamed units:", [n for t, n, s in files if types.get(t) == "unit" and n not in names][:12])
for a in ("91f45a686207a351", "d87885c7eaace597"):
    files = idx[a]
    print("\n== archive", a, [k for k, v in names.items() if k == a], len(files))
    for t, n, s in files:
        if types.get(t) in ("unit", "level", "package", "prefab", "entity"):
            print("    ", types.get(t), names.get(n, n))
st = sorted(names[n] for (t, n) in present if n in names and names[n].startswith("packages/generated/stamps/bug_"))
import re
fam = collections.Counter(re.sub(r"_\d+(/.*)?$", "", s.split("/")[3]) for s in st)
print("\nbug stamp families:", dict(fam))
