#!/usr/bin/env python3
"""Compare two PCode stage snapshots by durable instruction identity."""

import argparse
import json
from pathlib import Path

from allocator_snapshot import validate_snapshot


def instruction_map(snapshot: dict) -> dict[str, dict]:
    result = {}
    sequence = 0
    for block in snapshot["blocks"]:
        for instruction in block["instructions"]:
            record = {
                **instruction,
                "block_index": block["index"],
                "sequence": sequence,
            }
            result[instruction["address"]] = record
            sequence += 1
    return result


def instruction_summary(instruction: dict, creation: dict | None) -> dict:
    descriptor = instruction.get("opcode_descriptor") or {}
    return {
        "address": instruction["address"],
        "sequence": instruction["sequence"],
        "block_index": instruction["block_index"],
        "opcode": instruction["opcode"],
        "mnemonic": descriptor.get("mnemonic"),
        "flags": instruction["flags"],
        "operands": [operand["raw"] for operand in instruction["operands"]],
        "creation_sequence": creation["sequence"] if creation else None,
        "creation_epoch": creation["epoch"] if creation else None,
        "lowering_call_address": creation["call_address"] if creation else None,
        "codegen_item_address": (
            creation.get("codegen_item_address") if creation else None
        ),
    }


def compare_stages(
    before: dict, after: dict, creation_trace: dict | None = None
) -> dict:
    validate_snapshot(before)
    validate_snapshot(after)
    if before["target_sha256"] != after["target_sha256"]:
        raise ValueError("PCode snapshots target different compilers")
    if before.get("capture_index") != after.get("capture_index"):
        raise ValueError("PCode snapshots have different capture indices")

    creations = {}
    if creation_trace is not None:
        if creation_trace.get("format") != "mwcc-pcode-creation-trace-v1":
            raise ValueError("unsupported PCode creation trace")
        creations = {
            event["instruction"]["address"]: event
            for event in creation_trace["events"]
        }

    before_map = instruction_map(before)
    after_map = instruction_map(after)
    before_addresses = set(before_map)
    after_addresses = set(after_map)

    removed = [
        instruction_summary(before_map[address], creations.get(address))
        for address in sorted(before_addresses - after_addresses)
    ]
    added = [
        instruction_summary(after_map[address], creations.get(address))
        for address in sorted(after_addresses - before_addresses)
    ]
    modified = []
    moved = []
    common_addresses = before_addresses & after_addresses
    before_retained_order = {
        address: index
        for index, address in enumerate(
            sorted(common_addresses, key=lambda item: before_map[item]["sequence"])
        )
    }
    after_retained_order = {
        address: index
        for index, address in enumerate(
            sorted(common_addresses, key=lambda item: after_map[item]["sequence"])
        )
    }
    for address in sorted(common_addresses):
        before_summary = instruction_summary(
            before_map[address], creations.get(address)
        )
        after_summary = instruction_summary(after_map[address], creations.get(address))
        comparable_fields = ("opcode", "flags", "operands")
        if any(
            before_summary[field] != after_summary[field]
            for field in comparable_fields
        ):
            modified.append({"before": before_summary, "after": after_summary})
        elif (
            before_retained_order[address] != after_retained_order[address]
            or before_summary["block_index"] != after_summary["block_index"]
        ):
            moved.append({"before": before_summary, "after": after_summary})

    return {
        "format": "mwcc-pcode-stage-diff-v1",
        "target_sha256": before["target_sha256"],
        "capture_index": before.get("capture_index"),
        "before_phase": before.get("phase"),
        "after_phase": after.get("phase"),
        "before_instruction_count": len(before_map),
        "after_instruction_count": len(after_map),
        "removed": removed,
        "added": added,
        "modified": modified,
        "moved": moved,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare two MWCC PCode stages")
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--creations", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    with args.before.open(encoding="utf-8") as stream:
        before = json.load(stream)
    with args.after.open(encoding="utf-8") as stream:
        after = json.load(stream)
    creation_trace = None
    if args.creations:
        with args.creations.open(encoding="utf-8") as stream:
            creation_trace = json.load(stream)
    result = compare_stages(before, after, creation_trace)
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
