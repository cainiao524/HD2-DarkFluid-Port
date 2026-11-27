import json, sys, re, collections
M = 0xc6a4a7935bd1e995; MASK = (1 << 64) - 1
def mm(data, seed=0):
    h = (seed ^ (len(data) * M)) & MASK
    n = len(data) // 8
    for i in range(n):
        k = int.from_bytes(data[i*8:i*8+8], "little")
        k = (k * M) & MASK; k ^= k >> 47; k = (k * M) & MASK
        h ^= k; h = (h * M) & MASK
    t = data[n*8:]
    if t:
        h ^= int.from_bytes(t, "little"); h = (h * M) & MASK
    h ^= h >> 47; h = (h * M) & MASK; h ^= h >> 47
    return h
idx = json.load(open(sys.argv[1]))
pat = re.compile(sys.argv[2], re.I)
names = {}
for p in sys.argv[3:]:
    for line in open(p, encoding="utf-8", errors="ignore"):
        s = line.strip()
        if s: names[f"{mm(s.encode()):016x}"] = s
types = {f"{mm(t.encode()):016x}": t for t in "unit level package material texture bones animation state_machine physics particles wwise_bank wwise_stream wwise_dep strings prefab entity geometry_group render_config font lua runtime_font ah_bin behavior_tree network_config speedtree camera_shake cloth hash_lookup renderable shading_environment shading_environment_mapping vector_field wwise_metadata wwise_properties".split()}
assert mm(b"unit") == 0xe0a48d0be9a7453f, hex(mm(b"unit"))
present = collections.defaultdict(set)
for ar, files in idx.items():
    for t, n, sizes in files:
        present[(t, n)].add(ar)
known = sum(1 for (t, n) in present if n in names)
print("unique resources", len(present), "named", known)
hits = sorted((names[n], types.get(t, t), len(a), sorted(a)[:3]) for (t, n), a in present.items() if n in names and pat.search(names[n]))
for h in hits: print(h)
print("hits", len(hits))
# raw hashes of interest
for h in ("073270650f859dd0", "0e4e0a4947d5bfbe", "5d422ca6694a5129", "e14e9f44edda7bce", "6b6e6a1c8b8a0b7c"):
    for (t, n), a in present.items():
        if n == h: print("HASH", h, types.get(t, t), sorted(a)[:4])
