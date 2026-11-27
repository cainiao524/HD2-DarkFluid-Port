"""Round trip: every resource of existing mod patches is read with hd2data and written back with patchwriter; the
copies must read back identical (type, name, main, stream, gpu), and the header tail is kept.

  python -I test_patchwriter.py <out folder> <patch file> [...]
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import patchwriter as pw

out = sys.argv[1]
fails = 0
for src in sys.argv[2:]:
    d = hd.DataDir(os.path.dirname(src), files=[src])
    res = [(t, n, d.read(t, n, 0), d.read(t, n, 1), d.read(t, n, 2)) for (t, n) in d.index]
    dst = os.path.join(out, os.path.basename(src))
    pw.write_patch(dst, res, pw.header_tail(src))
    back = hd.DataDir(out, files=[dst])
    bad = [(t, n) for t, n, m, s, g in res if (back.read(t, n, 0), back.read(t, n, 1), back.read(t, n, 2)) != (m, s, g)]
    same_tail = pw.header_tail(src) == pw.header_tail(dst)
    sizes = [os.path.getsize(src + e) if os.path.exists(src + e) else 0 for e in ("", ".stream", ".gpu_resources")]
    nsizes = [os.path.getsize(dst + e) for e in ("", ".stream", ".gpu_resources")]
    print(f"{os.path.basename(os.path.dirname(src))}/{os.path.basename(src)}: {len(res)} resources, "
          f"{len(bad)} differ, header tail kept {same_tail}, sizes {sizes} -> {nsizes}")
    fails += len(bad) + (not same_tail)
print("PASSED" if fails == 0 else f"FAILED ({fails})")
sys.exit(1 if fails else 0)
