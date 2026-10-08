# Initial D Arcade Stage 3 Reimagined

A fan-made Initial D Arcade Stage 3 project for Windows, with Unity rendering and a native C++ gameplay core.
JOIN DISCORD: https://discord.gg/mJCZ5AvBb

**[Download the game](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/releases/latest)** · **[Community leaderboard](https://initial-d-leaderboard.initial-d-community-leaderboard.workers.dev)** · **[Report a bug](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/issues/new/choose)** · **[Changelog](CHANGELOG.md)**

![Time Attack in night and wet conditions](docs/screenshots/akagi-night-wet.png)

## Play

1. Download `Initial-D-Setup-0.3.95.45.zip` from **Releases**.
2. Extract it and run `Install Initial D.cmd`. It downloads and verifies the game and additional courses into a new folder.
3. Put your original **Initial D Arcade Stage 3 GDS-0033** dump in a `rom` folder beside `InitialDUnity.exe`.
4. Run `InitialDUnity.exe`. Keep its data folders and DLLs beside it.

Use `rom/gds-0033.chd`, or the complete set `rom/gds-0033.cue`, `rom/gds-0033-track1.bin`, `rom/gds-0033-track2.bin` and `rom/gds-0033-track3.bin`. The game validates the supported dump before gameplay; a filename alone is insufficient. It creates `rom/README.txt` on first launch when needed. Updates and Full Repair preserve your ROM files and do not replace a missing or invalid dump.

The download includes the runtime assets but no original ROM. You do not need Unity to play. For a manual installation, extract both the `Windows-x64.zip` and the matching `Initial-D-Additional-Courses` ZIP into the same folder. Source-code ZIPs are the development project, not the playable build.

**Current release:** `0.3.95-community-replays.45` · **Platform:** Windows x64 / Direct3D 11

**Latest update:** `.45` changes Gunsai to Usui handling and Odawara to Tsuchisaka handling with stronger acceleration. The Gunsai and Odawara community leaderboards have been reset for the new physics; other tracks and the current season are preserved. Both online drivers must update, and this version is required for new community Time Attack submissions.

If an older updater reports **"An update path resolves through a link"**, close the game and extract both release ZIPs over the existing installation, replacing game files. Personal saves, settings, replays and custom music are preserved. Normal updates use one smaller changed-file patch that includes all required courses. Full Repair in `.44` or newer fetches both parts automatically; if an older updater falls back to its full download, the new game prompts for Full Repair to finish installing missing courses.

Optional [Discord Rich Presence](docs/discord/README.md) shows **Initial D Arcade Stage 3**, the game logo, and your current race, menu, or replay activity. Toggle it under **Options > Gameplay > Discord Rich Presence**.

## Features

- GitHub update checks with small changed-file patches, full repair, installation and restart.
- Time Attack, Legend of the Streets, Bunta Challenge and online battles.
- Original course and car selections, plus Hakone, Sadamine, Enna Skyline, Momiji Line, Tsubaki Line, Gunsai, Odawara and the longer Special Stage layouts of Myogi and Usui.
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
| `RuntimeAssets` | Nine additional course packs, including four Special Stage layouts |
| `Native/tests` | Native regression tests and isolated gameplay fixtures |
| `Tools` | Local build, staging and asset utilities |
| `Leaderboard` | Community leaderboard service, migrations and tests |

This is an unofficial fan project and is not affiliated with or endorsed by SEGA or the Initial D rights holders. Existing third-party notices and licenses remain applicable; no blanket license is granted over third-party game content.
