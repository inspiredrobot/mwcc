#!/usr/bin/env python3
"""Explain one virtual register from allocator-provenance facts."""

import argparse
import json
from pathlib import Path


def explain_register(provenance: dict, register_id: str) -> dict:
    register = next(
        (
            item
            for item in provenance["registers"]
            if item["id"] == register_id
        ),
        None,
    )
    if register is None:
        raise ValueError(f"unknown register {register_id}")

    operands = {item["id"]: item for item in provenance["operands"]}
    instructions = {
        item["id"]: item for item in provenance["instructions"]
    }
    creation_links = {
        item["instruction"]: item["creation"]
        for item in provenance.get("created_by", [])
    }
    creations = {
        item["id"]: item for item in provenance.get("pcode_creations", [])
    }

    def site(operand_id: str, role: str) -> dict:
        operand = operands[operand_id]
        instruction = instructions[operand["instruction"]]
        creation_id = creation_links.get(instruction["id"])
        creation = creations.get(creation_id)
        return {
            "role": role,
            "operand": operand_id,
            "operand_index": operand["index"],
            "instruction": instruction["id"],
            "sequence": instruction["sequence"],
            "address": instruction["address"],
            "mnemonic": instruction["mnemonic"],
            "opcode": instruction["opcode"],
            "creation": creation_id,
            "creation_epoch": creation["epoch"] if creation else None,
            "lowering_call_address": (
                creation["call_address"] if creation else None
            ),
            "lowering_wrapper": creation["wrapper"] if creation else None,
        }

    sites = [site(item, "definition") for item in register["definitions"]]
    sites.extend(site(item, "use") for item in register["uses"])

    graph_states = [
        item
        for item in provenance.get("coloring_nodes", [])
        if item["register"] == register_id
    ]
    simplify_positions = [
        {
            "phase": item["phase"],
            "attempt": item["attempt"],
            "position": item["position"],
        }
        for item in provenance.get("simplify_order", [])
        if item["register"] == register_id
    ]
    object_bindings = [
        item
        for item in provenance.get("object_bindings", [])
        if item["register"] == register_id
    ]

    return {
        "format": "mwcc-register-explanation-v1",
        "capture_index": provenance.get("capture_index"),
        "function_pointer": provenance.get("function_pointer"),
        "register": register,
        "sites": sites,
        "graph_states": graph_states,
        "simplify_positions": simplify_positions,
        "object_bindings": object_bindings,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Explain one virtual register from MWCC provenance facts"
    )
    parser.add_argument("provenance", type=Path)
    parser.add_argument("register", help="register ID such as gpr:38 or fpr:265")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    with args.provenance.open(encoding="utf-8") as stream:
        provenance = json.load(stream)
    explanation = explain_register(provenance, args.register)
    text = json.dumps(explanation, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
