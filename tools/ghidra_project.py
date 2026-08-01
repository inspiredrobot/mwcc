#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import subprocess
from pathlib import Path

from verify_original import verify


def find_analyze_headless() -> Path:
    explicit = os.environ.get("GHIDRA_ANALYZE_HEADLESS")
    if explicit:
        path = Path(explicit)
        if path.is_file():
            return path
        raise FileNotFoundError(f"GHIDRA_ANALYZE_HEADLESS does not exist: {path}")

    executable = shutil.which("analyzeHeadless")
    if executable:
        return Path(executable).resolve()

    ghidra_run = shutil.which("ghidraRun")
    if ghidra_run:
        resolved = Path(ghidra_run).resolve()
        candidate = resolved.parent.parent / "libexec" / "support" / "analyzeHeadless"
        if candidate.is_file():
            return candidate

    candidates = sorted(Path("/opt/homebrew/Cellar/ghidra").glob("*/libexec/support/analyzeHeadless"))
    if candidates:
        return candidates[-1]
    raise FileNotFoundError("could not locate Ghidra analyzeHeadless")


def find_java_home() -> str:
    configured = os.environ.get("JAVA_HOME")
    if configured and (Path(configured) / "bin" / "java").is_file():
        return configured
    java_home = Path("/usr/libexec/java_home")
    if java_home.is_file():
        return subprocess.check_output([str(java_home)], text=True).strip()
    raise FileNotFoundError("set JAVA_HOME to a JDK supported by Ghidra")


def import_project(config_path: Path, stamp: Path | None) -> None:
    config, original = verify(config_path)
    project_root = Path("build") / config["version"] / "ghidra"
    user_root = Path("build") / config["version"] / "ghidra-user"
    project_root.mkdir(parents=True, exist_ok=True)
    user_root.mkdir(parents=True, exist_ok=True)

    project_name = config["version"].lower()
    project_file = project_root / f"{project_name}.gpr"
    command = [str(find_analyze_headless()), str(project_root), project_name]
    if project_file.exists():
        command.extend(["-process", original.name])
    else:
        command.extend(["-import", str(original.resolve())])
    command.extend(["-analysisTimeoutPerFile", "600", "-max-cpu", "4"])

    env = os.environ.copy()
    env["JAVA_HOME"] = find_java_home()
    option = f"-Duser.home={user_root.resolve()}"
    env["JAVA_TOOL_OPTIONS"] = " ".join(
        item for item in (env.get("JAVA_TOOL_OPTIONS"), option) if item
    )
    subprocess.run(command, check=True, env=env)

    if stamp:
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.write_text(config["sha256"] + "\n", encoding="utf-8")


def run_script(
    config_path: Path, script: str, output: Path, script_args: tuple[str, ...] = ()
) -> None:
    config, original = verify(config_path)
    project_root = Path("build") / config["version"] / "ghidra"
    user_root = Path("build") / config["version"] / "ghidra-user"
    project_name = config["version"].lower()
    project_file = project_root / f"{project_name}.gpr"
    if not project_file.is_file():
        raise FileNotFoundError("run 'ninja ghidra' before exporting analysis")

    command = [
        str(find_analyze_headless()),
        str(project_root),
        project_name,
        "-process",
        original.name,
        "-noanalysis",
        "-scriptPath",
        str((Path("tools") / "ghidra_scripts").resolve()),
        "-postScript",
        script,
        str(output.resolve()),
        *script_args,
    ]
    env = os.environ.copy()
    env["JAVA_HOME"] = find_java_home()
    option = f"-Duser.home={user_root.resolve()}"
    env["JAVA_TOOL_OPTIONS"] = " ".join(
        item for item in (env.get("JAVA_TOOL_OPTIONS"), option) if item
    )
    subprocess.run(command, check=True, env=env)


def main() -> None:
    parser = argparse.ArgumentParser(description="Manage the local MWCC Ghidra project")
    subparsers = parser.add_subparsers(dest="command", required=True)
    import_parser = subparsers.add_parser("import")
    import_parser.add_argument("--config", type=Path, required=True)
    import_parser.add_argument("--stamp", type=Path)
    export_parser = subparsers.add_parser("export-optimizer")
    export_parser.add_argument("--config", type=Path, required=True)
    export_parser.add_argument("--output", type=Path, required=True)
    subsystems_parser = subparsers.add_parser("export-subsystems")
    subsystems_parser.add_argument("--config", type=Path, required=True)
    subsystems_parser.add_argument("--output", type=Path, required=True)
    functions_parser = subparsers.add_parser("export-functions")
    functions_parser.add_argument("--config", type=Path, required=True)
    functions_parser.add_argument("--output", type=Path, required=True)
    functions_parser.add_argument("addresses", nargs="+")
    leaves_parser = subparsers.add_parser("rank-leaves")
    leaves_parser.add_argument("--config", type=Path, required=True)
    leaves_parser.add_argument("--output", type=Path, required=True)
    leaves_parser.add_argument("--start", required=True)
    leaves_parser.add_argument("--end", required=True)
    leaves_parser.add_argument("--limit", type=int, default=50)
    args = parser.parse_args()

    if args.command == "import":
        import_project(args.config, args.stamp)
    elif args.command == "export-optimizer":
        run_script(args.config, "ExportOptimizer.java", args.output)
    elif args.command == "export-subsystems":
        run_script(args.config, "ExportSubsystems.java", args.output)
    elif args.command == "export-functions":
        run_script(
            args.config,
            "ExportFunctions.java",
            args.output,
            tuple(args.addresses),
        )
    elif args.command == "rank-leaves":
        run_script(
            args.config,
            "RankLeafFunctions.java",
            args.output,
            (args.start, args.end, str(args.limit)),
        )


if __name__ == "__main__":
    main()
