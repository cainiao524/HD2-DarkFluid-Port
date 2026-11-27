"""Whether given text ids are in a build's localisation (stingray 'strings' resources: header, count, then u32 ids).

  python -I strings_check.py <data folder> <id> [...]
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

d = hd.DataDir(sys.argv[1])
ids = [int(x) for x in sys.argv[2:]]
pats = {i: struct.pack("<I", i) for i in ids}
found = {i: 0 for i in ids}
n = 0
for (t, name), es in d.index.items():
    if t != hd.TYPE["strings"]:
        continue
    data = d.read(t, name)
    if not data:
        continue
    n += 1
    for i, p in pats.items():
        if p in data:
            found[i] += 1
print(f"{sys.argv[1]}: {n} strings resources")
for i in ids:
    print(f"  {i}: in {found[i]} strings resources")
