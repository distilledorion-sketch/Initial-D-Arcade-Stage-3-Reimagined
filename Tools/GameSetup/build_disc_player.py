"""Build the Unity player in a project containing only explicitly selected resources."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

def prepare(work, plugin):
    def put(source, relative):
        dest = work / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
    if work.exists() and not (work / ".disc-player-project").is_file():
        raise ValueError("Use a fresh build workspace.")
    work.mkdir(parents=True, exist_ok=True)
    (work / ".disc-player-project").touch()
    for name in ("Packages", "ProjectSettings", "ThirdParty/EOS/com.initiald.eos-sdk"):
        shutil.copytree(ROOT / name, work / name, dirs_exist_ok=True)
    # The startup scene contains the camera and the Reimagined scene driver.
    for name in ("Assets/Scenes/InitialDUnityScene.unity", "Assets/Scripts", "Assets/Plugins"):
        source = ROOT / name
        if source.is_file():
            put(source, name)
        else:
            for file in source.rglob("*"):
                if file.is_file() and file.suffix in (".cs", ".meta", ".dll", ".txt", ".asmdef"):
                    put(file, file.relative_to(ROOT))
        meta = source.with_suffix(source.suffix + ".meta")
        if meta.exists():
            put(meta, meta.relative_to(ROOT))
    for relative in ("Assets/Branding.meta", "Assets/Branding/Reimagined.png", "Assets/Branding/Reimagined.png.meta"):
        put(ROOT / relative, relative)
    scene = ROOT / "Assets/Scenes/InitialDUnityScene.unity.meta"
    put(scene, scene.relative_to(ROOT))
    resources = ROOT / "Assets/Resources"
    selected_resources = set()
    for file in resources.rglob("*"):
        if not file.is_file():
            continue
        authored = file.suffix in (".shader", ".cginc") or (
            file.parent.name == "Fonts" and file.suffix in (".otf", ".txt")
        ) or file.name in ("CustomMenus.txt", "RankedMaps.txt", "Idas3EosOnline.json", "UpdateInstaller.exe.bytes")
        if authored:
            selected_resources.add(file.relative_to(resources))
            put(file, file.relative_to(ROOT))
            meta = Path(str(file) + ".meta")
            put(meta, meta.relative_to(ROOT))
    staged = work / "Assets/Resources"
    unexpected = [p for p in staged.rglob("*") if p.is_file()
                  and p.suffix != ".meta" and p.relative_to(staged) not in selected_resources]
    if unexpected or (work / "Assets/StreamingAssets").exists():
        raise ValueError("Unexpected assets in the build workspace. Select a fresh workspace.")
    for name in ("Idas3Unity.dll", "Idas3WheelFeedback.dll"):
        put(plugin / name, Path("Assets/Plugins/x86_64") / name)
    put(ROOT / "Assets/Editor/Idas3CompressedPlayerLayout.cs", "Assets/Editor/Idas3CompressedPlayerLayout.cs")
    put(HERE / "DiscPlayerBuild.cs", "Assets/Editor/DiscPlayerBuild.cs")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--plugin", required=True, type=Path)
    parser.add_argument("--unity", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError("Build the distributable player in an empty directory.")
    prepare(args.work.resolve(), args.plugin.resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, IDAS3_DISC_PLAYER=str(args.output.resolve() / "InitialDUnity.exe"),
               DOTNET_PROCESSOR_COUNT="2", DOTNET_gcServer="0", DOTNET_GCHeapCount="1",
               COMPlus_gcServer="0", NUMBER_OF_PROCESSORS="2")
    subprocess.run([str(args.unity), "-batchmode", "-quit", "-nographics", "-job-worker-count", "2",
                    "-projectPath", str(args.work.resolve()),
                    "-buildTarget", "Win64", "-executeMethod", "DiscPlayerBuild.Build",
                    "-logFile", str(args.work.resolve() / "disc-build.log")], env=env, check=True,
                   creationflags=subprocess.CREATE_NO_WINDOW)
    print(f"Built the disc player in {args.output}.")

if __name__ == "__main__":
    main()

