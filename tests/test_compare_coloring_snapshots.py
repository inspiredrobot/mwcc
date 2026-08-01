#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from allocator_snapshot import TARGET_SHA256
from compare_coloring_snapshots import compare_snapshots


def snapshot(order, nodes):
    return {
        "format": "mwcc-coloring-snapshot-v1",
        "compiler": "GC/1.2.5",
        "target_sha256": TARGET_SHA256,
        "register_class": 0,
        "register_count": 35,
        "simplify_order": order,
        "nodes": nodes,
    }


def node(register, physical, neighbors, object_address="0x00001000"):
    return {
        "address": f"0x{0x2000 + register * 0x20:08x}",
        "next": 0,
        "object": object_address,
        "spill_cost": 10,
        "virtual_register": register,
        "degree": len(neighbors),
        "physical_register": physical,
        "flags": 0,
        "neighbors": neighbors,
    }


def main():
    before = snapshot(
        [32, 33],
        [
            node(32, 28, [33]),
            node(33, 29, [32]),
        ],
    )
    after = snapshot(
        [33, 32, 34],
        [
            node(32, 29, [33], "0x00001100"),
            node(33, 28, [32]),
            node(34, 30, []),
        ],
    )

    changes = compare_snapshots(before, after)
    assert [change["virtual_register"] for change in changes] == [32, 33, 34]
    assert changes[0]["fields"]["object"] == ("0x00001000", "0x00001100")
    assert changes[0]["fields"]["physical_register"] == (28, 29)
    assert changes[0]["simplify_order"] == (0, 1)
    assert changes[1]["simplify_order"] == (1, 0)
    assert changes[2]["status"] == "added"
    print("coloring snapshot comparison tests passed")


if __name__ == "__main__":
    main()
