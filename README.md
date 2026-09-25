# Initial D Arcade Stage 3 Reimagined

A fan-made Initial D Arcade Stage 3 project for Windows, with Unity rendering and a native C++ gameplay core.
JOIN DISCORD: https://discord.gg/mJCZ5AvBb

**[Download the game](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/releases/latest)** · **[Community leaderboard](https://initial-d-leaderboard.initial-d-community-leaderboard.workers.dev)** · **[Report a bug](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/issues/new/choose)** · **[Changelog](CHANGELOG.md)**

![Time Attack in night and wet conditions](docs/screenshots/akagi-night-wet.png)

## Play

1. Download the Windows game ZIP from **Releases**.
2. Extract the entire archive into a folder.
3. Put your original **Initial D Arcade Stage 3 GDS-0033** dump in a `rom` folder beside `InitialDUnity.exe`.
4. Run `InitialDUnity.exe`. Keep its data folders and DLLs beside it.

Use `rom/gds-0033.chd`, or the complete set `rom/gds-0033.cue`, `rom/gds-0033-track1.bin`, `rom/gds-0033-track2.bin` and `rom/gds-0033-track3.bin`. The game validates the supported dump before gameplay; a filename alone is insufficient. It creates `rom/README.txt` on first launch when needed. Updates and Full Repair preserve your ROM files and do not replace a missing or invalid dump.

The download includes the runtime assets but no original ROM. You do not need Unity to play. Source-code ZIPs are the development project; use the Windows game ZIP for the playable build.

**Current release:** `0.3.95-community-replays.33` · **Platform:** Windows x64 / Direct3D 11

**Latest update:** `0.3.95-community-replays.33` corrects Miku meter alignment, restores missing tachometer effects and Halloween lantern animation, and fixes washed-out Steampunk glow colors. Audio-reactive meters follow game audio and custom music; needle trails and replay animation timing are corrected. A verified original GDS-0033 dump is still required. Only this version can submit new community Time Attack runs; existing published times and replay downloads remain available. Both online players need this version.

If an older updater reports **"An update path resolves through a link"**, close the game and extract the full Windows game ZIP over the existing installation, replacing game files. Personal saves, settings, replays and custom music are preserved. Normal updates use a smaller changed-file patch when one is available for the installed version. Installer speed improvements take effect once `.27` or newer is installed; the first update from an older version still uses your previous updater.

Optional [Discord Rich Presence](docs/discord/README.md) shows **Initial D Arcade Stage 3**, the game logo, and your current race, menu, or replay activity. Toggle it under **Options > Gameplay > Discord Rich Presence**.

## Features

- GitHub update checks with small changed-file patches, full repair, installation and restart.
- Time Attack, Legend of the Streets, Bunta Challenge and online battles.
- Original course and car selections, plus Hakone, Sadamine, Enna Skyline, Momiji Line and the longer Special Stage layouts of Myogi and Usui.
- Special Stage courses support Time Attack rankings, replays and online battles, with downhill/uphill and dry/wet options at night.
- Day, night and weather conditions supported by each course.
- Controller, keyboard and supported wheel input.
- Personal saves, progression, tuning and a Full Tune option.
- Community Time Attack rankings with downloadable driving replays.
- Local replay library, camera controls, online opponent POV and replay engine audio.
- Custom race music: put MP3, OGG and WAV files in `Custom Music` beside the game, or import them in Select BGM. Select BGM also lets you delete custom tracks.
- Graphics presets, lower output resolutions and adjustable effects/scenery detail.

This project is a work in progress. Rendering fidelity, performance and multiplayer behavior continue to receive fixes; hardware and course combinations vary.

## Records and replays

While connected, in-game rankings combine the selected save's personal bests with the current community leaderboard. Offline, only that save's personal records appear. Matching published personal times appear once.

New leaderboard submissions require a complete replay and a supported build. Old personal times are preserved locally and are not uploaded retrospectively. Replay telemetry is useful for reviewing a run; it is not authoritative anti-cheat verification.

Personal Online Battle and Legend recordings stay on the player's PC. Only a replay attached to a submitted Time Attack is uploaded. Options > Replays opens the local library and recording controls.

## Build and contribute

See **[BUILDING.md](BUILDING.md)** for the development setup and **[CONTRIBUTING.md](CONTRIBUTING.md)** for bug reports and changes.

The repository includes the runtime assets in `Native/data` and `RuntimeAssets`, along with the Unity project, native source, tests and leaderboard service source. The checkout is several gigabytes. Personal saves, uploaded recordings, credentials and local editor/build caches are excluded.

| Directory | Contents |
|---|---|
| `Assets` | Unity scripts, shaders, resources, scenes and native plugins |
| `Native/src` | Native gameplay, asset loading, menus, audio and renderer bridge |
| `Native/data` | Original runtime data used by the game |
| `RuntimeAssets` | Six additional course packs, including four Special Stage layouts |
| `Native/tests` | Native regression tests and isolated gameplay fixtures |
| `Tools` | Local build, staging and asset utilities |
| `Leaderboard` | Community leaderboard service, migrations and tests |

This is an unofficial fan project and is not affiliated with or endorsed by SEGA or the Initial D rights holders. Existing third-party notices and licenses remain applicable; no blanket license is granted over third-party game content.
