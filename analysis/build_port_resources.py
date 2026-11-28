"""Packs the resources the 2024 Deploy Dark Fluid stamps need and game build 25480438 lacks, converted to its formats,
into <out>/9ba626afa44a3aa3.patch_0 (+ .gpu_resources, .stream), the patch HellForge loads with the boot package.

  level      convert_level (format word, embedded unit)
  unit       convert_unit
  material   version word 0x102 -> 0x120 (layout unchanged; see material_diff.py)
  physics, texture, animation, bones, state_machine, package, prefab, entity: as they are (unchanged formats)
  particles, wwise_bank, wwise_dep: left out for now (particles changed format; the sound banks were remade)
  terrain patch mask (the R32 texture a terrain patch unit names at +0x404; patch units hold 0x120ad0a0030576f2 at
             +0x1FC): each texel's top 6 bits are a terrain layer index, renumbered after 1.001.104 (4 -> 3, 8 -> 22,
             9 -> 23, 10 -> 24, read off the one patch both builds have, d2292afe2483e88c, whose counts per index match
             exactly). Left as 8-10, the Dark Fluid sites' ground drew with a layer the game no longer has (magenta).

  python -I build_port_resources.py <out folder> [--set deploy|deactivate|all]
"""
import argparse, collections, json, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import patchwriter as pw
import convert_level as cl
import convert_unit as cu

OLD, CUR = r"F:\hd2_old\20240601\data", r"F:\Game\A\HD2\HellForge\game\data"
HEADER_FROM = r"F:\Game\A\HD2\HellForge\mods\1_AR768766\9ba626afa44a3aa3.patch_0"
SETS = {
    "deploy": ["fb04c61383402693", "8ac0fe970e89087f", "efd618be21cceeb3", "91f45a686207a351", "009da023c64d178d",
               "3a8b3b44156a9532"],
    "deactivate": ["271dd102d25ce55c", "e3319b35c54297f8", "18715471d03aaf51"],
}
SKIP = {hd.TYPE[t] for t in ("particles", "wwise_bank", "wwise_dep", "wwise_stream")}
PATCH_TAG = bytes.fromhex("120ad0a0030576f2")
LAYER_REMAP = {4: 3, 8: 22, 9: 23, 10: 24}


def patch_masks(old, archives):
    """Names of the R32 mask textures the terrain patch units of these archives use."""
    U, out = hd.TYPE["unit"], set()
    for (t, n), es in old.index.items():
        if t != U:
            continue
        e = next((x for x in es if x.archive in archives), None)
        if e is None or e.sizes[1] or e.sizes[2]:
            continue
        b = old.read(t, n, 0, e)
        if b and len(b) > 0x40C and struct.unpack_from("<Q", b, 0x68)[0] == 0x1D0 and b[0x1FC:0x204] == PATCH_TAG:
            out.add(f"{struct.unpack_from('<Q', b, 0x404)[0]:016x}")
    return out


def remap_mask(data):
    """Renumbers the terrain layer index (top 6 bits) of every R32 texel; 0x7C000000 (outside the patch) stays."""
    if not data:
        return data
    v = list(struct.unpack_from(f"<{len(data) // 4}I", data, 0))
    for i, x in enumerate(v):
        k = x >> 26
        if x != 0x7C000000 and k in LAYER_REMAP:
            v[i] = (LAYER_REMAP[k] << 26) | (x & 0x03FFFFFF)
    return struct.pack(f"<{len(v)}I", *v) + data[len(v) * 4:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--set", default="deploy")
    ap.add_argument("--old", default=OLD, help="old build data folder (e.g. the 1001104 data folder)")
    ap.add_argument("--cur", default=CUR, help="current build data folder")
    ap.add_argument("--header", default=HEADER_FROM, help="an existing fat archive file whose header tail is reused")
    a = ap.parse_args()
    archives = SETS["deploy"] + SETS["deactivate"] if a.set == "all" else SETS[a.set]
    old, cur = hd.DataDir(a.old), hd.DataDir(a.cur)
    take, report = {}, collections.Counter()
    masks = patch_masks(old, archives)
    for (t, n), es in old.index.items():
        e = next((x for x in es if x.archive in archives), None)
        if e is None or (t, n) in cur.index or (t, n) in take:
            continue
        typ = hd.TYPES.get(t, t)
        if t in SKIP:
            report[f"left out {typ}"] += 1
            continue
        main_b, stream, gpu = old.read(t, n, 0, e), old.read(t, n, 1, e), old.read(t, n, 2, e)
        if (e.sizes[1] and stream is None) or (e.sizes[2] and gpu is None):
            report[f"no data downloaded {typ}"] += 1
            continue
        try:
            if t == hd.TYPE["level"]:
                main_b = cl.convert(main_b)
            elif t == hd.TYPE["unit"]:
                main_b = cu.convert(main_b)
            elif t == hd.TYPE["material"] and struct.unpack_from("<I", main_b, 0)[0] == 0x102:
                main_b = struct.pack("<I", 0x120) + main_b[4:]
            elif t == hd.TYPE["texture"] and n in masks:
                stream, gpu = remap_mask(stream), remap_mask(gpu)
                report["terrain patch masks renumbered"] += 1
        except Exception as ex:  # noqa: BLE001
            report[f"conversion failed {typ}: {str(ex)[:40]}"] += 1
            continue
        take[(t, n)] = (t, n, main_b, stream or b"", gpu or b"")
        report[f"packed {typ}"] += 1
    # Each stamp's package resource (named like its archive) sat in the 2024 boot archive itself.
    P = hd.TYPE["package"]
    for ar in archives:
        k = (P, ar)
        if k in old.index and k not in cur.index and k not in take:
            take[k] = (P, ar, old.read(P, ar), b"", b"")
            report["packed package (stamp)"] += 1
    path = os.path.join(a.out, "9ba626afa44a3aa3.patch_0")
    pw.write_patch(path, list(take.values()), pw.header_tail(a.header))
    listing = [{"type": hd.TYPES.get(t, t), "name": n, "sizes": [len(m), len(s), len(g)]} for t, n, m, s, g in take.values()]
    with open(os.path.join(a.out, "contents.json"), "w", encoding="utf-8") as f:
        json.dump({"set": a.set, "archives": archives, "report": report, "resources": listing}, f, ensure_ascii=False, indent=1)
    for k, v in sorted(report.items()):
        print(f"  {k}: {v}")
    print("written", path, sum(os.path.getsize(path + x) for x in ("", ".gpu_resources", ".stream")), "bytes")


if __name__ == "__main__":
    main()
