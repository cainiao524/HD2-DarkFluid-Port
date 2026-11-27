"""Ports the Deploy Dark Fluid stamps' generation settings from the 2024-06-01 build into build 25480438.

The level generator's stamp settings are one resource in the boot bundle 9ba626afa44a3aa3: type b8fd4d2cede20ed7, name
8105986402e7f076. Layout (both builds; record size 0x34 in 2024, 0x44 now):

  u32 version 7, u32 n0, n0 x record      S0  per-stamp records (objects, entity spawns, objective slots)
  u32 n1, n1 x 0x40                      S1  stamp -> objective types hash table: u8 types[32], u64 key, u32 S0 start,
                                             u32 S0 count, u32 ?, u32 ?, u32 S5 start, u32 S5 count, 8 bytes
  u32 n2, n2 x 0x38                      S2  hash table (key at +0x20)                 (no Dark Fluid entries)
  u32 n3, n3 x 0x10                      S3  u64 key -> u32                             (no Dark Fluid entries)
  u32 n4, n4 x u64                       S4
  u32 n5, n5 x u64                       S5  resource names the stamps list

A 2024 record becomes a current one by its 0x34 bytes plus 16 zero bytes, with two enumerations renumbered (derived
from the stamps both builds have, record by record): +0x2C (1->3, 2->4, 3->5, 6->11, 15->17; 12->14 by the +2 rule)
and, in objective-slot records (+2 & 8), the objective type at +4 (Deploy Dark Fluid 26->28, Payload Site 27->29).
+0x10 indexes the stamp's own level, which is the 2024 level itself (converted), so it stays. S1 slots: the builder
puts a key at (2 * key) % n1 with linear probing over free (key 0) buckets.

  python -I port_lgs.py <2024 data> <current data> <out folder>
writes <out>/9ba626afa44a3aa3.patch_0 (+ .gpu_resources, .stream) holding the merged resource.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hd2data as hd
import patchwriter as pw

TYPE, NAME, BUNDLE = "b8fd4d2cede20ed7", "8105986402e7f076", "9ba626afa44a3aa3"
STAMPS = {  # every stamp the 2024 settings give the TCS / Dark Fluid objectives (old types 24-27)
    "deploy dark fluid site 1 (fb04)": 0x0B34988E28A9131B, "deploy dark fluid site 2 (8ac0)": 0xB10C9886D3006FD4,
    "dark fluid backpack site (efd6)": 0x2DDAA26E56F2CD4C,
    "payload site, records only (adab38f5)": 0xADAB38F5E35BE4AE, "payload site, records only (d0a2ca99)": 0xD0A2CA9932FDA6E7,
    "payload site, records only (ba1d0dda)": 0xBA1D0DDA4936F92E, "payload site, records only (74606ed3)": 0x74606ED3950F307E,
    "deploy dark fluid, records only (80aca473)": 0x80ACA473FBE35102,
    "deactivate tcs site A (271dd)": 0x465AF3D39AB42964, "deactivate tcs site B (e3319)": 0x004F0E2FC6BCDB32,
    "deactivate tcs, records only (a04d0a19)": 0xA04D0A191031ACAD}
MAP_2C = {1: 3, 2: 4, 3: 5, 6: 11, 10: 12, 12: 14, 13: 15, 15: 17, 20: 22}  # 12, 13: by the +2 rule
MAP_TYPE = {24: 26, 25: 27, 26: 28, 27: 29}  # deactivate termicide, mutated eggs, deploy dark fluid, payload site


class Settings:
    def __init__(self, b, recsize):
        self.b, self.rs = b, recsize
        self.ver, self.n0 = struct.unpack_from("<II", b, 0)
        o = 8 + self.n0 * recsize
        self.sec = {}
        for name, size in (("s1", 0x40), ("s2", 0x38), ("s3", 0x10), ("s4", 8), ("s5", 8)):
            n = struct.unpack_from("<I", b, o)[0]
            self.sec[name] = (n, o + 4, size)
            o += 4 + n * size
        if o != len(b):
            raise ValueError(f"layout: parsed to {o:#x} of {len(b):#x}")

    def records(self, start, count):
        return [self.b[8 + (start + i) * self.rs: 8 + (start + i + 1) * self.rs] for i in range(count)]

    def table(self, name):
        n, at, size = self.sec[name]
        return [self.b[at + i * size: at + (i + 1) * size] for i in range(n)]

    def s1_find(self, key):
        p = struct.pack("<Q", key)
        for e in self.table("s1"):
            if e[0x20:0x28] == p:
                return e
        return None


def read_settings(data_dir):
    d = hd.DataDir(data_dir)
    for (t, n), es in d.index.items():
        tv = int(t, 16) if isinstance(t, str) else t
        if tv == int(TYPE, 16) and n == NAME:
            return d.read(t, n, 0, es[0])
    raise SystemExit(f"{TYPE}/{NAME} not in {data_dir}")


def convert_record(old):
    r = bytearray(old[:0x34]) + bytearray(0x10)
    if r[0x2C]:
        if r[0x2C] not in MAP_2C:
            raise ValueError(f"+0x2C value {r[0x2C]} has no mapping")
        r[0x2C] = MAP_2C[r[0x2C]]
    if r[2] & 8:
        if r[4] not in MAP_TYPE:
            raise ValueError(f"objective type {r[4]} has no mapping")
        r[4] = MAP_TYPE[r[4]]
    return bytes(r)


def merge(old_b, cur_b):
    old, cur = Settings(old_b, 0x34), Settings(cur_b, 0x44)
    s0 = bytearray(cur_b[8: 8 + cur.n0 * 0x44])
    n0 = cur.n0
    s5n, s5at, _ = cur.sec["s5"]
    s5 = bytearray(cur_b[s5at: s5at + s5n * 8])
    n5 = s5n
    s1 = [bytearray(e) for e in cur.table("s1")]
    n1 = len(s1)
    report = []
    for label, key in STAMPS.items():
        if cur.s1_find(key):
            raise ValueError(f"{label} is already in the current settings")
        e = old.s1_find(key)
        start, count, w30, w34, s5start, s5count = struct.unpack_from("<IIIIII", e, 0x28)
        recs = [convert_record(r) for r in old.records(start, count)]
        names = old_b[old.sec["s5"][1] + s5start * 8: old.sec["s5"][1] + (s5start + s5count) * 8]
        new = bytearray(e)
        types = [MAP_TYPE[x] for x in e[:32] if x]
        new[:32] = bytes(types) + bytes(32 - len(types))
        struct.pack_into("<IIIIII", new, 0x28, n0, count, w30, w34, n5, s5count)
        s0 += b"".join(recs); n0 += count
        s5 += names; n5 += s5count
        slot = (2 * key) % n1
        for _ in range(n1):
            if struct.unpack_from("<Q", s1[slot], 0x20)[0] == 0:
                break
            slot = (slot + 1) % n1
        else:
            raise ValueError("S1 is full")
        s1[slot] = new
        report.append(f"{label}: types {types}, S0 {n0 - count}+{count}, S5 {n5 - s5count}+{s5count}, S1 slot {slot}")
    out = bytearray(struct.pack("<II", cur.ver, n0)) + s0
    out += struct.pack("<I", n1) + b"".join(s1)
    for name in ("s2", "s3", "s4"):
        n, at, size = cur.sec[name]
        out += struct.pack("<I", n) + cur_b[at: at + n * size]
    out += struct.pack("<I", n5) + s5
    Settings(bytes(out), 0x44)  # parses back
    return bytes(out), report


def main():
    old_dir, cur_dir, out_dir = sys.argv[1:4]
    merged, report = merge(read_settings(old_dir), read_settings(cur_dir))
    for line in report:
        print(line)
    path = os.path.join(out_dir, BUNDLE + ".patch_0")
    pw.write_patch(path, [(TYPE, NAME, merged, b"", b"")])
    print(f"wrote {path}: {len(merged)} bytes of settings")


if __name__ == "__main__":
    main()
