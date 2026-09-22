0.3.95-community-replays.22
Wine/Proton: automatic updates now use a bundled native installer without PowerShell. If an older build reports that the installer could not start, manually install this Windows release once through your existing Wine/Proton setup.
Updates download a matching changed-file patch when available. Unchanged game files are verified before installation; missing or damaged required files trigger a full download. Full Repair is available under Settings > Gameplay > Updates / Repair, including on the latest version. Saves, settings, replays and custom music are preserved. Builds older than .19 need one full download to gain patch support.
Settings > Audio: ENGINE and TIRE SQUEAL now have independent volume sliders. Lower TIRE SQUEAL without quieting the engine, then choose APPLY. Your old combined engine/tire volume is preserved for both sliders until you change it.
Fixed numeric driver names on the community website and in replay filenames. Leaderboard labels now distinguish the Sileighty, Evo V and Evo VI TME.
Settings > Controls > Toggle headlights: press H or click the controller's right stick during a race to switch headlights off/on. Select APPLY after rebinding. Existing custom controls are preserved; assign a free button if your old bindings already used H/right-stick click. Wheels can bind their own button.
Corrected Enna Skyline's course-selection map orientation and Sadamine's mirrored sponsor banners in every weather and lighting variant.
Corrected Sadamine's course-selection map orientation and rain appearing inside Tsuchisaka's tunnel. Exterior rain and wet-road tire spray remain visible.
AE86 Levin Tune A turbo audio now starts at Step 3; Step 5 retains continuous boost audio without turbo release sounds. Applies to races, online and replays.
Fixed missing road and incorrect scenery during Akagi uphill race intros; scenery now loads from the actual starting grid before the camera showcase.
Wheel feedback driver calls now run outside the game thread. Healthy active feedback no longer scans all devices every two seconds.

POST-RACE PAINT RECOVERY
Fixed the "Original material paint outside palette" crash after races. Invalid saved paint values use the default factory color for previews; existing driver saves and upgrades are preserved.

AI DRIVER DIFFICULTY
Settings > Gameplay > AI Driver Difficulty: Normal keeps the original pace; Hard adds 5% and Expert adds 10% to AI target pace. APPLY saves the setting for the next Legend of the Streets battle. Bunta Challenge, player handling, Time Attack and online human opponents are unchanged.

HUD OPTIONS
Settings > HUD > OPEN LIVE HUD EDITOR: Switch between bumper and third-person previews, drag HUD groups with the mouse, and resize with the mouse wheel or + / - buttons. Includes Time Extended, timer/sections, speedometer/gear, Time Attack records, Legend and Online opponent panels, mirror, minimap and Accepting Challengers. Time Attack records and both Legend/Online opponent panels share one position; moving any of them moves all three. Save & return applies the layout; Cancel discards edits. Countdown and finish/pass/fail/new-record announcements stay fixed. Minimap Zoom Out remains in HUD settings.

TODAY'S UPDATE - SEPTEMBER 21, 2026
Enna Skyline is included in the main game and online course selection: downhill/uphill, dry/wet, night scenery and Akina handling. Enna Time Attack boards and replay downloads are available on the community leaderboard.
Online boost and car collisions default ON and can be changed by the host before racing. Opponent smoothing, network input delivery, mouse/controller menus, scenery processing and replay-save performance have been improved. Both online players need this update.

GITHUB GAME UPDATES
At startup the game checks the latest public GitHub Windows release. If an update is available, choose YES to download, verify, apply it and restart, or NO to continue playing. Up-to-date builds proceed directly to the game. Offline checks time out and allow play. Saves, settings, custom music and replays are preserved. You can also check from the title screen through Options > Gameplay > Game Updates.

REPLAY RECORDING PERFORMANCE
Replay buffers are allocated during race loading and reused, avoiding buffer-growth copies during driving. Local replay compression, validation and file saving run in the background; community replay compression and upload preparation also run off the game thread. Exact 60 Hz samples, both cars, RPM, speed, movement and recorded audio controls are preserved. Pending background saves finish before a normal game exit.

