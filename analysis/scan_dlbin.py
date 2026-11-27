import os, struct, sys, math, collections
D = r"F:\Game\A\HD2\HellForge\game\data\game"
want = {
    "bug_darkfluid_backpack(073270..)": 0x073270650f859dd0,
    "ion_dark_fluid_backpack(0e4e..)": 0x0e4e0a4947d5bfbe,
    "dark_fluid_tcs_01(5d42..)": 0x5d422ca6694a5129,
    "dark_fluid_tcs_02(e14e..)": 0xe14e9f44edda7bce,
    "hellpod rack darkfluid(7741265416947841340)": 7741265416947841340,
    "thumper unit": None, "super_syringe unit": None,
    "obj DEPLOY DARK FLUID u32 629858657": 629858657,
    "obj PAYLOAD SITE u32 2314105256": 2314105256,
    "mdbe 258AE161": 0x258AE161,
    "thin dark_fluid_tcs_01 1564617894": 1564617894,
    "thin dark_fluid_tcs_02 3780026180": 3780026180,
    "thin bug_darkfluid_backpack 120746085": 120746085,
    "thin ion_dark_fluid_backpack 239995465": 239995465,
}
sys.path.insert(0, os.path.dirname(__file__))
M = 0xc6a4a7935bd1e995; MASK = (1 << 64) - 1
def mm(data, seed=0):
    h = (seed ^ (len(data) * M)) & MASK
    n = len(data) // 8
    for i in range(n):
        k = int.from_bytes(data[i*8:i*8+8], "little"); k = (k * M) & MASK; k ^= k >> 47; k = (k * M) & MASK
        h ^= k; h = (h * M) & MASK
    t = data[n*8:]
    if t: h ^= int.from_bytes(t, "little"); h = (h * M) & MASK
    h ^= h >> 47; h = (h * M) & MASK; h ^= h >> 47
    return h
want["thumper unit"] = mm(b"content/objectives/obj_bugs/thumper/thumper")
want["super_syringe unit"] = mm(b"content/objectives/obj_bugs/super_syringe/super_syringe")
for s in ("packages/content/bug_tcs_03_backpack", "packages/content/bug_tcs_03_thumper", "packages/generated/loadout/tcs_03_thumper",
          "packages/content/black_hole", "packages/content/bug_tcs_01", "packages/content/bug_tcs_02", "packages/content/bug_tcs_04"):
    want["pkg " + s] = mm(s.encode())
def ent(b):
    c = collections.Counter(b[:65536]); n = sum(c.values())
    return -sum(v / n * math.log2(v / n) for v in c.values())
for fn in sorted(os.listdir(D)):
    if not fn.endswith(".dl_bin"): continue
    b = open(os.path.join(D, fn), "rb").read()
    hits = []
    for k, v in want.items():
        pat = struct.pack("<Q", v) if v > 0xFFFFFFFF else struct.pack("<I", v)
        n = b.count(pat)
        if n: hits.append(f"{k} x{n}")
    if hits or "-v" in sys.argv:
        print(f"{fn}  size {len(b)}  head {b[:8].hex()}  entropy {ent(b):.2f}")
        for h in hits: print("    ", h)
