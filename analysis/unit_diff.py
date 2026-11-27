"""Unit header and section layout, version 1 (2024) against 10800438 (now), for units present in both builds.
Header fields after HD2SDK-CommunityEdition stingray/unit.py (StingrayMeshFile.Serialize).

  python -I unit_diff.py [name or hash16 ...]     (default: a few units whose GPU data did not change)
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

sys.stdout.reconfigure(encoding="utf-8")
FIELDS = ["UnkRef1:Q", "BonesRef:Q", "CompositeRef:Q", "UnkRef2:Q", "StateMachineRef:Q", "HeaderData1:I", "Version:I",
          "LODGroupList:I", "TransformInfo:I", "LightList:I", "UnkPreLightList:I", "WwiseCallback:I", "HeaderData2:8s",
          "CustomizationInfo:I", "UnkHeader1:I", "ConnectingBoneHash:I", "BoneInfo:I", "StreamInfo:I", "Ending:I",
          "MeshInfo:I", "HeaderUnk:Q", "Materials:I"]


def header(b):
    out, o = {}, 0
    for f in FIELDS:
        name, fmt = f.split(":")
        v = struct.unpack_from("<" + fmt, b, o)[0]
        o += struct.calcsize("<" + fmt)
        out[name] = v
    return out


OLD, CUR = r"F:\hd2_old\20240601\data", r"F:\Game\A\HD2\HellForge\game\data"
old, cur = hd.DataDir(OLD), hd.DataDir(CUR)
U = hd.TYPE["unit"]
names = (sys.argv[1:] if __name__ == "__main__" else ["-"]) or ["content/env_bugs/assets/bug_hive/spore_light_01_objective",
                         "content/env_bugs/assets/bug_hive/spore_light_02_objective",
                         "content/objectives/obj_bugs/thumper/thumper",
                         "content/objectives/obj_bugs/super_syringe/super_syringe", "0e4e0a4947d5bfbe"]
OFFS = ["LODGroupList", "TransformInfo", "LightList", "UnkPreLightList", "WwiseCallback", "CustomizationInfo", "UnkHeader1",
        "ConnectingBoneHash", "BoneInfo", "StreamInfo", "Ending", "MeshInfo", "Materials"]
for n in (names if __name__ == "__main__" else []):
    k = (U, n if hd.HEX16.match(n) else hd.h64(n))
    a, b = old.read(*k), cur.read(*k)
    if not a or not b:
        print("--", n, "missing in", "old" if not a else "current"); continue
    ga, gb = old.read(*k, 2), cur.read(*k, 2)
    ha, hb = header(a), header(b)
    print(f"-- {n}: main {len(a)} -> {len(b)}, gpu {'same' if ga == gb else f'{len(ga)} -> {len(gb)}'}")
    for f in FIELDS:
        name = f.split(":")[0]
        va, vb = ha[name], hb[name]
        mark = "" if va == vb else ("  delta %+d" % (vb - va) if isinstance(va, int) and name in OFFS and va and vb else "  CHANGED")
        if mark or name in OFFS:
            print(f"   {name:20s} {va!r:>22} {vb!r:>22}{mark}")
    # sections in file order, with their sizes, to see where bytes were inserted
    secs = sorted((v, n_) for n_, v in ha.items() if n_ in OFFS and v)
    secs_b = {n_: hb[n_] for n_ in OFFS}
    for i, (off, nm) in enumerate(secs):
        end = secs[i + 1][0] if i + 1 < len(secs) else len(a)
        nb = secs_b.get(nm) or 0
        nxt = sorted(v for v in secs_b.values() if v and v > nb)
        end_b = nxt[0] if nxt else len(b)
        same = a[off:end] == b[nb:end_b] if nb else None
        print(f"      section {nm:20s} old {off:#7x}+{end-off:<6} new {nb:#7x}+{end_b-nb:<6} same={same}")
