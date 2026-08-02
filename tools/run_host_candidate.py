#!/usr/bin/env python3

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def verify_sha256(path: Path, expected: str) -> str:
    actual = sha256(path)
    if actual != expected.lower():
        raise ValueError(
            f"{path}: SHA-256 mismatch: expected {expected.lower()}, got {actual}"
        )
    return actual


def bind_mount(source: Path, destination: str, read_only: bool) -> str:
    options = ["type=bind", f"src={source.resolve()}", f"dst={destination}"]
    if read_only:
        options.append("readonly")
    return ",".join(options)


def sandbox_command(
    candidate: Path,
    runner: Path,
    input_dir: Path,
    output_dir: Path,
    image: str,
    platform: str,
    candidate_args: list[str],
) -> list[str]:
    return [
        "docker",
        "run",
        "--rm",
        "--pull",
        "never",
        "--platform",
        platform,
        "--network",
        "none",
        "--read-only",
        "--cap-drop",
        "ALL",
        "--security-opt",
        "no-new-privileges",
        "--pids-limit",
        "64",
        "--memory",
        "512m",
        "--cpus",
        "1",
        "--tmpfs",
        "/tmp:rw,noexec,nosuid,nodev,size=64m",
        "--env",
        "HOME=/tmp",
        "--workdir",
        "/tmp",
        "--mount",
        bind_mount(candidate, "/candidate/mwcc.exe", True),
        "--mount",
        bind_mount(runner, "/sandbox/wibo", True),
        "--mount",
        bind_mount(input_dir, "/input", True),
        "--mount",
        bind_mount(output_dir, "/output", False),
        image,
        "qemu-i386",
        "/sandbox/wibo",
        "/candidate/mwcc.exe",
        *candidate_args,
    ]


def image_identity(image: str) -> str:
    result = subprocess.run(
        ["docker", "image", "inspect", "--format", "{{.Id}}", image],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run a hash-verified Win32 host compiler in an offline sandbox"
    )
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--candidate-sha256", required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--runner-sha256", required=True)
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--platform", default="linux/arm64")
    parser.add_argument("--timeout", type=int, default=30)
    parser.add_argument(
        "--expect-output",
        type=Path,
        help="relative path that must be created directly under --output-dir",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("candidate_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    for path in (args.candidate, args.runner):
        if not path.is_file():
            raise ValueError(f"not a regular file: {path}")
    for path in (args.input_dir, args.output_dir):
        if not path.is_dir():
            raise ValueError(f"not a directory: {path}")

    output_dir = args.output_dir.resolve()
    report = args.report.resolve()
    if report.parent != output_dir:
        raise ValueError("--report must be directly inside --output-dir")
    existing = [path for path in output_dir.iterdir() if path.resolve() != report]
    if existing:
        raise ValueError("--output-dir must be a dedicated empty directory")
    expected_output = None
    if args.expect_output is not None:
        if args.expect_output.is_absolute() or ".." in args.expect_output.parts:
            raise ValueError("--expect-output must be a safe relative path")
        expected_output = (output_dir / args.expect_output).resolve()
        if expected_output.parent != output_dir:
            raise ValueError("--expect-output must be directly inside --output-dir")

    candidate_hash = verify_sha256(args.candidate, args.candidate_sha256)
    runner_hash = verify_sha256(args.runner, args.runner_sha256)
    candidate_args = args.candidate_args
    if candidate_args[:1] == ["--"]:
        candidate_args = candidate_args[1:]
    command = sandbox_command(
        args.candidate,
        args.runner,
        args.input_dir,
        args.output_dir,
        args.image,
        args.platform,
        candidate_args or ["-version"],
    )
    report_data = {
        "format": "mwcc-host-candidate-run-v1",
        "candidate": {
            "path": str(args.candidate.resolve()),
            "sha256": candidate_hash,
        },
        "runner": {
            "path": str(args.runner.resolve()),
            "sha256": runner_hash,
        },
        "image": args.image,
        "platform": args.platform,
        "command": command,
        "limits": {"timeout_seconds": args.timeout},
        "executed": not args.dry_run,
    }

    if not args.dry_run:
        report_data["image_id"] = image_identity(args.image)
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                timeout=args.timeout,
            )
        except subprocess.TimeoutExpired as error:
            report_data["timed_out"] = True
            report_data["stdout"] = error.stdout or ""
            report_data["stderr"] = error.stderr or ""
            report.write_text(json.dumps(report_data, indent=2) + "\n")
            raise
        report_data["returncode"] = result.returncode
        report_data["stdout"] = result.stdout
        report_data["stderr"] = result.stderr
        if expected_output is not None:
            report_data["expected_output"] = {
                "path": str(args.expect_output),
                "created": expected_output.is_file(),
            }
            if expected_output.is_file():
                report_data["expected_output"]["sha256"] = sha256(expected_output)

    report.write_text(json.dumps(report_data, indent=2) + "\n")
    if not args.dry_run and report_data["returncode"] != 0:
        raise SystemExit(report_data["returncode"])
    if not args.dry_run and expected_output is not None:
        if not report_data["expected_output"]["created"]:
            raise SystemExit("candidate did not create the expected output")


if __name__ == "__main__":
    main()
