# Changelog

## 0.3.95-community-replays.11 - September 20, 2026

- Added a Special Stage tab immediately after Stage 2 with 15 additional PS2 race songs; excluded the 16 songs already represented in the catalog.
- Preserved existing song selections, original song files, and PS2 loop points.
- Added ADX v3 playback; all new songs and repeated loops matched an independent decoder. Controller navigation, selection, and countdown playback passed in Unity.



## 0.3.95-community-replays.10 — September 20, 2026

- Restored the arcade race-intro HUD entrance: TIME slides from the left and RECORD / DIFFERENCE / DRIVER slide from the right, with backing strips entering before their labels.
- Restored the Legend of the Streets rival portrait entrance and the delayed player names. Online battle panels use the same entrance timing.
- Uses the original game's slide filters on the race's 60 Hz clock, preserving numeric clocks and stable animation during pause or repeated renders.
- Verified filter output against original instructions, rendered all three HUD modes at three aspect ratios, and checked the Unity pre-race sequence.

## 0.3.95-community-replays.9 — September 20, 2026

- Improve Windows frame-cap pacing by waiting immediately before presentation with a high-resolution timer.
- Keep existing FPS choices, VSync behavior, and uncapped rendering.
- Reset pacing after focus changes and long stalls; avoid catch-up bursts and duplicate software limiters.

## 0.3.95-community-replays.8 — September 20, 2026

- Restore snowfall and tire snow powder on Akina Snow using original effect textures.
- Keep rain streaks and water trails separate from snow effects.
- Apply Full/Reduced weather detail to snow as well as rain; rename the Graphics setting to Weather & Spray.

## 0.3.95-community-replays.7 â€” September 20, 2026

- Add Discord Rich Presence with the Initial D Arcade Stage 3 title and supplied logo.
- Show menus, race mode, track direction, conditions, opponents, results, and replay activity.
- Add a View Leaderboard button and an on/off setting under Gameplay.
- Keep Discord communication off the race frame loop; no account linking required.

## 0.3.95-community-replays.6 â€” September 20, 2026

- Check GitHub before entering the game; show a Yes/No prompt for newer Windows releases.
- Download and verify accepted updates, install after the game closes, then restart automatically.
- Keep saves, settings, custom music and replays; restore replaced files if installation fails.
- Continue into the game when up to date, declined, or offline.
- Add a manual update check under Gameplay on the title screen.

## 0.3.95-community-replays.5 â€” September 20, 2026

- Combine personal Time Attack bests with current online leaderboard times while connected.
- Keep personal times visible when the online leaderboard is empty.
- Avoid duplicate rows when a personal best is already published.
- Retain personal-only records offline and continue excluding obsolete shared records.

## September 19â€“20, 2026 updates

- Added the replay library to Options, optional Online/Legend recording and required recording for shared Time Attacks.
- Added camera controls, online opponent POV, recorded speed/RPM/movement and reconstructed engine audio.
- Fixed missing replay scenery, including Usui, and Full Tune availability after closing replays.
- Added descriptive replay filenames and public leaderboard replay downloads.
- Reset the community board; require new replay-backed runs from supported builds. Historical uploads are disabled.
- Added MP3, OGG and WAV race music imports.
- Consolidated performance controls into Graphics, retaining mirrors and gameplay features.
- Reduced original-course scenery CPU work and Akagi scenery-update stalls.
- Removed the Time Attack progress percentage and placeholder BEST label.
- Preallocated recording buffers and moved replay compression/save processing off the game thread.

The reported larger sustained FPS drop during recording remains under investigation. Performance improvements vary by machine and track.
