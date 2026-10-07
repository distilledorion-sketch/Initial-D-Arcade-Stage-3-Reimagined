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

The full runtime data is included. An original Initial D Arcade Stage 3 **GDS-0033** dump is required to play and is not included or copied by the build. Allow disk space for the checkout, Git objects, Unity's Library cache and the built player. Steamworks.NET is included as a local Unity package with its existing license.

## Native plugin

Run `Build Native.cmd`. It locates the installed MSVC tools through `vswhere`, configures a Release build and writes `Idas3Unity.dll` into `Assets/Plugins/x86_64`.

The force-feedback plugin is also supplied. To rebuild it after configuring the native project, run the following from an x64 Visual Studio developer shell:

```powershell
cmake --build Native/build-unity-d --target Idas3WheelFeedback
```

### Android ARM64 native plugin (work in progress)

The portable scene ABI can be configured for Android ARM64 with the NDK CMake
toolchain. Install CMake 3.24 or newer, Ninja, and an Android NDK that contains
`build/cmake/android.toolchain.cmake`, then run:

```powershell
.\Tools\Build Native Android.ps1
```

The script builds only `Idas3Unity` with `IDAS3_PORTABLE_SCENE` and stages
`libIdas3Unity.so` under `Assets/Plugins/Android/arm64-v8a`. The Android Unity
player and asset-pack staging are separate steps; the existing Windows build
scripts do not select this target.

A ROM is never required to build. Without one, the APK asks the player to
import their own GDS-0033 (CHD, or the CUE plus three BIN files) through the
Android file picker on first launch and runs the same SHA-256 validation as
Windows. For a private local test only, a verified `gds-0033.chd` placed under
`Assets/StreamingAssets/rom/` is copied into the app-private ROM directory on
first launch instead. That folder is ignored by Git and must not be included in
public source, CI or release archives.

After installing Unity 6000.6 with Android Build Support (verified with
6000.6.4f1), run `Build Android.cmd`. The editor method selects ARM64, IL2CPP
and Vulkan and requires the Android native plugin plus the staged
`IDAS3/data.manifest.json`. To include the six imported course packs in the
same local staging tree, run `Tools\Stage Android Assets.ps1` after
`Stage-GameData.ps1`; it extends the manifest with `RuntimeAssets` entries and
preserves their hashes. `Tools\Verify Android APK.py <apk>` re-hashes every
packaged runtime file and checks 16 KB ELF alignment of the native libraries.

On Android the game shows on-screen controls (steering wheel or tilt
steering, pedals, gears, menu and story Skip buttons). Options replaces the
desktop-only settings with a TOUCH page, a Bluetooth CONTROLLER page and a
render-scale resolution that keeps the phone's aspect ratio.

## Unity player

Run `Build Unity.cmd` to build the native plugin and Windows player. The output is `Builds/Current/InitialDUnity.exe`. The script stages all native runtime data and all six imported courses beside the player.

Windows builds also create `Builds/Current/rom/README.txt` with the same instructions shown by the startup validator. Place your original dump beside those instructions as either `rom/gds-0033.chd` or the complete set `rom/gds-0033.cue`, `rom/gds-0033-track1.bin`, `rom/gds-0033-track2.bin` and `rom/gds-0033-track3.bin`. Startup validates the dump before gameplay. Building does not need a ROM, inspect existing ROMs or copy them from the checkout or another installation.

Release ZIPs must exclude every `rom/` entry, including the instructions and empty directory entries. First launch creates the folder and instructions locally. This keeps updates compatible with previous installers, whose archive allowlists reject the `rom` directory. Never include or hash players' dumps when packaging a release. See [release patch packaging](Tools/Update-Patches.md).

`Open in Unity.cmd` opens the project for editing. The main scene is `Assets/Scenes/InitialDUnityScene.unity`.

If Unity is installed outside the usual Hub location, set `IDAS3_UNITY_EDITOR` to the full path of `Editor/Unity.exe` before running the build.

The public checkout uses the six packs in `RuntimeAssets`: `HAKONE`, `SADAMINE`, `ENNA`, `MYOGI_SPECIAL`, `USUI_SPECIAL` and `MOMIJI`. Private development/staging folders from the maintainer's computer are not required for the normal player build. Some historical diagnostic and asset-extraction tools still require their own input files; they are not part of the normal player build. See [Special Stage imports](docs/special-stage-courses.md) for their provenance and handling choices.

## Tests

Native CMake registers tests through CTest. Many asset tests expect this repository's runtime data; independent reference tests also require explicitly configured reference inputs. Build a test target before selecting it with CTest.

The leaderboard tests use Node.js with `node:sqlite` support:

```powershell
cd Leaderboard
node --test test/*.test.mjs
```

These use an in-memory database and do not submit scores or change the live service.

## Leaderboard hosting

The distributed game points at the community service. Building the game does not require deploying a backend.

For a separate service, see `Leaderboard/README.md`. The checked-in Wrangler configuration uses a placeholder database ID. No production database credential, admin key, installation identity or private deployment settings are included.
