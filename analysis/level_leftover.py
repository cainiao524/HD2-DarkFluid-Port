"""For same-size level results that still differ: which header section each differing word falls in, and value pairs.

  python -I level_leftover.py
"""
import collections, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import convert_level as cl

sys.stdout.reconfigure(encoding="utf-8")
old, cur = hd.DataDir(r"F:\hd2_old\20240601\data"), hd.DataDir(r"F:\Game\A\HD2\HellForge\game\data")
L = hd.TYPE["level"]
where, pairs, shown = collections.Counter(), collections.defaultdict(collections.Counter), 0
for (t, n) in list(old.index):
    if t != L or (t, n) not in cur.index:
        continue
    a, b = old.read(t, n), cur.read(t, n)
    c = cl.convert(a)
    if c == b or len(c) != len(b):
        continue
    words = struct.unpack_from("<16I", b, 0)
    bounds = sorted(set(w for w in words[3:] if 0 < w < len(b)))
    for i in range(0, len(c) - 3, 4):
        x, y = struct.unpack_from("<I", c, i)[0], struct.unpack_from("<I", b, i)[0]
        if x == y:
            continue
        sec = max([j for j in range(16) if 3 <= j and 0 < words[j] <= i] or [0], key=lambda j: words[j])
        where[sec] += 1
        pairs[sec][(x, y)] += 1
    if shown < 3:
        d = [i for i in range(0, len(c), 4) if c[i:i + 4] != b[i:i + 4]]
        print(n, "header", " ".join(f"{w:x}" for w in words[:16]), "diff words at", [hex(i) for i in d[:8]])
        shown += 1
for sec, cnt in where.most_common():
    print(f"section after header word {sec}: {cnt} words;", {f'{k[0]:#x}->{k[1]:#x}': v for k, v in pairs[sec].most_common(5)})
