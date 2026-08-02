#!/usr/bin/env python3

import copy
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import TARGET_SHA256
from compare_pcode_stages import compare_stages


def instruction(address, opcode):
    return {
        "address": address,
        "next": 0,
        "previous": 0,
        "opcode": opcode,
        "flags": 0,
        "operands": [],
    }


def snapshot(phase, instructions):
    return {
        "format": "mwcc-allocator-snapshot-v1",
        "compiler": "GC/1.2.5",
        "target_sha256": TARGET_SHA256,
        "capture_index": 1,
        "function_pointer": "0x00002000",
        "phase": phase,
        "virtual_register_counts": {"gpr": 32, "fpr": 32, "vr": 32},
        "blocks": [
            {
                "address": "0x00001000",
                "next": 0,
                "index": 0,
                "successors": [],
                "execution_weight": 1,
                "flags": 0,
                "instructions": instructions,
            }
        ],
    }


def main():
    first = instruction("0x00001100", 1)
    removed = instruction("0x00001200", 2)
    changed_before = instruction("0x00001300", 3)
    changed_after = copy.deepcopy(changed_before)
    changed_after["flags"] = 4
    added = instruction("0x00001400", 4)
    before = snapshot("initial", [first, removed, changed_before])
    after = snapshot("optimized", [first, changed_after, added])
    trace = {
        "format": "mwcc-pcode-creation-trace-v1",
        "events": [
            {
                "sequence": 7,
                "epoch": "initial_lowering",
                "call_address": "0x00401000",
                "instruction": removed,
            }
        ],
        "clone_events": [
            {
                "sequence": 2,
                "epoch": "backend_optimization",
                "source_address": "0x00001100",
                "destination_address": "0x00001400",
                "call_address": "0x0052ab71",
            }
        ],
    }
    result = compare_stages(before, after, trace)
    assert result["before_instruction_count"] == 3
    assert result["after_instruction_count"] == 3
    assert result["removed"][0]["creation_sequence"] == 7
    assert result["added"][0]["address"] == "0x00001400"
    assert result["added"][0]["creation_kind"] == "optimizer_clone"
    assert result["added"][0]["derived_from_address"] == "0x00001100"
    assert result["added"][0]["lowering_call_address"] == "0x0052ab71"
    assert result["modified"][0]["after"]["flags"] == 4
    print("PCode stage comparison tests passed")


if __name__ == "__main__":
    main()
