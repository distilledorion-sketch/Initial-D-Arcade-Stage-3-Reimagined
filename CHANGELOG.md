# Changelog

## Unreleased

## 0.3.95-community-replays.33 - September 24, 2026

- Corrected the frame and needle alignment of all six Miku meters, including the Silhouette styles.
- Restored missing meter effects, including gear-change flashes, rolling digits, lightning, rotating frames, glow masks and LED sequences.
- Audio-reactive meters now follow the game audio and custom music through a 32-band spectrum.
- Fixed Future-style needle trails so they follow the moving RPM and speed needles.
- Restored Halloween lantern movement when entering and leaving a drift, and corrected night artwork for meters with a single day/night RPM face set.
- Restored authored glow colors and opacity, correcting the washed-out Steampunk lighting while keeping its electrical effects visible.
- Meter effects use consistent presentation timing across frame rates and freeze while paused. Replay effects follow playback speed and reset cleanly when seeking or switching drivers.
- Community Time Attack submissions require exactly `.33`. Existing scores, replays and the current season are preserved. Both online players need this version; changed-file patches are available from `.20` through `.32`.

## 0.3.95-community-replays.32 - September 24, 2026

- HUD resizing now uses a precise percentage slider. Mouse wheel, buttons and controller adjustments change size by 1% without wrapping at the limits; existing saved sizes are preserved.
- HUD customization now opens the layout editor directly. Custom meters and keychains can be moved and resized independently; keychain placement is saved, with separate reset and cancel controls.
- Tachometer needles and hanging ornaments now interpolate between simulation ticks for smoother motion above 60 FPS. Original and imported meters share the same display timing; driving physics and race timing remain unchanged.
- Hanging ornaments now use flexible chains with the recovered joint weights: metal links stay rigid while their joints bend, cord straps flex, and the pendant sways and twists independently. Road bumps and impacts produce vertical bounce that settles naturally; pausing freezes the entire assembly.
- Added 280 recovered 3D hanging ornaments and keychains to Settings > HUD > Customize, with a scrollable picker, animated previews and an Off option. Selected accessories swing with acceleration, braking and cornering and freeze while paused; they work with both original and custom meters.
- HUD customization now includes all 87 recovered meter styles in a scrollable picker, with live previews, per-style needles and digit layouts, and matching day/night and RPM-scale artwork. Existing Original and Stuttgart selections are preserved.
- Added Settings > HUD > Customize with animated previews, shift warnings, pedal indicators and a driver-name plate. The original HUD remains the default; meter size and position still use the HUD layout editor.
- The Stuttgart meter uses live speed, RPM, gear, transmission and pedal inputs, with matching day/night artwork and a tachometer scale selected for the current car's RPM range.
- The Stuttgart DRIFT lamp now lights green during sustained sideways motion and fades as grip returns. Wall contact, airborne movement and low-speed turns do not activate it; its preview is animated in HUD customization.
- Updates now remove completed downloads, staged files and verified rollback copies, including after a restart failure. Startup cleanup also recovers space from safe leftovers from older versions while preserving active updates and unresolved recovery backups.
- Corrected the Time Attack analysis map artwork selection for Tsuchisaka and Shomaru so the full-route trace uses the overview artwork.
- Exclude geometry from cameras that cannot display it before rendering, reducing unnecessary rear-view mirror submissions while preserving the mirror and existing visuals.
- Added Natural as an optional driving camera under Settings > Gameplay > Default Camera. It smooths following and turns, keeps a steady horizon, and gently adjusts distance and field of view with speed.
- Preserved the original Bumper and Chase cameras and the existing saved default. View Change now cycles through Bumper, Chase and Natural.
- Community Time Attack submissions require exactly `.32`. Existing published times, replays and the current season are preserved. Both online players need this version; changed-file patches are available from `.20` through `.31`.

## 0.3.95-community-replays.31 - September 23, 2026

- Added a Custom Music folder beside the game for MP3, OGG and WAV tracks, with deletion available in Select BGM. Updates preserve personal tracks and release packages exclude them.
- Continue and Change Car now offer Automatic or Manual before mode selection, starting on that car's saved transmission. Confirming stores the choice for that car while preserving its name, parts, tuning and progress.
- Back cancels an unconfirmed transmission choice; new cars are saved only after confirmation. Back from mode selection lets players revisit Automatic/Manual.
- Deleting a custom song asks for confirmation with No selected by default. Original files imported from outside the Custom Music folder stay untouched.
- Community Time Attack submissions require exactly `.31`. Existing published times, replays and the current season are preserved. Both online players need this version; changed-file patches are available from `.20` through `.30`.

## 0.3.95-community-replays.30 - September 23, 2026

