# Changed-file release patches

Every release must retain its `Initial-D-Arcade-Stage-3-Reimagined-...-Windows-x64.zip` core asset, the matching `Initial-D-Additional-Courses-...zip`, the setup downloader and SHA-256 checksums for new installations and Full Repair. The in-game updater stages both full archives together. Each public asset must stay below GitHub's 2 GiB limit. The local combined ZIP used for patch comparisons is not a public download.

Starting with `.46`, Unity resources use `InitialDUnity_Data/data.unity3d` and staged native textures/meshes use lossless block compression. Keep the eight legacy loose Unity file paths as zero-byte entries in both full and patch inventories. They replace the old large files during an update without requiring unsupported deletion instructions. `Build-UpdatePatch.py` validates the UnityFS signature and those zero-byte replacements. Do not remove these entries merely because a fresh install does not read them. `Idas3CompressedPlayerLayout` produces them for full and scripts-only builds; `Idas3StorageUpdateChecks` checks native full/patch installation and personal-file preservation.

Build full ZIPs with `Tools/Build-ReleasePackage.py --player Builds/Current --archive <release-Windows-x64.zip>`, or use its `collect_player_files()` in the release verification script. This collector prunes `Custom Music` and legacy `custom-music` folders before inspecting or descending into them. It excludes all their files and directory entries, including generated instructions. Build-specific smoke checks and source/binary checks still need to pass before publishing.

Players can put MP3, OGG and WAV files in `Custom Music` beside the EXE. Local builds stage `Custom Music/README.txt`; downloaded builds create the folder and instructions on first launch. Release ZIPs and patch manifests must omit the entire folder so existing installers accept the update and never inspect or replace personal tracks. The updater's existing game-directory allowlist already excludes this root; no native helper change is needed. A README-only folder in a full ZIP would still break older installers' Full Repair/fallback. The patch inventory rejects both music folder names before reading file contents.

Built players contain `rom/README.txt` beside the EXE. Release ZIPs must omit **all** `rom/` entries, including that README and empty directory entries; the game recreates the folder and instructions on first launch. Older managed and native installers reject the `rom` root, so including it would prevent the first update into the ROM-required build. No manual transition is needed when update ZIPs omit these entries.

Only `rom/README.txt` may be excluded as a build-generated instruction file. If release input contains any other file under `rom`, stop packaging and use a clean staging folder. Do not open, hash, copy or upload those files. The patch inventory skips that exact README path and rejects other ROM paths before reading their contents; this does not remove entries from an existing full ZIP. The full-ZIP packager must perform the exclusion itself. Original ROMs must never appear in a patch manifest, including as retained files.

Build a patch from each supported previous full release ZIP to the new full ZIP:

```powershell
python -X utf8 Tools/Build-UpdatePatch.py --base C:/releases/old-Windows-x64.zip --target C:/releases/new-Windows-x64.zip --base-version 0.3.95-community-replays.19 --target-version 0.3.95-community-replays.20 --out C:/releases/new
```

Upload the generated `Initial-D-Update-from-...-to-...-Patch.zip` alongside the full ZIP. Include its hash in SHA256SUMS.txt. The `.report.json` is local verification evidence, not a game asset. The tool reconstructs and hashes every target file using the old ZIP plus patch before reporting success. It rejects personal data, unsafe/duplicate paths and file removals (which currently need a full package). Patches replace whole changed files, not binary blocks within a file.

The updater accepts only a smaller, fully uploaded patch whose name matches both versions and whose URL and SHA-256 digest match the GitHub release. The patch includes the complete target file inventory. The installer verifies retained files locally, verifies new payloads, stages backups, waits for the exact game process to exit, rechecks retained files, and replaces only changed files. Missing/damaged retained files or invalid patch contents cause a full-package retry before shutdown. Network failures and unrelated installer failures stop without a large retry. Full Repair explicitly selects the full package even on the current version; it cannot downgrade a newer installation.

The installer does not remove unlisted files or include personal data. Existing saves, options, recordings, custom music and the user's `rom` folder are preserved. Neither changed-file updates nor Full Repair checks, replaces or repairs an original ROM. Startup performs the separate original-dump validation. Publish patches for all supported base versions; a version without a matching patch uses the full ZIP. Pre-.19 clients need one full update because their updater cannot read patches.

The production updater stages ZIPs inside Unity and uses the bundled static native helper from `Tools/Updater`. Build the helper with `Tools/Build-UpdateInstaller.cmd` before building Unity. It needs neither PowerShell nor an external .NET installation. The running installation and OS temporary base directories are resolved to their actual Windows paths before deriving update paths, allowing installations reached through Steam-library or home-directory links. Wine drive mappings are allowed. Links inside the installation, `InitialDUpdates` cache root, sessions and archive contents are still rejected; archive entries are never resolved through links. Windows DOS 8.3 paths are expanded before final-path comparison, so short names in TEMP or installation folders are not mistaken for links.

Validation:

```powershell
python -X utf8 Tests/Updates/test_native_installer.py
python -X utf8 Tests/Updates/test_native_paths.py
python -X utf8 Tests/Updates/test_patch.py
python -X utf8 Tests/Updates/test_rom_package.py
python -X utf8 Tests/Updates/test_custom_music_package.py
wsl -d Ubuntu -- xvfb-run -a python3 /path/to/repo/Tests/Updates/test_wine_installer.py
python -X utf8 Tests/Updates/test_cache.py
```

Unity: `Idas3IncrementalUpdateBuild.Build` runs release-selection checks and builds the isolated player. Run it with `-idas3-attract-options-smoke <fresh-output> -idas3-updates-check` to test modal/controller access and the patch/full retry state machine. The ordinary game folders are never test fixtures.

For the complete Unity-to-helper Wine flow, build `Idas3MenuFontBuild.BuildDiagnostic`, then run `Tests/Updates/test_wine_unity_flow.py` under `xvfb-run` with `IDAS3_FLOW_EXPECT_FIXED=1` and `IDAS3_FLOW_TEST_OUTPUT` pointing to a fresh evidence folder. This uses its own Wine prefix, loopback download server and disposable installation. It exercises the real Unity download, staging, helper launch, shutdown and restart, including linked installation/TEMP paths, changed-file patches, damaged-file fallback and rejected links/checksums. This does not substitute for testing an affected player's Steam-managed Proton environment.

ROM preservation checks should seed an isolated installation with a dummy `rom/gds-0033.chd` and the CUE/BIN filenames, apply both a full package and a patch, and verify their bytes and timestamps stay unchanged. Also verify that release inventory rejects ROM payloads before reading them, that the README is absent from patch manifests, and that neither full ZIPs nor patch ZIPs contain a `rom/` directory entry.
