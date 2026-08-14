#!/usr/bin/env python3
"""Prove whether a target coloring is rank-reachable when *parameters* may move.

`source_rank_solver` deliberately pins the parameter object webs (v32 is fixed
automatically), so it cannot answer residuals whose fix requires re-ranking a
parameter into the function-scope-local band -- the mpcoll `mpColl_80046904`
class, where both `coll` and `flags` parameter homes must colour into the
local stratum.

This tool keeps the captured interference graph fixed and asks the narrower,
decidable question: **is there a birth-rank assignment (parameters allowed to
move) that reproduces the target colours, and which webs must cross the
parameter/object stratum boundary to do it?** A hit proves the numbering is
reachable on the graph; it does NOT prove a source edit realises that
numbering. Realisability of a parameter promotion is a separate, lowering-level
fact (see `realizability_hint`): empirically a pointer/aggregate-base parameter
copy creates a distinct promotable object, while a scalar parameter copy is
aliased at initial lowering (folded) or forced to memory (spilled), so a
used-once scalar parameter promotion has no known stream-preserving realisation.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from allocator_snapshot import validate_coloring_snapshot
from coloring_model import (
    FIRST_VIRTUAL_REGISTER,
    replay_simplify,
    validate_colors,
)

FORMAT = "mwcc-param-rerank-reach-v1"


def load_snapshot(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        snapshot = json.load(stream)
    validate_coloring_snapshot(snapshot)
    return snapshot


def _object_registers(before: dict) -> list[int]:
    return sorted(
        node["virtual_register"]
        for node in before["nodes"]
        if node["virtual_register"] >= FIRST_VIRTUAL_REGISTER
        and node["virtual_register"] in set(before["simplify_order"])
    )


def ranks_from_target(before: dict, targets: dict[int, int]) -> dict[int, float]:
    """Build a rank map that reproduces the target claim order.

    Callee-saved colours are claimed high-to-low (r31 first). A web that must
    end at a higher physical register must be *selected* earlier, i.e. removed
    from the simplify graph last, i.e. carry a higher rank. Ranks only need to
    order the target webs among themselves; non-target webs keep their vreg.
    """
    # Higher physical register  ->  earlier selection  ->  higher rank.
    ordered = sorted(targets, key=lambda vr: targets[vr], reverse=True)
    # Assign descending fractional ranks spanning the object band so the
    # relative order is exactly `ordered` while staying inside plausible
    # object-web numbering.
    ranks: dict[int, float] = {}
    lo = min(ordered)
    hi = max(ordered)
    span = hi - lo if hi > lo else 1
    for position, vr in enumerate(ordered):
        # first (highest colour) gets the highest rank
        ranks[vr] = hi - (position * span / max(len(ordered) - 1, 1))
    return ranks


def realizability_hint(before: dict, promoted: list[int]) -> list[dict]:
    """Classify each promoted parameter web as pointer- or scalar-shaped.

    The capture does not carry C types, so we report the empirically-grounded
    rule and leave the type read to the caller. `object_index` is the web's
    position in the parameter/object stratum (0 == first parameter).
    """
    objects = _object_registers(before)
    hints = []
    for vr in promoted:
        hints.append(
            {
                "register": vr,
                "object_index": objects.index(vr) if vr in objects else None,
                "note": (
                    "parameter promotion; realisable only if a source copy "
                    "creates a distinct object (pointer/aggregate base). A "
                    "used-once scalar parameter copy is aliased at lowering "
                    "and is not known to be stream-preservingly realisable."
                ),
            }
        )
    return hints


def analyze(before: dict, after: dict, targets: dict[int, int]) -> dict:
    validation = validate_colors(before, after, replay_simplify(before)["colors"])
    ranks = ranks_from_target(before, targets)
    replay = replay_simplify(before, ranks=ranks)
    colors = replay["colors"]
    hits = {vr: colors.get(vr) for vr in targets}
    reached = all(colors.get(vr) == targets[vr] for vr in targets)

    objects = _object_registers(before)
    required = sorted(targets, key=lambda vr: ranks[vr], reverse=True)

    # A web "moves up" if its target colour is a higher register than the one
    # the baseline gave it (higher register == promoted in the callee-saved
    # claim order). Compare against the captured baseline colours.
    baseline_colors = {
        node["virtual_register"]: node.get("physical_register", -1)
        for node in after["nodes"]
    }
    moved_up = [
        vr for vr in targets
        if baseline_colors.get(vr, -1) >= 0
        and targets[vr] > baseline_colors[vr]
    ]
    moved_down = [
        vr for vr in targets
        if baseline_colors.get(vr, -1) >= 0
        and targets[vr] < baseline_colors[vr]
    ]
    # promoted parameters: moved-up webs sitting at the bottom of the object
    # band (the parameter-home stratum).
    param_floor = set(objects[:2]) if len(objects) >= 2 else set(objects)
    promoted_params = [vr for vr in moved_up if vr in param_floor]

    return {
        "format": FORMAT,
        "baseline_replay_validation": validation,
        "reachable": reached,
        "reached_colors": hits,
        "target_colors": targets,
        "required_select_order": required,
        "moved_up_registers": moved_up,
        "moved_down_registers": moved_down,
        "promoted_parameter_registers": promoted_params,
        "realizability_hint": realizability_hint(before, promoted_params),
        "note": (
            "reachable==True proves the target colours are birth-rank reachable "
            "on the fixed graph with parameters allowed to move; it does not "
            "prove a source edit realises the required numbering."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before")
    parser.add_argument("after")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--target", action="append", metavar="VREG=COLOR")
    group.add_argument("--target-file")
    parser.add_argument("--output")
    args = parser.parse_args()

    before = load_snapshot(Path(args.before))
    after = load_snapshot(Path(args.after))
    if args.target_file:
        with open(args.target_file, encoding="utf-8") as stream:
            targets = {int(k): int(v) for k, v in json.load(stream).items()}
    else:
        targets = {}
        for item in args.target:
            key, _, value = item.partition("=")
            targets[int(key)] = int(value)

    report = analyze(before, after, targets)
    text = json.dumps(report, indent=2)
    if args.output:
        Path(args.output).write_text(text + "\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
