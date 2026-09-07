#!/usr/bin/env python3
"""Verify the portable x86 release without third-party Python packages.

This is a packaging gate, not a substitute for a Windows 7 machine test.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def inspect(path):
    data = path.read_bytes()

    def u16(offset):
        return struct.unpack_from("<H", data, offset)[0]

    def u32(offset):
        return struct.unpack_from("<I", data, offset)[0]

    assert data[:2] == b"MZ", "Not a Windows executable"
    pe = u32(0x3C)
    assert data[pe:pe + 4] == b"PE\0\0", "Invalid PE signature"
    assert u16(pe + 4) == 0x14C, "Release must be x86 for 32-bit Windows"
    optional = pe + 24
    assert u16(optional) == 0x10B, "Release must be PE32"
    assert u16(optional + 68) == 2, "Release must be a GUI application"
    subsystem = (u16(optional + 48), u16(optional + 50))
    assert subsystem <= (6, 1), "Subsystem requires a version newer than Windows 7"
    assert u16(optional + 70) & 0x140 == 0x140, "ASLR and DEP must be enabled"
    assert u32(optional + 96 + 14 * 8) == 0, "Release must not require .NET"
    sections = []
    section_start = optional + u16(pe + 20)
    for index in range(u16(pe + 6)):
        start = section_start + index * 40
        virtual_size, virtual_address, raw_size, raw_address = struct.unpack_from("<IIII", data, start + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_address))

    def offset(rva):
        for address, size, raw in sections:
            if address <= rva < address + size:
                return raw + rva - address
        raise ValueError(f"Unmapped RVA: {rva:x}")

    def cstring(start):
        return data[start:data.index(0, start)].decode("ascii")

    imports = {}
    descriptor = offset(u32(optional + 96 + 8))
    while any(data[descriptor:descriptor + 20]):
        lookup, _, _, name_rva, address_table = struct.unpack_from("<IIIII", data, descriptor)
        name = cstring(offset(name_rva)).lower()
        entries = []
        thunk = offset(lookup or address_table)
        while u32(thunk):
            entry = u32(thunk)
            entries.append(f"ordinal:{entry & 0xFFFF}" if entry & 0x80000000 else cstring(offset(entry) + 2))
            thunk += 4
        imports[name] = entries
        descriptor += 20

    # These are Windows 7 inbox DLLs. This excludes UCRT, VC redist, MinGW
    # runtime DLLs, WebView2 and application-side dependencies.
    inbox = {"kernel32.dll", "user32.dll", "gdi32.dll", "gdiplus.dll", "winmm.dll",
             "comctl32.dll", "msvcrt.dll", "shell32.dll", "advapi32.dll", "ole32.dll"}
    assert set(imports) <= inbox, f"Non-inbox dependencies: {set(imports) - inbox}"
    # Catch common accidental newer APIs when the application/toolchain changes.
    newer = {"GetSystemTimePreciseAsFileTime", "GetDpiForWindow", "GetDpiForSystem",
             "SetProcessDpiAwarenessContext", "SetThreadDpiAwarenessContext",
             "AdjustWindowRectExForDpi", "WaitOnAddress", "WakeByAddressSingle",
             "WakeByAddressAll", "GetOverlappedResultEx", "DiscardVirtualMemory",
             "SetThreadDescription", "GetThreadDescription"}
    imported = {item for names in imports.values() for item in names}
    assert not (imported & newer), f"Newer Windows imports: {imported & newer}"
    assert b"asInvoker" in data and b"35138b9a" in data, "Missing non-admin / Windows 7 manifest"
    return {
        "file": path.name, "size_bytes": len(data), "architecture": "x86 (PE32)",
        "subsystem": f"Windows GUI {subsystem[0]}.{subsystem[1]}",
        "aslr": True, "dep": True, "external_runtime": False,
        "sha256": hashlib.sha256(data).hexdigest(), "imports": imports,
        "limitation": "Import gate only; Windows 7 runtime verification is still required.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = inspect(args.executable)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {result['architecture']}, {result['subsystem']}, {result['size_bytes']:,} bytes")
    print("System DLLs: " + ", ".join(result["imports"]))
    print("SHA256: " + result["sha256"])
