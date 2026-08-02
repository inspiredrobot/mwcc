#!/usr/bin/env python3
"""Catalog direct virtual-register allocations in an MWCC executable."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

from pe import PEFile, load_config


COUNTERS = {
    "gpr": 0x0058846E,
    "fpr": 0x0058846C,
    "vr": 0x0058849A,
}

OBJECT_ALLOCATOR_SITES = {
    0x004C1FF9,
    0x004C20D9,
    0x004C21C5,
    0x004C21D3,
    0x004C2325,
}

SITE_DETAILS = {
    0x004A0C36: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination while coercing an operand to a GPR",
    },
    0x004A0CA7: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination while coercing an operand to a GPR",
    },
    0x004A0D67: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination while coercing an operand to a GPR",
    },
    0x004A05B7: {
        "function": "Operands_ForceFPR",
        "operation": "allocate destination for a direct LFS/LFD load",
    },
    0x004A0617: {
        "function": "Operands_ForceFPR",
        "operation": "allocate destination for an indexed LFSX/LFDX load",
    },
    0x004A108E: {
        "function": "Operands_Normalize",
        "operation": "allocate a GPR while normalizing an operand",
    },
    0x004A10A3: {
        "function": "Operands_Normalize",
        "operation": "allocate a GPR while normalizing an operand",
    },
}

PRE_CODEGEN_SITES = {
    0x004855F8,
    0x0048590F,
}


def direct_increment_sites(pe: PEFile, counter: int) -> list[int]:
    encoding = b"\x66\xff\x05" + struct.pack("<I", counter)
    sites = []
    for address in pe.find(encoding):
        if pe.section_for_address(address).name == ".text":
            sites.append(address)
    return sites


def build_catalog(config: dict, original: Path) -> dict:
    pe = PEFile(original)
    sites = []
    for register_class, counter in COUNTERS.items():
        for address in direct_increment_sites(pe, counter):
            detail = SITE_DETAILS.get(address, {})
            sites.append(
                {
                    "address": f"0x{address:08x}",
                    "register_class": register_class,
                    "counter_address": f"0x{counter:08x}",
                    "allocation_kind": (
                        "object_allocator_internal"
                        if address in OBJECT_ALLOCATOR_SITES
                        else "temporary"
                    ),
                    "capture_before_codegen": address in PRE_CODEGEN_SITES,
                    "function": detail.get("function"),
                    "operation": detail.get("operation"),
                }
            )
    sites.sort(key=lambda site: int(site["address"], 0))
    return {
        "format": "mwcc-virtual-register-sites-v1",
        "compiler": config["version"],
        "target_sha256": hashlib.sha256(pe.data).hexdigest(),
        "sites": sites,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Find direct virtual-register counter increments"
    )
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--check", type=Path)
    parser.add_argument("--stamp", type=Path)
    args = parser.parse_args()
    if args.output and args.check:
        parser.error("--output and --check are mutually exclusive")

    config, original = load_config(args.config)
    catalog = build_catalog(config, original)
    text = json.dumps(catalog, indent=2) + "\n"
    if args.check:
        if args.check.read_text(encoding="utf-8") != text:
            raise SystemExit(f"virtual-register site catalog is stale: {args.check}")
    elif args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text(catalog["target_sha256"] + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
