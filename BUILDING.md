# Building the game

## Toolchain

- Windows x64.
- Unity **6000.6.0f1**, with Windows Mono build support.
- Visual Studio C++ build tools, including the Windows SDK.
- CMake **3.24 or newer**, available on PATH.
- Git, if cloning rather than downloading the source archive.

```powershell
git clone https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined.git
cd Initial-D-Arcade-Stage-3-Reimagined
```

The full runtime data is included. Allow disk space for the checkout, Git objects, Unity's Library cache and the built player. Steamworks.NET is included as a local Unity package with its existing license.

## Native plugin

Run `Build Native.cmd`. It locates the installed MSVC tools through `vswhere`, configures a Release build and writes `Idas3Unity.dll` into `Assets/Plugins/x86_64`.

The force-feedback plugin is also supplied. To rebuild it after configuring the native project, run the following from an x64 Visual Studio developer shell:

```powershell
cmake --build Native/build-unity-d --target Idas3WheelFeedback
```

## Unity player

Run `Build Unity.cmd` to build the native plugin and Windows player. The output is `Builds/Current/InitialDUnity.exe`. The script stages all native runtime data and both imported courses beside the player.

`Open in Unity.cmd` opens the project for editing. The main scene is `Assets/Scenes/InitialDUnityScene.unity`.

If Unity is installed outside the usual Hub location, set `IDAS3_UNITY_EDITOR` to the full path of `Editor/Unity.exe` before running the build.

The public checkout uses `RuntimeAssets/HAKONE` and `RuntimeAssets/SADAMINE`. Private development/staging folders from the maintainer's computer are not required for the normal player build. Some historical diagnostic and asset-extraction tools still require their own input files; they are not part of the normal player build.

## Tests

Native CMake registers tests through CTest. Many asset tests expect this repository's runtime data; independent reference tests also require explicitly configured reference inputs. Build a test target before selecting it with CTest.

The leaderboard tests use Node.js with `node:sqlite` support:

```powershell
cd Leaderboard
node --test test/worker.test.mjs
```

These use an in-memory database and do not submit scores or change the live service.

## Leaderboard hosting

The distributed game points at the community service. Building the game does not require deploying a backend.

For a separate service, see `Leaderboard/README.md`. The checked-in Wrangler configuration uses a placeholder database ID. No production database credential, admin key, installation identity or private deployment settings are included.
