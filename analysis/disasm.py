"""Disassemble a range of the unpacked game.dll image (build 25480438): python -I disasm.py <rva hex> [count] [stop at ret]"""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
img = open(r"E:\File\Codes\0\Claude\26.11.1\HD2\darkfluidbackoprt\analysis\image\game-25480438.bin", "rb").read()
rva = int(sys.argv[1], 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
md = Cs(CS_ARCH_X86, CS_MODE_64)
n = 0
for ins in md.disasm(img[rva:rva + 0x8000], rva):
    print(f"{ins.address:8x} {ins.mnemonic:8s} {ins.op_str}")
    n += 1
    if n >= count or (len(sys.argv) > 3 and ins.mnemonic == "int3"):
        break
