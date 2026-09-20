# Changelog

## 0.3.95-community-replays.7 — September 20, 2026

- Add Discord Rich Presence with the Initial D Arcade Stage 3 title and supplied logo.
- Show menus, race mode, track direction, conditions, opponents, results, and replay activity.
- Add a View Leaderboard button and an on/off setting under Gameplay.
- Keep Discord communication off the race frame loop; no account linking required.

## 0.3.95-community-replays.6 — September 20, 2026

- Check GitHub before entering the game; show a Yes/No prompt for newer Windows releases.
- Download and verify accepted updates, install after the game closes, then restart automatically.
- Keep saves, settings, custom music and replays; restore replaced files if installation fails.
- Continue into the game when up to date, declined, or offline.
- Add a manual update check under Gameplay on the title screen.

## 0.3.95-community-replays.5 — September 20, 2026

- Combine personal Time Attack bests with current online leaderboard times while connected.
- Keep personal times visible when the online leaderboard is empty.
- Avoid duplicate rows when a personal best is already published.
- Retain personal-only records offline and continue excluding obsolete shared records.

## September 19–20, 2026 updates

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
