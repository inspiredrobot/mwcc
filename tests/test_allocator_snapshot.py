#!/usr/bin/env python3

import struct
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import (
    COALESCED_REGISTERS_ADDRESS,
    COALESCE_RANGE_ADDRESSES,
    INITIAL_OBJECT_REGISTER_LAST_ADDRESSES,
    INTERFERENCE_GRAPH_ADDRESS,
    PCODE_BLOCKS_ADDRESS,
    PCODE_OPCODE_DESCRIPTORS_ADDRESS,
    TARGET_NINJI_SHA256,
    SnapshotReader,
    coalescing_groups,
    decode_operand_format,
    virtual_register_boundary,
)


class SparseMemory:
    def __init__(self):
        self.data = {}
        self.read_sizes = []

    def write(self, address, data):
        for offset, value in enumerate(data):
            self.data[address + offset] = value

    def read(self, address, size):
        self.read_sizes.append(size)
        return bytes(self.data.get(address + offset, 0) for offset in range(size))


def main():
    assert virtual_register_boundary(
        "optimized",
        {"gpr": 40, "fpr": 35, "vr": 32},
        {"gpr": 38, "fpr": 34, "vr": 32},
    )["allocated_since_previous"] == {
        "gpr": {"first": 38, "last_exclusive": 40, "count": 2},
        "fpr": {"first": 34, "last_exclusive": 35, "count": 1},
        "vr": {"first": 32, "last_exclusive": 32, "count": 0},
    }
    assert coalescing_groups([0, 1, 2, 2]) == [
        {"root": 2, "members": [2, 3]}
    ]
    assert decode_operand_format("=r,b,m,p") == {
        "dynamic_operand_count": False,
        "operands": [
            {
                "code": "r",
                "default_access_flags": 2,
                "default_access": "definition",
                "kinds": [0],
                "role": "gpr",
            },
            {
                "code": "b",
                "default_access_flags": 1,
                "default_access": "use",
                "kinds": [0],
                "role": "gpr_or_zero",
                "access_is_contextual": True,
            },
            {
                "code": "m",
                "default_access_flags": 1,
                "default_access": "use",
                "kinds": [4, 5],
                "role": "memory_or_immediate",
                "access_is_contextual": True,
            },
            {
                "code": "p",
                "default_access_flags": 1,
                "default_access": "use",
                "kinds": [10],
                "role": "marker",
            },
        ],
    }
    assert decode_operand_format("#,+r,V") == {
        "dynamic_operand_count": True,
        "operands": [
            {
                "code": "r",
                "default_access_flags": 3,
                "default_access": "use_definition",
                "kinds": [0],
                "role": "gpr",
            },
            {
                "code": "V",
                "default_access_flags": 1,
                "default_access": "use",
                "kinds": [0],
                "role": "gpr_list",
                "expands_to": "dynamic",
            },
        ],
    }

    memory = SparseMemory()
    block_address = 0x1000
    link_address = 0x1100
    instruction_address = 0x1200

    memory.write(PCODE_BLOCKS_ADDRESS, struct.pack("<I", block_address))
    memory.write(0x0058846E, struct.pack("<h", 40))
    memory.write(0x0058846C, struct.pack("<h", 35))
    memory.write(0x0058849A, struct.pack("<h", 32))
    memory.write(
        INITIAL_OBJECT_REGISTER_LAST_ADDRESSES["gpr"], struct.pack("<h", 33)
    )
    memory.write(
        INITIAL_OBJECT_REGISTER_LAST_ADDRESSES["fpr"], struct.pack("<h", 31)
    )
    memory.write(
        INITIAL_OBJECT_REGISTER_LAST_ADDRESSES["vr"], struct.pack("<h", 31)
    )

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

    mnemonic_address = 0x1300
    format_address = 0x1310
    descriptor = struct.pack(
        "<II BBHI",
        mnemonic_address,
        format_address,
        3,
        0,
        0x0A01,
        0x7C000378,
    )
    memory.write(PCODE_OPCODE_DESCRIPTORS_ADDRESS + 0x8B * 0x10, descriptor)
    memory.write(mnemonic_address, b"MR\0")
    memory.write(format_address, b"=r,r,p\0")

    snapshot = SnapshotReader(memory.read).snapshot(0xDEADBEEF, 0x004CDEF0)
    assert snapshot["virtual_register_counts"] == {"gpr": 40, "fpr": 35, "vr": 32}
    assert snapshot["initial_object_register_last"] == {
        "gpr": 33,
        "fpr": 31,
        "vr": 31,
    }
    assert snapshot["function_pointer"] == "0xdeadbeef"
    assert len(snapshot["blocks"]) == 1
    captured_block = snapshot["blocks"][0]
    assert captured_block["index"] == 7
    assert captured_block["successors"] == [7]
    assert captured_block["execution_weight"] == 3
    captured_instruction = captured_block["instructions"][0]
    assert captured_instruction["opcode"] == 0x8B
    assert captured_instruction["opcode_descriptor"] == {
        "mnemonic": "MR",
        "operand_format": "=r,r,p",
        "fixed_operand_count": 3,
        "unknown_09": 0,
        "flags": 0x0A01,
        "encoding": "0x7c000378",
        "operand_schema": {
            "dynamic_operand_count": False,
            "operands": [
                {
                    "code": "r",
                    "default_access_flags": 2,
                    "default_access": "definition",
                    "kinds": [0],
                    "role": "gpr",
                },
                {
                    "code": "r",
                    "default_access_flags": 1,
                    "default_access": "use",
                    "kinds": [0],
                    "role": "gpr",
                },
                {
                    "code": "p",
                    "default_access_flags": 1,
                    "default_access": "use",
                    "kinds": [10],
                    "role": "marker",
                },
            ],
        },
    }
    assert captured_instruction["flags"] == 0x800
    assert [operand["reg"] for operand in captured_instruction["operands"]] == [
        33,
        34,
    ]

    graph_address = 0x2000
    node_addresses = [0x2100, 0x2140]
    coalesced_address = 0x2300
    memory.write(INTERFERENCE_GRAPH_ADDRESS, struct.pack("<I", graph_address))
    memory.write(
        COALESCED_REGISTERS_ADDRESS, struct.pack("<I", coalesced_address)
    )
    parents = list(range(34))
    parents[33] = 32
    memory.write(coalesced_address, struct.pack("<34h", *parents))
    first_address, last_address = COALESCE_RANGE_ADDRESSES[0]
    memory.write(first_address, struct.pack("<h", 32))
    memory.write(last_address, struct.pack("<h", 39))
    memory.write(graph_address + 32 * 4, struct.pack("<I", node_addresses[0]))
    memory.write(graph_address + 33 * 4, struct.pack("<I", node_addresses[1]))
    memory.write(0x0058846E, struct.pack("<h", 34))
    for index, node_address in enumerate(node_addresses):
        node = bytearray(0x18)
        next_address = node_addresses[1] if index == 0 else 0
        struct.pack_into("<I", node, 0, next_address)
        struct.pack_into("<i", node, 8, 10 + index)
        struct.pack_into("<h", node, 0x0C, 32 + index)
        struct.pack_into("<h", node, 0x0E, 1)
        struct.pack_into("<h", node, 0x10, -1)
        node[0x12] = 2
        struct.pack_into("<h", node, 0x14, 1)
        struct.pack_into("<h", node, 0x16, 33 - index)
        memory.write(node_address, node)

    coloring = SnapshotReader(memory.read).coloring_snapshot(
        0, node_addresses[0], 0x004CE2D0
    )
    assert coloring["register_count"] == 34
    assert coloring["coalesced_registers_address"] == "0x00002300"
    assert coloring["coalesced_registers"][33] == 32
    assert coloring["coalescing_groups"] == [
        {"root": 32, "members": [32, 33]}
    ]
    assert coloring["coalesce_range"] == {"first": 32, "last": 39}
    assert coloring["simplify_order"] == [32, 33]
    assert coloring["nodes"][0]["neighbors"] == [33]
    assert coloring["nodes"][1]["neighbors"] == [32]

    zero_neighbor = bytearray(0x16)
    struct.pack_into("<h", zero_neighbor, 0x0C, 32)
    memory.write(node_addresses[0], zero_neighbor)
    memory.write(graph_address + 33 * 4, struct.pack("<I", 0))
    coloring = SnapshotReader(memory.read).coloring_snapshot(0, 0)
    assert coloring["nodes"][0]["neighbors"] == []
    assert memory.read_sizes[-1] != 0

    ninji_snapshot = SnapshotReader(
        memory.read, "GC/1.2.5n", TARGET_NINJI_SHA256
    ).snapshot()
    assert ninji_snapshot["compiler"] == "GC/1.2.5n"
    assert ninji_snapshot["target_sha256"] == TARGET_NINJI_SHA256
    print("allocator snapshot tests passed")


if __name__ == "__main__":
    main()
