#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="Report core decompilation status")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("config/GC_1_2_5/subsystems.json"),
    )
    args = parser.parse_args()

    with args.manifest.open(encoding="utf-8") as stream:
        manifest = json.load(stream)

    print("| Function | Address | Functional status | Binary match |")
    print("| --- | --- | --- | ---: |")
    for module in manifest["modules"]:
        for function in module["functions"]:
            if not function.get("decompiled", False):
                continue
            match = function.get("match_percent")
            if match is None:
                match_text = "unmeasured"
            else:
                match_text = f"{match:.2f}%"
            print(
                f"| `{function['name']}` | `{function['address']}` | "
                f"{function['functional_status']} | {match_text} |"
            )


if __name__ == "__main__":
    main()
