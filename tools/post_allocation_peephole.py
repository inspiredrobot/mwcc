#!/usr/bin/env python3
"""Replay the post-allocation ADDI combine rule over a captured PCode stage.

The post-allocation peephole driver at `0x004c60b0` dispatches per-opcode rule
lists from the table at `0x005813b0`. Opcode `0x3f` (`ADDI`) has a single
registered handler, `0x004c8d90`, which folds

    A: addi rD, rBase, immA
    B: addi rD, rD,    immB

into `addi rD, rBase, immA + immB` and drops `A`. This module reproduces that
handler's accept/reject decision from a snapshot so a rejected pair can be
explained without rerunning the compiler.

Every predicate below is read from the handler's instruction stream and named
after the field it tests. `ADDI_HANDLER_ADDRESS` is the authority; the offsets
are bound to the captured PCode record layout used by `allocator_snapshot`.
"""

import argparse
import json
from pathlib import Path

from allocator_snapshot import validate_snapshot

ADDI_HANDLER_ADDRESS = 0x004C8D90
ADDI_OPCODE = 0x3F
# `mov eax, [ebx + 0x16]; and eax, 0x80` rejects the fold outright.
BLOCKED_INSTRUCTION_FLAG = 0x80
# Operand access bits, matching `test byte [esi + 1], 2` and `..., 1`.
OPERAND_DEFINITION = 2
OPERAND_USE = 1
# `cmp byte [edi + 0x34], 4` requires a plain immediate, not a relocation.
IMMEDIATE_KIND = 4
SIGNED_IMMEDIATE_MINIMUM = -0x8000
SIGNED_IMMEDIATE_MAXIMUM = 0x7FFF


class RuleError(Exception):
    pass


def instruction_index(snapshot: dict) -> dict[str, dict]:
    """Index live instructions by address, recording block and position."""

    index = {}
    sequence = 0
    for block in snapshot["blocks"]:
        for instruction in block["instructions"]:
            index[instruction["address"]] = {
                **instruction,
                "block_index": block["index"],
                "sequence": sequence,
            }
            sequence += 1
    return index


def address_text(value: int) -> str:
    return f"0x{value:08x}"


def operand(instruction: dict, position: int) -> dict | None:
    operands = instruction.get("operands") or []
    if position >= len(operands):
        return None
    return operands[position]


def same_register(left: dict, right: dict) -> bool:
    """Compare an operand pair the way the handler does: kind byte then word."""

    return left["kind"] == right["kind"] and (left["reg"] & 0xFFFF) == (
        right["reg"] & 0xFFFF
    )


def reaching_definition(
    snapshot: dict, index: dict[str, dict], instruction: dict
) -> tuple[dict | None, str]:
    """Resolve `table[instruction.definition_index]`, or infer it.

    A capture taken before `reaching_definitions` was recorded still supports a
    useful replay, but the resolved predecessor is then this module's inference
    rather than the compiler's own table, and every caller must say so.
    """

    table = snapshot.get("reaching_definitions")
    if table is not None:
        key = str(instruction.get("definition_index", 0))
        address = table.get("entries", {}).get(key)
        if address is None:
            return None, "captured"
        return index.get(address), "captured"

    base = operand(instruction, 1)
    if base is None:
        return None, "inferred"
    address = instruction["previous"]
    while address:
        candidate = index.get(address_text(address))
        if candidate is None:
            return None, "inferred"
        destination = operand(candidate, 0)
        if (
            destination is not None
            and destination["flags"] & OPERAND_DEFINITION
            and same_register(destination, base)
        ):
            return candidate, "inferred"
        address = candidate["previous"]
    return None, "inferred"


def instructions_between(
    index: dict[str, dict], earlier: dict, later: dict
) -> list[dict] | None:
    """Walk `later.previous` back to `earlier`, exclusive at both ends."""

    span = []
    address = later["previous"]
    while address:
        if address_text(address) == earlier["address"]:
            return span
        candidate = index.get(address_text(address))
        if candidate is None:
            return None
        span.append(candidate)
        if len(span) > len(index):
            return None
        address = candidate["previous"]
    return None


def evaluate(
    snapshot: dict,
    index: dict[str, dict],
    candidate: dict,
    reserved_registers: int = 0,
) -> dict:
    """Decide whether `0x004c8d90` would fold `candidate` into its predecessor."""

    result = {
        "address": candidate["address"],
        "sequence": candidate["sequence"],
        "block_index": candidate["block_index"],
        "definition_index": candidate.get("definition_index"),
        "fires": False,
        "rejected_by": None,
        "reaching_definition": None,
        "reaching_definition_source": None,
    }
    predecessor, source = reaching_definition(snapshot, index, candidate)
    result["reaching_definition_source"] = source
    if predecessor is None:
        result["rejected_by"] = "no_reaching_definition"
        return result
    result["reaching_definition"] = predecessor["address"]

    if predecessor["opcode"] != ADDI_OPCODE:
        result["rejected_by"] = "reaching_definition_not_addi"
        return result

    predecessor_destination = operand(predecessor, 0)
    candidate_destination = operand(candidate, 0)
    predecessor_base = operand(predecessor, 1)
    predecessor_immediate = operand(predecessor, 2)
    candidate_immediate = operand(candidate, 2)
    if None in (
        predecessor_destination,
        candidate_destination,
        predecessor_base,
        predecessor_immediate,
        candidate_immediate,
    ):
        result["rejected_by"] = "operand_count"
        return result

    if not same_register(predecessor_destination, candidate_destination):
        if reserved_registers & (1 << (predecessor_destination["reg"] & 0xFFFF)):
            result["rejected_by"] = "destination_reserved"
            return result

    if predecessor["flags"] & BLOCKED_INSTRUCTION_FLAG:
        result["rejected_by"] = "reaching_definition_blocked"
        return result

    span = instructions_between(index, predecessor, candidate)
    if span is None:
        result["rejected_by"] = "predecessor_unreachable"
        return result

    for between in span:
        for field in between.get("operands") or []:
            if field["flags"] & OPERAND_DEFINITION and same_register(
                field, predecessor_base
            ):
                result["rejected_by"] = "base_redefined"
                result["conflict"] = between["address"]
                return result

    for between in span:
        for field in between.get("operands") or []:
            if field["flags"] & OPERAND_USE and same_register(
                field, predecessor_destination
            ):
                result["rejected_by"] = "destination_used"
                result["conflict"] = between["address"]
                return result

    if (
        candidate_immediate["kind"] != IMMEDIATE_KIND
        or predecessor_immediate["kind"] != IMMEDIATE_KIND
    ):
        result["rejected_by"] = "immediate_not_constant"
        return result

    total = candidate_immediate["value_signed"] + predecessor_immediate["value_signed"]
    if not SIGNED_IMMEDIATE_MINIMUM <= total <= SIGNED_IMMEDIATE_MAXIMUM:
        result["rejected_by"] = "immediate_out_of_range"
        return result

    result["fires"] = True
    result["rewrite"] = {
        "base_register": predecessor_base["reg"],
        "immediate": total,
        "removes": predecessor["address"],
    }
    return result


