#!/usr/bin/env python3

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

from coff import COFFArchive, COFFFile
from pe import PEFile, load_config


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def load_candidate(manifest_path: Path, candidate_id: str) -> dict:
    with manifest_path.open(encoding="utf-8") as stream:
        manifest = json.load(stream)
    for candidate in manifest["candidates"]:
        if candidate["id"] == candidate_id:
            return candidate
    raise ValueError(f"candidate not found: {candidate_id}")


def artifact(candidate: dict, role: str) -> dict:
    for value in candidate["artifacts"]:
        if value["role"] == role:
            return value
    raise ValueError(f"candidate lacks {role}")


def verify_artifact(path: Path, description: dict) -> str:
    actual = sha256(path)
    expected = description["sha256"]
    if actual != expected:
        raise ValueError(
            f"{path}: SHA-256 mismatch: expected {expected}, got {actual}"
        )
    return actual


def library_bytes(package: Path, member_name: str) -> bytes:
    with zipfile.ZipFile(package) as archive:
        try:
            info = archive.getinfo(member_name)
        except KeyError as error:
            raise ValueError(f"{package}: missing {member_name}") from error
        if info.flag_bits & 1:
            raise ValueError(f"{package}: encrypted members are not supported")
        return archive.read(info)


def calibration_matches(library: bytes, target: PEFile, label: str) -> list[dict]:
    matches = []
    archive = COFFArchive(Path(label), library)
    for member in archive.members():
        if not member.name.lower().endswith(".obj"):
            continue
        object_label = Path(f"{label}({member.name})")
        try:
            functions = COFFFile(object_label, member.data).functions()
        except (ValueError, struct.error):
            continue
        for function in functions:
            if function.has_relocations or len(function.data) < 8:
                continue
            for address in target.find(function.data):
                if target.section_for_address(address).name != ".text":
                    continue
                matches.append(
                    {
                        "object": member.name,
                        "function": function.name,
                        "size": len(function.data),
                        "target_address": f"0x{address:08x}",
                        "bytes": function.data.hex(),
                    }
                )
    matches.sort(key=lambda value: (-value["size"], value["target_address"]))
    return matches


def verify_expected(candidate: dict, matches: list[dict]) -> None:
    actual = {
        (
            value["function"],
            value["target_address"],
            value["size"],
            value["bytes"],
        )
        for value in matches
    }
    for expected in candidate["calibration"]["expected_matches"]:
        key = (
            expected["function"],
            expected["target_address"],
            expected["size"],
            expected["bytes"],
        )
        if key not in actual:
            raise ValueError(
                "missing expected calibration match: "
                f"{expected['function']} at {expected['target_address']}"
            )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Verify and calibrate a host-compiler candidate corpus"
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("config/host_candidates.json"),
    )
    parser.add_argument("--candidate", default="cmu-codewarrior-pro-5.3")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--binary-package", type=Path, required=True)
    parser.add_argument("--source-package", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    candidate = load_candidate(args.manifest, args.candidate)
    binary_description = artifact(candidate, "binary_package")
    binary_hash = verify_artifact(args.binary_package, binary_description)
    source_hash = None
    if args.source_package is not None:
        source_hash = verify_artifact(
            args.source_package, artifact(candidate, "source_package")
        )

    _config, target_path = load_config(args.config)
    target = PEFile(target_path)
    member_name = candidate["calibration"]["library_member"]
    library = library_bytes(args.binary_package, member_name)
    matches = calibration_matches(library, target, member_name)
    verify_expected(candidate, matches)

    report = {
        "format": "mwcc-host-calibration-v1",
        "candidate": candidate["id"],
        "family_status": candidate["family_status"],
        "compiler_status": candidate["compiler_status"],
        "source_page": candidate["source_page"],
        "target": str(target_path),
        "target_sha256": sha256(target_path),
        "artifacts": {
            "binary_package": {
                "path": str(args.binary_package),
                "sha256": binary_hash,
            },
            "source_package": (
                {"path": str(args.source_package), "sha256": source_hash}
                if args.source_package is not None
                else None
            ),
            "library_member": {
                "name": member_name,
                "sha256": hashlib.sha256(library).hexdigest(),
            },
        },
        "matches": matches,
        "expected_matches_verified": True,
    }
    text = json.dumps(report, indent=2) + "\n"
    if args.output is None:
        print(text, end="")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
