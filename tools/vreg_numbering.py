#!/usr/bin/env python3
"""Classify GC/1.2.5 virtual registers into their numbering strata.

Virtual-register numbers are assigned by the object-preallocation walk at
``0x00437230`` (reconstructed as ``CodeGen_PreallocateObjectRegisters`` in
``src/backend/CodeGen.c``).  Each ``Registers_Allocate{GPR,FPR,VR,GPRPair}``
call consumes the next value of a single monotonic per-class counter, so a
web's number is exactly its position in that walk.  The walk visits several
object lists in order and calls ``Registers_BeginCoalesceWindow`` partway
through, which records ``gGPRCoalesceFirst`` = the current counter.  That
boundary separates two strata that ``Coloring_SimplifyGraph`` treats
differently:

* ``[32, coalesce_first)`` -- the initial / shadow object stratum.  These webs
  are numbered by the walks before ``BeginCoalesceWindow`` and can never
  coalesce (``SpillCode_CoalesceCopies`` rejects roots below ``First``).
* ``[coalesce_first, next_unused)`` -- the coalescable stratum: the second
  local-object pass, the trailing list, and every temporary created during
  lowering.

Because within one simplify sweep the pop/claim order is by descending vreg,
the boundary is what decides whether a common-subexpression *recompute* load
(numbered right at ``coalesce_first``) out-ranks the *canonical* main-lowering
load of the same expression (numbered far higher).  This module turns a
coloring ``before`` snapshot plus its coalescing window into that stratum map,
and -- when object bindings are supplied -- groups object-backed webs by their
allocation arena so a recompute batch (a distinct arena clustered at the
boundary) is visible directly.

Reference case: the GALE01 ``efAsync_Dispatch`` capture has
``coalesce_first == 70``; its six ``GET_JOBJ(effect->gobj)`` recompute loads
occupy vregs ``70..75`` in a distinct object arena, while the canonical loads
sit at ``266+``.
"""

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path

FIRST_VIRTUAL_REGISTER = 32


class VregNumberingError(ValueError):
    pass


def stratum_for(virtual_register: int, coalesce_first: int) -> str:
    """Return the numbering stratum of one virtual register."""
    if virtual_register < FIRST_VIRTUAL_REGISTER:
        return "precolored"
    if coalesce_first <= FIRST_VIRTUAL_REGISTER:
        # No initial stratum was recorded; everything is coalescable.
        return "coalescable"
    if virtual_register < coalesce_first:
        return "initial"
    return "coalescable"


def coalesce_first_of(window: dict) -> int:
    """Extract the coalescing-window ``first`` boundary, validating shape."""
    if not isinstance(window, dict) or "first" not in window:
        raise VregNumberingError("coalescing window lacks a 'first' boundary")
    first = window["first"]
    if not isinstance(first, int) or first < FIRST_VIRTUAL_REGISTER:
        raise VregNumberingError(
            f"coalescing window 'first' is not a virtual register: {first!r}"
        )
    return first


def arena_of(object_pointer: str, width: int = 4) -> str | None:
    """Return the high-order arena tag of a compiler-object pointer.

    ``width`` counts hex digits after ``0x`` kept as the arena identity.  A
    null pointer (a compiler temporary with no frontend object) returns None.
    """
    if not object_pointer or not object_pointer.startswith("0x"):
        return None
    digits = object_pointer[2:]
    if set(digits) <= {"0"}:
        return None
    return "0x" + digits[:width]


def classify(snapshot: dict, window: dict, object_bindings=None) -> dict:
    """Partition a coloring ``before`` snapshot into numbering strata.

    ``object_bindings`` is an optional iterable of ``{"register", "object"}``
    mappings (the ``object_bindings`` facts of an allocator-provenance export);
    when present, coalescable webs are additionally grouped by arena so a
    recompute batch is reported explicitly.
    """
    coalesce_first = coalesce_first_of(window)
    nodes = snapshot.get("nodes")
    if not isinstance(nodes, list):
        raise VregNumberingError("snapshot has no node list")

    strata = defaultdict(list)
    for node in nodes:
        vr = node["virtual_register"]
        strata[stratum_for(vr, coalesce_first)].append(vr)
    for members in strata.values():
        members.sort()

    result = {
        "coalesce_first": coalesce_first,
        "counts": {name: len(vrs) for name, vrs in strata.items()},
        "strata": {name: vrs for name, vrs in strata.items()},
    }

    if object_bindings is not None:
        object_of = {}
        for binding in object_bindings:
            register = binding["register"]
            if isinstance(register, str) and register.startswith("gpr:"):
                register = int(register.split(":", 1)[1])
            object_of[register] = binding["object"]
        # The recompute batch is the run of webs at [coalesce_first, ...) whose
        # backing object lives in a different arena than the bulk.
        arena_counts = Counter(
            arena_of(object_of.get(vr, ""))
            for vr in strata["coalescable"]
            if object_of.get(vr)
        )
        arena_counts.pop(None, None)
        boundary_run = []
        vr = coalesce_first
        boundary_arena = arena_of(object_of.get(coalesce_first, "") or "")
        while boundary_arena is not None and arena_of(
            object_of.get(vr, "") or ""
        ) == boundary_arena:
            boundary_run.append(vr)
            vr += 1
        result["arena_counts"] = {
            arena: count for arena, count in arena_counts.most_common()
        }
        result["recompute_batch"] = {
            "arena": boundary_arena,
            "virtual_registers": boundary_run,
        }
    return result


def load_capture(capture_dir: Path, register_class: str = "gpr"):
    """Load the before-snapshot, coalescing window, and object bindings."""
    before = None
    for path in sorted(capture_dir.glob(f"coloring-*-{register_class}-01-before.json")):
        before = json.loads(path.read_text(encoding="utf-8"))
        break
    if before is None:
        raise VregNumberingError(
            f"no {register_class} before-snapshot in {capture_dir}"
        )
    window = None
    bindings = None
    provenance = capture_dir / "provenance.json"
    if provenance.exists():
        facts = json.loads(provenance.read_text(encoding="utf-8"))
        for entry in facts.get("coalescing_windows", []):
            if (
                entry.get("phase") == "before"
                and entry.get("register_class") == register_class
            ):
                window = entry
                break
        bindings = [
            entry
            for entry in facts.get("object_bindings", [])
            if entry.get("phase") == "before"
        ]
    if window is None:
        window = {"first": before.get("coalesce_first", FIRST_VIRTUAL_REGISTER)}
    return before, window, bindings


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Classify virtual registers by numbering stratum"
    )
    parser.add_argument("capture", type=Path, help="allocator capture directory")
    parser.add_argument("--class", dest="register_class", default="gpr")
    args = parser.parse_args()

    before, window, bindings = load_capture(args.capture, args.register_class)
    report = classify(before, window, bindings)
    print(f"coalesce_first: {report['coalesce_first']}")
    for name in ("precolored", "initial", "coalescable"):
        vrs = report["strata"].get(name, [])
        print(f"  {name:12} {len(vrs):5} webs")
    if "recompute_batch" in report:
        batch = report["recompute_batch"]
        print(
            f"recompute batch: arena {batch['arena']} "
            f"vregs {batch['virtual_registers']}"
        )
        print("arena counts:", report["arena_counts"])


if __name__ == "__main__":
    main()
