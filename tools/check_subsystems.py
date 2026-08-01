#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

from pe import PEFile, load_config


def parse_address(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Validate the core subsystem manifest against the target PE"
    )
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--stamp", type=Path)
    args = parser.parse_args()

    config, original = load_config(args.config)
    with args.manifest.open(encoding="utf-8") as stream:
        manifest = json.load(stream)
    if manifest["target_sha256"] != config["sha256"]:
        raise ValueError("subsystem manifest targets a different executable")

    pe = PEFile(original)
    names: set[str] = set()
    addresses: set[int] = set()
    function_count = 0
    for module in manifest["modules"]:
        source = Path(module["source"])
        if not source.is_file():
            raise FileNotFoundError(f"missing source placeholder: {source}")
        for function in module["functions"]:
            address = parse_address(function["address"])
            section = pe.section_for_address(address)
            if section.name != ".text":
                raise ValueError(f"{function['name']} is not in .text: {section.name}")
            if address in addresses:
                raise ValueError(f"duplicate function address: 0x{address:08x}")
            if function["name"] in names:
                raise ValueError(f"duplicate function name: {function['name']}")
            addresses.add(address)
            names.add(function["name"])
            function_count += 1

    for marker in manifest["trace_strings"]:
        address = parse_address(marker["address"])
        expected = marker["text"].encode("latin-1") + b"\0"
        actual = pe.read(address, len(expected))
        if actual != expected:
            raise ValueError(
                f"trace mismatch at 0x{address:08x}: "
                f"expected {expected!r}, got {actual!r}"
            )

    print(
        f"Validated {function_count} core functions and "
        f"{len(manifest['trace_strings'])} trace strings"
    )
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text(config["sha256"] + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
