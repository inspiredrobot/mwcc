#!/usr/bin/env python3
"""Print the vreg -> object-name/cluster/color table for a capture.

Usage:
  vreg_map.py CAPTURE_DIR IDX

Reads (and if necessary generates) CAPTURE_DIR/provenance.json, joins it with
the coloring snapshots, and prints one row per GPR virtual register:

  vreg  @name  cluster  degree  color  coalesce-root

Cluster classification (see docs/ALLOCATOR_CASEBOOK.md, grCastle_801CF868):
  fn-scope   outermost function's params + named locals (pre-pool, vr32..)
  pool       @NNN frontend/optimizer temps; @-names descend in creation
             order, so vreg order = creation order (load-CSEs, value temps,
             then each inline expansion's locals in declaration order)
  scratch    lowering temps (instruction order, above the pool)

The name comes from provenance object_before.opaque_value_0a_data (first
printable run after byte 10). Named locals print their C names; pool temps
print @NNN. Use this BEFORE classifying any coloring residual as terminal:
declaration order and the inline boundary both renumber webs.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent


def name_of(entry: dict) -> str:
    ob = entry.get("object_before") or {}
    data = ob.get("opaque_value_0a_data")
    if not data:
        return "?"
    raw = bytes.fromhex(data)
    m = re.search(rb"[ -~]{2,}", raw[10:])
    return m.group(0).decode() if m else "?"


def ensure_provenance(cap: Path, idx: str) -> dict:
    prov = cap / "provenance.json"
    if not prov.exists():
        cmd = [
            sys.executable,
            str(TOOLS / "allocator_provenance.py"),
            str(cap / f"allocator-{idx}.json"),
            "--coloring",
            str(cap / f"coloring-{idx}-gpr-01-before.json"),
            "--creations",
            str(cap / f"pcode-creations-{idx}-initial.json"),
            "--output",
            str(prov),
        ]
        subprocess.run(cmd, check=True, cwd=TOOLS.parent)
    return json.load(open(prov))


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    cap = Path(sys.argv[1])
    idx = sys.argv[2]

    prov = ensure_provenance(cap, idx)
    names: dict[int, str] = {}
    for entry in prov.get("virtual_register_creations", []):
        if entry.get("register_class") != "gpr":
            continue
        names[entry["primary_register"]] = name_of(entry)

    before = json.load(open(cap / f"coloring-{idx}-gpr-01-before.json"))
    try:
        after = json.load(open(cap / f"coloring-{idx}-gpr-01-after.json"))
        colors = {
            n["virtual_register"]: n.get("physical_register", -1)
            for n in after["nodes"]
        }
    except FileNotFoundError:
        colors = {}
    degrees = {
        n["virtual_register"]: n.get("degree") for n in before["nodes"]
    }
    roots = {}
    for group in before.get("coalescing_groups", []):
        for member in group["members"]:
            roots[member] = group["root"]

    pool_min = min(
        (vr for vr, nm in names.items() if nm.startswith("@")), default=None
    )

    print(f"{'vreg':>5} {'name':<16} {'cluster':<8} {'deg':>4} "
          f"{'color':>5} {'root':>5}")
    for vr in sorted(set(names) | set(degrees)):
        if vr < 32:
            continue
        nm = names.get(vr, "?")
        if nm.startswith("@"):
            cluster = "pool"
        elif pool_min is not None and vr >= pool_min and nm == "?":
            cluster = "scratch"
        else:
            cluster = "fn-scope" if nm != "?" else "scratch"
        color = colors.get(vr, -1)
        creg = f"r{color}" if isinstance(color, int) and color >= 0 else "-"
        root = roots.get(vr)
        print(f"{vr:>5} {nm:<16} {cluster:<8} {str(degrees.get(vr, '-')):>4} "
              f"{creg:>5} {str(root) if root is not None else '':>5}")


if __name__ == "__main__":
    main()
