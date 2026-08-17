#!/usr/bin/env python3

import struct
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import (
    REACHING_DEFINITION_TABLE_ADDRESS,
    TARGET_SHA256,
    SnapshotReader,
)
from post_allocation_peephole import ADDI_OPCODE, replay, replay_trace


def operand(kind, flags, register, value=None):
    return {
        "kind": kind,
        "flags": flags,
        "reg": register,
        "value_signed": register if value is None else value,
        "value_unsigned": register if value is None else value,
        "object": "0x00000000",
        "raw": "",
    }


def addi(address, previous, destination, base, immediate, flags=0x201):
    return {
        "address": address,
        "next": 0,
        "previous": previous,
        "block": 0,
        "definition_index": 0,
        "opcode": ADDI_OPCODE,
        "flags": flags,
        "operands": [
            operand(0, 2, destination),
            operand(0, 1, base),
            operand(4, 0x4D, immediate, immediate),
        ],
    }


def store(address, previous, value_register, base):
    return {
        "address": address,
        "next": 0,
        "previous": previous,
        "block": 0,
        "definition_index": 0,
        "opcode": 49,
        "flags": 0x50,
        "operands": [
            operand(0, 1, value_register),
            operand(0, 1, base),
            operand(4, 0xA8, 0, 0),
        ],
    }


def snapshot(instructions, reaching=None):
    result = {
        "format": "mwcc-allocator-snapshot-v1",
        "compiler": "GC/1.2.5",
        "target_sha256": TARGET_SHA256,
        "phase": "epilogue_merge",
        "blocks": [
            {
                "address": "0x00000100",
                "next": 0,
                "index": 0,
                "successors": [],
                "execution_weight": 1,
                "flags": 0,
                "instructions": instructions,
            }
        ],
    }
    if reaching is not None:
        result["reaching_definitions"] = reaching
    return result


def chain(*instructions):
    """Link a straight-line block through the `previous` field."""

    previous = 0
    for instruction in instructions:
        instruction["previous"] = previous
        previous = int(instruction["address"], 16)
    return list(instructions)


def test_folds_a_plain_address_chain():
    a = addi("0x00001000", 0, 4, 5, 4)
    b = addi("0x00001008", 0, 4, 4, 4)
    report = replay(snapshot(chain(a, b)))
    assert report["fire_count"] == 1, report
    decision = report["decisions"][-1]
    assert decision["fires"] is True
    assert decision["rewrite"] == {
        "base_register": 5,
        "immediate": 8,
        "removes": "0x00001000",
    }


def test_rejects_when_the_destination_is_used_between():
    a = addi("0x00001000", 0, 4, 5, 4)
    used = store("0x00001004", 0, 1, 4)
    b = addi("0x00001008", 0, 4, 4, 4)
    report = replay(snapshot(chain(a, used, b)))
    assert report["fire_count"] == 0, report
    assert report["decisions"][-1]["rejected_by"] == "destination_used"


def test_rejects_when_the_base_is_redefined_between():
    a = addi("0x00001000", 0, 4, 5, 4)
    redefine = addi("0x00001004", 0, 5, 6, 8)
    b = addi("0x00001008", 0, 4, 4, 4)
    report = replay(snapshot(chain(a, redefine, b)))
    assert report["decisions"][-1]["rejected_by"] == "base_redefined"


def test_rejects_a_blocked_reaching_definition():
    a = addi("0x00001000", 0, 4, 5, 4, flags=0x281)
    b = addi("0x00001008", 0, 4, 4, 4)
    report = replay(snapshot(chain(a, b)))
    assert report["decisions"][-1]["rejected_by"] == "reaching_definition_blocked"


def test_rejects_an_out_of_range_sum():
    a = addi("0x00001000", 0, 4, 5, 0x7000)
    b = addi("0x00001008", 0, 4, 4, 0x7000)
    report = replay(snapshot(chain(a, b)))
    assert report["decisions"][-1]["rejected_by"] == "immediate_out_of_range"


