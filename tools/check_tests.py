#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="Run host-side model tests")
    parser.add_argument("--stamp", type=Path)
    args = parser.parse_args()

    compiler_name = os.environ.get("CC", "cc")
    compiler = shutil.which(compiler_name)
    if compiler is None:
        raise FileNotFoundError(f"C compiler not found: {compiler_name}")

    with tempfile.TemporaryDirectory(prefix="mwcc-tests-") as temp_dir:
        executable = Path(temp_dir) / "test_registers"
        command = [
            compiler,
            "-std=c90",
            "-pedantic",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DMWCC_SKIP_LAYOUT_ASSERTS",
            "-Iinclude",
            "src/backend/Registers.c",
            "tests/test_registers.c",
            "-o",
            str(executable),
        ]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)

    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("register model tests passed\n", encoding="utf-8")


if __name__ == "__main__":
    main()
