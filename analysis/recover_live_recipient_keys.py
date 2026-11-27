#!/usr/bin/env python3
"""Read the sealed-table recipient key from a running Helldivers 2 process.

This is deliberately read-only. It opens the existing process with
PROCESS_VM_READ and PROCESS_QUERY_LIMITED_INFORMATION, resolves
helldivers2.exe through the Toolhelp module snapshot, and reads the two
obfuscated 32-byte values documented for build 25480438.

The key bytes are written only to an explicit output directory under the
user's temporary folder. Standard output contains validation hashes and the
public key, never the private key.

Usage:
    python -I analysis/recover_live_recipient_keys.py
    python -I analysis/recover_live_recipient_keys.py --pid 1234
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import os
import pathlib
import struct
import sys
from ctypes import wintypes


TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x0010
MAX_PATH = 260

RVA_PRIVATE = 0x1653160
RVA_PUBLIC = 0x16532B0
OBFUSCATION_XOR = 0xEB63463F0C116E2C


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", wintypes.HMODULE),
        ("szModule", wintypes.WCHAR * 256),
        ("szExePath", wintypes.WCHAR * MAX_PATH),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)

kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32FirstW.restype = wintypes.BOOL
kernel32.Module32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32NextW.restype = wintypes.BOOL
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.ReadProcessMemory.argtypes = [
    wintypes.HANDLE,
    wintypes.LPCVOID,
    wintypes.LPVOID,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
kernel32.ReadProcessMemory.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL


def process_ids(name: str) -> list[int]:
    """Return process ids whose executable basename matches *name*."""
    TH32CS_SNAPPROCESS = 0x00000002

    class PROCESSENTRY32W(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD),
            ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD),
            ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD),
            ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD),
            ("pcPriClassBase", ctypes.c_long),
            ("dwFlags", wintypes.DWORD),
            ("szExeFile", wintypes.WCHAR * MAX_PATH),
        ]

    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32NextW.restype = wintypes.BOOL

    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), "CreateToolhelp32Snapshot failed")
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        found = []
        ok = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            if entry.szExeFile.casefold() == name.casefold():
                found.append(int(entry.th32ProcessID))
            ok = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
        return found
    finally:
        kernel32.CloseHandle(snapshot)


def module_base(pid: int, name: str) -> tuple[int, int, str]:
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), "module snapshot failed")
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Module32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            if entry.szModule.casefold() == name.casefold():
                return ctypes.addressof(entry.modBaseAddr.contents), int(entry.modBaseSize), entry.szExePath
            ok = kernel32.Module32NextW(snapshot, ctypes.byref(entry))
        raise RuntimeError(f"module {name} is not loaded in pid {pid}")
    finally:
        kernel32.CloseHandle(snapshot)


def read_process(handle: int, address: int, size: int) -> bytes:
    output = ctypes.create_string_buffer(size)
    read = ctypes.c_size_t()
    if not kernel32.ReadProcessMemory(handle, ctypes.c_void_p(address), output, size, ctypes.byref(read)):
        raise OSError(ctypes.get_last_error(), f"ReadProcessMemory {address:#x}+{size:#x} failed")
    if read.value != size:
        raise RuntimeError(f"ReadProcessMemory returned {read.value} of {size} bytes")
    return output.raw


def deobfuscate(blob: bytes) -> bytes:
    if len(blob) != 32:
        raise ValueError("recipient value must be 32 bytes")
    words = struct.unpack("<4Q", blob)
    return b"".join(struct.pack("<Q", word ^ OBFUSCATION_XOR) for word in words)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--name", default="helldivers2.exe")
    ap.add_argument("--pid", type=int)
    ap.add_argument(
        "--out-dir",
        default=str(pathlib.Path(os.environ.get("TEMP", ".")) / "hd2-darkfluid-keys"),
        help="directory for recipient.sk / recipient.pk",
    )
    args = ap.parse_args()

    pids = [args.pid] if args.pid is not None else process_ids(args.name)
    if len(pids) != 1:
        print(f"expected exactly one {args.name} process, found {pids}", file=sys.stderr)
        return 1
    pid = pids[0]

    base, module_size, module_path = module_base(pid, args.name)
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, False, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), "OpenProcess failed")
    try:
        obfuscated_sk = read_process(handle, base + RVA_PRIVATE, 32)
        obfuscated_pk = read_process(handle, base + RVA_PUBLIC, 32)
    finally:
        kernel32.CloseHandle(handle)

    sk = deobfuscate(obfuscated_sk)
    stored_pk = deobfuscate(obfuscated_pk)

    try:
        from nacl.public import PrivateKey
    except ImportError:
        print("PyNaCl is required only for public-key validation", file=sys.stderr)
        return 1
    derived_pk = bytes(PrivateKey(sk).public_key)
    if derived_pk != stored_pk:
        print("recipient public key validation FAILED", file=sys.stderr)
        return 1

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    sk_path = out_dir / "recipient.sk"
    pk_path = out_dir / "recipient.pk"
    sk_path.write_bytes(sk)
    pk_path.write_bytes(stored_pk)

    print(f"process {pid}: {module_path}")
    print(f"module base {base:#x}, size {module_size:#x}")
    print(f"recipient public key {stored_pk.hex()}")
    print(f"private-key sha256 {hashlib.sha256(sk).hexdigest()}")
    print(f"validation X25519(sk) == stored public key: PASS")
    print(f"wrote {sk_path} and {pk_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
