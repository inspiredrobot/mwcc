#!/usr/bin/env python3

import struct
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import PCODE_BLOCKS_ADDRESS, SnapshotReader


class SparseMemory:
    def __init__(self):
        self.data = {}

    def write(self, address, data):
        for offset, value in enumerate(data):
            self.data[address + offset] = value

    def read(self, address, size):
        return bytes(self.data.get(address + offset, 0) for offset in range(size))


def main():
    memory = SparseMemory()
    block_address = 0x1000
    link_address = 0x1100
    instruction_address = 0x1200

    memory.write(PCODE_BLOCKS_ADDRESS, struct.pack("<I", block_address))
    memory.write(0x0058846E, struct.pack("<h", 40))
    memory.write(0x0058846C, struct.pack("<h", 35))
    memory.write(0x0058849A, struct.pack("<h", 32))

    block = bytearray(0x30)
    struct.pack_into("<I", block, 0x10, link_address)
    struct.pack_into("<I", block, 0x14, instruction_address)
    struct.pack_into("<i", block, 0x1C, 7)
    struct.pack_into("<i", block, 0x28, 3)
    struct.pack_into("<H", block, 0x2E, 0x12)
    memory.write(block_address, block)
    memory.write(link_address, struct.pack("<II", 0, block_address))

    instruction = bytearray(0x1C + 2 * 0x0C)
    struct.pack_into("<h", instruction, 0x14, 0x8B)
    struct.pack_into("<I", instruction, 0x16, 0x800)
    struct.pack_into("<h", instruction, 0x1A, 2)
    instruction[0x1C] = 0
    instruction[0x1D] = 2
    struct.pack_into("<h", instruction, 0x1E, 33)
    instruction[0x28] = 0
    instruction[0x29] = 1
    struct.pack_into("<h", instruction, 0x2A, 34)
    memory.write(instruction_address, instruction)

    snapshot = SnapshotReader(memory.read).snapshot(0xDEADBEEF, 0x004CDEF0)
    assert snapshot["virtual_register_counts"] == {"gpr": 40, "fpr": 35, "vr": 32}
    assert snapshot["function_pointer"] == "0xdeadbeef"
    assert len(snapshot["blocks"]) == 1
    captured_block = snapshot["blocks"][0]
    assert captured_block["index"] == 7
    assert captured_block["successors"] == [7]
    assert captured_block["execution_weight"] == 3
    captured_instruction = captured_block["instructions"][0]
    assert captured_instruction["opcode"] == 0x8B
    assert captured_instruction["flags"] == 0x800
    assert [operand["reg"] for operand in captured_instruction["operands"]] == [
        33,
        34,
    ]
    print("allocator snapshot tests passed")


if __name__ == "__main__":
    main()
