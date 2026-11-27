"""Three patch-only test mods that swap resources every bug mission loads for their 2024-06-01 versions, to learn
whether the current engine reads the old formats. Each is a folder with 9ba626afa44a3aa3.patch_0 (+ .gpu_resources,
.stream), like the community patch mods HellForge already loads.

  compat-level      the extraction stamps gen_extraction_01..05 (old level = current one but for 7 bytes: the header
                    u32 and one field). Every mission ends at one of them.
  compat-unit       bug hive spore lights (main data 24 bytes longer in the old format, GPU data identical) and four bug
                    rocks (old main and GPU data).
  compat-material   materials of those rocks whose only difference is the first byte (0x02 old, 0x20 now).

  python -I build_compat.py <out folder>
"""
import json, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import patchwriter as pw

OLD, CUR = r"F:\hd2_old\20240601\data", r"F:\Game\A\HD2\HellForge\game\data"
HEADER_FROM = r"F:\Game\A\HD2\HellForge\mods\1_AR768766\9ba626afa44a3aa3.patch_0"
out = sys.argv[1]
old, cur = hd.DataDir(OLD), hd.DataDir(CUR)
L, U, M = hd.TYPE["level"], hd.TYPE["unit"], hd.TYPE["material"]


def downloaded(e):
    return all(e.sizes[p] == 0 or os.path.exists(os.path.join(OLD, e.archive + ext)) for p, ext in ((1, ".stream"), (2, ".gpu_resources")))


def old_res(typ, name):
    k = (typ, name)
    e = next((x for x in old.index.get(k, []) if downloaded(x)), None)
    if e is None:
        return None
    return (typ, name, old.read(typ, name, 0, e), old.read(typ, name, 1, e), old.read(typ, name, 2, e))


report = {}


def build(folder, resources, note):
    resources = [r for r in resources if r]
    path = os.path.join(out, folder, "9ba626afa44a3aa3.patch_0")
    pw.write_patch(path, resources, pw.header_tail(HEADER_FROM))
    rows = []
    for t, n, m, s, g in resources:
        c = cur.index.get((t, n))
        rows.append({"type": hd.TYPES.get(t, t), "name": n, "old_sizes": [len(m), len(s), len(g)],
                     "current_sizes": list(c[0].sizes) if c else None})
    with open(os.path.join(out, folder, "README.txt"), "w", encoding="utf-8") as f:
        f.write(note + "\n\n" + json.dumps(rows, indent=1) + "\n")
    report[folder] = rows
    print(folder, len(resources), "resources")


levels = [hd.h64(f"packages/generated/stamps/gen_extraction_0{i}") for i in range(1, 6)]
level_res = []
for ar in levels:
    for (t, n), es in old.index.items():
        if t == L and any(e.archive == ar for e in es) and (t, n) in cur.index:
            a, b = old.read(t, n), cur.read(t, n)
            if len(a) == len(b) and sum(x != y for x, y in zip(a, b)) <= 7:
                level_res.append(old_res(t, n))
build("zz.compat-level", level_res,
      "相容性測試（地形）：撤離點印模 gen_extraction_01..05 換成 2024-06-01 的舊格式。每場任務都會用到其中一個。")

unit_names = ["content/env_bugs/assets/bug_hive/spore_light_01_objective", "content/env_bugs/assets/bug_hive/spore_light_02_objective",
              "content/env_bugs/assets/bug_rocks/bug_rock_medium_01", "content/env_bugs/assets/bug_rocks/bug_rock_wall_01",
              "content/env_bugs/assets/bug_rocks/bug_rock_flat_01", "content/env_bugs/assets/bug_rocks/bug_rock_cliff_face_01"]
unit_res = [old_res(U, hd.h64(n)) for n in unit_names]
build("zz.compat-unit", unit_res,
      "相容性測試（模型）：蟲族地圖的孢子燈與四種岩石換成 2024-06-01 的舊格式。")

mats = set()
for r in unit_res:
    if not r:
        continue
    data = r[2]
    for off in range(0, len(data) - 7, 8):
        v = f"{struct.unpack_from('<Q', data, off)[0]:016x}"
        if (M, v) in old.index and (M, v) in cur.index:
            mats.add(v)
mats |= {n for (t, n), es in old.index.items() if t == M and any(e.archive == "009da023c64d178d" for e in es)}
mat_res = []
for v in sorted(mats):
    a, b = old.read(M, v), cur.read(M, v)
    if a and b and len(a) == len(b) and a[1:] == b[1:] and a[0] != b[0]:
        mat_res.append(old_res(M, v))
build("zz.compat-material", mat_res[:40],
      "相容性測試（材質）：蟲族環境資源包（009da023…）裡、新舊只差第一個位元組的材質，換成 2024-06-01 的舊格式。")
json.dump(report, open(os.path.join(out, "compat-report.json"), "w", encoding="utf-8"), indent=1)