PERSONAL RECORDS WHEN OFFLINE
Offline course/model records and rankings now use only personal times from the selected save, including its saved cars and Hakone/Sadamine times. Old combined cabinet records and downloaded leaderboard caches are excluded. Existing personal saves are preserved. While connected, rankings combine personal bests with the current leaderboard; matching published rows appear once. Empty online boards still show your personal records.

CUSTOM RACE MUSIC
In Select BGM, choose CUSTOM > ADD MUSIC to import an MP3, OGG or WAV from your PC. Then select the imported song for your next race. Songs and the selected preference are saved locally under userdata-unity-scene/custom-music; music is never uploaded or sent to opponents. The existing music volume, pause, looping and race/result transitions apply. Import limits: 64 songs, 100 MB source files, mono/stereo up to 48 kHz, 10 minutes and 64 MB decoded PCM per song.

COMMUNITY LEADERBOARD — FRESH START
Online rankings have been cleared. Only new Time Attack runs from 0.3.95-community-replays.1 or newer can submit, with a complete replay. Earlier queued runs and saved personal bests are not uploaded. Local saves and personal records are preserved. Download public replays on the leaderboard website, then open Options > Replays > Open file to watch.

PERSONAL REPLAYS
New filenames include driver/opponent, track, direction, day/night, wet/dry, and mode: chris_vs_tak_akina_dh_night_wet_lots.idreplay. Modes: lots = Legend of the Streets, tat = Time Attack, ol = Online. Solo Time Attack omits the opponent. Repeated names get _2, _3, etc.; existing recordings are kept. Direction labels follow the course: uh/dh, ob/ib, cw/ccw, or rev.
Options > Replays opens your local replay library and controls Time Attack, Online Battle and Legend of the Streets recording. Apply saves the recording options. Community Times always requires Time Attack recording. Online and Legend recording are optional and start with the next race.
Personal recordings are saved under %USERPROFILE%\AppData\LocalLow\Chris\Initial D Unity\userdata-unity-scene\replays. They are not uploaded. Only the replay attached to a submitted Time Attack result is uploaded. Keep or back up these files yourself; no automatic deletion is performed.
The viewer opens separately so your game and save remain intact. Use D-pad/arrow keys and A/Enter to open a recording; B/Esc returns to the library. During playback, A/Start/Space pauses, Y/C switches camera, D-pad left/right seeks, and Select/H hides controls. X/Tab switches between your car and your opponent in current Online Battle recordings, preserving each driver's recorded speed, RPM, gear, movement and HUD clocks. Older connection formats without complete opponent telemetry retain the player POV. Legend replays show both cars from the player's cameras.

TIME ATTACK REPLAYS
Every new shared time requires a driving replay. Older clients cannot upload times without one. The Upload Previous Times option has been removed; old queued times without replays are excluded. New completed runs include a compact driving replay with the shared time. Replays queue offline and can be downloaded by the leaderboard administrator for review. New recordings capture car position, heading, RPM, speed, gear, body and wheel poses, clocks, lights and car appearance at 60 Hz with lossless compression. They are not verified simulations or video recordings.

TIME ATTACK HUD CLEANUP
Removed the development progress percentage and placeholder BEST time label.


AKAGI SCENERY PERFORMANCE
Reduced roadside scenery rebuild stalls by reusing unchanged objects and mesh buffers as the car moves. Tested in Akagi night/rain in both directions. Average FPS gains vary; this does not claim to eliminate every course slowdown. Visuals, weather, mirror behavior, draw distance and gameplay are retained.

LOW-END PC OPTIONS
Reduced CPU work on original courses by reusing unchanged scenery geometry. This applies automatically at every graphics preset, without changing mirror, weather, draw distance, handling or race timing. Imported Hakone/Sadamine rendering is unchanged by this optimization.
Settings > Graphics contains the quality preset, rain/spray and imported scenery options alongside resolution and anti-aliasing. Balanced (720p, 2x AA) and Low (540p, no AA) use reduced rain and imported scenery detail, targeting 60 FPS. Both retain the mirror, visible rain and all gameplay features. Choose Apply and confirm resolution changes; they revert after 15 seconds without confirmation.
Rain/spray and Hakone/Sadamine scenery detail can also be changed separately. Graphics now includes 640x360 and 960x540. Original restores full effects while retaining your chosen resolution. Existing settings retain their appearance until changed. Wet grip, handling, race timing and records are unaffected. Gains depend on the PC and track; no minimum FPS is guaranteed.

