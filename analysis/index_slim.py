"""Index every (archive, type, name) in a prod_slim (bundles.nxa) HD2 data dir. Read-only."""
import struct, sys, os, json, bisect
import lz4.block

D = sys.argv[1]
OUT = sys.argv[2]

def dsar(path):
    f = open(path, "rb")
    magic, _, n, _, _ = struct.unpack("<4s4sIIQ", f.read(24)); f.read(8)
    assert magic == b"DSAR", path
    raw = f.read(n * 32)
    chunks = [struct.unpack_from("<QQIIBB", raw, i * 32) for i in range(n)]
    return f, chunks

def chunk(f, c):
    f.seek(c[1]); d = f.read(c[3])
    return d if c[4] == 0 else lz4.block.decompress(d, uncompressed_size=c[2])

f, ch = dsar(os.path.join(D, "bundles.nxa"))
b = b"".join(chunk(f, c) for c in ch)
magic, _, _, nname, nar = struct.unpack_from("<4sIIII", b, 0)
assert magic == b"DSAA"
def cstr(o): return b[o:b.index(b"\0", o)].decode()
ars = [struct.unpack_from("<QIIQ", b, 24 + i * 24) for i in range(nar)]
names = [cstr(struct.unpack_from("<I", b, 24 + nar * 24 + i * 4)[0]) for i in range(nname)]
bundles = [dsar(os.path.join(D, n)) for n in names]
starts = [[c[0] for c in ch] for _, ch in bundles]

def read_archive_head(buf_fn):
    data = buf_fn(72)
    magic, nt, nf = struct.unpack_from("<III", data, 0)
    assert magic == 0xF0000011, hex(magic)
    need = 72 + nt * 32 + nf * 80
    data = buf_fn(need)
    types = [struct.unpack_from("<IIQIIII", data, 72 + i * 32) for i in range(nt)]
    files = []
    o = 72 + nt * 32
    for i in range(nf):
        name, typ = struct.unpack_from("<QQ", data, o + i * 80)
        sizes = struct.unpack_from("<III", data, o + i * 80 + 56)
        files.append((f"{typ:016x}", f"{name:016x}", sizes))
    return files

index = {}
for size, fno, ecount, eoff in ars:
    fn = cstr(fno)
    if "." in fn: continue
    ents = [struct.unpack_from("<I4xI3xB", b, eoff + i * 16) for i in range(ecount)]
    def buf_fn(n, ents=ents):
        out = b""
        aoff, uoff, bi = ents[0]
        bf, bch = bundles[bi]
        k = bisect.bisect_left(starts[bi], uoff)
        assert starts[bi][k] == uoff
        while len(out) < n:
            out += chunk(bf, bch[k]); k += 1
        return out
    try:
        index[fn] = read_archive_head(buf_fn)
    except Exception as e:
        print("ERR", fn, e)
for name in os.listdir(D):
    p = os.path.join(D, name)
    if "." in name or not os.path.isfile(p): continue
    bf, bch = dsar(p)
    def buf_fn(n):
        out = b""; k = 0
        while len(out) < n: out += chunk(bf, bch[k]); k += 1
        return out
    index[name] = read_archive_head(buf_fn)
json.dump(index, open(OUT, "w"))
print("archives", len(index), "files", sum(len(v) for v in index.values()))
