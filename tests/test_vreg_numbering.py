#!/usr/bin/env python3

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from vreg_numbering import (
    VregNumberingError,
    arena_of,
    classify,
    coalesce_first_of,
    stratum_for,
)


def test_stratum_boundaries():
    # Precolored physical registers.
    assert stratum_for(0, 70) == "precolored"
    assert stratum_for(31, 70) == "precolored"
    # Initial / shadow stratum lives below the coalesce boundary.
    assert stratum_for(32, 70) == "initial"
    assert stratum_for(69, 70) == "initial"
    # The first coalescable web is exactly gGPRCoalesceFirst.
    assert stratum_for(70, 70) == "coalescable"
    assert stratum_for(266, 70) == "coalescable"
    # A capture with no recorded initial stratum has everything coalescable.
    assert stratum_for(40, 32) == "coalescable"


def test_arena_tagging():
    assert arena_of("0x41678778") == "0x4167"
    assert arena_of("0x40a53bd0") == "0x40a5"
    assert arena_of("0x00000000") is None
    assert arena_of("") is None
    assert arena_of("0x41678778", width=6) == "0x416787"


def test_coalesce_first_validation():
    assert coalesce_first_of({"first": 70, "last": 1165}) == 70
    for bad in ({}, {"first": 31}, {"first": "x"}, []):
        try:
            coalesce_first_of(bad)
        except VregNumberingError:
            continue
        raise AssertionError(f"expected VregNumberingError for {bad!r}")


def _node(vr):
    return {"virtual_register": vr}


def test_classify_recompute_batch():
    # Reproduce the efAsync_Dispatch shape in miniature: an initial stratum
    # (32..69), a distinct recompute arena at the boundary (70..72), then
    # canonical main-lowering objects in the bulk arena (73..75).
    snapshot = {"nodes": [_node(vr) for vr in [0, 3, 40, 55, 70, 71, 72, 73, 200]]}
    window = {"first": 70, "last": 201}
    bindings = [
        {"phase": "before", "register": "gpr:40", "object": "0x40a04598"},
        {"phase": "before", "register": "gpr:55", "object": "0x40a08490"},
        {"phase": "before", "register": "gpr:70", "object": "0x41678778"},
        {"phase": "before", "register": "gpr:71", "object": "0x41678700"},
        {"phase": "before", "register": "gpr:72", "object": "0x41678688"},
        {"phase": "before", "register": "gpr:73", "object": "0x40a99d50"},
        {"phase": "before", "register": "gpr:200", "object": "0x40a53bd0"},
    ]

    report = classify(snapshot, window, bindings)
    assert report["coalesce_first"] == 70
    assert report["strata"]["precolored"] == [0, 3]
    assert report["strata"]["initial"] == [40, 55]
    assert report["strata"]["coalescable"] == [70, 71, 72, 73, 200]
    # The recompute batch is the run at the boundary sharing the 0x4167 arena.
    assert report["recompute_batch"] == {
        "arena": "0x4167",
        "virtual_registers": [70, 71, 72],
    }
    # The canonical load (vr200) lives in a bulk arena, not the batch.
    assert 200 not in report["recompute_batch"]["virtual_registers"]


def test_classify_without_bindings():
    snapshot = {"nodes": [_node(vr) for vr in [32, 70, 100]]}
    report = classify(snapshot, {"first": 70}, None)
    assert "recompute_batch" not in report
    assert report["strata"]["initial"] == [32]
    assert report["strata"]["coalescable"] == [70, 100]


def main():
    test_stratum_boundaries()
    test_arena_tagging()
    test_coalesce_first_validation()
    test_classify_recompute_batch()
    test_classify_without_bindings()
    print("virtual-register numbering tests passed")


if __name__ == "__main__":
    main()