- Fixed Full Tune skipping the tuning-package choice for stock cars added through Change Car. Each stock car can now choose its own package before upgrades are applied.
- Existing partial and completed tunes keep their package, parts and progress. Saved names, transmission, paint and points are preserved while choosing a package.
- Back returns to car selection without applying an unconfirmed package. Restarting selection cannot skip a new driver's setup.
- Community Time Attack submissions require exactly `.30`; complete a new run after updating. Existing published times, replay downloads and the current season are preserved. Both online players need this version.

## 0.3.95-community-replays.29 - September 23, 2026

- Require the original GDS-0033 disc before starting the game, including replay viewing and the legacy host. Startup verifies the complete supported CHD or CUE/BIN contents; renamed, missing, incomplete or damaged files do not unlock the game.
- Added a `rom` folder beside the executable with setup instructions and a blocking ROM screen offering Check Again, Open ROM Folder and Quit. Updates and Full Repair preserve locally supplied ROM files, which are excluded from release payloads.
- Community Time Attack submissions now require exactly `0.3.95-community-replays.29`. Older queued runs cannot be submitted after updating; complete a new Time Attack on this build. Previously published times and replay downloads remain available.
- Both online players need this version. Changed-file patches are provided from `.20` through `.28`; saves, settings, replays, custom music and locally supplied ROMs are preserved.

## 0.3.95-community-replays.28 - September 23, 2026

- Redesigned save selection with compact pause-menu styling, a car preview, and Continue, Change Car and Delete Save actions.
- Change Car goes directly through make and car selection to mode selection, retaining the driver's name. All 35 cars are available, with each car's parts, tuning and earned tuning progress kept independently within each save. Previously unused cars begin stock; the existing Full Tune option remains available in the pause menu.
- Save slots show the last-used car. Level replaces Wins and uses that car model's existing online aura progression; unreadable history displays an unknown level rather than resetting records.
- Added an "Are you sure?" delete confirmation that starts on No. Only explicitly choosing Yes deletes the selected save.
- Corrected saved-driver name display, including numeric and Japanese glyphs.
- Fixed undescribed controls inherited by Unity's generic HID joystick layouts interfering with wheel and pedal rebinding. Escape now works before capture arms, and cancellation or timeout cannot leave the menu disabled by a held controller input. Held shortcuts do not activate another screen when capture ends.
- Rain tire trails now follow rear-tire road contacts, surface slope and travel direction, and remain fixed on the road while fading. Snow powder and tunnel rain sheltering retain their behavior.
- Both online players need this version. Changed-file patches are provided from `.20` through `.27`; existing saves, settings, replays, custom music and leaderboard times are preserved unless a save is explicitly deleted.

## 0.3.95-community-replays.27 - September 22, 2026

- Fixed online opponents' headlights staying on at night, or off during the day, regardless of the driver's toggle. Both drivers' lights and pop-up lamp animations now follow their independent selections.
- Headlight updates repeat the current state and ignore older packets, so a dropped or reordered update cannot leave the opponent displaying an old setting. Driving physics and race verification are unchanged.
- Reduced redundant integrity scans and repeated folder-resolution work when applying updates. Final file verification, link checks, rollback and damaged-file detection remain in place.
- Update preparation now shows a checked-file count so verification progress remains visible.
- Both online players need this version. Changed-file patches are provided from `.20` through `.26`; personal saves, settings, replays and custom music are preserved.
- Installer improvements apply after `.27` is installed. Installing this update still uses the updater bundled with the previous build.

## 0.3.95-community-replays.26 - September 22, 2026

- Fixed Steam hosting, room searches, joining and Quick Match rejecting builds with the new Special Stage maps because their combined build identifier exceeded the lobby key limit. Lobby keys now hash the complete identity; peer handshakes still verify code, physics and every course fingerprint. Incomplete identities are not cached, and test lobbies remain isolated.
- Fixed the Windows updater incorrectly rejecting ordinary short folder names (such as `NAME~1`) as links. Installation and temporary folders are checked using their expanded names, while actual junctions and symbolic links remain blocked.
- Both online players need this version. Smaller changed-file updates are available from versions `.20` through `.25`; existing saves, settings, replays and custom music are preserved.
- If an older updater still reports "An update path resolves through a link", close the game and extract the full Windows ZIP over the existing installation, replacing game files once to receive the fix.

## 0.3.95-community-replays.25 - September 22, 2026

