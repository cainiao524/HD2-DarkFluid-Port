"""Level resource, 2024 (format 0x00A54D49) -> game build 25480438 (0x014A1A7F).

  word 0                        0x00A54D49 -> 0x014A1A7F
  embedded unit at word 7       converted with convert_unit (it ends where the next section, word 9, starts); when its
                                size changes, every header word pointing past it moves with it

  convert(main) -> new main
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_unit as cu

OLD_FORMAT, NEW_FORMAT = 0x00A54D49, 0x014A1A7F


def convert(main):
    words = list(struct.unpack_from("<16I", main, 0))
    if words[0] != OLD_FORMAT:
        raise ValueError(f"level format {words[0]:#x}")
    unit_at, unit_end = words[7], words[9]
    if not (0 < unit_at < unit_end <= len(main)):
        raise ValueError("unexpected level layout")
    unit = main[unit_at:unit_end]
    new_unit = cu.convert(unit) if struct.unpack_from("<I", unit, 0x2C)[0] == 1 else unit
    # The unit may be followed by padding inside [unit_at, unit_end); keep the region's alignment to 16.
    grow = len(new_unit) - len(unit)
    if grow % 16:
        new_unit += b"\0" * ((-grow) % 16)
        grow = len(new_unit) - len(unit)
    out = bytearray(main[:unit_at] + new_unit + main[unit_end:])
    struct.pack_into("<I", out, 0, NEW_FORMAT)
    if grow:
        for i in range(1, 16):
            if words[i] >= unit_end and i not in (1, 2):  # words 1, 2 are counts
                struct.pack_into("<I", out, 4 * i, words[i] + grow)
    return bytes(out)
