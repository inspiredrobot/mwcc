#!/usr/bin/env python3

import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from param_rerank_reach import FORMAT, analyze


VOLATILES = [0] + list(range(3, 13))


def node(register, neighbors, physical=-1):
    return {
        "virtual_register": register,
        "physical_register": physical,
        "neighbors": neighbors,
    }


def captures(colors):
    """A 4-web clique: v32 is a parameter home, v40/v41/v42 are locals."""
    physical = [node(r, [], r) for r in range(32)]
    webs = [32, 40, 41, 42]
    virtual = [
        node(w, VOLATILES + [x for x in webs if x != w]) for w in webs
    ]
    before = {
        "register_class": "gpr",
        "nodes": physical + virtual,
        # baseline simplify pops descending vreg -> v42=r31,.. v32=r28
        "simplify_order": [42, 41, 40, 32],
    }
    after = {
        "register_class": "gpr",
        "nodes": physical + [
            {**item, "physical_register": colors[item["virtual_register"]]}
            for item in virtual
        ],
        "simplify_order": before["simplify_order"],
    }
    return before, after


def main():
    # Baseline (pure descending vreg): v42=31,v41=30,v40=29,v32=28.
    # Target promotes the parameter v32 above two locals and demotes them:
    #   v32(param) -> r30, v40 -> r28, v41 -> r29, v42 -> r31.
    target = {32: 30, 40: 28, 41: 29, 42: 31}
    before, after = captures({32: 28, 40: 29, 41: 30, 42: 31})

    report = analyze(before, after, target)
    assert report["format"] == FORMAT
    assert report["baseline_replay_validation"]["mismatches"] == []
    assert report["reachable"] is True, report["reached_colors"]
    assert report["promoted_parameter_registers"] == [32], report
    assert 32 in report["moved_up_registers"]
    assert 40 in report["moved_down_registers"]

    # An unreachable request (asking a web for a colour no order can produce
    # on this fully-connected clique) must not be reported as reachable.
    impossible = {32: 31, 40: 31, 41: 31, 42: 31}
    bad, bad_after = captures({32: 28, 40: 29, 41: 30, 42: 31})
    report2 = analyze(bad, bad_after, impossible)
    assert report2["reachable"] is False

    print("param-rerank reachability tests passed")


if __name__ == "__main__":
    main()
