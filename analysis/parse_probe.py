"""Turns the Dark Fluid Probe's dumps (darkfluid-probe-*.bin in a session's logs folder) into probe-report.json and a
short printed summary of the Dark Fluid / TCS mission group.

  python -I parse_probe.py <session folder> [--out <folder>]

Layouts (game build 25480438; HellForge's native/mod-loader/difficulty.c and MDBE's mission.h):
  mission row 0x380: +0 id, +4 enabled, +8 faction, +0xC/+0x10 level range, +0x28/+0x2C optional/major counts,
                     +0x34 kind, +0x40 32 x {id, weight f32, ?, class, min, max}, +0x340 name locid, +0x360 delta,
                     +0x368/+0x370 layout list pointer/count
  objective   0xA0:  +0 id, +0x20 most per mission, +0x24/+0x28 levels, +0x34 name locid, +0x3C description locid
  operation   0x490: +0 id, +8 faction, +0x38 32 x {mission id, weight f32}
  location item 0x1C8: +8 stamp key, +0x60 difficulty word (min bits 2..5, max 6..9), +0xD8 radius, +0x15E size
"""
import argparse, glob, json, os, struct

STRINGS = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\shalzuth_HelldiversData\data\translations\English.json"
# Objective ids of the group: MISSION-OBJECTIVES (hd2-datamined) and MDBE's tables.h.
GROUP = {629858657: "DEPLOY DARK FLUID", 2314105256: "PAYLOAD SITE", 333852789: "ACTIVATE TCS+ STATION (a)",
         2464469546: "ACTIVATE TCS+ STATION (b)", 175894917: "DEACTIVATE TERMICIDE TOWER",
         824768011: "ERADICATE MUTATED NESTS"}


def load(folder, name):
    p = os.path.join(folder, f"darkfluid-probe-{name}")
    return open(p, "rb").read() if os.path.exists(p) else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("session")
    ap.add_argument("--out")
    a = ap.parse_args()
    folders = [a.session] + glob.glob(os.path.join(a.session, "**"), recursive=True)
    folder = next((f for f in folders if os.path.isdir(f) and os.path.exists(os.path.join(f, "darkfluid-probe-missions.bin"))), None)
    if folder is None:
        raise SystemExit(f"no darkfluid-probe-missions.bin under {a.session}")
    s = {int(k): v for k, v in json.load(open(STRINGS, encoding="utf-8")).items()} if os.path.exists(STRINGS) else {}
    text = lambda locid: s.get(locid, f"<loc {locid}>") if locid else ""

    objectives = {}
    ob = load(folder, "objectives.bin")
    for i in range(len(ob) // 0xA0):
        r = ob[i * 0xA0:(i + 1) * 0xA0]
        oid, = struct.unpack_from("<I", r, 0)
        objectives[oid] = {"row": i, "id": oid, "most": struct.unpack_from("<I", r, 0x20)[0],
                           "levels": struct.unpack_from("<II", r, 0x24), "name": text(struct.unpack_from("<I", r, 0x34)[0]),
                           "description": text(struct.unpack_from("<I", r, 0x3C)[0])[:160]}

    missions, mb = [], load(folder, "missions.bin")
    for i in range(len(mb) // 0x380):
        r = mb[i * 0x380:(i + 1) * 0x380]
        slots = []
        for k in range(32):
            oid, w, _, cls, lo, hi = struct.unpack_from("<IfIIII", r, 0x40 + k * 0x18)
            if oid:
                o = objectives.get(oid, {})
                slots.append({"id": oid, "weight": w, "class": cls, "min": lo, "max": hi,
                              "name": GROUP.get(oid) or o.get("name") or "?", "known": oid in objectives})
        missions.append({"row": i, "id": struct.unpack_from("<I", r, 0)[0], "enabled": r[4], "faction": r[8],
                         "levels": struct.unpack_from("<II", r, 0xC), "optional_major": struct.unpack_from("<II", r, 0x28),
                         "kind": r[0x34], "name": text(struct.unpack_from("<I", r, 0x340)[0]),
                         "delta": f"{struct.unpack_from('<Q', r, 0x360)[0]:#x}", "layouts": struct.unpack_from("<I", r, 0x370)[0],
                         "objectives": slots, "group": any(x["id"] in GROUP for x in slots)})

    operations, pb = [], load(folder, "operations.bin")
    row_of = {m["id"]: m["row"] for m in missions}
    for i in range(len(pb) // 0x490):
        r = pb[i * 0x490:(i + 1) * 0x490]
        lst = [(row_of.get(mid, f"{mid:#x}"), w) for mid, w in struct.iter_unpack("<If", r[0x38:0x38 + 256]) if mid]
        operations.append({"row": i, "id": struct.unpack_from("<I", r, 0)[0], "faction": r[8], "missions": lst})

    locations = []
    for p in sorted(glob.glob(os.path.join(folder, "darkfluid-probe-locations-*.bin"))):
        b, o, cur_set, cur_group = open(p, "rb").read(), 4 + 0xC0, None, None
        dump = {"file": os.path.basename(p), "sets": []}
        while o + 4 <= len(b):
            tag = b[o:o + 4]; o += 4
            if tag == b"SET ":
                cur_set = {"id": f"{struct.unpack_from('<I', b, o)[0]:#010x}", "groups": []}; dump["sets"].append(cur_set); o += 0x40
            elif tag == b"GRP ":
                cur_group = {"id": f"{struct.unpack_from('<I', b, o)[0]:#010x}", "items": []}; cur_set["groups"].append(cur_group); o += 0x18
            elif tag == b"ITEM":
                it = b[o:o + 0x1C8]; o += 0x1C8
                n, = struct.unpack_from("<I", b, o); o += 4
                classes = list(b[o:o + n]); o += n
                word, = struct.unpack_from("<H", it, 0x60)
                cur_group["items"].append({"stamp": f"{struct.unpack_from('<Q', it, 8)[0]:016x}", "radius": struct.unpack_from("<f", it, 0xD8)[0],
                                           "size": it[0x15E], "levels": [(word >> 2) & 0xF, (word >> 6) & 0xF], "classes": classes})
            else:
                break
        locations.append(dump)

    report = {"folder": folder, "missions": missions, "objectives": list(objectives.values()), "operations": operations, "locations": locations}
    out = a.out or folder
    json.dump(report, open(os.path.join(out, "probe-report.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print(f"{len(missions)} missions, {len(objectives)} objectives, {len(operations)} operations, {len(locations)} location dumps")
    for oid, name in GROUP.items():
        o = objectives.get(oid)
        print(f"objective {oid} {name}: " + (f"row {o['row']}, levels {o['levels']}, most {o['most']}" if o else "NOT IN TABLE"))
    for m in missions:
        if m["group"]:
            listed = [op["row"] for op in operations if any(r == m["row"] for r, _ in op["missions"])]
            print(f"mission row {m['row']} id {m['id']:#x} enabled {m['enabled']} faction {m['faction']} levels {m['levels']} "
                  f"kind {m['kind']} name {m['name']!r} in operations {listed}")
            for x in m["objectives"]:
                print(f"    {x['id']:#010x} class {x['class']} w {x['weight']:g} {x['min']}-{x['max']} {x['name']}")
    print("report:", os.path.join(out, "probe-report.json"))


if __name__ == "__main__":
    main()
