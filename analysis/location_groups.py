"""Reads a decrypted generated_location_groups.dl_bin and prints the registrations (LocationStampInfo) of given stamps.

  python -I location_groups.py <decrypted dl_bin> <stamp key hex> [...]

File: u32 instance count, then per instance a 24-byte subdata header (magic LDLD, version, type, size, is64, pad)
and `size` bytes of data. DL arrays are {i64 offset, u64 count}; the offset is relative to the start of the data of
the instance that holds the array (filediver datalibrary/region_settings.go does the same). Offset -1 = empty.

Two layouts, chosen by the instance size of the entry (the key is at +0 in both):
  2024-06 and 1.001.104 (0x118 per entry): arrays at +0x60 nature tags, +0x70 mission tags (u8), +0x90 region
  types, +0xA0 u32 list; +0x54 level word, +0xB0 u8, +0xC4 SubRegionType, +0xD4 clearance, +0xE0 GuardForceType in
  the top byte, +0xE4 flags.
  25480438 (memory LocationStampInfo less its first 8 bytes): arrays at +0xA8 nature tags, +0xB8 mission tags,
  +0xE8 region types, +0xF8 u32 list, +0x190 LocationFlag; +0x58 level word, +0x108 u8, +0x12C/+0x130
  SubRegionType, +0x140 clearance, +0x157 GuardForceType, +0x158 flags.
Field types from filediver's dl_library.dl_typelib (analysis/typelib.py LocationStampInfo); names are stripped there.
"""
import struct, sys


def instances(b):
    n = struct.unpack_from("<I", b, 0)[0]
    o, out = 4, []
    for _ in range(n):
        size = struct.unpack_from("<I", b, o + 12)[0]
        out.append((o + 24, size))
        o += 24 + size
    return out


def array(b, base, at, fmt):
    off, cnt = struct.unpack_from("<qQ", b, at)
    if off < 0 or cnt == 0 or cnt > 64:
        return []
    size = struct.calcsize(fmt)
    return [struct.unpack_from(fmt, b, base + off + size * i)[0] for i in range(cnt)]


def main():
    b = open(sys.argv[1], "rb").read()
    old = struct.unpack_from("<I", b, 4 + 12)[0] < 0x3E4  # the root instance grew from 0x170 to 0x3E4
    insts = instances(b)
    for arg in sys.argv[2:]:
        key = int(arg, 16)
        p = b.find(struct.pack("<Q", key))
        if p < 0:
            print(f"{key:016x}: not registered")
            continue
        base = next(bs for bs, sz in insts if bs <= p < bs + sz)
        u32 = lambda o: struct.unpack_from("<I", b, p + o)[0]
        f32 = lambda o: struct.unpack_from("<f", b, p + o)[0]
        if old:
            print(f"{key:016x} (2024 layout): level {u32(0x54):#x} u8 {u32(0xB0) & 0xFF} subregion {u32(0xC4)} clearance {f32(0xD4)} "
                  f"guard force {u32(0xE0) >> 24} flags {u32(0xE4):#x} mission tags {array(b, base, p + 0x70, '<B')} "
                  f"regions {array(b, base, p + 0x90, '<I')} u32 list {[hex(x) for x in array(b, base, p + 0xA0, '<I')]}")
        else:
            print(f"{key:016x} (25480438 layout): level {u32(0x58):#x} u8 {u32(0x108) & 0xFF} subregion {u32(0x12C)}/{u32(0x130)} "
                  f"clearance {f32(0x140)} guard force {b[p + 0x157]} flags {u32(0x158) & 0x7FF:#x} mission tags {array(b, base, p + 0xB8, '<B')} "
                  f"regions {array(b, base, p + 0xE8, '<I')} u32 list {[hex(x) for x in array(b, base, p + 0xF8, '<I')]} "
                  f"location flags {array(b, base, p + 0x190, '<B')}")


if __name__ == "__main__":
    main()
