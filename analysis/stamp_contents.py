"""What each stamp level places: units named in its level resource, flagged when the current build lacks them.

  python -I stamp_contents.py <old data> <current index json> <archive> [<archive> ...]
"""
import collections, json, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

R = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002"
names = hd.load_names([R + r"\filediver\hashes\hashes.txt", R + r"\filediver\hashes\cracked.txt",
                       R + r"\HD2SDK-CommunityEdition\hashlists\friendlynames.txt"])
old = hd.DataDir(sys.argv[1])
cur = json.load(open(sys.argv[2]))
curkeys = {(t, n) for fs in cur.values() for t, n, s in fs}
byar = collections.defaultdict(list)
for k, es in old.index.items():
    for e in es:
        byar[e.archive].append(k)
L, U = hd.TYPE["level"], hd.TYPE["unit"]
oldunits = {n for (t, n) in old.index if t == U}
for ar in sys.argv[3:]:
    lv = [k for k in byar[ar] if k[0] == L]
    if not lv:
        print("==", ar, "no level"); continue
    data = old.read(*lv[0])
    refs = collections.Counter()
    for off in range(0, len(data) - 7, 4):
        v = f"{struct.unpack_from('<Q', data, off)[0]:016x}"
        if v in oldunits:
            refs[v] += 1
    print(f"== {ar} level {lv[0][1]} size {len(data)} distinct units {len(refs)}")
    for n, c in refs.most_common():
        nm = names.get(n, n)
        if "objective" in nm or "marker" in nm or n not in names or (U, n) not in curkeys:
            print(f"    x{c:<3} {'MISSING ' if (U, n) not in curkeys else '        '}{nm}")
