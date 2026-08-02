#!/usr/bin/env python3

import sys
from pathlib import Path

sys.path.insert(0, str(Path("tools").resolve()))

from host_probe_match import compare_bytes  # noqa: E402


def main() -> None:
    exact = compare_bytes(b"\x01\x02\x03", b"\x01\x02\x03", ())
    assert exact["instruction_exact"]
    assert exact["raw_match_percent"] == 100.0

    relocated = compare_bytes(
        b"\x01\x00\x00\x00\x00\x06",
        b"\x01\x02\x03\x04\x05\x06",
        (1,),
    )
    assert relocated["relocation_bytes_excluded"] == 4
    assert relocated["comparable_bytes"] == 2
    assert relocated["comparable_match_percent"] == 100.0
    assert relocated["instruction_exact"]

    size_mismatch = compare_bytes(b"\x01", b"\x01\x02", ())
    assert not size_mismatch["instruction_exact"]
    assert size_mismatch["raw_match_percent"] == 50.0

    print("host probe matching tests passed")


if __name__ == "__main__":
    main()
