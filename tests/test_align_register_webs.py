#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from align_register_webs import FORMAT, align_register_webs


def reg_operand(operand_id, instruction, index, register, flags):
    return {
        "id": operand_id,
        "instruction": instruction,
        "index": index,
        "kind": 0,
        "flags": flags,
        "register_class": "gpr",
        "register": register,
        "is_use": bool(flags & 1),
        "is_definition": bool(flags & 2),
        "is_last_use": bool(flags & 4),
    }


def instruction(instruction_id, sequence, opcode, mnemonic):
    block, item = instruction_id.split(":")
    return {
        "id": instruction_id,
        "block": block,
        "block_instruction_index": int(item[1:]),
        "sequence": sequence,
        "opcode": opcode,
        "mnemonic": mnemonic,
    }


def register(register, occurrences, definitions, uses):
    return {
        "id": f"gpr:{register}",
        "class": "gpr",
        "register": register,
        "is_virtual": True,
        "definitions": definitions,
        "uses": uses,
        "last_uses": [],
        "occurrences": occurrences,
    }


def coloring_node(register, color, neighbors):
    return {
        "phase": "after",
        "attempt": 1,
        "register": f"gpr:{register}",
        "color_or_parent": color,
        "flags": 2,
        "spill_cost": 1,
        "is_coalesced": False,
        "neighbors": [f"gpr:{neighbor}" for neighbor in neighbors],
    }


def int_object():
    return {
        "object_tag_00": 5,
        "kind_02": 1,
        "flags_12": 0,
        "type": {
            "kind_00": 1,
            "size_02": 4,
            "flags_0a": 8,
            "subtype_0e": 11,
        },
    }


def capture(height, size, extra, window_first, carrier=False):
    instructions = [
        instruction("b1:i0", 0, 34, "LWZ"),
        instruction("b1:i1", 1, 137, "LI"),
        instruction("b1:i2", 2, 74, "MULLW"),
        instruction("b1:i3", 3, 60, "ADD"),
        instruction(
            "b1:i4",
            4,
            95 if carrier else 116,
            "RLWINM" if carrier else "SUBF",
        ),
    ]
    operands = [
        reg_operand("b1:i0:o0", "b1:i0", 0, height, 2),
        reg_operand("b1:i1:o0", "b1:i1", 0, size, 2),
        reg_operand("b1:i2:o2", "b1:i2", 2, height, 1),
        reg_operand("b1:i3:o0", "b1:i3", 0, size, 2),
        reg_operand("b1:i3:o1", "b1:i3", 1, size, 1),
        reg_operand("b1:i4:o0", "b1:i4", 0, extra, 2),
    ]
    registers = [
        register(height, ["b1:i0:o0", "b1:i2:o2"], ["b1:i0:o0"], ["b1:i2:o2"]),
        register(
            size,
            ["b1:i1:o0", "b1:i3:o0", "b1:i3:o1"],
            ["b1:i1:o0", "b1:i3:o0"],
            ["b1:i3:o1"],
        ),
        register(extra, ["b1:i4:o0"], ["b1:i4:o0"], []),
    ]
    calls = {
        "b1:i0": "0x00401000",
        "b1:i1": "0x00402000",
        "b1:i2": "0x00403000",
        "b1:i3": "0x00404000",
        "b1:i4": "0x00405000" if carrier else "0x00406000",
    }
    pcode_creations = []
    created_by = []
    for index, item in enumerate(instructions):
        creation_id = f"c{index}"
        pcode_creations.append(
            {
                "id": creation_id,
                "opcode": item["opcode"],
                "wrapper": "emit",
                "call_address": calls[item["id"]],
                "epoch": "initial_lowering",
            }
        )
        created_by.append({"instruction": item["id"], "creation": creation_id})

    virtual_register_creations = []
    register_created_by = []
    for index, (number, call) in enumerate(
        ((height, "0x00410000"), (size, "0x00420000"), (extra, "0x00430000"))
    ):
        creation_id = f"vrc{index}"
        virtual_register_creations.append(
            {
                "id": creation_id,
                "register_class": "gpr",
                "allocation_kind": "single",
                "allocator_address": "0x004c2280",
                "allocator_operation_category": "object_allocation",
                "call_address": call,
                "object_after": int_object(),
            }
        )
        register_created_by.append(
            {
                "register": f"gpr:{number}",
                "creation": creation_id,
                "role": "primary",
            }
        )

    height_neighbors = [4, size, extra] if carrier else [3, size, extra]
    return {
        "format": "mwcc-allocator-provenance-v1",
        "capture_index": 1,
        "function_pointer": "0x1000",
        "instructions": instructions,
        "operands": operands,
        "registers": registers,
        "pcode_creations": pcode_creations,
        "created_by": created_by,
        "pcode_clones": [],
        "derived_from": [],
        "virtual_register_creations": virtual_register_creations,
        "register_created_by": register_created_by,
        "coloring_nodes": [
            coloring_node(height, 5 if carrier else 8, height_neighbors),
            coloring_node(size, 29, [height]),
            coloring_node(extra, 7, [height]),
        ],
        "simplify_order": [
            {
                "phase": "before",
                "attempt": 1,
                "position": position,
                "register": f"gpr:{number}",
            }
            for position, number in enumerate((size, extra, height))
        ],
        "virtual_register_boundaries": [
            {
                "phase": "initial",
                "initial_object_register_last": {"gpr": 31},
            }
        ],
        "coalescing_windows": [
            {
                "phase": "after",
                "register_class": "gpr",
                "first": window_first,
                "last": 40,
            }
        ],
    }


