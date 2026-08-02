#!/usr/bin/env python3

import argparse
import json
import math
import struct
from pathlib import Path


def operand_constant(operand):
    compiler_object = operand.get("compiler_object")
    if compiler_object is None:
        return None
    register_info = compiler_object.get("register_info_26")
    if register_info is None:
        return None
    header = register_info.get("header", "")
    if len(header) < 16:
        return None
    return struct.unpack("<d", bytes.fromhex(header[:16]))[0]


def event_constants(event):
    result = []
    for index, operand in enumerate(event["instruction"].get("operands", [])):
        value = operand_constant(operand)
        if value is not None and math.isfinite(value):
            result.append({"operand": index, "value": value})
    return result


def select_events(trace, sequence=None, constant=None, tolerance=1e-6):
    events = []
    for event in trace["events"]:
        constants = event_constants(event)
        if sequence is not None and event["sequence"] != sequence:
            continue
        if constant is not None and not any(
            math.isclose(
                item["value"], constant, rel_tol=tolerance, abs_tol=tolerance
            )
            for item in constants
        ):
            continue
        selected = dict(event)
        selected["constants"] = constants
        events.append(selected)
    return events


def format_event(event):
    instruction = event["instruction"]
    descriptor = instruction.get("opcode_descriptor") or {}
    mnemonic = descriptor.get("mnemonic", f"opcode {instruction['opcode']}")
    block = event["block"]
    node = event["node"]
    lines = [
        (
            f"event {event['sequence']}: {mnemonic} "
            f"at {instruction['address']}"
        ),
        (
            f"  block {block['index']} weight {block['execution_weight']}; "
            f"node has {node['instruction_count']} instructions"
        ),
    ]
    if event["constants"]:
        values = ", ".join(
            f"operand {item['operand']}={item['value']:.17g}"
            for item in event["constants"]
        )
        lines.append(f"  constants: {values}")
    predicates = event.get("predicate_results", {})
    if predicates:
        values = " -> ".join(
            f"{name}={value}" for name, value in predicates.items()
        )
        lines.append(f"  predicates: {values}")
    decision = "moved" if event.get("moved") else "not moved"
    if event.get("decision_path"):
        decision += f" via {event['decision_path']} path"
    lines.append(f"  decision: {decision}")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Explain loop code-motion decisions from a GDB trace"
    )
    parser.add_argument("trace", type=Path)
    parser.add_argument("--sequence", type=int)
    parser.add_argument("--constant", type=float)
    parser.add_argument("--tolerance", type=float, default=1e-6)
    args = parser.parse_args()

    trace = json.loads(args.trace.read_text(encoding="utf-8"))
    if trace.get("format") != "mwcc-code-motion-trace-v1":
        raise ValueError("input is not an mwcc-code-motion-trace-v1 trace")
    events = select_events(
        trace, args.sequence, args.constant, args.tolerance
    )
    for index, event in enumerate(events):
        if index != 0:
            print()
        print(format_event(event))
    if not events:
        raise SystemExit("no matching code-motion events")


if __name__ == "__main__":
    main()
