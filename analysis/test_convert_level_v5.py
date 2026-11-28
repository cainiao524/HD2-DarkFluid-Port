"""Scores convert_level against every level both builds have.

  python -I test_convert_level.py
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import convert_level as cl

sys.stdout.reconfigure(encoding="utf-8")
old, cur = hd.DataDir(os.environ.get("HD2_OLD_DATA", r"F:\Game\A\HD2\A\0\hd2_old\20240601\data")),hd.DataDir(r"F:\Game\A\HD2\HellForgev5\game\data")
L = hd.TYPE["level"]
stats, errs = collections.Counter(), collections.Counter()
diffs = []
for (t, n) in list(old.index):
    if t != L or (t, n) not in cur.index:
        continue
    a, b = old.read(t, n), cur.read(t, n)
    try:
        c = cl.convert(a)
    except Exception as ex:
        stats["error"] += 1; errs[str(ex)[:50]] += 1; continue
    if c == b:
        stats["exact"] += 1
    elif len(c) == len(b):
        stats["same size, content differs"] += 1
        diffs.append(sum(1 for i in range(len(c)) if c[i] != b[i]))
    else:
        stats["size differs"] += 1
print(dict(stats), dict(errs))
if diffs:
    diffs.sort()
    print("differing bytes in same-size results: median", diffs[len(diffs) // 2], "max", diffs[-1])
