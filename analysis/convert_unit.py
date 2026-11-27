"""Unit resource, version 1 (2024) -> 10800438 (game build 25480438). Rules found by comparing units both builds have
(analysis/FINDINGS-20240601.md):

  header +0x2C version            1 -> 10800438
  LOD group list                  each group gains a trailing u32 0xFFFFFFFF; flags keep their low byte; entries are
                                  sorted from the highest threshold down, the top max becomes 100 and the bottom min 0;
                                  the list is padded to 16 bytes and the group offsets follow
  every section after it          moves by the list's growth; the header's offsets follow
  stream info                     vertex format codes above 16 gain 4 (as HD2SDK does for 10800437)
  ending                          8 -> 16 bytes (8 zero bytes)

  convert(main_bytes) -> new main bytes      (GPU and stream data are kept as they are)
"""
import struct

HEADER_FIELDS = ["UnkRef1:Q", "BonesRef:Q", "CompositeRef:Q", "UnkRef2:Q", "StateMachineRef:Q", "HeaderData1:I", "Version:I",
                 "LODGroupList:I", "TransformInfo:I", "LightList:I", "UnkPreLightList:I", "WwiseCallback:I", "HeaderData2:8s",
                 "CustomizationInfo:I", "UnkHeader1:I", "ConnectingBoneHash:I", "BoneInfo:I", "StreamInfo:I", "Ending:I",
                 "MeshInfo:I", "HeaderUnk:Q", "Materials:I"]
OFFSET_FIELDS = ["LODGroupList", "TransformInfo", "LightList", "UnkPreLightList", "WwiseCallback", "CustomizationInfo",
                 "UnkHeader1", "ConnectingBoneHash", "BoneInfo", "StreamInfo", "Ending", "MeshInfo", "Materials"]
NEW_VERSION = 10800438


def field_offsets():
    out, o = {}, 0
    for f in HEADER_FIELDS:
        name, fmt = f.split(":")
        out[name] = (o, fmt)
        o += struct.calcsize("<" + fmt)
    return out


FIELD_AT = field_offsets()


def read_header(b):
    return {n: struct.unpack_from("<" + fmt, b, o)[0] for n, (o, fmt) in FIELD_AT.items()}


def section_end(h, total, name):
    off = h[name]
    later = sorted(v for n, v in h.items() if n in OFFSET_FIELDS and v and v > off)
    return later[0] if later else total


def convert_lod_list(data):
    count = struct.unpack_from("<I", data, 0)[0]
    starts = [struct.unpack_from("<I", data, 4 + 4 * i)[0] for i in range(count)]
    groups = []
    for i, s in enumerate(starts):
        flags, name, zero, one, n = struct.unpack_from("<IIIII", data, s)
        offs = list(struct.unpack_from(f"<{n + 1}I", data, s + 20))
        # entries are {f32 max, f32 min, ...} of any size (usually 16: count 1 and one mesh index; wider ones list more)
        entries = [bytearray(data[s + offs[e]:s + offs[e + 1]]) for e in range(n)]
        tail_at = s + offs[n]
        tail = data[tail_at:tail_at + 40]  # 7 bound floats, u32, f32 1.0, u32 0
        if len(tail) != 40:
            raise ValueError("LOD group runs past the list")
        entries.sort(key=lambda e: struct.unpack_from("<f", e, 0)[0], reverse=True)
        if entries:
            struct.pack_into("<f", entries[0], 0, 100.0)
            struct.pack_into("<f", entries[-1], 4, 0.0)
        new_offs, at = [], 20 + 4 * (n + 1)
        for e in entries:
            new_offs.append(at)
            at += len(e)
        new_offs.append(at)
        g = struct.pack("<IIIII", flags & 0xFF, name, zero, one, n) + struct.pack(f"<{n + 1}I", *new_offs)  # 0x1xx -> 0xxx
        g += b"".join(entries) + tail + struct.pack("<I", 0xFFFFFFFF)
        groups.append(g)
    head = struct.pack("<I", count)
    new_starts, at = [], 4 + 4 * count
    for g in groups:
        new_starts.append(at)
        at += len(g)
    out = head + b"".join(struct.pack("<I", x) for x in new_starts) + b"".join(groups)
    return out + b"\0" * ((-len(out)) % 16)


def convert_stream_info(data):
    """Vertex component formats above 16 gain 4. Layout as HD2SDK's StreamInfo.Serialize: the section is u32 count,
    count u32 offsets (from the section start), count u32 ids, u32; each stream is u64 id, a 320-byte table of 20-byte
    components {u32 type, u32 format, u32 index, u64}, then u64 component count."""
    b = bytearray(data)
    count = struct.unpack_from("<I", b, 0)[0]
    offs = struct.unpack_from(f"<{count}I", b, 4)
    for o in offs:
        n = struct.unpack_from("<Q", b, o + 8 + 320)[0]
        if n > 16:
            raise ValueError("unexpected stream info layout")
        for c in range(n):
            at = o + 8 + 20 * c
            fmt = struct.unpack_from("<I", b, at + 4)[0]
            if fmt > 16:
                struct.pack_into("<I", b, at + 4, fmt + 4)
    return bytes(b)


def convert_mesh_info(data):
    """Mesh records (u32 count, count u32 offsets): the flag word at record +0x24 loses bit 0x100, the change seen most
    often between the builds (0x109 -> 0x9, 0x100 -> 0x0). Everything else that differs there is re-exported content."""
    b = bytearray(data)
    count = struct.unpack_from("<I", b, 0)[0]
    if count > 4096:
        raise ValueError("unexpected mesh info layout")
    for o in struct.unpack_from(f"<{count}I", b, 4):
        if o + 0x28 <= len(b):
            v = struct.unpack_from("<I", b, o + 0x24)[0]
            struct.pack_into("<I", b, o + 0x24, v & ~0x100)
    return bytes(b)


def convert(main):
    h = read_header(main)
    if h["Version"] != 1:
        raise ValueError(f"unit version {h['Version']}, not 1")
    sections = sorted((v, n) for n, v in h.items() if n in OFFSET_FIELDS and v)
    out = bytearray(main[:sections[0][0]])
    new_at = {}
    for i, (off, name) in enumerate(sections):
        end = sections[i + 1][0] if i + 1 < len(sections) else len(main)
        piece = main[off:end]
        if name == "LODGroupList":
            piece = convert_lod_list(piece)
        elif name == "StreamInfo":
            piece = convert_stream_info(piece)
        elif name == "Ending":
            piece = piece[:8] + b"\0" * 8
        elif name == "MeshInfo":
            piece = convert_mesh_info(piece)
        new_at[name] = len(out)
        out += piece
    for name, (o, fmt) in FIELD_AT.items():
        if name in new_at:
            struct.pack_into("<I", out, o, new_at[name])
    struct.pack_into("<I", out, FIELD_AT["Version"][0], NEW_VERSION)
    return bytes(out)
