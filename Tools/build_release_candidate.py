"""Build a new Windows player with native tests and source-to-output evidence.

Does not publish, replace Current, or copy ROMs/saves. Bundled third-party SDKs
and the existing updater resource are recorded inputs, not rebuilt dependencies.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET

import verify_release as verify

ROOT = verify.ROOT
NATIVE_TESTS = (
    "host_steering_smoothing", "physics", "race", "original_host_input",
    "original_driving_session", "online_race_timeline", "online_scored_finish", "original_race_rules",
    "original_collision_reference", "native_multiplayer", "native_asset_storage",
    "unity_shared_renderer", "unity_ui_memory", "unity_ui_capture", "unity_ui_reload", "hud_composition_perf", "course_mesh_cache",
    "local_driver_profiles", "local_driver_profile_recovery", "online_result_reward", "time_attack_ghost",
)
INPUT_TREES = ("Assets", "Packages", "ProjectSettings", "Native/data", "RuntimeAssets", "ThirdParty")


def require_plain_path(path):
    path = Path(path).absolute()
    for ancestor in (path, *path.parents):
        if ancestor.is_symlink() or (hasattr(ancestor, "is_junction") and ancestor.is_junction()):
            raise ValueError("Release paths cannot use links: " + str(ancestor))
    return path.resolve()


def build_inputs(progress=False):
    """Hash actual content/config and prebuilt SDK inputs without copying secrets.

    These trees contain build inputs only. Never walk the repository root,
    Native/userdata, ROMs, Downloads, or desktop installations.
    """
    files = {}
    source_root = require_plain_path(ROOT)
    for tree in INPUT_TREES:
        base = require_plain_path(source_root / tree)
        if not base.is_dir():
            raise FileNotFoundError("Build input missing: " + tree)
        if progress:
            print("Inventory " + tree, flush=True)
        for directory, folders, names in os.walk(base, followlinks=False):
            for name in folders + names:
                path = Path(directory) / name
                info = path.lstat()
                # The root's ancestors were checked above; each descendant is
                # checked once before os.walk can descend into it. Re-resolving
                # every ancestor for every asset made the inventory very slow.
                if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
                    raise ValueError("Build input contains a link: " + str(path))
                if stat.S_ISREG(info.st_mode):
                    files[path.relative_to(source_root).as_posix()] = {"bytes": info.st_size, "sha256": verify.sha256(path)}
    return {"inventorySha256": verify.digest_json(files), "fileCount": len(files), "files": files,
            "scope": "Build-input hashes only. Includes local client configuration and bundled binaries; no credential values."}


def require_native_results(path, expected=NATIVE_TESTS):
    root = ET.parse(path).getroot()
    cases = root.findall(".//testcase")
    actual = [case.attrib.get("name") for case in cases]
    if len(actual) != len(expected) or set(actual) != set(expected):
        raise ValueError("Native test report does not contain exactly the required tests")
    for case in cases:
        if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")) or case.attrib.get("status") not in (None, "run"):
            raise ValueError("Native test did not pass: " + str(case.attrib.get("name")))
    return {"passed": True, "count": len(cases), "tests": actual}


def require_clean_unity_build(report):
    # A fresh destination can still receive Unity's cached player data. Require
    # the build's actual reported flags, not the caller's intended build mode.
    if report.get("cleanBuildCache") is not True or report.get("scriptsOnly") is not False:
        raise ValueError("Unity build lacks explicit clean player-data and script rebuild evidence")


def require_submission_versions(root):
    """Reject a candidate which cannot submit its own results; no service access."""
    root = Path(root)
    sources = (
        ("player", "ProjectSettings/ProjectSettings.asset", r"^\s*bundleVersion:\s*(\S+)\s*$"),
        ("client", "Assets/Scripts/Idas3CommunityTimes.cs", r'public const string RequiredSubmissionBuild\s*=\s*"([^"\r\n]+)";'),
        ("backend", "Leaderboard/src/core.mjs", r"export const REQUIRED_CLIENT_BUILD\s*=\s*'([^'\r\n]+)';"),
        ("deployment", "Leaderboard/wrangler.jsonc", r'"REQUIRED_CLIENT_BUILD"\s*:\s*"([^"\r\n]+)"'),
    )
    versions = {}
    for name, filename, pattern in sources:
        matches = re.findall(pattern, (root / filename).read_text(encoding="utf-8-sig"), re.M)
        if len(matches) != 1 or not re.fullmatch(r"\d+\.\d+\.\d+-community-replays\.\d+", matches[0]):
            raise ValueError("Missing or ambiguous submission version: " + name)
        versions[name] = matches[0]
    if len(set(versions.values())) != 1:
        raise ValueError("Submission version mismatch: " + repr(versions))
    return versions


def find_visual_studio():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    data = json.loads(subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
                         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-format", "json"],
                        **verify.hidden_process_options()))
    if not data:
        raise RuntimeError("Visual Studio C++ tools are required")
    selected = data[0]
    major = selected["installationVersion"].split(".")[0]
    generators = {"17": "Visual Studio 17 2022", "18": "Visual Studio 18 2026"}
    if major not in generators:
        raise RuntimeError("Unvalidated Visual Studio generator: " + major)
    return selected, generators[major]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="NEW evidence directory; player is created inside it")
    parser.add_argument("--unity", type=Path, help="Exact editor from ProjectSettings/ProjectVersion.txt")
    parser.add_argument("--original-image", required=True, type=Path, help="Local reference image for native parity tests; never packaged")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if os.name != "nt" or sys.flags.optimize:
        parser.error("Use Windows Python with assertions enabled")
    if not 1 <= args.jobs <= 16:
        parser.error("--jobs must be between 1 and 16")
    output = require_plain_path(args.output or ROOT / "Verification" / ("fresh-" + uuid.uuid4().hex[:10]))
    if any(output.is_relative_to(ROOT / tree) for tree in INPUT_TREES) or output == ROOT:
        parser.error("Build output must be outside input trees")
    original = args.original_image.resolve(strict=True)
    run = verify.Run(output, "fresh-build")
    try:
        run.summary["submissionVersions"] = require_submission_versions(ROOT)
        run.save()
        version = re.search(r"^m_EditorVersion: (\S+)", (ROOT / "ProjectSettings/ProjectVersion.txt").read_text(), re.M)[1]
        unity = (args.unity or Path("C:/Program Files/Unity/Hub/Editor") / version / "Editor/Unity.exe").resolve(strict=True)
        studio, generator = find_visual_studio()
        cmake = Path(shutil.which("cmake") or "").resolve(strict=True)
        ctest = cmake.with_name("ctest.exe")
        source = verify.source_identity()
        verify.write_json(output / "source.json", source)
        print("Hashing native data, Unity assets and imported course inputs...", flush=True)
        inputs = build_inputs(progress=True)
        verify.write_json(output / "inputs.json", inputs)
        run.summary.update(source={k: v for k, v in source.items() if k != "files"},
                           inputInventorySha256=inputs["inventorySha256"])
        run.summary["tools"].update(unity=str(unity), unitySha256=verify.sha256(unity), requiredUnityVersion=version,
                                   cmake=str(cmake), cmakeSha256=verify.sha256(cmake),
                                   visualStudio=studio["installationVersion"], visualStudioPath=studio["installationPath"],
                                   originalReferenceSha256=verify.sha256(original))
        build = output / "native"
        binaries = output / "native-bin"
        run.command("native-configure", [cmake, "-S", ROOT / "Native", "-B", build, "-G", generator, "-A", "x64",
                    "-DCMAKE_GENERATOR_INSTANCE=" + studio["installationPath"],
                    "-DIDAS3_ORIGINAL_IMAGE=" + str(original),
                    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=" + binaries.as_posix(),
                    "-DIDAS3_PLUGIN_OUTPUT_DIRECTORY=" + binaries.as_posix()], timeout=300)
        targets = [name + "_tests" for name in NATIVE_TESTS]
        run.command("native-build", [cmake, "--build", build, "--config", "Release", "--parallel", str(args.jobs),
                    "--target", "Idas3Unity", "pack_native_assets", *targets], timeout=3600)
        compiler_files = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
        if len(compiler_files) != 1:
            raise RuntimeError("Missing unique C++ compiler identity")
        compiler_text = compiler_files[0].read_text()
        compiler = Path(re.search(r'set\(CMAKE_CXX_COMPILER "([^"]+)"\)', compiler_text)[1])
        run.summary["tools"].update(cppCompiler=str(compiler), cppCompilerSha256=verify.sha256(compiler))
        shutil.copyfile(compiler_files[0], output / "CMakeCXXCompiler.cmake")
        run.command("native-tests", [ctest, "--test-dir", build, "-C", "Release", "-R", "^(" + "|".join(NATIVE_TESTS) + ")$",
                    "--output-on-failure", "--no-tests=error", "--output-junit", output / "native-results.xml"], timeout=900)
        result = require_native_results(output / "native-results.xml")
        verify.write_json(output / "native-report.json", result)
        run.summary["nativeTests"] = result
        native = binaries / "Release"
        run.summary["nativeArtifacts"] = {name: verify.sha256(native / name) for name in
                                          ("Idas3Unity.dll", "Idas3WheelFeedback.dll", "pack_native_assets.exe")}
        environment = os.environ.copy()
        temporary = output / "temp"
        temporary.mkdir()
        environment.update(IDAS3_RELEASE_OUTPUT=str(output / "player"), IDAS3_RELEASE_EVIDENCE=str(output),
                           IDAS3_RELEASE_NATIVE_DIRECTORY=str(native), TEMP=str(temporary), TMP=str(temporary))
        run.command("unity-build", [unity, "-batchmode", "-quit", "-force-d3d11", "-projectPath", ROOT,
                    "-buildTarget", "Win64", "-executeMethod", "Idas3Build.BuildReleaseCandidate",
                    "-logFile", output / "unity-editor.log"], timeout=3600, env=environment, report=output / "unity-build.json")
        unity_report = verify.require_passed_report(output / "unity-build.json")
        require_clean_unity_build(unity_report)
        if unity_report["unityVersion"] != version:
            raise RuntimeError("Built with a different Unity version than the project requires")
        if unity_report["applicationVersion"] != run.summary["submissionVersions"]["player"]:
            raise RuntimeError("Built player version differs from its submission policy")
        if Path(unity_report["output"]).resolve() != output / "player":
            raise RuntimeError("Unity built a different player path")
        player = verify.candidate_inventory(output / "player")
        for name in ("Idas3Unity.dll", "Idas3WheelFeedback.dll"):
            if player["files"]["InitialDUnity_Data/Plugins/x86_64/" + name]["sha256"] != run.summary["nativeArtifacts"][name]:
                raise RuntimeError("Player does not contain the freshly built plugin: " + name)
        verify.write_json(output / "candidate.json", player)
        run.summary["candidate"] = {k: v for k, v in player.items() if k != "files"}
        if verify.source_identity()["codeAndConfigSha256"] != source["codeAndConfigSha256"]:
            raise RuntimeError("Source/config changed while building; inspect the changes and build again")
        if build_inputs(progress=True)["inventorySha256"] != inputs["inventorySha256"]:
            raise RuntimeError("Build inputs changed while building; inspect the changes and build again")
        run.summary.update(sourceUnchanged=True, inputsUnchanged=True)
        run.summary["pendingGates"] = list(verify.PENDING_GATES[1:])
        run.summary["pendingGates"][0] = f"Remaining native reference coverage and packaged integration checks ({len(NATIVE_TESTS)} selected native tests passed)"
        run.finish()
    except Exception as error:
        run.finish(error)
        print("FAIL: " + str(error), file=sys.stderr)
        print("Evidence: " + str(output / "summary.json"), file=sys.stderr)
        return 1
    print("Fresh candidate: " + str(output / "player"))
    print("Build passed; integration and release acceptance are still required.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
