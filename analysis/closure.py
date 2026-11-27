"""Dependency closure of a mission group in one data folder, and its diff against another.

  python -I closure.py --data <old data> --against <current data> --seeds seeds.txt --out <folder>

Seeds are resource names (one per line, any type; '#' comments) or 16-hex name hashes. From each seed every resource
whose name matches is taken; a package adds its items; any other resource adds every resource whose name hash appears
in its main data as an aligned u64 (the stingray way of referencing: units name their materials, levels their units,
and so on). That last rule over-collects a little (a hash can sit in a resource by coincidence only with ~2^-64 odds,
but shared resources such as a common material pull in their own trees); the report keeps the path to each resource
so a stray branch can be cut.

Output in <out>:
  closure.json   every resource reached: type, name, readable name, sizes, which seed and parent reached it
  missing.json   the reached resources the --against folder does not have (same type and name)
  changed.json   present in both but with different sizes (a format change is likely)
  archives.txt   the --data archives that hold the missing ones (a DepotDownloader -filelist for a fat build)
"""
import argparse, collections, json, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd

HERE = os.path.dirname(os.path.abspath(__file__))
HASHES = [r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\filediver\hashes\hashes.txt",
          r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\filediver\hashes\cracked.txt"]
SKIP_SCAN = {hd.TYPE[t] for t in ("texture", "wwise_stream", "wwise_bank", "animation", "bones", "strings", "font")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True)
    ap.add_argument("--against")
    ap.add_argument("--seeds", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max", type=int, default=20000)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    names = hd.load_names([p for p in HASHES if os.path.exists(p)])
    d = hd.DataDir(a.data)
    print(f"{a.data}: {'slim' if d.slim else 'fat'}, {len(d.index)} resources")
    by_name = collections.defaultdict(list)
    for key in d.index:
        by_name[key[1]].append(key)

    seeds = []
    for line in open(a.seeds, encoding="utf-8"):
        s = line.split("#", 1)[0].strip()
        if s:
            seeds.append((s, s.lower() if hd.HEX16.match(s.lower()) else hd.h64(s)))
    reached, queue = {}, collections.deque()
    for text, h in seeds:
        keys = by_name.get(h, [])
        if not keys:
            print("seed not in data:", text)
        for k in keys:
            if k not in reached:
                reached[k] = {"seed": text, "parent": None}
                queue.append(k)
    while queue and len(reached) < a.max:
        k = queue.popleft()
        typ, name = k
        data = d.read(typ, name, 0)
        if data is None:
            continue
        if typ == hd.TYPE["package"]:
            children = hd.package_items(data)
        elif typ in SKIP_SCAN:
            children = []
        else:
            children = []
            for off in range(0, len(data) - 7, 8):
                v = f"{struct.unpack_from('<Q', data, off)[0]:016x}"
                if v in by_name and v != name:
                    children.extend(by_name[v])
        for c in children:
            if c in d.index and c not in reached:
                reached[c] = {"seed": reached[k]["seed"], "parent": f"{hd.TYPES.get(typ, typ)}:{names.get(name, name)}"}
                queue.append(c)
    print("reached", len(reached), "(capped)" if queue else "")

    def row(k):
        e = d.index[k][0]
        return {"type": hd.TYPES.get(k[0], k[0]), "type_hash": k[0], "name_hash": k[1], "name": names.get(k[1]),
                "archive": e.archive, "sizes": e.sizes, **reached[k]}
    rows = [row(k) for k in reached]
    json.dump(rows, open(os.path.join(a.out, "closure.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    if not a.against:
        return
    cur = hd.DataDir(a.against)
    print(f"{a.against}: {'slim' if cur.slim else 'fat'}, {len(cur.index)} resources")
    missing = [r for r, k in zip(rows, reached) if k not in cur.index]
    changed = [dict(r, current_sizes=cur.index[k][0].sizes) for r, k in zip(rows, reached)
               if k in cur.index and tuple(cur.index[k][0].sizes) != tuple(d.index[k][0].sizes)]
    json.dump(missing, open(os.path.join(a.out, "missing.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    json.dump(changed, open(os.path.join(a.out, "changed.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    archives = sorted({r["archive"] for r in missing})
    with open(os.path.join(a.out, "archives.txt"), "w") as f:
        for ar in archives:
            f.write(f"regex:^data[\\\\/]{ar}(\\..*)?$\n")
    by_type = collections.Counter(r["type"] for r in missing)
    print(f"missing {len(missing)} in {len(archives)} archives: {dict(by_type)}")
    print(f"changed {len(changed)}: {dict(collections.Counter(r['type'] for r in changed))}")


if __name__ == "__main__":
    main()
