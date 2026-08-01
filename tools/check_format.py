#!/usr/bin/env python3

import argparse
import shutil
import subprocess
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="Check or format recovered C sources")
    parser.add_argument("--fix", action="store_true")
    parser.add_argument("--stamp", type=Path)
    args = parser.parse_args()

    formatter = shutil.which("clang-format")
    if formatter is None:
        raise FileNotFoundError("clang-format is required")
    paths = sorted(Path("src").rglob("*.c"))
    paths.extend(sorted(Path("include").rglob("*.h")))
    paths.extend(sorted(Path("tests").rglob("*.c")))
    if not paths:
        raise FileNotFoundError("no C sources or headers found")

    command = [formatter]
    if args.fix:
        command.append("-i")
    else:
        command.extend(["--dry-run", "--Werror"])
    command.extend(str(path) for path in paths)
    subprocess.run(command, check=True)
    action = "Formatted" if args.fix else "Format-checked"
    print(f"{action} {len(paths)} C source files with {formatter}")

    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("\n".join(str(path) for path in paths) + "\n")


if __name__ == "__main__":
    main()
