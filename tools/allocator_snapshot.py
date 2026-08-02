#!/usr/bin/env python3

import argparse
import json
import struct
from pathlib import Path
from typing import Callable


TARGET_SHA256 = "0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914"
TARGET_NINJI_SHA256 = (
    "ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c"
)
SUPPORTED_TARGETS = {
    TARGET_SHA256: "GC/1.2.5",
    TARGET_NINJI_SHA256: "GC/1.2.5n",
}
PCODE_BLOCKS_ADDRESS = 0x00587C74
PCODE_OPCODE_DESCRIPTORS_ADDRESS = 0x005654B0
PCODE_MAX_OPCODE = 0x01D1
INTERFERENCE_GRAPH_ADDRESS = 0x00587E3C
VIRTUAL_REGISTER_COUNT_ADDRESSES = {
    "gpr": 0x0058846E,
    "fpr": 0x0058846C,
    "vr": 0x0058849A,
}

PCODE_FORMAT_OPERANDS = {
    "?": {"kinds": [], "role": "unsupported"},
    "t": {"kinds": [], "role": "unsupported"},
    "b": {"kinds": [0], "role": "gpr_or_zero", "access_is_contextual": True},
    "r": {"kinds": [0], "role": "gpr"},
    "f": {"kinds": [1], "role": "fpr"},
    "S": {"kinds": [2], "role": "special_register"},
    "C": {"kinds": [2], "role": "special_register_1"},
    "L": {"kinds": [2], "role": "special_register_2"},
    "X": {"kinds": [2], "role": "special_register_0"},
    "c": {"kinds": [3], "role": "condition_register"},
    "Y": {"kinds": [3], "role": "all_condition_registers", "expands_to": 8},
    "Z": {"kinds": [3], "role": "condition_register_0"},
    "i": {"kinds": [4], "role": "immediate"},
    "m": {
        "kinds": [4, 5],
        "role": "memory_or_immediate",
        "access_is_contextual": True,
    },
    "M": {
        "kinds": [4, 5],
        "role": "memory_or_immediate",
        "access_is_contextual": True,
    },
    "l": {
        "kinds": [5, 6],
        "role": "object_or_direct_label",
        "access_is_contextual": True,
    },
    "v": {"kinds": [9], "role": "vector_register"},
    "V": {"kinds": [0], "role": "gpr_list", "expands_to": "dynamic"},
    "p": {"kinds": [10], "role": "marker"},
}


def decode_operand_format(operand_format: str) -> dict:
    access_flags = 1
    operands = []
    for code in operand_format:
        if code in "#,":
            continue
        if code == "=":
            access_flags = 2
            continue
        if code == "+":
            access_flags = 3
            continue
        operand = PCODE_FORMAT_OPERANDS.get(code)
        if operand is None:
            raise SnapshotError(f"unknown PCode operand format code {code!r}")
        decoded = {
            "code": code,
            "default_access_flags": access_flags,
            "default_access": {1: "use", 2: "definition", 3: "use_definition"}[
                access_flags
            ],
            **operand,
        }
        operands.append(decoded)
        access_flags = 1
    return {
        "dynamic_operand_count": operand_format.startswith("#"),
        "operands": operands,
    }


class SnapshotError(ValueError):
    pass


