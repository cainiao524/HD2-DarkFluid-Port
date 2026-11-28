"""Reads a decrypted dl_library.dl_typelib (LTLD; layout from filediver datalibrary/datalib.go) and prints types.

  python -I typelib.py <typelib> <type name or substring> [...]
Type names come from filediver hashes/dl_type_names.txt (DL hash: djb2 - 5381). Member names come from the
typelib's own string block when it has one, else the member's offset into that block is shown.
"""
import struct, sys
NAMES = r"E:\File\Codes\0\26.10.1\HD2\public_research_20261002\filediver\hashes\dl_type_names.txt"

def dlhash(t):
    v = 5381
    for ch in t: v = ((v * 33) & 0xffffffff) + ord(ch) & 0xffffffff
    return (v - 5381) & 0xffffffff

def load(path):
    b = open(path, "rb").read()
    magic, ver, tc, ec, mc, evc, eac, dvs, tss = struct.unpack_from("<4sIIIIIIII", b, 0)
    o = 36
    thash = struct.unpack_from(f"<{tc}I", b, o); o += 4 * tc
    ehash = struct.unpack_from(f"<{ec}I", b, o); o += 4 * ec
    types = []
    for i in range(tc):
        name_off, flags, s32, s64, a32, a64, mcount, mstart, com = struct.unpack_from("<IIIIIIIII", b, o); o += 36
        types.append((thash[i], name_off, s64, a64, mcount, mstart))
    o += 32 * ec
    members = []
    for i in range(mc):
        name_off, com, unk, atom, storage, bf, tid, s32, s64, a32, a64, of32, of64, dvo, dvsz, fl = struct.unpack_from("<IIIBBHIIIIIIIIII", b, o)
        members.append((name_off, atom, storage, bf, tid, s64, of64)); o += 72
    o += 16 * evc + 8 * eac + dvs
    strings = b[o:o + tss]
    return types, members, strings, (magic, ver, tc, ec, mc, tss)

def main():
    types, members, strings, hdr = load(sys.argv[1])
    names = {}
    for line in open(NAMES, encoding="utf-8", errors="ignore"):
        s = line.strip()
        if s and not s.startswith("//"): names[dlhash(s)] = s
    print("header", hdr)
    def sname(off):
        if off != 0xffffffff and off < len(strings):
            e = strings.find(b"\0", off)
            if e > off: return strings[off:e].decode(errors="replace")
        return None
    for want in sys.argv[2:]:
        for th, noff, size, al, mcount, mstart in types:
            nm = sname(noff) or names.get(th, f"{th:08x}")
            if want.lower() not in nm.lower() and want != f"{th:08x}": continue
            print(f"== {nm} ({th:08x}) size {size:#x} members {mcount}")
            for mi in range(mstart, mstart + mcount):
                n_off, atom, storage, bf, tid, s, of = members[mi]
                print(f"   +{of:#06x} size {s:#6x} atom {atom} storage {storage} arr {bf} type {names.get(tid, f'{tid:08x}'):40s} {sname(n_off) or '#'+str(n_off)}")

if __name__ == "__main__":
    main()