def replay(snapshot: dict, reserved_registers: int = 0) -> dict:
    """Evaluate the rule for every ADDI in the snapshot, in stream order."""

    validate_snapshot(snapshot)
    index = instruction_index(snapshot)
    decisions = []
    for candidate in sorted(index.values(), key=lambda item: item["sequence"]):
        if candidate["opcode"] != ADDI_OPCODE:
            continue
        decisions.append(evaluate(snapshot, index, candidate, reserved_registers))
    fired = [decision for decision in decisions if decision["fires"]]
    return {
        "format": "mwcc-post-allocation-peephole-replay-v1",
        "handler_address": address_text(ADDI_HANDLER_ADDRESS),
        "compiler": snapshot.get("compiler"),
        "capture_index": snapshot.get("capture_index"),
        "phase": snapshot.get("phase"),
        "reaching_definitions": (
            "captured" if snapshot.get("reaching_definitions") else "inferred"
        ),
        "addi_count": len(decisions),
        "fire_count": len(fired),
        "removed_addresses": [decision["rewrite"]["removes"] for decision in fired],
        "decisions": decisions,
    }


def replay_trace(snapshot: dict, trace: dict) -> dict:
    """Decide each captured invocation using the rule's own recorded inputs.

    A trace supplies the two things a stage snapshot cannot: the reaching
    definition that `0x004cc180` chose inside the pass, and the live-register
    mask the dispatcher held after the candidate. Everything else is a function
    of the instruction stream, so the outcome follows without stopping the
    compiler at each precondition.
    """

    validate_snapshot(snapshot)
    if trace.get("format") != "mwcc-peephole-trace-v1":
        raise RuleError("unsupported peephole trace format")
    index = instruction_index(snapshot)
    decisions = []
    for event in trace["events"]:
        candidate = index.get(event["candidate"]["address"])
        if candidate is None:
            decisions.append(
                {
                    "address": event["candidate"]["address"],
                    "sequence": event["sequence"],
                    "fires": False,
                    "rejected_by": "candidate_not_in_snapshot",
                }
            )
            continue
        definition = event.get("reaching_definition")
        local = dict(snapshot)
        local["reaching_definitions"] = {
            "table_address": None,
            "entries": (
                {str(candidate.get("definition_index", 0)): definition["address"]}
                if definition
                else {}
            ),
        }
        decision = evaluate(
            local,
            index,
            candidate,
            int(event["live_registers"], 0),
        )
        decision["sequence"] = event["sequence"]
        decision["reaching_definition_source"] = "traced"
        decisions.append(decision)
    fired = [decision for decision in decisions if decision["fires"]]
    return {
        "format": "mwcc-post-allocation-peephole-replay-v1",
        "handler_address": address_text(ADDI_HANDLER_ADDRESS),
        "compiler": snapshot.get("compiler"),
        "capture_index": snapshot.get("capture_index"),
        "phase": snapshot.get("phase"),
        "reaching_definitions": "traced",
        "addi_count": len(decisions),
        "fire_count": len(fired),
        "removed_addresses": [decision["rewrite"]["removes"] for decision in fired],
        "decisions": decisions,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Replay the post-allocation ADDI combine rule"
    )
    parser.add_argument("snapshot")
    parser.add_argument(
        "--trace",
        help="peephole-NNNN.json, supplying each invocation's recorded inputs",
    )
    parser.add_argument(
        "--reserved-registers",
        default="0",
        help="caller register mask consulted when the two destinations differ",
    )
    parser.add_argument("--only-fires", action="store_true")
    parser.add_argument("--output")
    arguments = parser.parse_args()

    snapshot = json.loads(Path(arguments.snapshot).read_text())
    if arguments.trace:
        report = replay_trace(
            snapshot, json.loads(Path(arguments.trace).read_text())
        )
    else:
        report = replay(snapshot, int(arguments.reserved_registers, 0))
    if arguments.only_fires:
        report["decisions"] = [
            decision for decision in report["decisions"] if decision["fires"]
        ]
    text = json.dumps(report, indent=2)
    if arguments.output:
        Path(arguments.output).write_text(text + "\n")
    else:
        print(text)


if __name__ == "__main__":
    main()