class SnapshotReader:
    def __init__(
        self,
        read_memory: Callable[[int, int], bytes],
        compiler: str = "GC/1.2.5",
        target_sha256: str = TARGET_SHA256,
    ):
        if SUPPORTED_TARGETS.get(target_sha256) != compiler:
            raise SnapshotError("unsupported compiler snapshot identity")
        self.read_memory = read_memory
        self.compiler = compiler
        self.target_sha256 = target_sha256
        self._opcode_descriptors = {}

    def _read(self, address: int, size: int) -> bytes:
        if size == 0:
            return b""
        data = bytes(self.read_memory(address, size))
        if len(data) != size:
            raise SnapshotError(
                f"short read at 0x{address:08x}: expected {size}, got {len(data)}"
            )
        return data

    def u8(self, address: int) -> int:
        return self._read(address, 1)[0]

    def raw(self, address: int, size: int) -> bytes:
        return self._read(address, size)

    def s16(self, address: int) -> int:
        return struct.unpack("<h", self._read(address, 2))[0]

    def u16(self, address: int) -> int:
        return struct.unpack("<H", self._read(address, 2))[0]

    def u32(self, address: int) -> int:
        return struct.unpack("<I", self._read(address, 4))[0]

    def s32(self, address: int) -> int:
        return struct.unpack("<i", self._read(address, 4))[0]

    def c_string(self, address: int, max_length: int = 4096) -> str:
        if address == 0:
            return ""
        data = bytearray()
        while len(data) < max_length:
            chunk = self._read(address + len(data), min(64, max_length - len(data)))
            terminator = chunk.find(b"\0")
            if terminator >= 0:
                data.extend(chunk[:terminator])
                return data.decode("ascii", errors="replace")
            data.extend(chunk)
        raise SnapshotError(f"unterminated string at 0x{address:08x}")

    def opcode_descriptor(self, opcode: int) -> dict:
        if opcode < 0 or opcode > PCODE_MAX_OPCODE:
            raise SnapshotError(f"invalid PCode opcode 0x{opcode:x}")
        cached = self._opcode_descriptors.get(opcode)
        if cached is not None:
            return cached
        raw = self._read(PCODE_OPCODE_DESCRIPTORS_ADDRESS + opcode * 0x10, 0x10)
        mnemonic_address, format_address = struct.unpack_from("<II", raw)
        descriptor = {
            "mnemonic": self.c_string(mnemonic_address),
            "operand_format": self.c_string(format_address),
            "fixed_operand_count": raw[8],
            "unknown_09": raw[9],
            "flags": struct.unpack_from("<H", raw, 0x0A)[0],
            "encoding": f"0x{struct.unpack_from('<I', raw, 0x0C)[0]:08x}",
        }
        descriptor["operand_schema"] = decode_operand_format(
            descriptor["operand_format"]
        )
        self._opcode_descriptors[opcode] = descriptor
        return descriptor

    def operand(self, address: int) -> dict:
        raw = self._read(address, 0x0C)
        return {
            "kind": raw[0],
            "flags": raw[1],
            "reg": struct.unpack_from("<h", raw, 2)[0],
            "value_signed": struct.unpack_from("<i", raw, 2)[0],
            "value_unsigned": struct.unpack_from("<I", raw, 2)[0],
            "object": f"0x{struct.unpack_from('<I', raw, 6)[0]:08x}",
            "raw": raw.hex(),
        }

    def instruction(self, address: int) -> dict:
        header = self._read(address, 0x1C)
        opcode = struct.unpack_from("<h", header, 0x14)[0]
        operand_count = struct.unpack_from("<h", header, 0x1A)[0]
        if operand_count < 0 or operand_count > 4096:
            raise SnapshotError(
                f"invalid operand count {operand_count} at 0x{address:08x}"
            )
        return {
            "address": f"0x{address:08x}",
            "next": struct.unpack_from("<I", header, 0)[0],
            "previous": struct.unpack_from("<I", header, 4)[0],
            "opcode": opcode,
            "opcode_descriptor": self.opcode_descriptor(opcode),
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

    def interference_node(self, address: int) -> dict:
        header = self._read(address, 0x16)
        neighbor_count = struct.unpack_from("<h", header, 0x14)[0]
        if neighbor_count < 0 or neighbor_count > 32767:
            raise SnapshotError(
                f"invalid neighbor count {neighbor_count} at 0x{address:08x}"
            )
        neighbor_data = self._read(address + 0x16, neighbor_count * 2)
        return {
            "address": f"0x{address:08x}",
            "next": struct.unpack_from("<I", header, 0)[0],
            "object": f"0x{struct.unpack_from('<I', header, 4)[0]:08x}",
            "spill_cost": struct.unpack_from("<i", header, 8)[0],
            "virtual_register": struct.unpack_from("<h", header, 0x0C)[0],
            "degree": struct.unpack_from("<h", header, 0x0E)[0],
            "physical_register": struct.unpack_from("<h", header, 0x10)[0],
            "flags": header[0x12],
            "neighbors": [
                struct.unpack_from("<h", neighbor_data, index * 2)[0]
                for index in range(neighbor_count)
            ],
        }

    def coloring_snapshot(
        self, reg_class: int, simplify_stack: int, program_counter: int = 0
    ) -> dict:
        count_name = {0: "gpr", 1: "fpr", 9: "vr"}.get(reg_class)
        if count_name is None:
            raise SnapshotError(f"unsupported register class {reg_class}")
        register_count = self.s16(VIRTUAL_REGISTER_COUNT_ADDRESSES[count_name])
        graph_address = self.u32(INTERFERENCE_GRAPH_ADDRESS)
        nodes = []
        nodes_by_address = {}
        for reg in range(register_count):
            node_address = self.u32(graph_address + reg * 4)
            if node_address == 0:
                continue
            node = self.interference_node(node_address)
            nodes.append(node)
            nodes_by_address[node_address] = node

        simplify_order = []
        seen = set()
        while simplify_stack != 0:
            if simplify_stack in seen:
                raise SnapshotError(
                    f"simplify-stack cycle at 0x{simplify_stack:08x}"
                )
            seen.add(simplify_stack)
            node = nodes_by_address.get(simplify_stack)
            if node is None:
                raise SnapshotError(
                    f"simplify-stack node 0x{simplify_stack:08x} is not in graph"
                )
            simplify_order.append(node["virtual_register"])
            simplify_stack = node["next"]

        snapshot = {
            "format": "mwcc-coloring-snapshot-v1",
            "compiler": self.compiler,
            "target_sha256": self.target_sha256,
            "program_counter": f"0x{program_counter:08x}",
            "register_class": reg_class,
            "register_count": register_count,
            "simplify_order": simplify_order,
            "nodes": nodes,
        }
        validate_coloring_snapshot(snapshot)
        return snapshot

    def snapshot(self, function_pointer: int = 0, program_counter: int = 0) -> dict:
        snapshot = {
            "format": "mwcc-allocator-snapshot-v1",
            "compiler": self.compiler,
            "target_sha256": self.target_sha256,
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
    target_sha256 = snapshot.get("target_sha256")
    if SUPPORTED_TARGETS.get(target_sha256) != snapshot.get("compiler"):
        raise SnapshotError("snapshot does not identify a verified compiler target")

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


def validate_coloring_snapshot(snapshot: dict) -> None:
    if snapshot.get("format") != "mwcc-coloring-snapshot-v1":
        raise SnapshotError("unsupported coloring snapshot format")
    target_sha256 = snapshot.get("target_sha256")
    if SUPPORTED_TARGETS.get(target_sha256) != snapshot.get("compiler"):
        raise SnapshotError("snapshot does not identify a verified compiler target")
    nodes = snapshot.get("nodes")
    if not isinstance(nodes, list):
        raise SnapshotError("nodes must be a list")
    registers = {node.get("virtual_register") for node in nodes}
    if len(registers) != len(nodes):
        raise SnapshotError("virtual-register nodes must be unique")
    for node in nodes:
        if not set(node.get("neighbors", ())).issubset(registers):
            raise SnapshotError(
                f"register {node.get('virtual_register')} has an unknown neighbor"
            )
    if not set(snapshot.get("simplify_order", ())).issubset(registers):
        raise SnapshotError("simplify order contains an unknown register")


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate an allocator snapshot")
    parser.add_argument("snapshot", type=Path)
    args = parser.parse_args()
    with args.snapshot.open(encoding="utf-8") as stream:
        snapshot = json.load(stream)
    if snapshot.get("format") == "mwcc-allocator-snapshot-v1":
        validate_snapshot(snapshot)
        print(f"Validated {len(snapshot['blocks'])} PCode blocks")
    else:
        validate_coloring_snapshot(snapshot)
        print(f"Validated {len(snapshot['nodes'])} interference nodes")


if __name__ == "__main__":
    main()
