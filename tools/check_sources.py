#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run a strict host-side syntax check over recovered C sources"
    )
    parser.add_argument("--stamp", type=Path)
    args = parser.parse_args()

    compiler_name = os.environ.get("CC", "cc")
    compiler = shutil.which(compiler_name)
    if compiler is None:
        raise FileNotFoundError(f"C compiler not found: {compiler_name}")

    sources = sorted(Path("src").rglob("*.c"))
    if not sources:
        raise FileNotFoundError("no C sources under src/")
    command = [
        compiler,
        "-m32",
        "-std=c90",
        "-pedantic",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Iinclude",
        "-fsyntax-only",
        *(str(source) for source in sources),
    ]
    subprocess.run(command, check=True)
    print(f"Syntax-checked {len(sources)} C source files with {compiler}")

    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("\n".join(str(source) for source in sources) + "\n")


if __name__ == "__main__":
    main()
