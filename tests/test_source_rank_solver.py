#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from source_rank_solver import FORMAT, search_snapshots


VOLATILES = [0] + list(range(3, 13))
WEBS = [32, 56, 57, 65, 74]


def node(register, neighbors, physical=-1, object_address="0x00000000"):
    return {
        "virtual_register": register,
        "physical_register": physical,
        "neighbors": neighbors,
        "object": object_address,
    }


def captures():
    physical = [node(register, [], register) for register in range(32)]
    common = list(VOLATILES)
    virtual = [
        node(32, common + [56, 57, 65, 74], object_address="0x00001000"),
        node(56, common + [32, 57, 65, 74], object_address="0x00002000"),
        node(57, common + [32, 56, 65], object_address="0x00003000"),
        node(65, common + [32, 56, 57], object_address="0x00004000"),
        node(74, common + [32, 56], object_address="0x00005000"),
    ]
    before = {
        "register_class": "gpr",
        "nodes": physical + virtual,
        "simplify_order": [74, 65, 57, 56, 32],
    }
    colors = {32: 28, 56: 29, 57: 30, 65: 31, 74: 31}
    after = {
        "register_class": "gpr",
        "nodes": physical + [
            {**item, "physical_register": colors[item["virtual_register"]]}
            for item in virtual
        ],
        "simplify_order": before["simplify_order"],
    }
    pcode = {
        "blocks": [
            {
                "instructions": [
                    {
                        "opcode_descriptor": {"mnemonic": "LI"},
                        "operands": [
                            {"kind": 0, "reg": register, "flags": 2}
                        ],
                    }
                    for register in WEBS
                ]
            }
        ]
    }
    return before, after, pcode


def main():
    before, after, pcode = captures()
    targets = {32: 28, 56: 31, 57: 30, 65: 29, 74: 30}
    report = search_snapshots(
        before,
        after,
        pcode,
        targets,
        max_permutations=1000,
    )
    assert report["format"] == FORMAT
    assert report["status"] == "reachable"
    assert report["conclusion_proven"]
    assert report["search_exact"]
    assert not report["search_complete"]
    assert report["best_score"] == len(targets)
    assert report["witness"]["colors"] == targets
    assert report["fixed_object_registers"] == [32]
    assert report["baseline_replay_validation"] == {
        "checked": 37,
        "matched": 37,
        "mismatches": [],
    }

    impossible = {32: 99}
    incomplete = search_snapshots(
        before,
        after,
        pcode,
        impossible,
        max_permutations=1,
        samples=1,
    )
    assert incomplete["status"] == "not_found"
    assert not incomplete["conclusion_proven"]
    assert not incomplete["search_complete"]

    complete = search_snapshots(
        before,
        after,
        pcode,
        impossible,
        max_permutations=1000,
    )
    assert complete["status"] == "unreachable"
    assert complete["conclusion_proven"]
    assert complete["search_complete"]
    print("source-rank solver tests passed")


if __name__ == "__main__":
    main()
