# Changed-file release patches

Every release must retain its complete `Initial-D-Arcade-Stage-3-Reimagined-...-Windows-x64.zip` asset and SHA-256 checksum for old clients, new installations and Full Repair.

Build a patch from each supported previous full release ZIP to the new full ZIP:

```powershell
python -X utf8 Tools/Build-UpdatePatch.py --base C:/releases/old-Windows-x64.zip --target C:/releases/new-Windows-x64.zip --base-version 0.3.95-community-replays.19 --target-version 0.3.95-community-replays.20 --out C:/releases/new
```

Upload the generated `Initial-D-Update-from-...-to-...-Patch.zip` alongside the full ZIP. Include its hash in SHA256SUMS.txt. The `.report.json` is local verification evidence, not a game asset. The tool reconstructs and hashes every target file using the old ZIP plus patch before reporting success. It rejects personal data, unsafe/duplicate paths and file removals (which currently need a full package). Patches replace whole changed files, not binary blocks within a file.

The updater accepts only a smaller, fully uploaded patch whose name matches both versions and whose URL and SHA-256 digest match the GitHub release. The patch includes the complete target file inventory. The installer verifies retained files locally, verifies new payloads, stages backups, waits for the exact game process to exit, rechecks retained files, and replaces only changed files. Missing/damaged retained files or invalid patch contents cause a full-package retry before shutdown. Network failures and unrelated installer failures stop without a large retry. Full Repair explicitly selects the full package even on the current version; it cannot downgrade a newer installation.

The installer does not remove unlisted files or include personal data. Existing saves, options, recordings and custom music are preserved. Publish patches for all supported base versions; a version without a matching patch uses the full ZIP. Pre-.19 clients need one full update because their updater cannot read patches.

The production updater stages ZIPs inside Unity and uses the bundled static native helper from `Tools/Updater`. Build the helper with `Tools/Build-UpdateInstaller.cmd` before building Unity. It needs neither PowerShell nor an external .NET installation. Wine drive mappings are allowed; links below drive roots are rejected.

Validation:

```powershell
python -X utf8 Tests/Updates/test_native_installer.py
python -X utf8 Tests/Updates/test_patch.py
wsl -d Ubuntu -- xvfb-run -a python3 /path/to/repo/Tests/Updates/test_wine_installer.py
```

Unity: `Idas3IncrementalUpdateBuild.Build` runs release-selection checks and builds the isolated player. Run it with `-idas3-attract-options-smoke <fresh-output> -idas3-updates-check` to test modal/controller access and the patch/full retry state machine. The ordinary game folders are never test fixtures.
