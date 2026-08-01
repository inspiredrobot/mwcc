#!/usr/bin/env python3

import argparse
import hashlib
import json
import struct
from pathlib import Path


def hash_file(path: Path, algorithm: str) -> str:
    digest = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def pe_timestamp(path: Path) -> int:
    with path.open("rb") as stream:
        stream.seek(0x3C)
        pe_offset = struct.unpack("<I", stream.read(4))[0]
        stream.seek(pe_offset)
        if stream.read(4) != b"PE\0\0":
            raise ValueError(f"{path} is not a PE executable")
        stream.seek(pe_offset + 8)
        return struct.unpack("<I", stream.read(4))[0]


def verify(config_path: Path) -> tuple[dict, Path]:
    with config_path.open(encoding="utf-8") as stream:
        config = json.load(stream)
    original = Path(config["original"])
    if not original.is_file():
        raise FileNotFoundError(
            f"missing {original}; see README.md for the expected local input"
        )
    if original.stat().st_size != config["size"]:
        raise ValueError(f"size mismatch for {original}")
    for algorithm in ("md5", "sha1", "sha256"):
        actual = hash_file(original, algorithm)
        if actual != config[algorithm]:
            raise ValueError(
                f"{algorithm.upper()} mismatch for {original}: {actual}"
            )
    timestamp = pe_timestamp(original)
    if timestamp != config["pe_timestamp"]:
        raise ValueError(
            f"PE timestamp mismatch for {original}: {timestamp}"
        )
    return config, original


def main() -> None:
    parser = argparse.ArgumentParser(description="Verify the configured MWCC binary")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--stamp", type=Path)
    args = parser.parse_args()

    config, original = verify(args.config)
    print(f"{original}: verified {config['version']} ({config['sha256']})")
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text(config["sha256"] + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
