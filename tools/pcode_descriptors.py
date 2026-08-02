#!/usr/bin/env python3

import argparse
import json
import struct
from pathlib import Path

from allocator_snapshot import (
    PCODE_MAX_OPCODE,
    PCODE_OPCODE_DESCRIPTORS_ADDRESS,
    decode_operand_format,
)
from pe import PEFile
from verify_original import verify


def read_c_string(pe: PEFile, address: int, max_length: int = 4096) -> str:
    data = bytearray()
    for offset in range(max_length):
        value = pe.read(address + offset, 1)[0]
        if value == 0:
            return data.decode("ascii")
        data.append(value)
    raise ValueError(f"unterminated string at 0x{address:08x}")


def read_descriptor(pe: PEFile, opcode: int) -> dict:
    if opcode < 0 or opcode > PCODE_MAX_OPCODE:
        raise ValueError(f"invalid PCode opcode 0x{opcode:x}")
    raw = pe.read(PCODE_OPCODE_DESCRIPTORS_ADDRESS + opcode * 0x10, 0x10)
    mnemonic_address, format_address = struct.unpack_from("<II", raw)
    operand_format = read_c_string(pe, format_address)
    return {
        "opcode": opcode,
        "opcode_hex": f"0x{opcode:03x}",
        "mnemonic": read_c_string(pe, mnemonic_address),
        "operand_format": operand_format,
        "operand_schema": decode_operand_format(operand_format),
        "fixed_operand_count": raw[8],
        "unknown_09": raw[9],
        "flags": f"0x{struct.unpack_from('<H', raw, 0x0A)[0]:04x}",
        "encoding": f"0x{struct.unpack_from('<I', raw, 0x0C)[0]:08x}",
    }


def export(config_path: Path, opcodes: list[int] | None = None) -> dict:
    config, original = verify(config_path)
    pe = PEFile(original)
    selected = range(PCODE_MAX_OPCODE + 1) if opcodes is None else opcodes
    descriptors = [read_descriptor(pe, opcode) for opcode in selected]
    return {
        "format": "mwcc-pcode-opcodes-v1",
        "compiler": config["version"],
        "target_sha256": config["sha256"],
        "table_address": f"0x{PCODE_OPCODE_DESCRIPTORS_ADDRESS:08x}",
        "max_opcode": PCODE_MAX_OPCODE,
        "opcodes": descriptors,
    }


def parse_int(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Export the verified GC/1.2.5 PCode opcode descriptors"
    )
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--opcode", type=parse_int, action="append")
    args = parser.parse_args()

    result = export(args.config, args.opcode)
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