COMMUNITY TIME ATTACK RECORDS
Shared course/model times and rankings use the existing screens. New completed Time Attacks upload in the background, with offline upload queueing. Downloaded rankings display only after a successful current connection; offline records come from the selected save. Personal records remain with your save.
No Steam linking or player account: a random installation ID identifies your submissions. Driver name, car, points, conditions, checkpoint splits, finish time and game version are shared. Historical saves are not uploaded.
Settings > Records > Community Times controls sharing and shared record display. OFF stops new uploads and uses the selected save's personal records; previously shared runs remain until moderated.
Public rankings: https://initial-d-leaderboard.initial-d-community-leaderboard.workers.dev
Times receive basic validation, not anti-cheat or replay verification. All games need this update to share times.


- Enabled GPU instancing for repeated Hakone/Sadamine trees, retaining their lighting, foliage faces and distance settings. Performance gains depend on hardware; this does not claim to resolve every reported course FPS drop.

- Fixed a late-race car-lighting crash on Hakone and prevented the same invalid shadow-table lookup on Sadamine, in Time Attack and online races.

Gameplay Full Tune: choose a save, then select a make and car, complete normal new-car setup if needed, receive 999999 points, apply mandatory upgrades and choose optional parts. Returns to Choose a Mode with the same save. Optional parts retain their normal costs; Back finishes optional selection.
- Fixed hidden car panels showing through menu cars as they rotate, including their reflected previews.
- Kept the attract intro's close camera passes outside the car body, with consistent clearance throughout each affected shot.
- Reduced per-frame scenery CPU work on Hakone and Sadamine by caching fixed placements and avoiding unchanged tree-rotation writes. Existing scenery visibility and detail transitions are preserved.
- Post-race audio plays to completion before analysis. Press Start to skip; accelerator/confirm does not skip it. Record announcements still appear immediately after FINISH.
- Original-course race intros use the original projected title size and condition placement.
- Time Attack retains FINISH until the audio completes when no record was beaten. New course/model/personal records retain their HUD-free announcement.
- Wheel-only navigation: steer to select menu items/songs, accelerate to edit or confirm, brake to back out. No D-pad or paddles required to reach Apply or enter room codes.
- Back/Select opens Online Battle; D-pad/stick navigates, A confirms and B goes back. Room codes can be entered with the on-screen keyboard.
- Binding slots offer Rebind, Clear and Back with controller navigation. Unused rebind prompts cancel after 15 seconds.
- Settings starts on categories. Confirm enters the options; Back returns to categories. Apply saves changes.
- Connected wheels/HID menus use saved accelerator/brake, steering and shift controls. Online can be bound in Controls.
- Hold Start to skip dialogue; conversation skipping no longer depends on a single render frame.
- Course and condition choices remain visible until the final selection.
- Restored countdown numeral growth and GO fade on the original race clock, reduced its size and centered the visible artwork.
- Rain uses narrow falling streaks and low directional water trails at the rear tires.

Loading artwork now fades to black, holds for two seconds, then cuts to the race intro. The intro includes both top and bottom black bars until the countdown begins.
Accepting Challengers is positioned on the right below the split-time panel and above the speedometer, including widescreen.
Removed the internal handling-success popup that could overlap the course-selection artwork.
Online Battle now offers Retry Steam after Steam initialization fails. Open Steam and sign in, then retry from the same screen.
Reduced repeated HUD geometry copies and temporary allocations while preserving rendered pixels and UI commands.
Conquered sequences now include the original staggered text entrance, trailing layers, arrival sounds, music and volume fade, followed by a clean handoff to the next rival.

