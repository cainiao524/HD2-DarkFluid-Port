"""Scores convert_unit against every unit both builds have whose GPU data did not change (same mesh): converted old
bytes vs the current bytes, ignoring the LOD thresholds (re-tuned by the artists). Reports exact matches, same-size
results and which sections still differ.

  python -I test_convert_unit.py [limit]
"""
import collections, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import convert_unit as cu

sys.stdout.reconfigure(encoding="utf-8")
OLD = os.environ.get("HD2_OLD_DATA", r"F:\Game\A\HD2\A\0\hd2_old\20240601\data")  # e.g. ...\hd2_old\1001104\data
CUR = r"F:\Game\A\HD2\HellForgev5\game\data"
old, cur = hd.DataDir(OLD), hd.DataDir(CUR)
U = hd.TYPE["unit"]
limit = int(sys.argv[1]) if len(sys.argv) > 1 else 400


def downloaded(e):
    return e.sizes[2] == 0 or os.path.exists(os.path.join(OLD, e.archive + ".gpu_resources"))


def mask_lod(b):
    """Zero the LOD thresholds and flags so re-tuned values do not count."""
    b = bytearray(b)
    h = cu.read_header(b)
    s = h["LODGroupList"]
    if not s:
        return bytes(b)
    count = struct.unpack_from("<I", b, s)[0]
    for i in range(count):
        g = s + struct.unpack_from("<I", b, s + 4 + 4 * i)[0]
        n = struct.unpack_from("<I", b, g + 16)[0]
        offs = struct.unpack_from(f"<{n + 1}I", b, g + 20)
        for e in range(n):
            b[g + offs[e]:g + offs[e] + 16] = b"\0" * 16
    return bytes(b)


stats = collections.Counter()
section_diffs = collections.Counter()
errors = collections.Counter()
examples = {}
for (t, n), es in old.index.items():
    if t != U or (t, n) not in cur.index or stats["tried"] >= limit:
        continue
    e = next((x for x in es if downloaded(x)), None)
    if e is None:
        continue
    a = old.read(t, n, 0, e)
    if not a or len(a) < 0x80 or struct.unpack_from("<I", a, 0x2C)[0] != 1:
        continue
    if e.sizes[2] and old.read(t, n, 2, e) != cur.read(t, n, 2):
        continue
    stats["tried"] += 1
    b = cur.read(t, n)
    try:
        c = cu.convert(a)
    except Exception as ex:
        errors[str(ex)[:60]] += 1
        stats["error"] += 1
        continue
    if len(c) != len(b):
        stats["size differs"] += 1
        examples.setdefault("size", n)
        continue
    stats["same size"] += 1
    if mask_lod(c) == mask_lod(b):
        stats["exact (LOD thresholds aside)"] += 1
        continue
    hb = cu.read_header(b)
    secs = sorted((v, k) for k, v in hb.items() if k in cu.OFFSET_FIELDS and v)
    mc, mb = mask_lod(c), mask_lod(b)
    if mc[:0x80] != mb[:0x80]:
        section_diffs["header"] += 1
    for i, (off, name) in enumerate(secs):
        end = secs[i + 1][0] if i + 1 < len(secs) else len(b)
        if mc[off:end] != mb[off:end]:
            section_diffs[name] += 1
            examples.setdefault(name, n)
print(dict(stats))
print("errors:", dict(errors))
print("sections still differing:", dict(section_diffs))
print("examples:", examples)
