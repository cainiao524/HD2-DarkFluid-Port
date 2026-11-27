"""Print the non-zero fields of location items captured by the probe mod."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


def unpack_value(data: bytes, offset: int, width: int) -> int:
    if offset + width > len(data):
        return 0
    return int.from_bytes(data[offset : offset + width], "little")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log_file", type=Path)
    args = parser.parse_args()

    pending: tuple[str, str, str, str] | None = None
    for line in args.log_file.read_text(encoding="utf-8-sig").splitlines():
        if "location: set " in line:
            prefix = line.split("location: set ", 1)[1]
            set_id, rest = prefix.split(" group ", 1)
            group_id, rest = rest.split(" item ", 1)
            item_index, rest = rest.split(" of ", 1)
            item_count = rest.split(":", 1)[0]
            pending = (set_id, group_id, item_index, item_count)
            continue
        marker = " raw "
        if pending is None or marker not in line:
            continue
        set_id, group_id, item_index, item_count = pending
        raw_hex = line.split(marker, 1)[1]
        data = bytes.fromhex(raw_hex)
        print(
            f"set={set_id} group={group_id} item={item_index}/{item_count} "
            f"size={len(data):#x}"
        )
        for offset in range(0, len(data), 8):
            if not any(data[offset : offset + 8]):
                continue
            u16 = unpack_value(data, offset, 2)
            u32 = unpack_value(data, offset, 4)
            u64 = unpack_value(data, offset, 8)
            f32 = struct.unpack_from("<f", data + b"\0" * 8, offset)[0]
            print(
                f"  {offset:03x}: "
                f"u16={u16:04x} u32={u32:08x} u64={u64:016x} f32={f32:g}"
            )
        pending = None
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
