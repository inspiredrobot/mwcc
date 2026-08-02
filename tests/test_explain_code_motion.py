#!/usr/bin/env python3

import struct
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from explain_code_motion import format_event, select_events


def main():
    constant_header = struct.pack("<d", -2.2).hex() + "00" * 8
    event = {
        "sequence": 12,
        "instruction": {
            "address": "0x1234",
            "opcode": 142,
            "opcode_descriptor": {"mnemonic": "LFS"},
            "operands": [
                {"compiler_object": None},
                {
                    "compiler_object": {
                        "register_info_26": {"header": constant_header}
                    }
                },
            ],
        },
        "block": {"index": 7, "execution_weight": 8},
        "node": {"instruction_count": 258},
        "predicate_results": {"00526d80": 1, "00526b50": 1},
        "moved": True,
        "decision_path": "direct",
    }
    trace = {"events": [event]}

    selected = select_events(trace, constant=-2.2)
    assert len(selected) == 1
    assert selected[0]["constants"] == [{"operand": 1, "value": -2.2}]
    assert select_events(trace, sequence=11) == []
    output = format_event(selected[0])
    assert "event 12: LFS" in output
    assert "block 7 weight 8" in output
    assert "00526d80=1 -> 00526b50=1" in output
    assert "moved via direct path" in output
    print("code-motion explanation tests passed")


if __name__ == "__main__":
    main()
