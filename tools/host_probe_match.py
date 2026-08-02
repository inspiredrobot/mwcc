#!/usr/bin/env python3

import argparse
import hashlib
import json
from pathlib import Path

from coff import COFFFile
from pe import PEFile, load_config


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def function_name(name: str) -> str:
    return name.removeprefix("_")


def compare_bytes(
    candidate: bytes, target: bytes, relocation_offsets: tuple[int, ...]
) -> dict:
    relocated = {
        index
        for offset in relocation_offsets
        for index in range(offset, min(offset + 4, len(candidate)))
    }
    comparable = min(len(candidate), len(target))
    comparable_indices = [
        index for index in range(comparable) if index not in relocated
    ]
    matched = sum(candidate[index] == target[index] for index in comparable_indices)
    raw_matched = sum(
        candidate[index] == target[index] for index in range(comparable)
    )
    denominator = max(len(candidate), len(target))
    return {
        "candidate_size": len(candidate),
        "target_size": len(target),
        "raw_matched_bytes": raw_matched,
        "raw_match_percent": 100.0 * raw_matched / denominator,
        "comparable_bytes": len(comparable_indices),
        "comparable_matched_bytes": matched,
        "comparable_match_percent": (
            100.0 * matched / len(comparable_indices)
            if comparable_indices
            else 100.0
        ),
        "relocation_bytes_excluded": len(relocated),
        "instruction_exact": (
            len(candidate) == len(target) and matched == len(comparable_indices)
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Measure host probe code bytes against the stock compiler"
    )
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("config/host_probe_targets.json"),
    )
    parser.add_argument("--object", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    with args.manifest.open(encoding="utf-8") as stream:
        manifest = json.load(stream)
    _config, target_path = load_config(args.config)
    target_hash = sha256(target_path)
    if target_hash != manifest["target_sha256"]:
        raise ValueError(
            f"target SHA-256 mismatch: expected {manifest['target_sha256']}, "
            f"got {target_hash}"
        )

    candidate_functions = {
        function_name(function.name): function
        for function in COFFFile(args.object).functions()
    }
    target = PEFile(target_path)
    functions = []
    for expected in manifest["functions"]:
        name = expected["name"]
        if name not in candidate_functions:
            raise ValueError(f"candidate object lacks function: {name}")
        function = candidate_functions[name]
        address = int(expected["address"], 0)
        target_bytes = target.read(address, expected["size"])
        result = {
            "name": name,
            "target_address": expected["address"],
            "relocation_offsets": list(function.relocation_offsets),
        }
        result.update(
            compare_bytes(function.data, target_bytes, function.relocation_offsets)
        )
        functions.append(result)

    raw_matched = sum(value["raw_matched_bytes"] for value in functions)
    raw_total = sum(
        max(value["candidate_size"], value["target_size"])
        for value in functions
    )
    comparable_matched = sum(
        value["comparable_matched_bytes"] for value in functions
    )
    comparable_total = sum(value["comparable_bytes"] for value in functions)
    report = {
        "format": "mwcc-host-probe-match-v1",
        "target_sha256": target_hash,
        "object": str(args.object),
        "object_sha256": sha256(args.object),
        "compiler_flags": manifest["compiler_flags"],
        "functions": functions,
        "summary": {
            "exact_functions": sum(
                value["instruction_exact"] for value in functions
            ),
            "function_count": len(functions),
            "raw_match_percent": 100.0 * raw_matched / raw_total,
            "comparable_match_percent": (
                100.0 * comparable_matched / comparable_total
            ),
        },
    }
    text = json.dumps(report, indent=2) + "\n"
    if args.output is None:
        print(text, end="")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