Presentation and finish update: corrected Continue camera framing, post-race braking/input lock, menu fades, original attract prompts, thirteen loading pictures, conquered course screens, clearer time differences and tach needle, course ordering, Sadamine/Hakone wall sweeps, and FPS counter overhead.

IMPORTED TRACK TREE FIX — 0.3.87-imported-trees.1
Hakone and Sadamine: prevent overlapping front/back leaf faces from fighting,
stabilize tree detail-distance changes, and face distant tree cards toward the camera.
Leaf edges use coverage smoothing when MSAA is enabled in graphics options.

SADAMINE USUI HANDLING — 0.3.86-sadamine-usui.1
Sadamine now uses Usui handling in Time Attack and Online Battle, both directions and dry/wet.
Hakone continues to use Myogi handling. Both online players need this updated build.

SADAMINE — 0.3.85-sadamine.1
Sadamine is selectable in Time Attack and Online Battle alongside Hakone and the original courses.
Includes downhill/uphill, day/night, dry/wet, D3 HUD, four sections/time extensions,
points/tuning, coaching, rankings and separate saved records. Uses Myogi handling.
Both online players need this updated build. Existing cars, saves and progress are preserved.

HAKONE MYOGI HANDLING — 0.3.84-hakone-myogi.1
Hakone now uses Myogi handling in Time Attack and Online Battle, both directions and dry/wet.
Both online players need this updated build.

HAKONE ONLINE — 0.3.83-hakone-online.1
Hakone is selectable in Online Battle: downhill/uphill, day/night, dry/wet.
Uses the existing shared two-car simulation, collisions and host-controlled boost.
Both players need this complete updated build. Time Attack support remains included.

HAKONE TIME ATTACK — 0.3.82-hakone-ta.1
Hakone is available in Time Attack alongside the original nine courses.
Downhill/uphill, day/night and dry/wet; D3 HUD, checkpoints and time extensions.
Includes points/tuning, course/model/personal records, coaching, ranking and Continue.
Normal saved cars, progress and startup are preserved. Hakone supports Time Attack and Online Battle.

TSUCHISAKA OIL FIX — 0.3.81-tsuchisaka-oil.1
Restored the original oil spill artwork in dry Time Attack, online races,
and the later four Legend races, in both directions and at day/night.
Oil remains absent in wet races, Bunta Challenge, and the first two Legend races.
Fixed old saved opponent selections incorrectly disabling oil grip in solo/online.
Both online players need this complete updated build.

CONTROLLER RECOVERY — 0.3.80-controller-fallback.1
A disconnected selected controller now temporarily falls back to an available
controller using that device's own bindings. Your selected device and bindings
are retained, and restored automatically when the device reconnects.
Keyboard controls remain available when no controller is connected.
Controls options identify when a temporary device is active.

DIALOGUE STILL FIX — 0.3.79-dialogue-stills.1
Restored missing original still/background selections for the final seven rivals.
Covers Kyoko, later Ryosuke/Keisuke/Takumi, Evo V/VI opponents and Legend Bunta.
Both online players need this complete updated build.

ANALYSIS CRASH HOTFIX — 0.3.78-analysis-fix.1
Fixed Street Racing Analysis crashing while cycling Shomaru/Tsuchisaka maps.
All map artwork remains selectable; pages without telemetry skip that overlay.
Both online players need this complete updated build.

ONLINE BOOST AND PRESENTATION UPDATE — 0.3.77-online-boost.1
F1 lobby: BOOST / TURBO ON/OFF is set by the host (default ON).
Changing boost clears both READY states; the setting locks for the race.
Uses the original trailing-distance correction in the shared rollback simulation.
Opponent fixed-step poses now interpolate between rendered frames.
Restored opponent headlight flags and night ambient lighting in authority races.
Original opponent headlight projection is available in both main and mirror views.
Both players need this complete updated build; prior versions are incompatible.
INITIAL D UNITY - CURRENT BUILD

