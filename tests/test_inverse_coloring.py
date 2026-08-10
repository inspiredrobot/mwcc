#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from inverse_coloring import (
    FORMAT,
    decode_object_name,
    decompose_pressure_additions,
    degree_hypothesis_search,
    inverse_order_search,
    replay_selection,
)
from replay.simplify_replay import replay as replay_simplify


VOLATILES = [0] + list(range(3, 13))
WEBS = [32, 56, 57, 65, 74]


def node(register, neighbors, physical=-1):
    return {
        "virtual_register": register,
        "physical_register": physical,
        "neighbors": neighbors,
    }


def snapshot():
    physical = [node(register, [], register) for register in range(32)]
    common = list(VOLATILES)
    virtual = [
        node(32, common + [56, 57, 65, 74]),
        node(56, common + [32, 57, 65, 74]),
        node(57, common + [32, 56, 65]),
        node(65, common + [32, 56, 57]),
        node(74, common + [32, 56]),
    ]
    return {
        "register_class": "gpr",
        "nodes": physical + virtual,
        "simplify_order": [74, 65, 57, 56, 32],
    }


def test_replay():
    capture = snapshot()
    colors = replay_selection(capture, capture["simplify_order"])
    assert {register: colors[register] for register in WEBS} == {
        32: 28,
        56: 29,
        57: 30,
        65: 31,
        74: 31,
    }
    order, simplify_colors = replay_simplify(capture)
    assert order == capture["simplify_order"]
    assert {register: simplify_colors[register] for register in WEBS} == {
        register: colors[register] for register in WEBS
    }


def test_coalesced_parent_blocks_root_color():
    physical = [node(register, [], register) for register in range(32)]
    capture = {
        "register_class": "gpr",
        "nodes": physical + [
            node(32, list(VOLATILES)),
            node(33, list(VOLATILES) + [40]),
            {
                **node(40, [], 32),
                "flags": 4,
            },
        ],
        "simplify_order": [32, 33],
    }
    colors = replay_selection(capture, capture["simplify_order"])
    assert colors[32] == 31
    assert colors[33] == 30


def test_inverse_search():
    capture = snapshot()
    targets = {32: 31, 56: 29, 57: 30, 65: 28, 74: 30}
    report = inverse_order_search(capture, targets)
    assert report["format"] == FORMAT
    assert report["permutations_tested"] == 120
    assert report["solution_count"] > 0
    assert report["best_colors"] == targets
    assert report["best_order"][0] == 32
    reversals = {
        (item["before"], item["after"])
        for item in report["required_reversals"]
    }
    assert (32, 74) in reversals
    assert (32, 65) in reversals
    assert (57, 65) in reversals


def test_degree_hypothesis_search():
    capture = snapshot()
    blockers = [
        {
            **node(100 + index, [], 0),
            "flags": 4,
        }
        for index in range(13)
    ]
    capture["nodes"].extend(blockers)
    blocker_registers = [item["virtual_register"] for item in blockers]
    for item in capture["nodes"]:
        if item["virtual_register"] in WEBS:
            item["neighbors"].extend(blocker_registers)

    targets = {32: 31, 56: 29, 57: 30, 65: 28, 74: 30}
    report = degree_hypothesis_search(capture, targets, 3)
    assert report["solution_count"] > 0
    assert report["best_additions"] == {32: 3, 56: 1, 57: 2}
    assert report["best_colors"] == targets
    assert report["pressure_overlap_lower_bound"] == (
        decompose_pressure_additions({32: 3, 56: 1, 57: 2})
    )
    overlap = report["pressure_overlap_lower_bound"]
    assert overlap["minimum_pressure_webs"] == 3
    assert overlap["canonical_overlap_windows"] == [
        [32, 56, 57],
        [32, 57],
        [32],
    ]
    assert overlap["required_pairwise_overlaps"] == [
        {"left": 32, "right": 56, "minimum_shared_pressure_webs": 1},
        {"left": 32, "right": 57, "minimum_shared_pressure_webs": 2},
    ]


def test_name_decode():
    prefix = bytes.fromhex("10b09b40ffffffff2304")
    suffix = b"gobj" + bytes(16)
    assert decode_object_name(
        {"opaque_value_0a_data": (prefix + suffix).hex()}
    ) == "gobj"
    assert decode_object_name(None) is None


def main():
    test_replay()
    test_coalesced_parent_blocks_root_color()
    test_inverse_search()
    test_degree_hypothesis_search()
    test_name_decode()
    print("inverse coloring tests passed")


if __name__ == "__main__":
    main()