- Added Momiji Line and the longer Myogi and Usui layouts from Special Stage, using their extracted roads, collision, scenery, trees, spectators and checkpoints.
- Matched their handling to similar Arcade Stage 3 courses: Momiji Line uses Akagi, Myogi (Special Stage) uses Shomaru, and Usui (Special Stage) uses Happogahara. Direction and dry/wet handling variants are preserved.
- Added downhill/uphill and dry/wet Time Attack and online races. These Special Stage packs use their available night scenery.
- Added separate personal records, community leaderboard boards, replay recordings and playback for all three layouts. Existing records and replay downloads are retained.
- Added course-selection artwork and race titles. Section timers scale up for longer sections relative to the handling donor.
- Fixed a forest backdrop appearing across the road on Usui (Special Stage) by restoring the course's original scenery visibility ranges in both directions.
- Corrected the course-selection orientation of Enna Skyline, Myogi (Special Stage), Usui (Special Stage) and Momiji Line to match the original Special Stage maps, with direction labels at the correct endpoints.
- Fixed the pause and race-options headers calling Enna Skyline and the new Special Stage maps Hakone.
- Fixed Discord activity and replay descriptions for the new Special Stage courses.
- Preserved the existing Myogi, Usui, Enna Skyline, Legend and Bunta course selections and behavior.

## 0.3.95-community-replays.24 - September 22, 2026

- New Time Attack course setups default to the left route, Dry and Day where available, instead of inheriting previous race conditions.
- Browsing Akina Snow no longer makes a subsequently selected course default to Wet/Night.
- Manual choices remain selected when moving between condition screens. Happogahara and Enna remain night-only; Akina Snow keeps its required snow/night conditions.

## 0.3.95-community-replays.23 - September 22, 2026

- Fixed flickering Enna Skyline start/finish banner poles by drawing only the facing side of overlapping support surfaces.
- Covers downhill and uphill in dry and wet conditions, including views from behind the banners.

## 0.3.95-community-replays.22 - September 22, 2026

- Fixed mirrored sponsor logos on Hakone in both directions and day/night, dry/wet conditions.
- Fixed Hakone flickering start-banner poles and overlapping START/FINISH graphics by selecting the correct downhill or uphill scenery.
- Preserved the existing Sadamine sign correction and the surrounding scenery, road markings, lighting and geometry.


## 0.3.95-community-replays.21 - September 22, 2026

- Added Online, Queuing and Racing counts to the community leaderboard website.
- Shares fresh game-scoped Steam activity surveys while Community Times is enabled. Counts include older builds using the same activity protocol; expired or unavailable surveys display a dash. Steam search limits display a plus sign.
- Retains the smaller updates, Full Repair and Linux/Wine installer fixes from .20.


## 0.3.95-community-replays.20 - September 22, 2026

- Replaced the PowerShell update installer with a bundled native executable. Update archive validation and staging use the game's existing runtime; applying patches/full repairs, rollback and restart no longer require PowerShell or a separately installed .NET runtime under Wine/Proton.
- Preserved small-patch selection, full repair, checksums, private-file protections and process-exit handoff.

- Added version-specific changed-file updates. Healthy installations download the matching small patch; unchanged files are verified locally. Missing or damaged required files trigger the full package fallback. Interrupted downloads do not silently switch to a large download.
- Added Full Repair under Settings > Gameplay > Updates / Repair, including when already up to date. Saves, settings, replays and custom music remain untouched. The update prompt shows download sizes.
- Releases retain the complete Windows ZIP for new installations, older clients and unsupported patch bases. Builds older than .19 need one full update before they can use patches.

## 0.3.95-community-replays.18 - September 22, 2026

- Redesigned the online browser, room/code entry, lobby, course draw, results and connection-loss UI with compact pause-menu styling. Existing Steam/LAN, controller/wheel navigation, saved cars, transmission, course/rule choices, music, readiness, race and rematch behavior remain available.
- Open rooms use the host's name (for example, Chris Lobby), with three visible rows and an always-visible proportional scrollbar.
- Added namespace-isolated Steam presence for online, queuing and racing counts. It also covers connected users outside race rooms. Counts refresh while the online menu is open; unavailable/stale data shows a dash and capped discovery shows a lower bound with +. Older clients without presence are not included. LAN has no global census.

- Added dry tire smoke using recovered arcade smoke art and bounded road-contact skid ribbons. Effects pause, expire and break across lost contact/teleports; wet races keep their separate spray.
- Enabled authored car reflection geometry with a recovered highlight texture for player and rival cars, with race-only additive highlights.
- Projected car shadows onto the road in online races and replays as well as solo play.
- Separated Engine and Tire Squeal volume under Settings > Audio. Tire PCM now remains separate through the queued mixer, allowing either channel to be muted without muting the other. Existing combined-volume settings migrate to both sliders without changing the player's current balance. Master, music and effects controls retain their behavior.

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