def empty_capture(registers):
    return {
        "format": "mwcc-allocator-provenance-v1",
        "instructions": [],
        "operands": [],
        "registers": [register(number, [], [], []) for number in registers],
    }


def main():
    baseline = capture(height=32, size=33, extra=36, window_first=34)
    candidate = capture(height=35, size=34, extra=32, window_first=35, carrier=True)
    report = align_register_webs(baseline, candidate, register_class="gpr")
    assert report["format"] == FORMAT
    assert report["summary"] == {
        "matched": 2,
        "ambiguous": 0,
        "inserted": 1,
        "deleted": 1,
    }

    mappings = {item["old_register"]: item for item in report["mappings"]}
    height = mappings["gpr:32"]
    assert height["new_register"] == "gpr:35"
    assert height["confidence"]["label"] == "high"
    assert height["changes"]["physical_color"] == {"old": 8, "new": 5}
    assert height["changes"]["allocation_stratum"] == {
        "old": "pre_coalescing_window",
        "new": "coalescing_window",
    }
    assert height["changes"]["graph_edges"]["removed"] == [
        {"old": "gpr:3", "expected_new": "gpr:3"}
    ]
    assert height["changes"]["graph_edges"]["added"] == [
        {"new": "gpr:4", "expected_old": "gpr:4"}
    ]
    assert mappings["gpr:33"]["new_register"] == "gpr:34"
    assert report["deleted"][0]["register"] == "gpr:36"
    assert report["inserted"][0]["register"] == "gpr:32"

    ambiguous_report = align_register_webs(
        empty_capture([32]), empty_capture([33, 34]), register_class="gpr"
    )
    assert ambiguous_report["summary"] == {
        "matched": 0,
        "ambiguous": 1,
        "inserted": 1,
        "deleted": 0,
    }
    ambiguity = ambiguous_report["ambiguous"][0]
    assert ambiguity["old_register"] == "gpr:32"
    assert ambiguity["confidence"]["margin"] == 0.0
    assert [item["register"] for item in ambiguity["candidates"]] == [
        "gpr:33",
        "gpr:34",
    ]
    print("semantic register-web alignment tests passed")


if __name__ == "__main__":
    main()
