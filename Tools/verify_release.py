"""Run offline regressions and identify the exact candidate being checked.

This command does not publish, contact production services, or certify a release.
Missing integration/manual gates remain explicit in summary.json, even on success.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
MANAGED_TESTS = (
    "build_compatibility", "eos_platform", "eos_packets", "eos_room_rules",
    "eos_sanctions", "eos_transport", "eos_activity", "online_session",
    "online_player_directory", "online_directory_http", "record_storage",
    "lobby_chat_session", "online_username",
)
PACKAGE_TESTS = ("rom_package", "custom_music_package", "eos_migration_patch")
PENDING_GATES = (
    "Fresh native/Unity build provenance bound to this source and candidate",
    "Native reference and integration tests for the candidate",
    "Packaged startup, save/input, results and rematch diagnostics",
    "Two-PC Quick Match and room-code race cycles on different networks",
    "Supported update and fresh-install journeys with user-data preservation",
    "Declared hardware performance and language/resolution acceptance matrix",
)


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def digest_json(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def write_json(path, value):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def hidden_process_options():
    return {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, **hidden_process_options())


def source_identity():
    """Hash code/config, including relevant untracked files, without private data.

    Large runtime content belongs to the candidate inventory. This is not proof
    that a pre-existing assembly was built from these inputs.
    """
    paths = git("ls-files", "-z", "--cached", "--others", "--exclude-standard", "--",
                "Assets", "Packages", "ProjectSettings", "Native", "ThirdParty",
                "Build Native.cmd", "Build Unity.cmd", "Tools", "Tests", "Leaderboard", ".github").decode("utf-8").split("\0")
    files = {}
    for name in sorted(set(filter(None, paths))):
        path = ROOT / name
        # Native plugin DLLs are build outputs; their candidate bytes are hashed
        # separately. Never inspect ignored game-client or admin credentials.
        if name.startswith(("Assets/Plugins/", "Native/data/")) or path.suffix.lower() in (".pdb", ".log"):
            continue
        if path.is_symlink():
            raise ValueError("Source fingerprint does not follow symlinks: " + name)
        files[name] = sha256(path) if path.is_file() else "deleted"
    return {"commit": git("rev-parse", "HEAD").decode().strip(),
            "workingTreeChanges": git("status", "--porcelain=v1", "-uno").decode("utf-8").splitlines(),
            "codeAndConfigSha256": digest_json(files), "files": files,
            "scope": "Tracked and relevant nonignored code/config; runtime content and built plugins are inventoried separately. No build provenance claim."}


def load_tool(name, filename):
    spec = importlib.util.spec_from_file_location(name, ROOT / "Tools" / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def candidate_inventory(player):
    """Reuse the shipping allowlist before opening any candidate file."""
    collector = load_tool("release_package", "Build-ReleasePackage.py")
    patch = load_tool("update_patch", "Build-UpdatePatch.py")
    entries = collector.collect_player_files(player)
    names = {path.relative_to(player).as_posix() for path in entries}
    missing = sorted(patch.REQUIRED - names)
    if missing:
        raise ValueError("Incomplete player: " + ", ".join(missing))
    files = {path.relative_to(player).as_posix(): {"bytes": path.stat().st_size, "sha256": sha256(path)}
             for path in entries}
    return {"root": str(player), "fileCount": len(files),
            "bytes": sum(row["bytes"] for row in files.values()),
            "inventorySha256": digest_json(files), "files": files,
            "scope": "Files allowed by release packaging. Personal music is pruned; ROM payloads and player saves are rejected before hashing."}


def require_passed_report(path):
    report = json.loads(path.read_text(encoding="utf-8-sig"))
    if report.get("passed") is not True:
        raise ValueError("Missing positive passed flag in " + str(path))
    return report


def supported_node(version, engines):
    required = re.fullmatch(r"\^(\d+)\.(\d+)\.(\d+)", engines)
    actual = re.fullmatch(r"v?(\d+)\.(\d+)\.(\d+)", version.strip())
    if not required or not actual:
        return False
    minimum = tuple(map(int, required.groups()))
    current = tuple(map(int, actual.groups()))
    return current[0] == minimum[0] and current >= minimum


class Run:
    def __init__(self, output, suite):
        self.output = output
        output.mkdir(parents=True, exist_ok=False)
        self.summary = {"schema": "idas3-release-verification-v1", "suite": suite,
                        "startedAt": datetime.now(timezone.utc).isoformat(),
                        "status": "running", "passed": False, "releaseReady": False,
                        "pendingGates": list(PENDING_GATES), "checks": [], "tools": {}}
        self.save()

    def save(self):
        write_json(self.output / "summary.json", self.summary)

    def command(self, name, command, cwd=ROOT, timeout=180, report=None, env=None):
        entry = {"name": name, "passed": False, "command": list(map(str, command)),
                 "log": name + ".log"}
        self.summary["checks"].append(entry)
        self.save()
        began = time.monotonic()
        print("RUN " + name, flush=True)
        try:
            with (self.output / entry["log"]).open("wb") as log:
                with subprocess.Popen(list(map(str, command)), cwd=cwd, stdout=log,
                                      stderr=subprocess.STDOUT, env=env,
                                      **hidden_process_options()) as process:
                    try:
                        exit_code = process.wait(timeout=timeout)
                    except subprocess.TimeoutExpired:
                        # Only terminate this command and its children, never
                        # another editor/game already running on the machine.
                        if os.name == "nt":
                            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                                           stdout=log, stderr=subprocess.STDOUT, **hidden_process_options())
                        if process.poll() is None:
                            process.kill()
                        process.wait()
                        raise
            entry["exitCode"] = exit_code
            if exit_code:
                raise RuntimeError(f"{name} exited {exit_code}; see {entry['log']}")
            if report is not None:
                data = require_passed_report(report)
                entry["report"] = str(report.relative_to(self.output))
                entry["reportSha256"] = sha256(report)
                entry["reportedCount"] = data.get("count", data.get("total_checks"))
            entry["passed"] = True
            print("PASS " + name, flush=True)
        except Exception as error:
            entry["error"] = str(error)
            raise
        finally:
            entry["elapsedSeconds"] = round(time.monotonic() - began, 3)
            self.save()

    def finish(self, error=None):
        self.summary["passed"] = error is None and bool(self.summary["checks"]) and all(
            check["passed"] for check in self.summary["checks"])
        self.summary["status"] = self.summary["suite"] + "_checks_passed" if self.summary["passed"] else "failed"
        self.summary["finishedAt"] = datetime.now(timezone.utc).isoformat()
        if error is not None:
            self.summary["error"] = str(error)
        self.save()


def run_source(run, args):
    executable = shutil.which(args.node)
    if executable is None:
        raise FileNotFoundError("Node executable not found: " + args.node)
    args.node = str(Path(executable).resolve())
    node = subprocess.check_output([args.node, "--version"], text=True, **hidden_process_options()).strip()
    engine = json.loads((ROOT / "Leaderboard/package.json").read_text())["engines"]["node"]
    if not supported_node(node, engine):
        raise RuntimeError(f"Node {node} does not meet patched tooling requirement {engine}")
    run.summary["tools"].update(python=sys.version, node=node, nodeExecutable=args.node)
    compiler_module = load_tool("compatibility_compiler", "../Tests/Multiplayer/test_build_compatibility.py")
    compiler = compiler_module.find_compiler(args.compiler)
    run.summary["tools"]["csharpCompiler"] = str(compiler)
    run.summary["tools"]["csharpCompilerSha256"] = sha256(compiler)
    run.save()
    run.command("verification-tool-tests", [sys.executable, "-X", "utf8", "Tests/Release/test_verification.py"])
    run.command("fresh-build-tool-tests", [sys.executable, "-X", "utf8", "Tests/Release/test_fresh_build.py"])
    run.command("packaged-player-tool-tests", [sys.executable, "-X", "utf8", "Tests/Release/test_packaged_player.py"])
    run.command("setup-package-tool-tests", [sys.executable, "-X", "utf8", "Tests/Release/test_setup_package.py"])
    tests = sorted(str(path) for path in (ROOT / "Leaderboard/test").glob("*.test.mjs"))
    if not tests:
        raise RuntimeError("No backend tests found")
    run.command("backend", [args.node, "--test", *tests], cwd=ROOT / "Leaderboard")
    for name in MANAGED_TESTS:
        folder = run.output / name
        report_name = "unit-report.json" if name == "build_compatibility" else "report.json"
        run.command(name, [sys.executable, "-X", "utf8", f"Tests/Multiplayer/test_{name}.py",
                          "--compiler", compiler, "--report-dir", folder], timeout=300,
                    report=folder / report_name)
    for name in PACKAGE_TESTS:
        command = [sys.executable, "-X", "utf8", f"Tests/Updates/test_{name}.py"]
        report = None
        if name == "eos_migration_patch":
            folder = run.output / name
            folder.mkdir()
            installer = folder / "Idas3UpdateInstaller.exe"
            shutil.copyfile(ROOT / "Assets/Resources/UpdateInstaller.exe.bytes", installer)
            run.summary["tools"]["bundledInstallerSha256"] = sha256(installer)
            command.extend(["--installer", installer, "--report-dir", folder])
            report = folder / "report.json"
        run.command(name, command, report=report)
    folder = run.output / "setup-launcher"
    run.command("setup-launcher", [sys.executable, "-X", "utf8", "Tests/Updates/test_setup_launcher.py",
                                    "--report-dir", folder], report=folder / "report.json")
    folder = run.output / "setup-extraction"
    run.command("setup-extraction", [sys.executable, "-X", "utf8", "Tests/Updates/test_setup_extraction.py",
                                      "--report-dir", folder], report=folder / "report.json")
    run_updater(run)


def run_updater(run):
    # Build the current helper source and also exercise the binary that Unity
    # actually embeds. A passing source build cannot stand in for a stale resource.
    builder=load_tool("updater_build_tools", "build_release_candidate.py")
    studio,generator=builder.find_visual_studio()
    cmake=shutil.which("cmake")
    if not cmake:raise FileNotFoundError("CMake is required for native updater verification")
    folder=run.output/"updater"
    binaries=folder/"bin"
    run.command("updater-configure",[cmake,"-S",ROOT/"Tools/Updater","-B",folder/"build","-G",generator,"-A","x64",
                "-DCMAKE_GENERATOR_INSTANCE="+studio["installationPath"],"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY="+binaries.as_posix()],timeout=300)
    run.command("updater-build",[cmake,"--build",folder/"build","--config","Release","--parallel","2"],timeout=300)
    child=binaries/"Release/UpdateTestChild.exe"
    bundled=folder/"bundled.exe"
    shutil.copyfile(ROOT/"Assets/Resources/UpdateInstaller.exe.bytes",bundled)
    for kind,helper in (("compiled",binaries/"Release/Idas3UpdateInstaller.exe"),("bundled",bundled)):
        normal=folder/(kind+"-installation")
        recovery=folder/(kind+"-recovery")
        run.command("updater-"+kind+"-installation",[sys.executable,"-X","utf8","Tests/Updates/test_native_installer.py",
                    "--helper",helper,"--child",child,"--report-dir",normal],timeout=180,report=normal/"native-windows-report.json")
        run.command("updater-"+kind+"-recovery",[sys.executable,"-X","utf8","Tests/Updates/test_native_recovery.py",
                    "--helper",helper,"--child",child,"--seed-helper",bundled,"--report-dir",recovery],timeout=300,report=recovery/"report.json")
    cache=folder/"cache"
    run.command("updater-cache",[sys.executable,"-X","utf8","Tests/Updates/test_cache.py","--report-dir",cache],report=cache/"report.json")
    launcher=folder/"launcher"
    run.command("recovery-launcher",[sys.executable,"-X","utf8","Tests/Updates/test_recovery_launcher.py",
                "--helper",bundled,"--report-dir",launcher],report=launcher/"report.json")
    run.command("setup-package",[sys.executable,"-X","utf8","Tools/Build-SetupPackage.py","--helper",bundled,
                "--archive",folder/"Initial-D-Setup.zip","--installation-report",folder/"bundled-installation/native-windows-report.json",
                "--recovery-report",folder/"bundled-recovery/report.json","--launcher-report",launcher/"report.json"],report=folder/"Initial-D-Setup.report.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=("source", "candidate"), default="source")
    parser.add_argument("--player", type=Path, help="Clean candidate directory, required for candidate suite")
    parser.add_argument("--output", type=Path, help="NEW evidence directory; defaults to Verification/release-check-<unique-id>")
    parser.add_argument("--node", default="node", help="Patched Node executable")
    parser.add_argument("--compiler", help="Visual Studio Roslyn csc.exe")
    parser.add_argument("--mono", type=Path, help="Matching Unity Mono for candidate localization check")
    args = parser.parse_args()
    if sys.flags.optimize:
        parser.error("Do not disable Python assertions; existing packaging checks depend on them")
    if os.name != "nt":
        parser.error("The source suite requires Windows Roslyn/.NET and PowerShell 5.1")
    if args.suite == "candidate" and args.player is None:
        parser.error("--suite candidate requires an explicit --player; Current is not assumed")
    if args.suite == "source" and args.player is not None:
        parser.error("Use --suite candidate to verify --player")
    output = (args.output or ROOT / "Verification" / ("release-check-" + uuid.uuid4().hex)).resolve()
    if args.player and output.is_relative_to(args.player.resolve()):
        parser.error("Evidence must be outside the candidate directory")
    run = Run(output, args.suite)
    try:
        print("Fingerprinting source and configuration...", flush=True)
        before = source_identity()
        write_json(output / "source.json", before)
        run.summary["source"] = {key: value for key, value in before.items() if key != "files"}
        run.summary["tools"]["unityProjectVersion"] = (ROOT / "ProjectSettings/ProjectVersion.txt").read_text().strip()
        player = args.player.resolve(strict=True) if args.player else None
        if player:
            print("Hashing candidate files using the release allowlist...", flush=True)
            inventory = candidate_inventory(player)
            write_json(output / "candidate.json", inventory)
            run.summary["candidate"] = {key: value for key, value in inventory.items() if key != "files"}
            run.save()
        run_source(run, args)
        if player:
            folder = output / "localization"
            command = [sys.executable, "-X", "utf8", "Tests/Integrations/test_localization_catalog.py",
                       "--player", player, "--report-dir", folder]
            if args.mono:
                command.extend(["--mono", args.mono])
            run.command("localization", command, report=folder / "report.json")
            folder = output / "candidate-compatibility"
            run.command("candidate-compatibility", [sys.executable, "-X", "utf8", "Tests/Multiplayer/test_build_compatibility.py",
                        "--build", player, "--compiler", run.summary["tools"]["csharpCompiler"], "--report-dir", folder], report=folder / "unit-report.json")
            print("Checking candidate files stayed unchanged...", flush=True)
            after = candidate_inventory(player)
            if after["inventorySha256"] != inventory["inventorySha256"]:
                raise RuntimeError("Candidate files changed during verification; discard these results")
            run.summary["candidateUnchanged"] = True
        if source_identity()["codeAndConfigSha256"] != before["codeAndConfigSha256"]:
            raise RuntimeError("Source/config changed during verification; discard these results")
        run.summary["sourceUnchanged"] = True
        run.finish()
    except Exception as error:
        run.finish(error)
        print("FAIL: " + str(error), file=sys.stderr)
        print("Evidence: " + str(output / "summary.json"), file=sys.stderr)
        return 1
    print("Evidence: " + str(output / "summary.json"))
    print("Selected checks passed. Release readiness remains incomplete; see pendingGates.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
