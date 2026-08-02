#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from rank_register_origins import compare_summaries, summarize_origins


def provenance(allocated_count, linked_count):
    events = []
    links = []
    registers = []
    operands = []
    instructions = []
    for index in range(allocated_count):
        events.append(
            {
                "id": f"vrc{index}",
                "register_class": "gpr",
                "allocation_kind": "temporary",
                "allocator_address": "0x004a0d67",
                "allocator_function": "Operands_ForceGPR",
                "allocator_operation": "coerce operand",
            }
        )
        if index >= linked_count:
            continue
        register_id = f"gpr:{32 + index}"
        operand_id = f"b0:i{index}:o0"
        instruction_id = f"b0:i{index}"
        links.append(
            {"register": register_id, "creation": f"vrc{index}", "role": "primary"}
        )
        registers.append({"id": register_id, "definitions": [operand_id]})
        operands.append({"id": operand_id, "instruction": instruction_id})
        instructions.append(
            {"id": instruction_id, "mnemonic": "LWZ", "opcode": 0x1e}
        )
    return {
        "capture_index": 1,
        "function_pointer": "0x1000",
        "virtual_register_creations": events,
        "register_created_by": links,
        "registers": registers,
        "operands": operands,
        "instructions": instructions,
    }


def main():
    left = summarize_origins(provenance(3, 2))
    group = left["groups"][0]
    assert group["allocated_count"] == 3
    assert group["live_count"] == 2
    assert group["dead_count"] == 1
    assert group["definition_mnemonics"] == {"LWZ": 2}
    assert group["first_live_register"] == "gpr:32"
    assert group["last_live_register"] == "gpr:33"

    right = summarize_origins(provenance(5, 4))
    comparison = compare_summaries(left, right)
    assert comparison["changes"][0]["allocated_delta"] == 2
    assert comparison["changes"][0]["live_delta"] == 2
    print("register origin ranking tests passed")


if __name__ == "__main__":
    main()