Quick Match now searches while you keep playing offline. The original
ACCEPTING CHALLENGERS indicator appears only in offline races while searching.
When a compatible driver connects, CHALLENGE RECEIVED flashes with the original
arcade sound, then both players enter course selection with the lobby open.
The interrupted offline race does not count as a loss. Cancelling a search or
losing the challenger removes the indicator. No search badge appears online.
The taskbar flashes if the game is in the background; audio respects settings.
Both players need this updated build to match. Host-authoritative rollback and car collisions are enabled.

Options and music-menu exits no longer lock all input when a pedal or steering
axis is held. Applying/resetting bindings also preserves usable controls.
Keyboard steering reaches full lock in about 0.17 seconds instead of 0.67,
with quicker countersteering and centering. Car physics and analog controls
are unchanged. Online rollback and car collisions are enabled.
Both players need the same updated build for online rooms.

Legend refusal dialogue now plays before returning to course selection.
The offline Legend pause menu has one exit option: RETIRE RACE. It records
a time-out loss and
continues through results, rival dialogue and the retry/course choices.

Time Attack course selection now shows the original COURSE RECORDS panel:
TOTAL, MODEL and PAST times, refreshed for the selected route and weather.

Course occlusion rendering skips off-camera geometry and reuses unchanged draw
lists independently for the main camera and rear-view mirror.

Removed the Time Attack ghost car. Saved best times and replay telemetry remain.

Online garage lists every stored car in each save slot, with its exact paint,
parts and tuning. Legacy saves are separately labeled. Both players need this
build for the updated saved-car selection protocol.

Wet races now show falling rain and tire spray for both cars, using original
rain-bank textures. Effects use bounded host particle placement/timing.

Time Attack HUD labels render above their original black backing strips.
Unchanged world meshes stay cached as roadside objects enter or leave view;
scenery, draw distance and source rendering order are preserved.

Pause/resume now preserves held acceleration and steering inputs. The final
Time Attack section remains included on the result/points screen.

Run InitialDUnity.exe. Unity Editor is not required to play.
Keep this entire folder together: the executable needs its adjacent Data,
MonoBleedingEdge, and DLL files. Copy the whole folder to move the game.

This build includes the current 35-car catalog, nine course selections,
menus, attract mode, opponents, lighting, audio, HUD, mirror, and cameras.
Unity renders the world and interface and outputs audio. The existing
C++ driving and race simulation are preserved.

Course scenery now includes the recovered original tree placements and
roadside spectator billboards, with course-specific visibility and original
draw order. Akagi's rotating prop also animates during the race.
Placed course trees now render to at least 600 metres, replacing the arcade's
100-180 metre limits. Original placements, textures and handling are retained.

Before a race, the original moving cameras show both cars, followed by
the animated player-versus-opponent names and original 3, 2, 1, GO.
The race waits until the showcase ends. Your selected driving camera is
restored automatically. Online battles show both drivers' online names.
Online car showcases also display each driver's battles, wins and win
percentage. The room closes as soon as the race scene is rendered; F1 can
reopen it during the race. During the car showcase, original animated auras
appear from battle level 11, with the original level colors and special
consecutive-win effect. Both cars' auras disappear before the 3, 2, 1
countdown and stay off throughout the race.
Online records and battle experience are saved per car under
userdata-unity-scene/online_records_v1. Draws, double time-ups and dropped
connections leave those records unchanged. Earlier builds did not store
online battle history, so unrecorded past races cannot be reconstructed.
During online races, the original ADVANTAGE display measures the signed
gap along the course. OPPONENT and DRIVER show each peer's name and car
code. Both bumper and chase views have the original rearview mirror.

DEFAULT CONTROLS
F1: multiplayer room browser / host / join / ready / leave
Enter: confirm / start
W or Up: accelerate
S, Down, or Space: brake
A/D or Left/Right: steer
Q/E: manual gears
C: bumper / chase camera (bumper is the default)
F11: fullscreen at the display resolution / return to window
Escape or controller Start: pause / online race menu
Backspace or controller B in the pause menu: back
R: restart
F5: quick Akina Time Attack

