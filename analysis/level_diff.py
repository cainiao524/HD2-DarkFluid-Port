"""Level resources, 2024 vs now: header words and where they differ; whether a unit (version 1 at +0x2C) is embedded.

  python -I level_diff.py <level hash16> [...]
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

sys.stdout.reconfigure(encoding="utf-8")
old, cur = hd.DataDir(r"F:\hd2_old\20240601\data"), hd.DataDir(r"F:\Game\A\HD2\HellForge\game\data")
L = hd.TYPE["level"]
for n in sys.argv[1:]:
    a, b = old.read(L, n), cur.read(L, n)
    print(f"== {n}: {len(a)} -> {len(b) if b else None}")
    print("   old head:", " ".join(f"{x:08x}" for x in struct.unpack_from("<16I", a, 0)))
    if b:
        print("   new head:", " ".join(f"{x:08x}" for x in struct.unpack_from("<16I", b, 0)))
    # embedded units: a version 1 at p with plausible unit header around p-0x2C
    for p in range(0x2C, len(a) - 4, 4):
        if struct.unpack_from("<I", a, p)[0] == 1 and b and len(a) == len(b) and struct.unpack_from("<I", b, p)[0] == 10800438:
            print(f"   embedded unit version at {p:#x} -> unit starts {p - 0x2C:#x}")
    if b and len(a) == len(b):
        d = [i for i in range(len(a)) if a[i] != b[i]]
        print("   differing bytes:", len(d), [hex(i) for i in d[:24]])
