#!/usr/bin/env python3

import argparse
import json
import struct
from pathlib import Path
from typing import Callable


TARGET_SHA256 = "0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914"
PCODE_BLOCKS_ADDRESS = 0x00587C74
VIRTUAL_REGISTER_COUNT_ADDRESSES = {
    "gpr": 0x0058846E,
    "fpr": 0x0058846C,
    "vr": 0x0058849A,
}


class SnapshotError(ValueError):
    pass


class SnapshotReader:
    def __init__(self, read_memory: Callable[[int, int], bytes]):
        self.read_memory = read_memory

    def _read(self, address: int, size: int) -> bytes:
        data = bytes(self.read_memory(address, size))
        if len(data) != size:
            raise SnapshotError(
                f"short read at 0x{address:08x}: expected {size}, got {len(data)}"
            )
        return data

    def u8(self, address: int) -> int:
        return self._read(address, 1)[0]

    def s16(self, address: int) -> int:
        return struct.unpack("<h", self._read(address, 2))[0]

    def u32(self, address: int) -> int:
        return struct.unpack("<I", self._read(address, 4))[0]

    def s32(self, address: int) -> int:
        return struct.unpack("<i", self._read(address, 4))[0]

    def operand(self, address: int) -> dict:
        raw = self._read(address, 0x0C)
        return {
            "kind": raw[0],
            "flags": raw[1],
            "reg": struct.unpack_from("<h", raw, 2)[0],
            "raw": raw.hex(),
        }

    def instruction(self, address: int) -> dict:
        header = self._read(address, 0x1C)
        operand_count = struct.unpack_from("<h", header, 0x1A)[0]
        if operand_count < 0 or operand_count > 4096:
            raise SnapshotError(
                f"invalid operand count {operand_count} at 0x{address:08x}"
            )
        return {
            "address": f"0x{address:08x}",
            "next": struct.unpack_from("<I", header, 0)[0],
            "previous": struct.unpack_from("<I", header, 4)[0],
            "opcode": struct.unpack_from("<h", header, 0x14)[0],
            "flags": struct.unpack_from("<I", header, 0x16)[0],
            "operands": [
                self.operand(address + 0x1C + index * 0x0C)
                for index in range(operand_count)
            ],
        }

    def instruction_list(self, address: int) -> list[dict]:
        instructions = []
        seen = set()
        while address != 0:
            if address in seen:
                raise SnapshotError(f"instruction-list cycle at 0x{address:08x}")
            if len(seen) >= 100000:
                raise SnapshotError("instruction-list limit exceeded")
            seen.add(address)
            instruction = self.instruction(address)
            instructions.append(instruction)
            address = instruction["next"]
        return instructions

    def successor_indices(self, address: int) -> list[int]:
        successors = []
        seen = set()
        while address != 0:
            if address in seen:
                raise SnapshotError(f"block-link cycle at 0x{address:08x}")
            seen.add(address)
            block_address = self.u32(address + 4)
            successors.append(self.s32(block_address + 0x1C))
            address = self.u32(address)
        return successors

    def block(self, address: int) -> dict:
        header = self._read(address, 0x30)
        instruction_address = struct.unpack_from("<I", header, 0x14)[0]
        return {
            "address": f"0x{address:08x}",
            "next": struct.unpack_from("<I", header, 0)[0],
            "index": struct.unpack_from("<i", header, 0x1C)[0],
            "successors": self.successor_indices(
                struct.unpack_from("<I", header, 0x10)[0]
            ),
            "execution_weight": struct.unpack_from("<i", header, 0x28)[0],
            "flags": struct.unpack_from("<H", header, 0x2E)[0],
            "instructions": self.instruction_list(instruction_address),
        }

    def blocks(self) -> list[dict]:
        blocks = []
        address = self.u32(PCODE_BLOCKS_ADDRESS)
        seen = set()
        while address != 0:
            if address in seen:
                raise SnapshotError(f"block-list cycle at 0x{address:08x}")
            if len(seen) >= 100000:
                raise SnapshotError("block-list limit exceeded")
            seen.add(address)
            block = self.block(address)
            blocks.append(block)
            address = block["next"]
        return blocks

    def snapshot(self, function_pointer: int = 0, program_counter: int = 0) -> dict:
        snapshot = {
            "format": "mwcc-allocator-snapshot-v1",
            "compiler": "GC/1.2.5",
            "target_sha256": TARGET_SHA256,
            "function_pointer": f"0x{function_pointer:08x}",
            "program_counter": f"0x{program_counter:08x}",
            "virtual_register_counts": {
                name: self.s16(address)
                for name, address in VIRTUAL_REGISTER_COUNT_ADDRESSES.items()
            },
            "blocks": self.blocks(),
        }
        validate_snapshot(snapshot)
        return snapshot


def validate_snapshot(snapshot: dict) -> None:
    if snapshot.get("format") != "mwcc-allocator-snapshot-v1":
        raise SnapshotError("unsupported snapshot format")
    if snapshot.get("target_sha256") != TARGET_SHA256:
        raise SnapshotError("snapshot does not identify the verified GC/1.2.5 target")

    blocks = snapshot.get("blocks")
    if not isinstance(blocks, list):
        raise SnapshotError("blocks must be a list")
    indices = [block.get("index") for block in blocks]
    if len(indices) != len(set(indices)):
        raise SnapshotError("block indices must be unique")
    index_set = set(indices)
    for block in blocks:
        if not set(block.get("successors", ())).issubset(index_set):
            raise SnapshotError(f"block {block.get('index')} has an unknown successor")
        for instruction in block.get("instructions", ()):
            if not isinstance(instruction.get("operands"), list):
                raise SnapshotError("instruction operands must be a list")


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate an allocator snapshot")
    parser.add_argument("snapshot", type=Path)
    args = parser.parse_args()
    with args.snapshot.open(encoding="utf-8") as stream:
        snapshot = json.load(stream)
    validate_snapshot(snapshot)
    print(f"Validated {len(snapshot['blocks'])} PCode blocks")


if __name__ == "__main__":
    main()
