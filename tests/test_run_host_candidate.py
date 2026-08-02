#!/usr/bin/env python3

import sys
from pathlib import Path

sys.path.insert(0, str(Path("tools").resolve()))

from run_host_candidate import bind_mount, sandbox_command, verify_sha256  # noqa: E402


def main() -> None:
    command = sandbox_command(
        Path("/candidate/mwcc.exe"),
        Path("/runner/wibo"),
        Path("/probe"),
        Path("/result"),
        "local-image:test",
        "linux/arm64",
        ["-O4", "-c", "/input/probe.c", "-o", "/output/probe.obj"],
    )

    required_pairs = [
        ("--pull", "never"),
        ("--network", "none"),
        ("--cap-drop", "ALL"),
        ("--security-opt", "no-new-privileges"),
        ("--pids-limit", "64"),
        ("--memory", "512m"),
        ("--cpus", "1"),
    ]
    for option, value in required_pairs:
        index = command.index(option)
        assert command[index + 1] == value
    assert "--read-only" in command
    assert "/tmp:rw,noexec,nosuid,nodev,size=64m" in command

    mounts = [
        command[index + 1]
        for index, value in enumerate(command)
        if value == "--mount"
    ]
    assert len(mounts) == 4
    assert all("readonly" in mount for mount in mounts[:3])
    assert "readonly" not in mounts[3]
    assert command[-5:] == [
        "-O4",
        "-c",
        "/input/probe.c",
        "-o",
        "/output/probe.obj",
    ]
    assert bind_mount(Path("/a"), "/b", True).endswith(",readonly")

    try:
        verify_sha256(Path("tests/test_run_host_candidate.py"), "0" * 64)
    except ValueError as error:
        assert "SHA-256 mismatch" in str(error)
    else:
        raise AssertionError("bad expected hash was accepted")

    print("host candidate sandbox command tests passed")


if __name__ == "__main__":
    main()