RACE MUSIC
On opponent selection, hold View Change (C / controller Y by default) to
open Select BGM. The bottom hint follows your custom camera bindings.
Choose a track, use the Stage 1/2/3/4/5/6/7/8 filters, then confirm with Enter/A or
the mouse. Escape/B cancels. There are 101 original-format race tracks,
including 14 each from Stages 4, 5, 6 and 7, and 16 from Stage 8, with original loop points.
Stages 6, 7 and 8 preserve their banks' Microsoft ADPCM audio in WAVE containers;
Gamble Rumble is excluded. Game Default plays Speedy Speed Boy.
Your choice is saved for your next race.
In a multiplayer room, use the MUSIC button or hold View Change to open the
same picker. Each driver chooses their own local music. Opening the picker
clears your Ready state; choose Ready again when finished.
Songs cannot be switched during a race. F3 cycling and the extra in-race
button hints are removed. Restart is unavailable in multiplayer; R restarts
offline races only.
Finish announcements use the original win, loss or time-up audio. Online
finishes wait for the confirmed result; draws and connection losses do not
play a victory announcement. Later result screens keep their original music.

The game renders at the current window or screen resolution. Wider windows
reveal more of the 3D scene; cars and HUD elements keep their proportions.
Original flat menu artwork keeps its authored proportions.

PAUSE MENU / OPTIONS
On the attract screen, hold View Change (C / controller Y by default) for
OPTIONS. A small inset hint at the bottom right shows your current binding.
Settings open directly;
choose APPLY to save, or BACK to return to attract and discard unapplied edits.
Short View Change taps still change conditions on the original ranking screens.

Resume, restart the offline race, return to course selection, or quit.
Online races offer Leave Online Battle instead of course selection.
Options includes Audio, Graphics, Gameplay, editable Controls, and Wheel.
Adjust master/music/engine and tire/effects and voice volumes; resolution,
window mode, VSync, frame limit and anti-aliasing; default camera, FPS
display, and optional background mute. Choose APPLY to save changes.
Display changes revert after 15 seconds unless you choose KEEP CHANGES.
Navigate with arrows and Enter/Escape, controller D-pad and A/B, or mouse.
Settings are saved to game-options.json beside the existing scene saves.

Gameplay includes STEERING SMOOTHING beside STEERING DEADZONE. At 0% the
current steering response is preserved; higher values soften sudden steering
changes for keyboard, controller, or wheel. Choose APPLY to save. The slider
affects driving steering only. Menu navigation and pedal response stay immediate.

In Controls, use the controller selector to choose AUTOMATIC, KEYBOARD ONLY,
or a connected controller. Automatic follows the device receiving input;
select a specific device to keep other controllers from taking over.
Select an action's keyboard or controller cell, release the activating
control and leave axes at rest, then press a button or move a stick/pedal.
Three keyboard slots and one controller binding are available per action.
Assigning an occupied controller button swaps the two actions. Keyboard
conflicts still require clearing the previous assignment first.
Choose APPLY to save all edited controller profiles to controls.json.
Keyboard bindings are shared. Switching controllers preserves draft edits;
BACK discards them. RESET DEFAULTS needs APPLY; Escape cancels capture.
Menu arrows/D-pad, Enter/A, and Escape/B remain available. XInput's four
slots, Unity gamepads, and OS-exposed gaming HID/joystick devices appear in
the selector, including after reconnecting. Generic wheels and joysticks
start unbound: bind their pedals, steering directions and buttons here.
Devices without unique hardware serials share a profile by model; XInput
profiles follow Windows slot numbers. USB/Bluetooth support depends on the
device and its Windows driver exposing controls to the game.
Rebinding preserves the existing car handling.

Offline races pause while this menu is open or the window loses focus.
Online races and network updates continue when either driver opens the
menu or switches to another window. Driving input is released while the
menu is open or the game is unfocused; the race continues without waiting.

Existing Unity scene saves are used automatically from:
%USERPROFILE%\AppData\LocalLow\Chris\Initial D Unity\userdata-unity-scene

