"""Minimal minidump reader: the crashing thread's validated return addresses (preceded by a call) in exe / game.dll.

  python -I mdstack.py <crash.dmp>

Needs analysis/image/helldivers2-25480438.bin and game-25480438.bin (unpacked images, not in this repo)."""
import os, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
IMG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "analysis", "image")
d = open(sys.argv[1], "rb").read()
exe_img = open(os.path.join(IMG, "helldivers2-25480438.bin"), "rb").read()
game_img = open(os.path.join(IMG, "game-25480438.bin"), "rb").read()
sig, ver, n, dirrva = struct.unpack_from("<4sIII", d, 0)
streams = {}
for i in range(n):
    t, size, rva = struct.unpack_from("<III", d, dirrva + 12 * i); streams[t] = (size, rva)
def rd_str(rva):
    ln = struct.unpack_from("<I", d, rva)[0]; return d[rva + 4: rva + 4 + ln].decode("utf-16-le")
mods = []
size, rva = streams[4]; nm = struct.unpack_from("<I", d, rva)[0]
for i in range(nm):
    o = rva + 4 + 108 * i
    base, sz = struct.unpack_from("<QI", d, o); namerva = struct.unpack_from("<I", d, o + 20)[0]
    mods.append((base, sz, rd_str(namerva).split("\\")[-1]))
size, rva = streams[6]
tid = struct.unpack_from("<I", d, rva)[0]
code, flags, rec, addr = struct.unpack_from("<IIQQ", d, rva + 8)
ctx_size, ctx_rva = struct.unpack_from("<II", d, rva + 8 + 152)
rsp, = struct.unpack_from("<Q", d, ctx_rva + 0x98); rip, = struct.unpack_from("<Q", d, ctx_rva + 0xF8)
def modof(v):
    for b, s, nme in mods:
        if b <= v < b + s: return nme, v - b
    return None, None
print("crash thread", tid, "code", hex(code), "rip", modof(rip), "rsp", hex(rsp))
size, rva = streams[3]; nt = struct.unpack_from("<I", d, rva)[0]; stack = None
for i in range(nt):
    o = rva + 4 + 48 * i
    t = struct.unpack_from("<I", d, o)[0]
    if t == tid:
        start, ds, drva = struct.unpack_from("<QII", d, o + 24); stack = (start, d[drva:drva + ds])
md = Cs(CS_ARCH_X86, CS_MODE_64)
start, mem = stack
out = []
for off in range((rsp - start) & ~7, len(mem) - 8, 8):
    v = struct.unpack_from("<Q", mem, off)[0]
    m, rel = modof(v)
    if m not in ("helldivers2.exe", "game.dll", "darkfluid_port.dll"): continue
    img = exe_img if m == "helldivers2.exe" else (game_img if m == "game.dll" else None)
    call = "?"
    if img is not None:
        ok = False
        for back in (5, 6, 2, 3, 7):
            for ins in md.disasm(img[rel - back: rel], rel - back):
                if ins.address + ins.size == rel and ins.mnemonic == "call": ok = True; call = ins.op_str
                break
            if ok: break
        if not ok: continue
    out.append(f"  [rsp+{off - (rsp - start):#06x}] {m}+{rel:#x}  (after call {call})")
print("\n".join(out[:40]))
