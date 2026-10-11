"""Stage the desktop EOS runtime from the pinned, unmodified upstream UPM archive.

This does not install the package into Unity or alter project dependencies.
"""
import hashlib
import json
import shutil
import uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "package"
OUTPUT = HERE / "com.initiald.eos-sdk"
ARCHIVE = HERE / "com.playeveryware.eos-6.2.0.tgz"
EXPECTED = "aafe5a1cc278f2f65e0373777706d0a1028eea49b9548ef4a7520cc50f647c4b"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def native_meta(path, platform, os_name):
    guid = uuid.uuid5(uuid.NAMESPACE_URL, "initiald-eos-sdk/" + path.name).hex
    path.with_name(path.name + ".meta").write_text(
        f"""fileFormatVersion: 2
guid: {guid}
PluginImporter:
  externalObjects: {{}}
  serializedVersion: 2
  iconMap: {{}}
  executionOrder: {{}}
  defineConstraints: []
  isPreloaded: 0
  isOverridable: 0
  isExplicitlyReferenced: 0
  validateReferences: 1
  platformData:
  - first:
      Any:
    second:
      enabled: 0
      settings: {{}}
  - first:
      Editor: Editor
    second:
      enabled: 0
      settings:
        CPU: x86_64
        DefaultValueInitialized: true
        OS: {os_name}
  - first:
      Standalone: {platform}
    second:
      enabled: 1
      settings:
        CPU: x86_64
  userData:
  assetBundleName:
  assetBundleVariant:
""",
        encoding="utf-8",
    )


assert sha256(ARCHIVE) == EXPECTED, "Upstream archive checksum differs"
assert SOURCE.is_dir(), "Extract the pinned archive into ThirdParty/EOS first"
if OUTPUT.exists():
    raise RuntimeError("Staging output already exists; inspect it before replacing anything")
OUTPUT.mkdir()
runtime = OUTPUT / "Runtime"
runtime.mkdir()
shutil.copytree(SOURCE / "Runtime/EOS_SDK", runtime / "EOS_SDK")
shutil.copy2(SOURCE / "LICENSE.md", OUTPUT / "LICENSE-PLUGIN.md")

binary_manifest = []
for subdir, name, platform, os_name in (
    ("Windows/x64", "EOSSDK-Win64-Shipping.dll", "Win64", "Windows"),
    ("Linux", "libEOSSDK-Linux-Shipping.so", "Linux64", "Linux"),
):
    source_dir = SOURCE / "Runtime" / subdir
    target_dir = runtime / subdir
    target_dir.mkdir(parents=True)
    destination = target_dir / name
    shutil.copy2(source_dir / name, destination)
    native_meta(destination, platform, os_name)
    for notice in ("license.txt", "ThirdPartySoftwareNotice.txt"):
        shutil.copy2(source_dir / notice, target_dir / notice)
    binary_manifest.append({
        "path": destination.relative_to(OUTPUT).as_posix(),
        "bytes": destination.stat().st_size,
        "sha256": sha256(destination),
    })

write_json(OUTPUT / "package.json", {
    "name": "com.initiald.eos-sdk",
    "version": "1.19.2",
    "displayName": "EOS SDK Desktop Runtime (Initial D Packaging)",
    "description": "Unmodified Epic EOS SDK 1.19.2.1-58105819 C# bindings and Windows/Linux x64 native runtime, extracted from official EOS Plugin for Unity v6.2.0. No samples, overlays, RTC helpers, or editor automation.",
    "unity": "2021.3",
    "license": "See Runtime/EOS_SDK/license.txt",
})

write_json(OUTPUT / "upstream-manifest.json", {
    "upstream": "https://github.com/EOS-Contrib/eos_plugin_for_unity",
    "release": "v6.2.0",
    "releaseCommit": "65e44063d5f908002e74a3080343fb0be561dacb",
    "archiveUrl": "https://github.com/EOS-Contrib/eos_plugin_for_unity/releases/download/v6.2.0/com.playeveryware.eos-6.2.0.tgz",
    "archiveSha256": EXPECTED,
    "sdkVersion": "1.19.2.1-58105819",
    "modifications": [
        "Created minimal desktop Unity package manifest.",
        "Native importer metadata enables each binary only for its matching x64 player; the game manually loads editor binaries for safe unload between play sessions.",
        "C# SDK source, native binary contents, and original notices are unchanged.",
    ],
    "binaries": binary_manifest,
})
print(json.dumps({"package": str(OUTPUT), "binaries": binary_manifest}, indent=2))