TWO-PLAYER MULTIPLAYER TEST
QUICK MATCH: Press F1 > STEAM ONLINE > QUICK MATCH to find an open room.
If none is available, your game opens a room and waits for another driver.
Use CANCEL SEARCH to stop. Once connected, both select READY and the host
starts. Quick Match also retries rooms that fill or close before joining.
Both players must update to this same build to appear in each other's search.

Both PCs must have this same complete Current folder. For internet play,
run Steam and sign in to a DIFFERENT Steam account on each PC. This test
uses Valve's Spacewar development App ID480 with the official Steam API.
Keep steam_appid.txt beside the executable. No dedicated server is needed.

1. Press F1, select STEAM ONLINE, then HOST A BATTLE on the first PC.
2. On the second PC press F1, refresh and join the room, or paste its code.
3. Each driver selects a car and their own course, direction, surface and time.
   Both picks appear in F1. Changing a car or course pick clears both READY states.
4. Both select READY, then the host starts. The game randomly selects one
   driver's complete course pick with equal odds and loads it on both PCs.
   F1 shows whose pick was selected. Close F1 to drive after the showcase
   and original countdown. There is no coin animation or extra waiting screen.
5. Open F1 to see the room/result or LEAVE ROOM to return to single player.

LAN DIRECT also works between two PCs on the same network: host and enter
the displayed IP address:port on the other PC. If Windows asks, allow the
game on your private network. Default LAN TCP port is27035.

Online racing uses host-authoritative simulation, local prediction and rollback,
with car-to-car collisions. Saved cars, tuning, appearance and AT/MT are used.
Both players must use the updated build with authority_test.txt included.
Multiplayer races do not change single-player cards, records or progression.
F1 captures controls while open but the online race continues. If a connection
drops during a race, FINISH appears with CONNECTION LOST and no result recorded.
No points, win, or loss are awarded. Press Enter/A or Escape/B, or click RETURN
TO MENU, to leave the finish screen. Host ownership is not transferred.

Remaining original-game fidelity work is not completed merely by packaging
this build. See MULTIPLAYER TEST.txt for this build's verification scope.
CONTROLLER RESPONSE (0.3.42)
Pause > Options > Gameplay > Controller Response:
- FLYCAST GAMEPAD (default): matches Flycast v2.5's 10% radial dead zone,
  full JVS analog range and the game's default cabinet calibration.
- FLYCAST WHEEL: the same analog range with no stick dead zone.
- PREVIOUS: restores this remake's earlier steering and trigger response.
Choose APPLY to save. Keyboard bindings and the original driving solver remain
the same. Physical trigger resolution and custom cabinet calibration can differ
from an emulator setup; this is a controller response profile, not a physics mod.

STEERING AND WHEEL OPTIONS (0.3.43)
Pause > Options > Gameplay > Steering Deadzone: adjust from 0 to 30%.
Each Controller Response profile remembers its own value. Defaults are 10%
for Flycast Gamepad, 13% for Previous, and 0% for Flycast Wheel. This slider
affects driving steering only; pedal response and menu navigation are unchanged.

Pause > Options > Wheel: enable force feedback, select the feedback wheel,
set strength, and reverse its direction if your driver requires it. Choose
APPLY to save. Feedback is off by default; its initial strength is 35%.
Install the wheel manufacturer's Windows driver and bind its controls under
Controls. Automatic feedback selection requires a unique match to the active
input device; otherwise select the feedback wheel explicitly.

Feedback uses standard Windows DirectInput force-feedback devices. It adds
steering load, lighter resistance during sliding, and wall impacts. This is
an initial synthesized model, not the original cabinet's exact force output.
Feedback stops in menus, while paused or unfocused, and outside driving.
Hardware compatibility and physical feel still require testing on real wheels.

The upgraded AE86 now uses its original 12,000 RPM tachometer. The HUD also
bounds the needle to its dial sweep without changing engine RPM or gearing.


