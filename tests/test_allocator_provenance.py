#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import TARGET_SHA256
from allocator_provenance import build_provenance
from explain_register import explain_register


def operand(kind, flags, register, raw=None):
    if raw is None:
        raw = bytes([kind, flags, register & 0xFF, register >> 8]) + bytes(8)
    return {
        "kind": kind,
        "flags": flags,
        "reg": register,
        "raw": raw.hex(),
    }


def allocator_snapshot():
    return {
        "format": "mwcc-allocator-snapshot-v1",
        "compiler": "GC/1.2.5",
        "target_sha256": TARGET_SHA256,
        "function_pointer": "0x00002000",
        "virtual_register_counts": {"gpr": 34, "fpr": 33, "vr": 32},
        "capture_index": 7,
        "blocks": [
            {
                "address": "0x00001000",
                "next": 0,
                "index": 1,
                "successors": [],
                "execution_weight": 3,
                "flags": 0,
                "instructions": [
                    {
                        "address": "0x00001100",
                        "next": 0,
                        "previous": 0,
                        "opcode": 0x8B,
                        "flags": 0x0A01,
                        "operands": [
                            operand(0, 2, 32),
                            operand(0, 5, 33),
                            operand(10, 0, 0),
                            operand(1, 2, 32),
                        ],
                    },
                    {
                        "address": "0x00001200",
                        "next": 0,
                        "previous": 0,
                        "opcode": 0x8B,
                        "flags": 0x0A01,
                        "operands": [],
                    },
                ],
            }
        ],
    }


def coloring_snapshot(phase, color):
    return {
        "format": "mwcc-coloring-snapshot-v1",
        "compiler": "GC/1.2.5",
        "target_sha256": TARGET_SHA256,
        "register_class": 0,
        "register_count": 34,
        "capture_index": 7,
        "attempt": 1,
        "phase": phase,
        "simplify_order": [32, 33] if phase == "before" else [],
        "nodes": [
            {
                "address": "0x00003000",
                "next": 0,
                "object": "0x00004000",
                "spill_cost": 6,
                "virtual_register": 32,
                "degree": 1,
                "physical_register": color,
                "flags": 2,
                "neighbors": [33],
            },
            {
                "address": "0x00003020",
                "next": 0,
                "object": "0x00000000",
                "spill_cost": 3,
                "virtual_register": 33,
                "degree": 1,
                "physical_register": 32,
                "flags": 4,
                "neighbors": [32],
            },
        ],
    }


