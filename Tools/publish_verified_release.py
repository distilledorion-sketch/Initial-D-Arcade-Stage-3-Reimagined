"""Verify an existing GitHub draft against complete local acceptance and assets.

Read-only by default. --publish is the only mutation: make that verified draft
public. Does not upload, delete assets, modify tags, edit notes, or bypass gates.
After an uncertain publish response, inspect with --verify-published; do not
blindly repeat a write. GitHub does not provide a transaction spanning a release
and its assets, so keep other uploaders stopped during this final step.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

import release_acceptance as gate
import verify_release as verify
import verify_release_upgrade as upgrade

SCHEMA = "idas3-publication-plan-v1"
REPO = upgrade.REPO
ROOT = verify.ROOT
SETUP_FILES = {"README.txt", "Install Initial D.cmd", "Install-InitialD.ps1",
               "Recover Initial D.cmd", "Recover-InitialD.ps1", "Idas3UpdateInstaller.exe"}


class GitHub:
    def request(self, route, payload=None):
        args = ["gh", "api", "--hostname", "github.com", "repos/" + REPO + "/" + route]
        if payload is not None:
            args += ["--method", "PATCH", "--input", "-"]
        result = subprocess.run(args, input=None if payload is None else json.dumps(payload),
                                text=True, encoding="utf-8", capture_output=True, timeout=120, cwd=verify.ROOT,
                                **verify.hidden_process_options())
        if result.returncode:
            # Do not echo authentication/environment or the complete response.
            raise RuntimeError("GitHub request failed for " + route + "; exit " + str(result.returncode))
        return json.loads(result.stdout)


def require(condition, message):
    gate.require(condition, message)


def version_names(version):
    match = re.fullmatch(r"(\d+\.\d+\.\d+)-community-replays\.(\d+)", version)
    require(match is not None, "Unsupported release version format")
    short = match[1] + "." + match[2]
    return {"core": "Initial-D-Arcade-Stage-3-Reimagined-" + short + "-Windows-x64.zip",
            "courses": "Initial-D-Additional-Courses-" + version + ".zip",
            "setup": "Initial-D-Setup-" + short + ".zip"}


def numeric_version(version):
    version_names(version)
    return tuple(map(int, re.findall(r"\d+", version)))


def zip_names(path, expected=None):
    with zipfile.ZipFile(path) as archive:
        rows = archive.infolist()
    names = [row.filename for row in rows]
    require(len(names) == len(set(name.casefold() for name in names)), "Duplicate ZIP entries")
    require(not any(row.is_dir() for row in rows), "Download ZIPs must contain only explicit files")
    require(not any((row.external_attr & 0x400) or ((row.external_attr >> 16) & 0xf000) == 0xa000 for row in rows),
            "Download ZIP contains a link or reparse entry")
    if expected is not None:
        require(set(names) == set(expected), "ZIP contains missing or unexpected entries: " + path.name)
    return names


def asset(base, ref, expected_name, expected_hash=None):
    require(isinstance(ref, dict) and isinstance(ref.get("path"), str), "Missing asset reference")
    path = gate.plain_file(base / ref["path"])
    require(path.name == expected_name, "Unexpected public asset filename: " + path.name)
    require(0 < path.stat().st_size < 2 * 1024**3, "Public asset exceeds download size limit")
    gate.reference(base, ref)
    if expected_hash is not None:
        require(ref["sha256"] == expected_hash, "Asset differs from the accepted/tested download: " + expected_name)
    return path, dict(bytes=path.stat().st_size, sha256=ref["sha256"])


def remote_assets(release):
    rows = release.get("assets", [])
    require(isinstance(rows, list), "GitHub asset inventory is missing")
    names = [row.get("name") for row in rows]
    require(all(isinstance(name, str) for name in names) and len(names) == len(set(name.casefold() for name in names)),
            "GitHub asset inventory has duplicate names")
    return {row["name"]: row for row in rows}


def require_public(release, version):
    require(release.get("draft") is False and release.get("prerelease") is False
            and release.get("tag_name") == "v" + version, "Expected a published stable baseline")


def require_remote_asset(row, expected, name):
    require(row.get("state") == "uploaded" and row.get("size") == expected["bytes"]
            and row.get("digest") == "sha256:" + expected["sha256"], "GitHub asset bytes differ: " + name)


def supported_bases(latest):
    tag = latest.get("tag_name", "")
    require(isinstance(tag, str) and tag.startswith("v"), "Latest release lacks its version tag")
    version = tag[1:]
    version_names(version)
    require_public(latest, version)
    versions = {version}
    pattern = re.compile(r"Initial-D-Update-from-(.+)-to-" + re.escape(version) + r"-Patch\.zip")
    for name in remote_assets(latest):
        match = pattern.fullmatch(name)
        if match:
            version_names(match[1])
            require(numeric_version(match[1]) < numeric_version(version), "Invalid baseline patch version")
            versions.add(match[1])
    return versions


def baseline_inventory(base, refs, release, version):
    require_public(release, version)
    published = remote_assets(release)
    names = version_names(version)
    expected = {names["core"]}
    if names["courses"] in published:
        expected.add(names["courses"])
    require(isinstance(refs, list) and len(refs) == len(expected), "Missing baseline downloads")
    paths = [gate.plain_file(base / ref["path"]) for ref in refs]
    require({path.name for path in paths} == expected, "Baseline downloads do not match their published names")
    # The normal inventory rejects personal/unsafe entries before opening their
    # payloads. Parse it before hashing the complete compressed archives.
    combined = upgrade.archive_inventory(paths)
    for ref, path in zip(refs, paths):
        require(path.name in published, "Baseline download is absent on GitHub")
        gate.reference(base, ref)
        require_remote_asset(published[path.name], dict(bytes=path.stat().st_size, sha256=ref["sha256"]), path.name)
    return combined


def verify_patch(path, base_inventory, target_inventory, base_version, target_version):
    patch_tool = verify.load_tool("publication_patch", "Build-UpdatePatch.py")
    patch_tool.validate_file_removals(base_inventory, target_inventory)
    with zipfile.ZipFile(path) as archive:
        names = zip_names(path)
        require("update-patch.json" in names, "Patch has no manifest")
        require(archive.getinfo("update-patch.json").file_size <= 16 * 1024**2, "Patch manifest is too large")
        manifest = json.loads(archive.read("update-patch.json"))
        require(type(manifest.get("schema")) is int and manifest["schema"] == 1 and manifest.get("baseVersion") == base_version
                and manifest.get("targetVersion") == target_version, "Patch versions do not match publication")
        records = manifest.get("files", [])
        paths = [row.get("path") for row in records]
        require(len(paths) == len(set(paths)) and set(paths) == set(target_inventory), "Patch target inventory is incomplete")
        included = {row["path"] for row in records if row.get("included") is True}
        require(set(names) == included | {"update-patch.json"}, "Patch has missing or unexpected payloads")
        for row in records:
            name = row["path"]
            expected = target_inventory[name]
            require(type(row.get("included")) is bool and type(row.get("size")) is int
                    and row["size"] == expected["bytes"] and row.get("sha256") == expected["sha256"],
                    "Patch entry differs from accepted candidate: " + name)
            if row["included"]:
                require(archive.getinfo(name).file_size == expected["bytes"], "Patch payload has wrong size")
                with archive.open(name) as stream:
                    require(hashlib.file_digest(stream, "sha256").hexdigest() == expected["sha256"], "Patch payload hash differs")
            else:
                old = base_inventory.get(name, {})
                require(old.get("size") == expected["bytes"] and old.get("sha256") == expected["sha256"],
                        "Retained patch file does not reconstruct candidate: " + name)


def verify_setup(path, report, version):
    gate.positive(report)
    require(report.get("version") == version, "Setup downloader is pinned to a different release")
    require(set(report.get("files", {})) == SETUP_FILES, "Setup evidence does not cover all recovery files")
    zip_names(path, SETUP_FILES)
    with zipfile.ZipFile(path) as archive:
        for name in SETUP_FILES:
            require(archive.getinfo(name).file_size < 16 * 1024**2, "Unexpected setup entry size")
            data = archive.read(name)
            require(hashlib.sha256(data).hexdigest() == report["files"][name], "Setup differs from its tested package")
            if name.endswith((".ps1", ".cmd")):
                require(data == (ROOT / "Tools" / name).read_bytes(), "Setup script differs from accepted source")
            elif name == "Idas3UpdateInstaller.exe":
                require(data == (ROOT / "Assets/Resources/UpdateInstaller.exe.bytes").read_bytes(), "Setup recovery helper differs from game helper")
        script = archive.read("Install-InitialD.ps1").decode("utf-8-sig")
        pinned = re.search(r"^\$version='([^']+)';", script, re.M)
        require(pinned is not None and pinned[1] == version, "Actual setup script downloads a different release")


def validate_local(plan_path, player, github, published=False):
    plan_path = gate.plain_file(plan_path)
    plan_hash = verify.sha256(plan_path)
    plan = gate.load(plan_path)
    require(plan.get("schema") == SCHEMA and plan.get("repository") == REPO, "Unsupported publication plan or repository")
    require(type(plan.get("releaseId")) is int and plan["releaseId"] > 0, "An explicit draft release ID is required")
    require(type(plan.get("baselineReleaseId")) is int and plan["baselineReleaseId"] > 0, "An explicit baseline release ID is required")
    accepted_path = gate.reference(plan_path.parent, plan.get("acceptance"))
    receipt = gate.check(accepted_path, player)
    version = receipt["applicationVersion"]
    require(plan.get("tag") == "v" + version and plan.get("sourceCommit") == receipt["sourceCommit"], "Publication targets a different version/commit")
    names = version_names(version)
    spec = gate.load(accepted_path)
    upgrade_path = gate.reference(accepted_path.parent, spec["reports"]["upgrade"])
    upgraded = gate.load(upgrade_path)
    candidate_path = gate.reference(accepted_path.parent, spec["reports"]["candidate"])
    candidate = gate.load(candidate_path)
    setup_rows = [row for row in candidate["checks"] if row["name"] == "setup-package"]
    require(len(setup_rows) == 1, "Setup packaging evidence is missing")
    setup_report_path = gate.inside(candidate_path.parent, setup_rows[0]["report"])
    require(verify.sha256(setup_report_path) == setup_rows[0]["reportSha256"], "Setup evidence changed")
    setup_report = gate.load(setup_report_path)
    inventory = verify.candidate_inventory(player)
    require(inventory["inventorySha256"] == receipt["candidateInventorySha256"], "Candidate changed after acceptance")
    supplied = plan.get("assets", {})
    require(set(supplied) == {"core", "courses", "setup", "patches", "checksums"}, "Publication asset roles are incomplete")
    expected = {}
    full_paths = []
    course_files = {name for name in inventory["files"] if len(Path(name).parts) > 3
                    and Path(name).parts[:2] == ("InitialDUnity_Data", "StreamingAssets") and Path(name).parts[2] in upgrade.COURSES}
    for index, role in enumerate(("core", "courses")):
        path = gate.plain_file(plan_path.parent / supplied[role]["path"])
        zip_names(path, course_files if role == "courses" else set(inventory["files"]) - course_files)
        path, row = asset(plan_path.parent, supplied[role], names[role], upgraded["targetArchives"][index]["sha256"])
        expected[path.name] = row
        full_paths.append(path)
    combined = upgrade.archive_inventory(full_paths)
    require({name: {"bytes": row["size"], "sha256": row["sha256"]} for name, row in combined.items()} == inventory["files"], "Full downloads differ from accepted game")
    setup = gate.plain_file(plan_path.parent / supplied["setup"]["path"])
    verify_setup(setup, setup_report, version)
    setup, row = asset(plan_path.parent, supplied["setup"], names["setup"], setup_report["sha256"])
    expected[setup.name] = row
    # Only after local acceptance and full/setup validation do we query GitHub.
    latest = github.request("releases/" + str(plan["baselineReleaseId"]) if published else "releases/latest")
    require(latest.get("id") == plan["baselineReleaseId"], "Published baseline changed; review a new plan")
    bases = supported_bases(latest)
    require(numeric_version(version) > numeric_version(latest["tag_name"][1:]), "Publication must advance the stable version")
    patches = supplied["patches"]
    require(isinstance(patches, list) and len(patches) == len(bases)
            and {item.get("baseVersion") for item in patches} == bases, "Patches omit supported baseline versions")
    for item in patches:
        old_version = item["baseVersion"]
        old_release = latest if old_version == latest["tag_name"][1:] else github.request("releases/tags/v" + old_version)
        old = baseline_inventory(plan_path.parent, item.get("baseDownloads"), old_release, old_version)
        path = gate.plain_file(plan_path.parent / item["path"])
        verify_patch(path, old, inventory["files"], old_version, version)
        name = "Initial-D-Update-from-" + old_version + "-to-" + version + "-Patch.zip"
        path, row = asset(plan_path.parent, item, name)
        expected[path.name] = row
    checksum = gate.plain_file(plan_path.parent / supplied["checksums"]["path"])
    require(checksum.stat().st_size < 1024**2, "Checksum file is too large")
    rows = []
    for line in checksum.read_text(encoding="utf-8-sig").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9._-]+)", line)
        require(match is not None, "Malformed checksum entry")
        rows.append((match[2], match[1]))
    require(len(rows) == len(expected) and dict(rows) == {name: row["sha256"] for name, row in expected.items()}, "Checksum file omits or changes release assets")
    checksum, row = asset(plan_path.parent, supplied["checksums"], "SHA256SUMS.txt")
    expected[checksum.name] = row
    notes = gate.reference(plan_path.parent, plan.get("notes"))
    require(notes.stat().st_size < 128 * 1024, "Release notes are too large")
    body = notes.read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    require(body.strip() and isinstance(plan.get("title"), str) and plan["title"].strip(), "Reviewed release notes/title are required")
    require(verify.sha256(plan_path) == plan_hash, "Publication plan changed during verification")
    return dict(plan=plan, planPath=plan_path, planSha256=plan_hash, acceptancePath=accepted_path,
                receipt=receipt, assets=expected, body=body, baselineId=latest["id"])


def verify_tag(github, tag, commit, required):
    refs = github.request("git/matching-refs/tags/" + tag)
    exact = [row for row in refs if row.get("ref") == "refs/tags/" + tag]
    require(len(exact) <= 1 and (exact or not required), "Release tag is missing or ambiguous")
    if not exact:
        return
    obj = exact[0]["object"]
    for _ in range(5):
        if obj.get("type") != "tag":
            break
        require(re.fullmatch(r"[0-9a-f]{40}", obj.get("sha", "")), "Invalid annotated tag object")
        obj = github.request("git/tags/" + obj["sha"])["object"]
    require(obj.get("type") == "commit" and obj.get("sha") == commit, "Release tag points to a different commit")


def verify_remote(github, context, published=False):
    plan, receipt = context["plan"], context["receipt"]
    release = github.request("releases/" + str(plan["releaseId"]))
    require(release.get("id") == plan["releaseId"] and release.get("draft") is (not published)
            and release.get("prerelease") is False, "Release is not in the expected draft/public state")
    require(release.get("tag_name") == plan["tag"] and release.get("target_commitish") == receipt["sourceCommit"], "GitHub release targets a different build")
    require(release.get("name") == plan["title"] and (release.get("body") or "").replace("\r\n", "\n") == context["body"], "GitHub notes/title differ from the reviewed plan")
    actual = remote_assets(release)
    require(set(actual) == set(context["assets"]), "GitHub draft has missing or extra assets")
    for name, expected in context["assets"].items():
        require_remote_asset(actual[name], expected, name)
    verify_tag(github, plan["tag"], receipt["sourceCommit"], published)
    remote_commit = github.request("git/commits/" + receipt["sourceCommit"])
    tree = verify.git("rev-parse", receipt["sourceCommit"] + "^{tree}").decode().strip()
    require(remote_commit.get("tree", {}).get("sha") == tree, "GitHub commit tree differs from the accepted source")
    return release


def execute(plan_path, player, github, mode, report, save):
    require(mode in ("check", "publish", "verify-published"), "Unsupported publication mode")
    report.update(phase="validating", publicationRequested=False, published=False)
    save()
    context = validate_local(plan_path, player, github, published=mode == "verify-published")
    report.update(planSha256=context["planSha256"], sourceCommit=context["receipt"]["sourceCommit"],
                  candidateInventorySha256=context["receipt"]["candidateInventorySha256"], assets=context["assets"],
                  releaseId=context["plan"]["releaseId"], phase="local-accepted")
    save()
    release = verify_remote(github, context, mode == "verify-published")
    if mode == "publish":
        # Asset reconstruction may take time. Recheck the full acceptance chain
        # and then the live draft/baseline immediately before the only write.
        require(gate.check(context["acceptancePath"], player) == context["receipt"], "Acceptance changed before publication")
        require(verify.sha256(context["planPath"]) == context["planSha256"], "Publication plan changed")
        require(github.request("releases/latest").get("id") == context["baselineId"], "Stable baseline changed before publication")
        verify_remote(github, context)
        report.update(phase="publishing", publicationRequested=True)
        save()
        github.request("releases/" + str(context["plan"]["releaseId"]), {"draft": False, "make_latest": "true"})
        release = verify_remote(github, context, published=True)
    if mode != "check":
        latest = github.request("releases/latest")
        require(latest.get("id") == context["plan"]["releaseId"], "Published release is not the current stable release")
    report.update(passed=True, phase="verified", published=mode != "check", url=release.get("html_url"),
                  checkedAt=datetime.now(timezone.utc).isoformat())
    save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--player", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="NEW private local report file")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--publish", action="store_true")
    modes.add_argument("--verify-published", action="store_true")
    args = parser.parse_args()
    require(not sys.flags.optimize, "Run with Python assertions enabled")
    output = gate.build.require_plain_path(args.output)
    require(not output.exists() and not output.is_relative_to(args.player.resolve()), "Use a new report outside the candidate")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Reserve the path before making any remote calls.
    with output.open("x", encoding="utf-8") as stream:
        stream.write("{}\n")
    report = dict(schema="idas3-publication-result-v1", passed=False)
    def save():
        verify.write_json(output, report)
    mode = "publish" if args.publish else "verify-published" if args.verify_published else "check"
    try:
        execute(args.plan, args.player, GitHub(), mode, report, save)
    except Exception as error:
        report.update(passed=False, error=str(error))
        save()
        print("Publication not confirmed. " + str(error), file=sys.stderr)
        if report.get("publicationRequested"):
            print("A publish request was sent. Use --verify-published to inspect it; do not retry blindly.", file=sys.stderr)
    print("Local result: " + str(output))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
