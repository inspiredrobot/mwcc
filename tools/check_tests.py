#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
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

    tests = [
        ("registers", "src/backend/Registers.c", "tests/test_registers.c"),
        ("coloring", "src/backend/Coloring.c", "tests/test_coloring.c"),
        ("spill_code", "src/backend/SpillCode.c", "tests/test_spill_code.c"),
        ("pcode", "src/backend/PCode.c", "tests/test_pcode.c"),
        ("operands", "src/backend/Operands.c", "tests/test_operands.c"),
        (
            "pcode_utilities",
            "src/backend/PCodeUtilities.c",
            "tests/test_pcode_utilities.c",
        ),
    ]
    with tempfile.TemporaryDirectory(prefix="mwcc-tests-") as temp_dir:
        for name, source, test in tests:
            executable = Path(temp_dir) / f"test_{name}"
            command = [
                compiler,
                "-std=c90",
                "-pedantic",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-DMWCC_SKIP_LAYOUT_ASSERTS",
                "-Iinclude",
                source,
                test,
                "-o",
                str(executable),
            ]
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)

    subprocess.run([sys.executable, "tests/test_allocator_snapshot.py"], check=True)
    subprocess.run(
        [sys.executable, "tests/test_allocator_provenance.py"], check=True
    )
    subprocess.run(
        [sys.executable, "tests/test_compare_coloring_snapshots.py"], check=True
    )
    subprocess.run(
        [sys.executable, "tests/test_compare_pcode_stages.py"], check=True
    )
    subprocess.run(
        [sys.executable, "tests/test_rank_register_origins.py"], check=True
    )

    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("core model tests passed\n", encoding="utf-8")


if __name__ == "__main__":
    main()
