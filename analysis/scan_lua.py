"""Scan every lua resource in a prod_slim data dir for keywords. Read-only."""
import struct, sys, os, bisect, re, collections
import lz4.block
D = sys.argv[1]
PAT = re.compile(sys.argv[2].encode(), re.I)
LUA = 0xa14e8dfa2cd117e2
def dsar(path):
    f = open(path, "rb")
    magic, _, n, _, _ = struct.unpack("<4s4sIIQ", f.read(24)); f.read(8)
    raw = f.read(n * 32)
    return f, [struct.unpack_from("<QQIIBB", raw, i * 32) for i in range(n)]
cache = collections.OrderedDict()
def chunk(bi, f, c):
    key = (bi, c[1])
    if key in cache: return cache[key]
    f.seek(c[1]); d = f.read(c[3])
    d = d if c[4] == 0 else lz4.block.decompress(d, uncompressed_size=c[2])
    cache[key] = d
    if len(cache) > 64: cache.popitem(last=False)
    return d
f, ch = dsar(os.path.join(D, "bundles.nxa"))
b = b"".join(chunk(-1, f, c) for c in ch)
_, _, _, nname, nar = struct.unpack_from("<4sIIII", b, 0)
def cstr(o): return b[o:b.index(b"\0", o)].decode()
ars = [struct.unpack_from("<QIIQ", b, 24 + i * 24) for i in range(nar)]
names = [cstr(struct.unpack_from("<I", b, 24 + nar * 24 + i * 4)[0]) for i in range(nname)]
bundles = [dsar(os.path.join(D, n)) for n in names]
starts = [[c[0] for c in ch] for _, ch in bundles]
def read(ents, off, n):
    aoffs = [e[0] for e in ents]
    i = bisect.bisect_right(aoffs, off) - 1
    aoff, uoff, bi = ents[i]
    bf, bch = bundles[bi]
    k = bisect.bisect_left(starts[bi], uoff)
    skip = off - aoff
    out = b""
    while len(out) < skip + n:
        out += chunk(bi, bf, bch[k]); k += 1
    return out[skip:skip + n]
total = 0; hits = collections.defaultdict(set); seen = set()
for size, fno, ecount, eoff in ars:
    fn = cstr(fno)
    if "." in fn: continue
    ents = [struct.unpack_from("<I4xI3xB", b, eoff + i * 16) for i in range(ecount)]
    head = read(ents, 0, 72)
    magic, nt, nf = struct.unpack_from("<III", head, 0)
    tbl = read(ents, 0, 72 + nt * 32 + nf * 80)
    for i in range(nf):
        o = 72 + nt * 32 + i * 80
        name, typ, moff = struct.unpack_from("<QQQ", tbl, o)
        msize, = struct.unpack_from("<I", tbl, o + 56)
        if typ != LUA or name in seen: continue
        seen.add(name); total += 1
        data = read(ents, moff, msize)
        for m in PAT.finditer(data):
            s = data[max(0, m.start() - 40): m.end() + 40]
            txt = re.sub(rb"[^\x20-\x7e]+", b" ", s).decode()
            hits[f"{name:016x}"].add(txt.strip())
print("lua files scanned", total)
for k, v in hits.items():
    print("==", k, fn if False else "")
    for s in sorted(v)[:25]: print("    ", s)
print("files with hits", len(hits))
