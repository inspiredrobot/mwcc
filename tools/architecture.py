#!/usr/bin/env python3

import argparse
import json
import struct
from pathlib import Path


PE_MACHINES = {
    0x014C: "i386",
    0x8664: "x86_64",
}

ELF_MACHINES = {
    3: "i386",
    20: "powerpc",
    21: "powerpc64",
    62: "x86_64",
}


def identify(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if data[:2] == b"MZ":
        pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise ValueError(f"{path}: invalid PE signature")
        machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
        return {
            "path": str(path),
            "format": "PE",
            "bits": 32,
            "byte_order": "little",
            "machine": PE_MACHINES.get(machine, f"unknown-0x{machine:04x}"),
            "machine_id": machine,
        }

    if data[:4] == b"\x7fELF":
        bits = {1: 32, 2: 64}.get(data[4])
        byte_order = {1: "little", 2: "big"}.get(data[5])
        if bits is None or byte_order is None:
            raise ValueError(f"{path}: unsupported ELF identification")
        endian = "<" if byte_order == "little" else ">"
        machine = struct.unpack_from(endian + "H", data, 18)[0]
        return {
            "path": str(path),
            "format": "ELF",
            "bits": bits,
            "byte_order": byte_order,
            "machine": ELF_MACHINES.get(machine, f"unknown-{machine}"),
            "machine_id": machine,
        }

    raise ValueError(f"{path}: unsupported executable or object format")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Report the machine architecture of PE and ELF files"
    )
    parser.add_argument("paths", type=Path, nargs="+")
    args = parser.parse_args()
    print(json.dumps([identify(path) for path in args.paths], indent=2))


if __name__ == "__main__":
    main()