BUNTA AND TIME ATTACK FLOW (0.3.44)
Bunta course selection now uses the original portrait, BUNTA LEVEL strip,
5/10/15 tiers and animated earned stars. It reads each course's saved progress.
The original challenge, win and loss dialogue uses Bunta's own portrait,
authored scripts and music. START skips the dialogue; the car showcase follows
the challenge. Completing the final challenge saves the original level15 cap.

Time Attack now proceeds from FINISH and the frozen time/record summary into
Street Racing Analysis, then the original points/tuning screen. A qualifying
record shows the original ranking artwork populated with actual local records.
CONTINUE returns to course selection; NO returns to the title. Driver name,
transmission and time of day are retained for newly saved record rows.

Analysis uses the original Ryosuke artwork, course maps, coaching rules and
advice text. Feedback uses actual source driving statistics, impact and ditch
records, section durations and this driver's previous personal record. The
three-second result-summary reading hold is native behavior. The original
Internet password/registration service is not implemented.

Hold VIEW CHANGE on Bunta or Time Attack course selection to open the existing
race music picker, as on Legend rival selection. Gameplay steering and tuning,
controller bindings, wheel settings and the previous tachometer fix are kept.

TIME ATTACK COACHING AND RANKINGS (0.3.45)
Personal records now belong to the selected saved driver and retain original
checkpoint times. The shared course/model leaderboard is separate. Existing
shared leaderboard history remains readable; it is not assigned to a driver.
After the points screen, a qualifying run shows the course ranking with the
original entry/exit fades and timer, followed by CONTINUE.

The analysis screen restores its animated original backdrop, countdown,
original statistic digits and current/previous section timing table. The
ranking and Continue screens use their own original rotating-car backgrounds.
The separate original animated route replay still needs implementation.

At the title's ranking display, Q/E or controller LB/RB opens/switches the
original model-detail pages. VIEW CHANGE (C or controller Y) changes the
course condition. START/confirm still begins the normal game menu.

Online car selection lists saved cars by save number. Saved tuning, transmission, paint, parts and number plate are used in races; opponent appearance is synchronized. Unsaved models are labelled STOCK. Both peers need this build.

Online transmission: use AT / CHANGE or MT / CHANGE under your readiness label before readying. Each driver can choose independently; the choice affects this online session without rewriting the offline save.

Ryosuke analysis now shows the driven acceleration/braking/coasting path on full and zoomed maps. Legend pre-race banners use the original opponent race number and direction, including FINAL and EXTRA.
Controller Start now confirms in original menus without also sending Escape. Time Attack now shows its original record panel during racing, and the points results include the final section time.
Returning to course selection during the pre-race car showcase now restores the music selector without restarting the game.






REPLAY VIEWER
Run Replay Viewer.cmd in this folder. In leaderboard admin, choose Download for 3D viewer, then Open replay in the viewer and select the downloaded .idreplay file. You can also drag a replay onto Replay Viewer.cmd.
The normal D3 HUD uses recorded RPM, speed, gear and clocks for detailed recordings. Body and wheel movement interpolate between the captured 60 Hz states. H/Select hides playback controls. Older recordings are labeled LEGACY: missing RPM, tuning and body/wheel movement cannot be recovered. Playback renders the scene again; it is not an exact recording of every pixel, sound, particle or original camera frame.
Space/A/Start pauses playback, arrows/D-pad seek, C/Y switches camera, Q/E or right stick rotates the orbit camera. The viewer does not change saves or upload times. Cosmetic tuning and engine audio are not included in recordings.

DISCORD RICH PRESENCE
With Discord running, your profile shows Initial D Arcade Stage 3 and your current game activity. Turn it off through Options > Gameplay > Discord Rich Presence, then APPLY. No account linking is required.


Race visuals: dry tire smoke and skid marks, car highlights, and road-projected shadows (including opponents and replays). Smoke/highlight art comes from the original files; effect timing and skid geometry are host presentation.

Online menu: compact pause-menu styling, host-named lobbies, three-row scrolling room list, and live Steam activity counts. A dash means activity is unavailable; + means Steam returned a capped sample. Counts cover presence-enabled clients.
