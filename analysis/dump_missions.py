import struct, sys, json
DLL = r"F:\Game\A\HD2\HellForge\game\data\game\game.dll"
STR = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\shalzuth_HelldiversData\data\translations\English.json"
data = open(DLL, "rb").read()
strings = {int(k): v for k, v in json.load(open(STR, encoding="utf-8")).items()}
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
opt = struct.unpack_from("<H", data, pe + 20)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + i * 40
    name = data[o:o + 8].rstrip(b"\0").decode()
    vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
    secs.append((name, va, vsize, raw, rsize))
if "-s" in sys.argv:
    for s in secs: print(s[0], hex(s[1]), hex(s[2]), hex(s[3]), hex(s[4]))

def rd(rva, n):
    for name, va, vsize, raw, rsize in secs:
        if va <= rva < va + max(vsize, rsize):
            off = rva - va
            if off + n > rsize: return None
            return data[raw + off: raw + off + n]
    return None

OBJ, OBJ_N, OBJ_SZ = 0x32EF870, 153, 0xA0
MIS, MIS_N, MIS_SZ = 0x3773420, 162, 0x380
OPS, OPS_N, OPS_SZ = 0x32FEF10, 25, 0x490
objname = {}
for d in range(OBJ_N):
    r = rd(OBJ + d * OBJ_SZ, OBJ_SZ)
    if r is None: print("objective table not in file"); break
    h, = struct.unpack_from("<I", r, 0)
    n, = struct.unpack_from("<I", r, 0x34)
    objname[h] = (d, strings.get(n, "?"), struct.unpack_from("<III", r, 0x20))
if "-o" in sys.argv:
    for h, v in objname.items(): print(v[0], hex(h), h, v[1], v[2])
rows = {}
for m in range(MIS_N):
    r = rd(MIS + m * MIS_SZ, MIS_SZ)
    if r is None: print("mission table not in file"); break
    mid, = struct.unpack_from("<I", r, 0)
    rows[mid] = m
    ents = []
    for k in range(32):
        e = struct.unpack_from("<IfIIII", r, 0x40 + k * 0x18)
        if e[0]: ents.append(e)
    name = strings.get(struct.unpack_from("<I", r, 0x340)[0], "?")
    want = "-a" in sys.argv or any(e[0] in (629858657, 2314105256) for e in ents) or "DARK" in name.upper()
    if want or ("-l" in sys.argv):
        print(f"row {m} id {mid:#x} enabled {r[4]} faction {r[8]} range {struct.unpack_from('<II', r, 0xC)} kind {r[0x34]} "
              f"opt/major {struct.unpack_from('<II', r, 0x28)} name {name!r} delta {struct.unpack_from('<Q', r, 0x360)[0]:#x} "
              f"layouts {struct.unpack_from('<QI', r, 0x368)} +340 {r[0x340:0x360].hex()} +378 {r[0x378:0x380].hex()}")
        if want:
            print("   head", r[:0x40].hex())
            for e in ents:
                print(f"   {e[0]:#010x} w {e[1]:g} {e[2]} class {e[3]} min {e[4]} max {e[5]:#x}  {objname.get(e[0], ('?','?'))[1]}")
if "-p" in sys.argv:
    for o in range(OPS_N):
        r = rd(OPS + o * OPS_SZ, OPS_SZ)
        ids = [struct.unpack_from("<If", r, 0x38 + 8 * k) for k in range(32)]
        print(f"op {o} id {struct.unpack_from('<I', r, 0)[0]:#x} faction {r[8]} head {r[:0x38].hex()}")
        print("   ", [(rows.get(i, hex(i)), w) for i, w in ids if i])
