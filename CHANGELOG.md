# Changelog

## 0.3.95-community-replays.15 - September 22, 2026

- Fixed numeric driver-name glyphs on the community website and in replay filename generation. The original digit order is 1–9, then 0.
- Corrected the leaderboard car labels for the Sileighty, Lancer Evo V and Evo VI TME while retaining their existing car IDs and records.

- Added a rebindable Toggle headlights action in Settings > Controls: H on keyboard and right-stick click on standard controllers. Existing bindings migrate without replacing assigned controls; wheels can bind their own button. Verified projected lights and popup headlights off/on on original and imported courses.

- Corrected Enna Skyline's mirrored course-selection route, keeping direction labels readable and attached to their endpoints.
- Fixed mirrored Sadamine sponsor banners from either viewing side across day/night and dry/wet conditions. The correction is limited to sponsor logos and preserves the surrounding scenery.

- Corrected the horizontally mirrored Sadamine route outline on the course-selection screen while keeping the direction labels readable.
- Rain now respects the Tsuchisaka tunnel roof. Exterior rain and wet-road tire spray remain visible; verified day/night, both directions and both rain-detail settings.

- Corrected AE86 Levin Tune A turbo audio to start with the Step 3 installation, instead of Step 2. Shared by race, online and replay audio. Step 5 retains its continuous boost loop without turbo release cues; sustained rendered audio passed silence and clipping checks.

- Fixed missing road and incorrect scenery during Akagi uphill race intros by initializing scenery from the actual starting grid before the first driving tick. Camera movement is unchanged. Verified Time Attack, both online grid slots and initial scenery cells for all 18 original course/direction combinations.

- Moved wheel feedback driver calls off the game thread and stopped repeated device discovery during healthy race output. Only the latest fresh force request is sent; pause/focus loss/disconnect cancels queued output. The original force model and finite native effects are unchanged. Verified with simulated slow drivers; physical-wheel FPS verification remains pending.

## 0.3.95-community-replays.14 - September 21, 2026

- Fixed post-race "Original material paint outside palette" crashes by validating saved and packed car colors before rendering. Verified Enna finish/results/Continue with invalid paint data and all 35 cars; saved profiles and upgrades are preserved.
- Added Gameplay > AI Driver Difficulty: Normal, Hard (+5% target pace) and Expert (+10%). Saved per installation and applied at the next Legend of the Streets battle; Bunta Challenge, Time Attack and human opponents are unaffected.
- Added a HUD category in Settings with independent minimap size (100%, 125%, 150%) and zoom-out (Original, Wider/75%, Widest/50%) controls.
- Larger maps scale their road lines and markers while retaining the bottom-left anchor. Zoom-out reveals more road ahead without changing marker size; the original view is the maximum zoom.
- Added independent HUD size controls (50%, 75%, 100%, 125%, 150%) for the timer/section times, complete speedometer/gear display, Time Attack records, Legend opponent portrait panel, online opponent/driver panel, rear-view mirror, Time Extended and accepting-challengers indicator. Each group retains its anchor and scales its labels, numbers and artwork together. Replaces the whole-HUD-only control.
- Time Attack records and Legend/Online battle panels now share one saved position, including portraits, advantage and driver details.
- Added a live HUD editor with a void background, bumper/third-person preview, mouse dragging, independent resizing, mode previews, reset, save and cancel. Countdown and finish/pass/fail/new-record announcements remain fixed.
- Save HUD settings for racing and replay playback. Existing settings retain the original size and zoom until changed.

## 0.3.95-community-replays.13 - September 21, 2026

- Added Enna Skyline to the main game, including scenery, collision data, start title and replay support. Supports downhill/uphill and dry/wet at night, using Akina handling.
- Added Enna to online course selection and community Time Attack rankings, with replay-backed submissions and public replay downloads.
- Restored online car collisions and boost through the shared two-car simulation with prediction and rollback. Both default on; the host can change either before the race, with synchronized rules and readiness resets.
- Smoothed opponent visual corrections and sent fresh online input on simulation ticks. Reduced repeated Steam lobby membership queries during races.
- Fixed mouse selection being overridden by connected controllers in settings, online menus and the replay library.
- Reduced scenery copying and repeated transformed-mesh work during course-sector changes. Buffered replay CSV writing reduces finish-time save overhead while preserving recorded samples.
- Retained mirrors, weather, scenery detail and gameplay features. Performance gains vary by machine; occasional frame-time spikes remain under investigation.

Verification covered all twelve tracks in night/wet conditions (snow on Akina Snow), replay sample and rendered-scene comparisons, menu input checks, and two-player Enna races in all four direction/weather combinations with injected latency, jitter and loss. Online tests ran on one PC; they are not two-account Steam Internet tests.

Both online players must install this update. The updater preserves personal saves, settings, custom music and replays.

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
