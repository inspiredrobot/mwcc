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
    0x0049FC0C: {
        "function": "FUN_0049fbc0",
        "operation": "allocate the FPR loaded by the first LFD conversion step",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x0049FCA2: {
        "function": "FUN_0049fbc0",
        "operation": "allocate the GPR defined by XORIS in conversion lowering",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x0049FCF0: {
        "function": "FUN_0049fbc0",
        "operation": "allocate the GPR defined by LIS in conversion lowering",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x0049FD32: {
        "function": "FUN_0049fbc0",
        "operation": "allocate the FPR loaded by the second LFD conversion step",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x0049FD66: {
        "function": "FUN_0049fbc0",
        "operation": "allocate the floating-point subtraction result",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x004A0C36: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination while coercing an operand to a GPR",
        "category": "operand_coercion",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004A0CA7: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination while coercing an operand to a GPR",
        "category": "operand_coercion",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004A0D67: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination while coercing an operand to a GPR",
        "category": "operand_coercion",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004A0E37: {
        "function": "Operands_ForceGPR",
        "operation": "allocate destination for an indexed load while coercing to a GPR",
        "category": "operand_coercion",
        "evidence": "confirmed",
        "evidence_source": "function boundary and target disassembly",
    },
    0x004A05B7: {
        "function": "Operands_ForceFPR",
        "operation": "allocate destination for a direct LFS/LFD load",
        "category": "operand_coercion",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004A0617: {
        "function": "Operands_ForceFPR",
        "operation": "allocate destination for an indexed LFSX/LFDX load",
        "category": "operand_coercion",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004A108E: {
        "function": "Operands_Normalize",
        "operation": "allocate a GPR while normalizing an operand",
        "category": "operand_normalization",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004A10A3: {
        "function": "Operands_Normalize",
        "operation": "allocate a GPR while normalizing an operand",
        "category": "operand_normalization",
        "evidence": "confirmed",
        "evidence_source": "typed reconstruction and target disassembly",
    },
    0x004B5826: {
        "function": "FUN_004b5710",
        "operation": "allocate the result of scalar floating-point arithmetic",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x004B65F7: {
        "function": "FUN_004b5860",
        "operation": "allocate the first GPR used to materialize boolean constants",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target control flow and grhomerun provenance",
    },
    0x004B660E: {
        "function": "FUN_004b5860",
        "operation": "allocate the second GPR used to materialize boolean constants",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target control flow and grhomerun provenance",
    },
    0x004B79D6: {
        "function": "FUN_004b5860",
        "operation": "allocate the result of rotate-and-mask expression lowering",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target control flow and grhomerun provenance",
    },
    0x004BA156: {
        "function": "PCodeExpressionLowering",
        "operation": "allocate the result of floating-point negation",
        "category": "expression_lowering",
        "evidence": "inferred",
        "evidence_source": "target disassembly and grhomerun provenance",
    },
    0x00518F82: {
        "function": "FUN_00518ab0",
        "operation": "allocate an FPR call-result carrier copied from ABI f1",
        "category": "call_result",
        "evidence": "confirmed",
        "evidence_source": "target return-type branch and emitted FMR",
    },
    0x00518FD7: {
        "function": "FUN_00518ab0",
        "operation": "allocate a vector call-result carrier copied from ABI vr2",
        "category": "call_result",
        "evidence": "confirmed",
        "evidence_source": "target return-type branch and emitted VMR",
    },
    0x0051906C: {
        "function": "FUN_00518ab0",
        "operation": "allocate the first GPR of a wide call result copied from ABI r3",
        "category": "call_result",
        "evidence": "confirmed",
        "evidence_source": "target return-type branch and emitted MR",
    },
    0x00519082: {
        "function": "FUN_00518ab0",
        "operation": "allocate the second GPR of a wide call result copied from ABI r4",
        "category": "call_result",
        "evidence": "confirmed",
        "evidence_source": "target return-type branch and emitted MR",
    },
    0x005190DF: {
        "function": "FUN_00518ab0",
        "operation": "allocate a GPR call-result carrier copied from ABI r3",
        "category": "call_result",
        "evidence": "confirmed",
        "evidence_source": "target return-type branch and emitted MR",
    },
    0x005298A8: {
        "function": "LoopOptimization_00529480",
        "operation": "allocate a GPR while rewriting a loop condition",
        "category": "optimizer_rewrite",
        "evidence": "inferred",
        "evidence_source": "LoopOptimization.c anchor and grhomerun provenance",
    },
    0x0052A4D1: {
        "function": "LoopOptimization_0052a200",
        "operation": "allocate a rotate-and-mask result during loop optimization",
        "category": "optimizer_rewrite",
        "evidence": "inferred",
        "evidence_source": "function adjacency and grhomerun provenance",
    },
    0x0052A57A: {
        "function": "LoopOptimization_0052a200",
        "operation": "allocate a transient GPR during loop optimization",
        "category": "optimizer_rewrite",
        "evidence": "inferred",
        "evidence_source": "function adjacency and grhomerun provenance",
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
            site = {
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
            if detail:
                site.update(
                    {
                        "operation_category": detail["category"],
                        "evidence": detail["evidence"],
                        "evidence_source": detail["evidence_source"],
                    }
                )
            sites.append(site)
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
