#!/usr/bin/env python3

import copy
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import TARGET_NINJI_SHA256
from stack_frame_trace import (
    StackFrameTraceError,
    compare_traces,
    format_comparison,
    format_legacy_provenance,
    format_trace,
    validate_trace,
)


def compiler_object(address):
    return {
        "address": address,
        "object_tag_00": 5,
        "kind_02": 1,
        "flags_12": 0,
        "type": {
            "kind_00": 4,
            "size_02": 4,
            "subtype_0e": 7,
        },
    }


def allocation(sequence, address, cursor_before, slot, uses_name):
    obj = compiler_object(address)
    return {
        "sequence": sequence,
        "allocator_address": "0x004ac4a0",
        "alignment_routine_address": "0x004aaa40",
        "object_address": address,
        "cursor_before": cursor_before,
        "alignment": 4,
        "size": 4,
        "slot": slot,
        "cursor_after": slot + 4,
        "object_before": obj,
        "object_after": {**obj, "stack_offset_2a": slot},
        "test_use_name": uses_name,
    }


def trace(allocations):
    return {
        "format": "mwcc-stack-frame-trace-v1",
        "compiler": "GC/1.2.5n",
        "target_sha256": TARGET_NINJI_SHA256,
        "capture_index": 7,
        "function_pointer": "0x40800000",
        "object_allocations": allocations,
        "frame_finalization": {
            "routine_address": "0x004ac240",
            "function_argument": "0x40800000",
            "checkpoints": [
                {
                    "sequence": 0,
                    "program_counter": "0x004ac240",
                    "routine_offset": "+0x0",
                    "state": {
                        "object_slot_cursor": 0x24,
                        "frame_size_0058825c": 0,
                        "linkage_size_005880cc": 8,
                        "secondary_cursor_0058712c": 0,
                    },
                },
                {
                    "sequence": 1,
                    "program_counter": "0x004ac496",
                    "routine_offset": "+0x256",
                    "state": {
                        "object_slot_cursor": 0x28,
                        "frame_size_0058825c": 0x90,
                        "linkage_size_005880cc": 8,
                        "secondary_cursor_0058712c": 0,
                    },
                },
            ],
        },
    }


def provenance(object_uses):
    instructions = []
    operands = []
    sequence = 0
    for address, mnemonics in object_uses.items():
        for mnemonic in mnemonics:
            instruction_id = f"b1:i{sequence}"
            instructions.append(
                {
                    "id": instruction_id,
                    "opcode": 0x3F if mnemonic == "ADDI" else 0x55,
                    "mnemonic": mnemonic,
                }
            )
            operands.append(
                {
                    "id": f"{instruction_id}:o0",
                    "instruction": instruction_id,
                    "index": 0,
                    "kind": 5,
                    "flags": 1,
                    "object": address,
                }
            )
            sequence += 1
    return {
        "format": "mwcc-allocator-provenance-v1",
        "compiler": "GC/1.2.5n",
        "target_sha256": TARGET_NINJI_SHA256,
        "capture_index": 7,
        "function_pointer": "0x40800000",
        "instructions": instructions,
        "operands": operands,
    }


def main():
    before = trace(
        [
            allocation(0, "0x00001000", 0x18, 0x18, "background"),
            allocation(1, "0x00001100", 0x20, 0x20, "text"),
        ]
    )
    after = trace(
        [
            allocation(0, "0x00002100", 0x10, 0x10, "text"),
            allocation(1, "0x00002000", 0x18, 0x18, "background"),
        ]
    )
    before_provenance = provenance(
        {
            "0x00001000": ["STW"],
            "0x00001100": ["ADDI", "STW"],
        }
    )
    after_provenance = provenance(
        {
            "0x00002000": ["STW"],
            "0x00002100": ["ADDI", "STW"],
        }
    )
    legacy_header = bytearray(0x32)
    legacy_header[0x2A : 0x2E] = (0x18).to_bytes(4, "little")
    before_provenance["creation_operands"] = [
        {
            "compiler_object": {
                **compiler_object("0x00001000"),
                "header": legacy_header.hex(),
            }
        }
    ]

    validate_trace(before)
    rendered = format_trace(before, before_provenance)
    assert "slot local +0x20 / SP +0x28, size 4, align 4" in rendered
    assert "ADDI[o0], STW[o0]" in rendered
    assert "0x004ac496: object_slot_cursor=0x28" in rendered
    legacy = format_legacy_provenance(before_provenance)
    assert "raw +0x2a=0x18" in legacy
    assert "final SP-relative slots were not captured" in legacy

    comparison = compare_traces(
        before,
        after,
        before_provenance,
        after_provenance,
    )
    assert comparison["unmatched_before"] == []
    assert comparison["unmatched_after"] == []
    text_match = next(
        match
        for match in comparison["matched_objects"]
        if match["before_sequence"] == 1
    )
    assert text_match["after_sequence"] == 0
    assert text_match["before_slot"] == 0x20
    assert text_match["after_slot"] == 0x10
    assert text_match["slot_delta"] == -0x10
    assert text_match["before_sp_relative_slot"] == 0x28
    assert text_match["after_sp_relative_slot"] == 0x18
    assert text_match["sp_relative_slot_delta"] == -0x10
    assert "#1 -> #0 (pcode-uses-v1): SP +0x28 -> +0x18" in format_comparison(
        comparison
    )

    ambiguous = compare_traces(before, after)
    assert ambiguous["matched_objects"] == []
    assert ambiguous["ambiguous_groups"] == [
        {
            "match_basis": "pcode-uses-v1",
            "before_sequences": [0, 1],
            "after_sequences": [0, 1],
            "before_slots": [0x18, 0x20],
            "after_slots": [0x10, 0x18],
            "before_sp_relative_slots": [0x20, 0x28],
            "after_sp_relative_slots": [0x18, 0x20],
        }
    ]
    assert "ambiguous pcode-uses-v1 group" in format_comparison(ambiguous)

    invalid = copy.deepcopy(before)
    invalid["object_allocations"][0]["cursor_after"] = 0x99
    try:
        validate_trace(invalid)
    except StackFrameTraceError as error:
        assert "next cursor" in str(error)
    else:
        raise AssertionError("invalid stack geometry passed validation")
    print("stack-frame trace tests passed")


if __name__ == "__main__":
    main()