def test_rejects_a_live_differing_destination():
    a = addi("0x00001000", 0, 3, 5, 4)
    b = addi("0x00001008", 0, 4, 3, 4)
    instructions = chain(a, b)
    assert replay(snapshot(instructions))["fire_count"] == 1
    report = replay(snapshot(instructions), reserved_registers=1 << 3)
    assert report["decisions"][-1]["rejected_by"] == "destination_reserved"


def test_prefers_the_captured_reaching_definition_table():
    a = addi("0x00001000", 0, 4, 5, 4)
    b = addi("0x00001008", 0, 4, 4, 4)
    b["definition_index"] = 7
    instructions = chain(a, b)
    empty = {"table_address": "0x00002000", "entries": {}}
    report = replay(snapshot(instructions, empty))
    assert report["reaching_definitions"] == "captured"
    assert report["decisions"][-1]["rejected_by"] == "no_reaching_definition"

    linked = {"table_address": "0x00002000", "entries": {"7": "0x00001000"}}
    report = replay(snapshot(instructions, linked))
    assert report["fire_count"] == 1, report


class SparseMemory:
    """Byte-addressable stand-in for the debugger's inferior memory."""

    def __init__(self):
        self.data = {}

    def write(self, address, data):
        for offset, value in enumerate(data):
            self.data[address + offset] = value

    def read(self, address, size):
        return bytes(self.data.get(address + offset, 0) for offset in range(size))


def test_trace_supplies_the_reaching_definition_the_rule_used():
    """A trace decides candidates a stage snapshot alone cannot resolve."""

    a = addi("0x00001000", 0, 4, 5, 4)
    b = addi("0x00001008", 0, 4, 4, 4)
    b["definition_index"] = 9
    instructions = chain(a, b)

    unlinked = replay(snapshot(instructions, {"table_address": None, "entries": {}}))
    assert unlinked["fire_count"] == 0

    trace = {
        "format": "mwcc-peephole-trace-v1",
        "events": [
            {
                "sequence": 0,
                "candidate": {"address": "0x00001008"},
                "live_registers": "0x00000000",
                "reaching_definition": {"address": "0x00001000"},
            }
        ],
    }
    report = replay_trace(snapshot(instructions), trace)
    assert report["reaching_definitions"] == "traced"
    assert report["fire_count"] == 1, report
    assert report["removed_addresses"] == ["0x00001000"]


def test_trace_applies_the_recorded_live_mask():
    a = addi("0x00001000", 0, 3, 5, 4)
    b = addi("0x00001008", 0, 4, 3, 4)
    trace_event = {
        "sequence": 0,
        "candidate": {"address": "0x00001008"},
        "reaching_definition": {"address": "0x00001000"},
    }
    instructions = chain(a, b)

    free = {
        "format": "mwcc-peephole-trace-v1",
        "events": [{**trace_event, "live_registers": "0x00000000"}],
    }
    assert replay_trace(snapshot(instructions), free)["fire_count"] == 1

    live = {
        "format": "mwcc-peephole-trace-v1",
        "events": [{**trace_event, "live_registers": "0x00000008"}],
    }
    report = replay_trace(snapshot(instructions), live)
    assert report["decisions"][0]["rejected_by"] == "destination_reserved"


def test_reaching_definition_table_read_tolerates_stale_indices():
    """Stages captured before the pass builds the table must not abort."""

    memory = SparseMemory()
    table = 0x2000
    memory.write(REACHING_DEFINITION_TABLE_ADDRESS, struct.pack("<I", table))
    memory.write(table + 4 + 3 * 4, struct.pack("<I", 0x1234))
    reader = SnapshotReader(memory.read)
    blocks = [
        {
            "instructions": [
                {"definition_index": 3},
                {"definition_index": 0},
                {"definition_index": 0x7FFFFFFF},
            ]
        }
    ]
    resolved = reader.reaching_definitions(blocks)
    assert resolved["entries"] == {"3": "0x00001234"}
    assert resolved["unreadable_indices"] == 1


def main():
    for name, test in sorted(globals().items()):
        if name.startswith("test_") and callable(test):
            test()
            print(f"ok {name}")


if __name__ == "__main__":
    main()
