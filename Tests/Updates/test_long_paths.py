"""Windows staging/install/recovery/cache at paths beyond MAX_PATH.

Uses inert files, the production managed code and the supplied native helper.
The optional Mono executable exercises the same managed sources with Unity's
runtime. Framework tests opt into modern path parsing in their own config;
neither mode changes the machine's long-path policy.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
from framework_runtime import configure_managed_test_driver
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ASSET = "InitialDUnity_Data/StreamingAssets/IDAS3/data/localization/ja/original_assets/attract/cards/adv_caution/adv_caution.idasmesh"
ADDED = ASSET.replace("adv_caution.idasmesh", "new_caution.idasmesh")
CORE = {"InitialDUnity.exe": b"executable fixture", "UnityPlayer.dll": b"engine fixture",
        "InitialDUnity_Data/globalgamemanagers": b"version fixture",
        "InitialDUnity_Data/Managed/Assembly-CSharp.dll": b"managed fixture"}
PRIVATE = {"userdata/card.json": b"private save", "rom/keep.bin": b"inert ROM sentinel"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def io(path):
    return "\\\\?\\" + str(path.resolve())


def write_tree(root, entries):
    for relative, data in entries.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def archive(path, entries):
    with zipfile.ZipFile(path, "x", zipfile.ZIP_DEFLATED) as package:
        for name, data in entries.items():
            package.writestr(name, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--mono", type=Path)
    parser.add_argument("--report-dir", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt" or not __debug__:
        parser.error("Windows with assertions enabled is required")
    proof = args.report_dir.resolve()
    proof.mkdir(parents=True, exist_ok=False)
    helper = args.helper.resolve(strict=True)
    runtime = [args.mono.resolve(strict=True)] if args.mono else []
    report = {"passed": False, "scope": __doc__.strip(), "helperSha256": sha(helper),
              "runtime": str(runtime[0]) if runtime else "Framework with modern path parsing",
              "sourceHashes": {name: sha(ROOT / name) for name in ("Assets/Scripts/Idas3UpdatePaths.cs",
                  "Assets/Scripts/Idas3UpdateStaging.cs", "Assets/Scripts/Idas3UpdateCache.cs")}, "tests": []}
    compiler = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                                  ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]

    def run(command):
        return subprocess.run(list(map(str, command)), capture_output=True, text=True, errors="replace",
                              timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)

    def managed(exe, *params):
        return run([*runtime, exe, *params])

    def compile_driver(name, sources, references):
        exe = proof / (name + ".exe")
        configure_managed_test_driver(exe)
        result = run([compiler, "/nologo", "/out:" + str(exe), *["/r:" + r for r in references],
                      ROOT / "Assets/Scripts/Idas3UpdatePaths.cs", *[ROOT / s for s in sources]])
        assert result.returncode == 0, result.stdout + result.stderr
        return exe

    def fixture(name):
        folder = proof / name
        # Keep every component legal, but force real game, stage and backup
        # filenames past MAX_PATH even on a short CI checkout.
        folder /= "日本語 spaces " + "x" * max(1, 185 - len(str(folder)))
        game = folder / "Game"
        session = folder / "InitialDUpdates" / uuid.uuid4().hex
        game.mkdir(parents=True)
        session.mkdir(parents=True)
        (folder / "ISOLATED_UPDATE_TEST.txt").write_text("Disposable updater fixture")
        assert len(str(game / ASSET)) > 300 and len(str(session / "stage" / ASSET)) > 300
        return folder, game, session

    try:
        driver = compile_driver("StageDriver", ["Assets/Scripts/Idas3UpdateStaging.cs", "Tests/Updates/StageDriver.cs"],
                                ["System.IO.Compression.dll", "System.IO.Compression.FileSystem.dll", "System.Web.Extensions.dll"])
        cache = compile_driver("LongPathCacheDriver", ["Assets/Scripts/Idas3UpdateCache.cs", "Tests/Updates/LongPathCacheDriver.cs"],
                               ["System.Runtime.Serialization.dll"])
        for name in ("full", "patch-retained", "patch-corrupt-retained", "rollback-locked", "recover", "recover-corrupt-backup", "target-junction"):
            row = {"name": name, "passed": False}
            report["tests"].append(row)
            folder, game, session = fixture(name)
            old = {**{k: b"old " + v for k, v in CORE.items()}, ASSET: b"old long asset"}
            new = {**CORE, ASSET: b"new long asset", ADDED: b"added long asset"}
            write_tree(game, {**old, **PRIVATE})
            stamps = {k: (game / k).stat().st_mtime_ns for k in PRIVATE}
            patch = name.startswith("patch-")
            if patch:
                new[ASSET] = old[ASSET]
                entries = {k: v for k, v in new.items() if k != ASSET}
                entries["update-patch.json"] = json.dumps({"schema": 1, "baseVersion": "1.0.0", "targetVersion": "1.0.1",
                    "files": [{"path": k, "size": len(v), "sha256": hashlib.sha256(v).hexdigest(), "included": k != ASSET} for k, v in new.items()]}).encode()
                archive(session / "game.zip", entries)
                if name == "patch-corrupt-retained":
                    old[ASSET] = b"damaged retained file"
                    (game / ASSET).write_bytes(old[ASSET])
            else:
                archive(session / "game.zip", CORE)
                archive(session / "content.zip", {ASSET: new[ASSET], ADDED: new[ADDED]})
            outside = None
            if name == "target-junction":
                # A junction above the long file must still be detected. Move
                # only this exact owned subtree to its fixture sibling.
                target = game / "InitialDUnity_Data/StreamingAssets/IDAS3"
                outside = folder / "outside"
                assert target.resolve().is_relative_to(proof) and outside.is_relative_to(proof)
                target.rename(outside)
                # PowerShell is only used to create the disposable junction.
                env = dict(os.environ, IDAS3_LINK=str(target), IDAS3_OUTSIDE=str(outside))
                result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                    "$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path $env:IDAS3_LINK -Target $env:IDAS3_OUTSIDE | Out-Null"],
                    env=env, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
                assert result.returncode == 0, result.stdout + result.stderr
            extra = [] if patch else [session / "content.zip", sha(session / "content.zip")]
            staged = managed(driver, game, session, sha(session / "game.zip"), "patch" if patch else "full", "1.0.0", "1.0.1", "0", "0", *extra)
            (folder / "staging.log").write_text(staged.stdout + staged.stderr, encoding="utf-8")
            rejected = name in ("patch-corrupt-retained", "target-junction")
            assert (staged.returncode != 0) == rejected, staged.stdout + staged.stderr
            expected = old
            if rejected:
                assert not (session / "install.plan").exists()
                assert ("full download" if patch else "links") in staged.stderr
            elif name.startswith("recover"):
                write_tree(session / "backup", old)
                write_tree(game, new)
                (game / ASSET).write_bytes(b"torn replacement")
                (session / "ready").write_text("Prepared")
                corrupt = name == "recover-corrupt-backup"
                if corrupt:
                    (session / "backup" / ASSET).write_bytes(b"bad backup")
                    expected = {**new, ASSET: b"torn replacement"}
                recovered = run([helper, session / "install.plan", "--recover", "--test"])
                assert (recovered.returncode == 0) != corrupt, recovered.stderr
                if not corrupt:
                    status = json.loads((session / "result.json").read_text())
                    assert status["recovered"] and status["cleanupSafe"] and not status["rollbackNeeded"]
                    assert run([helper, session / "install.plan", "--recover", "--test"]).returncode == 0
                else:
                    assert not (session / "result.json").exists()
                assert (session / "backup" / ASSET).exists()
            else:
                handle = None
                try:
                    if name == "rollback-locked":
                        handle = kernel.CreateFileW(io(game / ASSET), 0x80000000, 1, None, 3, 0, None)
                        assert handle not in (None, ctypes.c_void_p(-1).value)
                    installed = run([helper, session / "install.plan", "--test"])
                    assert (installed.returncode == 0) == (name != "rollback-locked"), installed.stderr
                finally:
                    if handle not in (None, ctypes.c_void_p(-1).value):
                        kernel.CloseHandle(handle)
                status = json.loads((session / "result.json").read_text())
                assert status["cleanupSafe"] and not status["rollbackNeeded"], status
                if name != "rollback-locked":
                    expected = new
                for payload in ("stage", "backup", "game.zip", "content.zip"):
                    assert not (session / payload).exists(), "Leaked payload: " + payload
            for relative, data in {**expected, **PRIVATE}.items():
                assert (game / relative).read_bytes() == data, (name, relative)
            assert all((game / k).stat().st_mtime_ns == v for k, v in stamps.items())
            if ADDED not in expected:
                assert not (game / ADDED).exists()
            row.update(passed=True, gamePathLength=len(str(game / ASSET)), stagedPathLength=len(str(session / "stage" / ASSET)), privatePreserved=True)
            print("PASS " + name, flush=True)

        for name in ("cache-safe", "cache-lease", "cache-uncertain", "cache-locked"):
            row = {"name": name, "passed": False}
            report["tests"].append(row)
            _, _, session = fixture(name)
            payload = session / "stage" / ASSET
            write_tree(session / "stage", {ASSET: b"staged fixture"})
            if name == "cache-uncertain":
                write_tree(session / "backup", {ASSET: b"recovery fixture"})
                (session / "ready").write_text("Prepared")
            else:
                marked = managed(cache, "lease" if name == "cache-lease" else "safe", session)
                assert marked.returncode == 0, marked.stderr
            handle = None
            try:
                if name == "cache-locked":
                    handle = kernel.CreateFileW(io(payload), 0x80000000, 1, None, 3, 0, None)
                    assert handle not in (None, ctypes.c_void_p(-1).value)
                cleaned = managed(cache, "clean", session.parent)
                assert cleaned.returncode == 0, cleaned.stderr
                assert session.exists() == (name in ("cache-uncertain", "cache-locked")), cleaned.stdout
            finally:
                if handle not in (None, ctypes.c_void_p(-1).value):
                    kernel.CloseHandle(handle)
            if name == "cache-locked":
                assert (session / "result.json").exists()
                cleaned = managed(cache, "clean", session.parent)
                assert cleaned.returncode == 0 and not session.exists(), cleaned.stdout + cleaned.stderr
            if name == "cache-uncertain":
                assert payload.read_bytes() == b"staged fixture" and (session / "backup" / ASSET).read_bytes() == b"recovery fixture"
            row.update(passed=True, stagedPathLength=len(str(payload)))
            print("PASS " + name, flush=True)
        report.update(passed=True, count=len(report["tests"]))
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (proof / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