def main():
    catalog = {
        0x8B: {
            "mnemonic": "MR",
            "operand_format": "=r,r,p",
            "flags": "0x0a01",
            "encoding": "0x7c000378",
            "operand_schema": {"dynamic_operand_count": False, "operands": []},
        }
    }
    trace = {
        "format": "mwcc-pcode-creation-trace-v1",
        "target_sha256": TARGET_SHA256,
        "capture_index": 7,
        "events": [
            {
                "sequence": 0,
                "epoch": "initial_lowering",
                "wrapper": "emit",
                "wrapper_address": "0x004a25d0",
                "caller_return_address": "0x00401005",
                "call_address": "0x00401000",
                "codegen_item_address": "0x00005000",
                "codegen_item_header": "00" * 0x12,
                "instruction": {
                    "address": "0x00001100",
                    "opcode": 0x8B,
                    "flags": 0x0A01,
                    "opcode_descriptor": catalog[0x8B],
                    "operands": [operand(0, 2, 32), operand(0, 1, 33)],
                },
            }
        ],
        "clone_events": [
            {
                "sequence": 0,
                "epoch": "backend_optimization",
                "source_address": "0x00001100",
                "destination_address": "0x00001200",
                "caller_return_address": "0x0052ab76",
                "call_address": "0x0052ab71",
                "source_instruction": {
                    "address": "0x00001100",
                    "opcode": 0x8B,
                    "opcode_descriptor": catalog[0x8B],
                    "flags": 0x0A01,
                    "operands": [operand(0, 2, 32), operand(0, 1, 33)],
                },
                "destination_instruction": {
                    "address": "0x00001200",
                    "opcode": 0x8B,
                    "opcode_descriptor": catalog[0x8B],
                    "flags": 0x0A01,
                    "operands": [operand(0, 2, 32), operand(0, 1, 33)],
                },
            }
        ],
        "unwrapped_instruction_allocations": [],
        "virtual_register_events": [
            {
                "sequence": 0,
                "epoch": "initial_lowering",
                "allocator_address": "0x004c2280",
                "register_class": "gpr",
                "allocation_kind": "single",
                "caller_return_address": "0x00402005",
                "call_address": "0x00402000",
                "object_address": "0x00004000",
                "object_before": None,
                "object_after": {
                    "kind_02": 0,
                    "register_info_26": None,
                    "register_info_2e": {
                        "physical_register_24": 32,
                        "secondary_register_26": 0,
                    },
                },
                "codegen_item_address": "0x00005000",
            },
            {
                "sequence": 1,
                "epoch": "initial_lowering",
                "allocator_address": "0x004a05b7",
                "register_class": "fpr",
                "allocation_kind": "temporary",
                "allocator_function": "Operands_ForceFPR",
                "allocator_operation": "allocate destination for an LFD load",
                "caller_return_address": None,
                "call_address": None,
                "object_address": "0x00000000",
                "object_before": None,
                "object_after": None,
                "codegen_item_address": "0x00005000",
                "primary_register": 32,
                "secondary_register": None,
            },
        ],
    }
    trace["events"][0]["instruction"]["operands"][0]["compiler_object"] = {
        "address": "0x00004000",
        "object_tag_00": 5,
        "kind_02": 1,
    }
    facts = build_provenance(
        allocator_snapshot(),
        [coloring_snapshot("before", -1), coloring_snapshot("after", 5)],
        catalog,
        trace,
    )
    assert facts["format"] == "mwcc-allocator-provenance-v1"
    assert facts["instructions"][0]["mnemonic"] == "MR"
    assert facts["operands"][0]["id"] == "b1:i0:o0"
    assert facts["registers"][0]["definitions"] == ["b1:i0:o0"]
    assert facts["registers"][1]["uses"] == ["b1:i0:o1"]
    assert facts["registers"][1]["last_uses"] == ["b1:i0:o1"]
    assert facts["interference_edges"] == [
        {
            "phase": "before",
            "attempt": 1,
            "left": "gpr:32",
            "right": "gpr:33",
        },
        {
            "phase": "after",
            "attempt": 1,
            "left": "gpr:32",
            "right": "gpr:33",
        },
    ]
    assert facts["simplify_order"][1]["register"] == "gpr:33"
    assert facts["coalesces"][0]["parent"] == "gpr:32"
    assert facts["object_bindings"][0]["object"] == "0x00004000"
    assert facts["pcode_creations"][0]["call_address"] == "0x00401000"
    assert facts["pcode_creations"][0]["codegen_item"] == "cg0"
    assert facts["creation_operands"][0]["compiler_object"]["kind_02"] == 1
    assert facts["codegen_items"][0]["capture_address"] == "0x00005000"
    assert facts["created_by"] == [
        {"instruction": "b1:i0", "creation": "c0"}
    ]
    assert facts["pcode_clones"][0]["call_address"] == "0x0052ab71"
    assert facts["derived_from"] == [
        {
            "instruction": "b1:i1",
            "source_instruction": "b1:i0",
            "source_address": "0x00001100",
            "clone": "cl0",
        }
    ]
    assert facts["creation_coverage"]["linked_live_instruction_count"] == 2
    assert facts["creation_coverage"]["unlinked_live_instructions"] == []
    assert facts["register_created_by"] == [
        {"register": "gpr:32", "creation": "vrc0", "role": "primary"},
        {"register": "fpr:32", "creation": "vrc1", "role": "primary"},
    ]
    explanation = explain_register(facts, "gpr:32")
    assert explanation["sites"][0]["mnemonic"] == "MR"
    assert explanation["sites"][0]["lowering_call_address"] == "0x00401000"
    assert explanation["virtual_register_origins"][0]["event"][
        "call_address"
    ] == "0x00402000"
    assert explanation["simplify_positions"][0]["position"] == 0
    fpr_explanation = explain_register(facts, "fpr:32")
    assert fpr_explanation["virtual_register_origins"][0]["event"][
        "allocator_address"
    ] == "0x004a05b7"
    assert fpr_explanation["virtual_register_origins"][0]["event"][
        "allocator_function"
    ] == "Operands_ForceFPR"
    print("allocator provenance tests passed")


if __name__ == "__main__":
    main()
