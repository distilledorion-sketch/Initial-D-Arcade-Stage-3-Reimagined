# Discord bug follow-up — November 10, 2026

Continues the [November 4 follow-up](bugs-channel-followup-2026-11-04.md).
Game tests remain hidden and silent. The completed local game changes were
installed in `D:\Current` on November 11, 2026, at the user's request. GitHub
has not been updated. Earlier entries below retain their verification history.

## Moving rear-view smoke — corrected, November 12

The user's `2026-11-12 18-57-24.mp4` shows a pale cloud repeatedly covering
the lower rear-view mirror during solo driving on Akina at night. Unlike the
earlier parked-car report, this supplied a visible moving-frame reproduction.

The host was submitting the source smoke density mesh twice, once per camera.
The original race call at `0662AE..0662D2` registers **one** smoke view with
`13E960`: the main camera at `ARace+76+52`, with no second view. The smoke-view
reference test now executes this original call site and checks all four
constructor arguments. The production correction removes only the extra
mirror submission; smoke emission, lifetime, shared RNG, main-view projection
and materials remain unchanged.

Private Unity fixtures restore exact smoke inputs and poses from the user's
recent local Akina replay at 33.75s and 158.4s, in bumper and chase views, and
also exercise live Tsuchisaka dust. The original replay file is unchanged.
The reference reproduces the same pale mirror cloud, contributing 19,579 and
19,523 pixels in the two recorded scenes. The corrected build contributes
zero smoke pixels to the mirror. Across all six before/after images, every
pixel outside the mirror is identical; ordinary main-view smoke and dust
remain visible. Removing/restoring particles reproduces all captures exactly
without changing the physics digest. Tests ran muted on an unshown desktop.

Source view/display/controller checks, original dust rendering, online smoke
rollback and recorded smoke playback also pass. This is a rendering fix,
not a claim of new two-PC network verification or complete cabinet parity.
Evidence: `Verification/smoke-video-20261112/`, especially
`before-unity-04/`, `after-unity-01/`, `before-after-pixels.json`, and the
source/presentation/online/replay test logs. Earlier unsuccessful diagnostic
setups remain preserved; their empty or moving captures are not passing proof.
The native correction is installed in `D:\Current`, with its previous DLL and
build metadata preserved in `Verification/smoke-video-20261112/current-backup/`.
The installed player also passed all 109 stationary-mirror checks and exited
cleanly. Saves, settings, recordings, ROM and custom-music fingerprints remained
unchanged. GitHub publication remains on hold.

## Current installation updated — November 11

A fresh native build and clean, non-development Unity player were built from
the complete current source and assets. All 21 selected native checks passed;
the source/config and build-input fingerprints remained unchanged throughout.
The approximately 3.90 GiB candidate contains 19,297 distributable files.

The updater backed up and replaced 41 changed game files (about 153 MiB), then
verified every installed file against the candidate manifest. It preserved
ordinary saves, ROM files and custom music. `D:\Current\BUILD INFO.json` now
records the local build's fingerprint and backup location. This remains the
local .53 version; no new release or backend deployment was performed.

The installed player passed the muted, isolated-desktop music setup check
(364 checks and 36 captures), attract rear-lamp trail check (10 paired probes),
and ending backfire check (153 checks and 20 captures). The actual-window music
prompt was visually reviewed on the final time-of-day page. All installed game
file hashes and protected user-file fingerprints still matched afterward.
The diagnostic players exited; no game was left running.

Evidence and the rollback backup are under
`Verification/current-update-20261111/`: `build/summary.json`,
`build/candidate.json`, `installed.json`, `installed-verification.json`,
`installed-{music,attract,ending}/`, and `backup/`.
Open investigations below remain open. The original daytime ending lens flare
has since been integrated and privately checked, but is not in this installed
build. These checks do not establish two-PC online or complete cabinet parity.

## Mirror smoke while stopped — investigation, November 11

The user subsequently said the issue appears fixed and asked to move on.
No further mirror change or installation was made. The additional private
stop-sequence probe was ended at that request: its completed first solo scene
showed 4,193 smoke-contributed mirror pixels during driving, followed by zero
particles, tyre effects and smoke-contributed pixels while stopped. The other
five scenes did not complete, so this is not a six-scene pass or a reproduction
of the original report. Evidence: `Verification/mirror-smoke-20261111/stop-sequence-01/`.

The user reports random smoke in the rear-view mirror even while stopped.
The cause is **not reproduced or fixed**. Track/conditions and a visual example
are still needed to distinguish smoke from another effect or stale geometry.

`car_smoke_application_tests --stopped-mirror` now follows actual driving inputs
through an idle start, acceleration/turn, braking to zero, and released controls.
On Tsuchisaka dry day, dry night and rain, 4,500 simulation frames and 157 checks
show particles/intensity returning to zero after braking, with no smoke pixel
contribution in the stopped full-scene captures. Released controls subsequently
allow the car to roll at about 3.95 km/h; this phase is not claimed stationary.
These runs use private saves, no audio, and an offscreen native renderer.
Evidence: `Verification/mirror-smoke-20261111/run-02/`. Run01 failed its setup
requirement because the initial unbraked car rolled, not because smoke persisted.
The BMP mirror-region counter was corrected to account for bottom-up rows;
independent image decoding confirms zero mirror differences in these captures.
Moving-frame smoke was visible in the main view, but not the mirror: this does
not establish cleanup of a previously visible mirror cloud.

The installed Unity build was also run muted on an unshown desktop with its
existing dust disable/restore check. Forward Tsuchisaka contributed 1,843 pixels
and restored byte-identically; the reverse capture contributed none and failed
the existing visible-dust assertion. The mirror showed no smoke in either case.
This is a failed/incomplete broader check, not evidence of a mirror fix. Its
report and captures are under `Verification/mirror-smoke-20261111/unity-02/`.
Unity01 used the distributable staging folder without a ROM and stopped at its
startup check; that owned process was terminated before trying the installed
copy. All diagnostic processes exited. Installed managed/native assemblies still
match the verified November 11 build; no new desktop update was applied.

Further stationary probes checked the full five-tick source emission cycle on
Tsuchisaka's dirt shoulder in both directions. Its stationary dust owner remains
attached and briefly owns zero-lifetime particles, as the source specifies, but
these contribute zero pixels. A recent ordinary local replay identified Akina
downhill/night/rain as another useful condition; it does not prove that this
was the user's reported scene, and its recorded driving segment has no sustained
stop. The original replay was only read; an extracted diagnostic copy remains
private under `Verification/mirror-smoke-20261111/user-replay/`.

`car_smoke_application_tests --stationary-shoulder` also tests Akina night/rain
at its actual starting grid. Build05 and `shoulder-03` passed: all 15 captures
show zero smoke pixels in either camera at 0 km/h, and disabling/restoring smoke
preserves the simulation digest and reproduces the image exactly. The Akina
capture was visually inspected. Shoulder02 initially used a Tsuchisaka-specific
relocation on Akina, which placed the car off the road; that setup is superseded
by Shoulder03's untouched Akina starting pose. All tests are silent and offscreen.
These are native renderer checks, not a reproduction of the user's Unity mirror
artifact. No production change or installation was made for this investigation;
the visible artifact and its exact conditions are still needed.

The clean Unity follow-up now passes 109 checks and 18 captures across six
stationary scenes: Tsuchisaka day grid/reverse shoulder, Akina night dry/wet grid,
and Tsuchisaka/Akina wet-night after driving then braking. All captured speeds
are zero. These tests advance both simulation and scene-render time during the
approach and stop; the earlier simulation-only probes did not exercise the
host rain/spray clock. They compare smoke/dust on, removed and restored through
the actual Unity main/mirror/HUD camera stack. Every pair has zero changed
pixels and exact restoration. Legacy smoke is also asserted empty. The Akina
wet-night post-stop and Tsuchisaka reverse-shoulder screenshots were reviewed;
neither shows the reported cloud. This still does not reproduce the user's
artifact or prove cleanup of smoke previously visible in the mirror.

Evidence: `Verification/mirror-smoke-20261111/stopped-mirror-unity-01/`,
`unity-findings-01.json`, native diagnostic Build07 and the clean non-development
Unity build under `clean-unity-01/`. The test used isolated saves, no sound, and
an unshown desktop; all owned test processes exited. `D:\Current` and the
repository Editor plugin retain their prior hashes. The pending question asks
for the actual track and weather; no speculative smoke suppression was shipped.

Additional November 11 source inspection confirms that the Unity mirror uses a
solid-color clear and that dynamic smoke geometry is refreshed independently of
immutable course geometry. This inspection does not establish the cause of the
reported stationary cloud. No game was running during the follow-up, and the
track/weather question remains pending. No speculative suppression was applied.

### Tsuchisaka dust diagnostic follow-up — November 11


A fresh read of the live Discord bugs forum confirms the November 1 report,
"No dirt effects on Tsuchisaka," describes track-edge dust outside the tunnel,
with a follow-up specifying dry conditions. The report was read only; no reply
or resolution status was posted.

The earlier single-frame Unity assertion sampled input tick 60. Its reverse
capture has zero changed pixels, although the next tick has 131 changed pixels
and 104 pixels shifted toward brown. A first expanded diagnostic at ticks
60/61/90/120 still failed its requirement for two visible samples in each
direction. These failures are preserved under `mirror-smoke-20261111/unity-02`
and `tsuchisaka-dust-followup-20261111/unity-01`; they were not production fixes.

The replacement diagnostic includes the complete five-tick emission interval
(60–64) and retains later samples 90/120. It leaves the driving inputs, particle
strength, render depth and original random sequence unchanged. Both dry
directions must exceed the same visibility threshold at multiple samples.
Night and rain must have no brown source emitters; legitimate white wet-road
spray is allowed. Every case removes and restores particles at the same frozen
physics state and requires exact image restoration.

`tsuchisaka-dust-followup-20261111/unity-02` passes 501 checks and 84 captures.
Forward samples 60–64 change 1,727–2,580 pixels; reverse samples 61–64 change
131–592 pixels. Tick 60 in reverse and ticks 90/120 remain zero, so visibility
is not assumed on every frame. All 28 comparisons restore exactly. Brown
source emitters are absent in night/wet cases. The reverse tick-64 image was
visually reviewed; the mirror is clear. These counts cover the entire frame,
not a claim of smoke appearing in the mirror.

The existing `-idas3-roadside-dust-check` entry point now uses this sequence,
replacing its invalid single-frame and no-wet-particle assumptions. This change
is diagnostic only. Its separate `unity-03` run also passes 501 checks and 84
captures, with identical results for all 28 comparisons. The retained reports,
source/build hashes and protected installation checks are summarized in
`Verification/tsuchisaka-dust-followup-20261111/findings.json`.
It does not reproduce or fix the user's stopped-mirror
report, establish full-lap/tunnel coverage, or show cabinet/online parity.
`D:\Current` remains unchanged.

## Ending flare material/projection reference — November 11 follow-up

The full original flare owner now executes its real material iterator against
the untouched original polygon bank. Both diffuse alpha writes match the source
byte truncation and preserve RGB; all seven chunks are also checked across 205
opacity inputs. The ending projection and 640x480 viewport constants match the
executed original camera setup, including its quantized field of view.

`Verification/ending-flare-20261111/reference-09.log` passes 49,686 cases,
133,770 checks and 17,942,946 original instructions, including 240 complete owner
draw cases and 1,166 submitted pieces. Resource lookup and final GPU/render-state
submission remain boundaries in that earlier run.

The follow-up now executes the original model-submission routine for all seven
chunks under three explicit global TSP settings. Reference11 passes 49,686 cases,
133,945 checks and 18,020,502 instructions, including 21 GPU packets. The original
cursor routine is no longer stubbed. Verified store-queue transfers and final
GPU queue submission remain explicit boundaries; this is not proof of every
scene's previous global state. Reference10 exposed a fixture queue pointer that
needed resetting between submissions; it was corrected in Reference11.

A production ending-only flare owner is now integrated after backfire and before
lamp trails. It preserves the authored geometry/material words and eleven-piece
ordering, applies source road visibility and camera placement, and adds no RNG
consumption. Its recorded path index comes from the bounded path projection,
not accumulated lap progress. Native application01 passes 321,132 ending checks;
paired removal changes 3,029/3,029/6,583 pixels at 960x720/1280x720/1920x1080,
and each restoration is exact. Night, rain, unsupported courses, invalid road
indices, repaint, pause and save/physics isolation are covered. This does not
establish whole-cabinet visual parity.

The first private Unity scripts-only attempt returned success but changed its
packed player data, failing the launcher's provenance assertion. That package is
not accepted as visual verification. A clean private build is being prepared;
That clean non-development build and the follow-up Unity check now pass:
107 checks and nine captures verify geometry/material publication and three
source admission cases. Removing the daylight flare changes 2,971 pixels;
night and wet exclusions change none, and all restorations are exact. The
positive capture was visually reviewed. Evidence is under
`Verification/mirror-smoke-20261111/ending-flare-unity-01/`; its inherited pixel
report's unused `wet` member remains the default and must not be read as the
scenario state. The native scenario/admission checks define the conditions.
No flare update has been installed in `D:\Current` or published.

## Music prompt throughout race setup — November 11 follow-up

The requested held-View-Change music prompt now starts after Mode selection
and remains through course, direction, weather, time and rival selection.
The same native gate permits selecting music on each idle setup page. Hint
visibility is separate from input eligibility so intermediate confirmation
animations do not flash it off. Loading, dialogue, races, results, Mode and
attract exclude it; covered pause/online menus retain their existing routing.
The label still follows the actual bindings (default keyboard C/controller Y).
The formerly reserved final word in the existing 32-byte music-state ABI now
reports hint visibility; its other fields and version remain unchanged.

Private native build75 and Unity scripts build12 passed. The muted player on
an isolated, unshown desktop passed 364 checks with 36 GPU captures. Real
Legend course/rival and Time Attack course/direction/weather/time screens
accept synthetic controller Y through production bindings, reject short taps,
commit songs without restarting menu audio and preserve the underlying page.
The final time-screen selection becomes the next race's active song. Returning
to Mode and entering the race remove the prompt. Pixel checks find the prompt
in all six page pairs and four intermediate confirmation pairs, and no prompt
in the Mode/race controls. Representative actual-window captures were reviewed.
This does not constitute physical-controller or new multiplayer verification.

Evidence: `Verification/music-setup-prompt-20261111/findings.json`, task-only
`change.diff`, and `unity-02/captures`. The initial hidden-window run could not
repaint IMGUI; the corrected test permits drawing only on its isolated desktop,
which is never activated or displayed. Audio remained disabled. `D:\Current`,
the repository Editor plugin, GitHub and Discord were not updated.

## Replay TIME EXTENSION extra pixel row

[Report](https://discord.com/channels/1548169613206884355/1566723556270940190).
The Unity license now permits the prepared GPU comparison to run. The test
uses the real game2d chunk 4 geometry, textures 38/39, negative wrapping UVs,
source blending and the production UI shader. It renders into an offscreen
target without opening a game window or audio device.

The original shader reproduces the extra lettering row with 4x MSAA. At a
partially covered bottom edge, the pixel-center UV lies outside the sprite and
wraps into the last visible texture row. `centroid` interpolation on the UV
keeps the sample inside the covered primitive. The production shader now uses
that qualifier; texture data, wrapping behavior and antialiasing are retained.

Validation on the AMD Radeon RX 9070 XT:

- Direct3D 11 and Direct3D 12: 168 cases per API, 336 total.
- Three scales, seven subpixel offsets, 1x/4x targets, and camera MSAA both off
  and on. Disabling MSAA on the UI camera alone does not prevent the artifact
  on a multisampled render target.
- On each API the original shader has the artifact in 12 cases; the corrected
  shader has none in 84 cases.
- All 42 non-MSAA before/after image pairs per API are byte-identical.
- Representative before/after images were inspected. The tested candidate
  matches the production shader apart from shader name and comments.

The isolated source-sprite render is verified. The complete private Unity build
also compiles this shader. The prepared project,
logs and images are in `Verification/bugs-20261104-followup/`; the comparison
summary and production shader hash are in `Verification/bugs-20261110/ui-proof.json`.

The actual packaged replay viewer now also passes 60 cases: Direct3D 11 and 12,
five resolutions (640x480, 1280x720, 1366x768, 1920x1080 and 1280x800),
1x/4x multisampled render targets, and forward/backward/repeated seeks. The
1280x800 case places the bottom edge at a fractional pixel coordinate, rather
than testing only integer-aligned edges. The diagnostic loads a valid isolated
IDR2 file through the normal replay parser and drives the real native replay
detail/pose, HUD and Unity overlay cameras with the production shader.

Paired captures toggle only the recorded extension flag in memory. Every case
adds the two original sprite quads (12 vertices), has visible text, and has
zero changed pixels on the unwanted bottom row. Hiding the text restores
identical RGB pixels across the whole scene. Representative 4x full-scene
captures were visually inspected. Ordinary save hashes remain unchanged and
all ten muted, hidden player processes exit successfully. This establishes
replay rendering of the reported sprite; the fixture uses a controlled pose
and extension flag, not a newly driven full race or the reporter's own file.

Evidence: `unity-local/replay-ui-{d3d11,d3d12}-*-a/` reports, paired images and
player logs. `run-replay-ui.py` creates the isolated replay and owns each hidden
process. `Idas3ReplayViewer.UiChecks.cs` is opt-in diagnostic code; it does not
alter ordinary replay playback. The script build changes only the private
managed assembly and generated boot configuration; the prior boot configuration
is restored. No native DLL, release or desktop installation is changed here.
`replay-ui-runtime-verification.json` records all 120 image hashes and verifies
that repeated seeks reproduce identical captures. A final guard-only rebuild
passes another six cases with images identical to the earlier 1280x800 run;
attempting to reuse its proof folder is rejected without changing any evidence.
`replay-ui-final-verification.json` binds that check to the final private assembly.

## RPM before the countdown

[Report](https://discord.com/channels/1548169613206884355/1568279372379590766).
Players report faster rev rise/fall during the opening camera sweep and RPM
returning to idle at the countdown. The old App advanced a separate
transmission during the sweep, passed `filtered18` to presentation audio, and
discarded that copy when normal driving started. The normal driving audio path
uses `tach1c`, which includes the original transmission's additional filter.

The real App probe reproduced an audio-control difference of up to 3,412.93 RPM
against complete neutral driving frames. The old copy's tach output matched
those reference frames exactly, but its faster internal value was used for
audio. At the handoff the old audio value dropped from 6,999.31 to 778.736.

The opening sequence now advances the actual neutral driving session and uses
the same live audio instrument as the countdown. Online, the existing shared
input/rollback timeline owns these 241 opening frames. Race rules and the
countdown remain held until the opening ends; neither player can engage a
driving gear early. The first countdown update is still 239 remaining, and GO
still occurs on countdown update 180. Network stalls hold the opening frame
rather than advancing the camera beyond the numerical state.

The retired pose-only path retains its isolated presentation but now uses the
correct tach value for audio. Its countdown handoff is not covered by this
shared-timeline fix. Normal online play uses the shared authority path.

Validation:

- Six actual App cases: AE86, BNR32 and FD3S Type R, dry and wet, with throttle
  press/release/repress during 241 opening frames. Audio/tach values match
  complete neutral driving reference frames exactly. The same engine controller
  advances from frame 241 to 242 without reinitializing.
- AE86 handoff: 6,846.01 to 6,853.68 RPM; BNR32: 8,803.95 to 8,813.71; FD3S Type R:
  7,826.79 to 7,835.41. Each first countdown value exactly matches the next
  reference frame, rather than merely meeting a speed threshold.
- 4,960 online application checks across both local slots, boost Off/On/Off
  rematches, 241 neutral frames, steering/lights, warning artwork, unchanged
  profiles, repaints, and the first countdown frame. Each App advances alongside
  a second numerical peer and compares their complete digests.
- Four original-course cases with the opening enabled: 38,400 confirmed peer
  frames at 50/100/200/250 ms simulated RTT, with/without jitter/loss/duplicates.
- Gunsai both directions: 19,200 confirmed peer frames with the opening enabled,
  including 31 later car-contact frames. No desynchronization.
- Default no-opening numerical callers: another 38,400 confirmed peer frames.
  Existing asymmetric packet-outage/retransmission recovery checks also pass.
- Final native DLL rebuilt successfully. Existing DLL/managed-code hashes in
  matchmaking compatibility keep different implementations out of the same room.

The subsequent Unity pre-race run passes 7,518 checks through the actual
Legend menu, dialogue, opening shots, VS, countdown and driving. Throttle
raises RPM from 764 to approximately 7,000 during the opening. Engine-control
frames advance continuously from 241 at the opening's end to 242 on the first
countdown update and 421 at GO. Neutral gear and the held race/countdown clocks
are checked independently. The VS and countdown captures were inspected.
The previous diagnostic incorrectly required RPM to remain frozen during the
opening; its failed report is retained, and the revised diagnostic checks the
intended neutral-engine behavior instead.

These are native application, local Unity and simulated-network tests, not a
two-PC EOS test. Logs, per-frame RPM traces, network CSVs and build output are
under `Verification/bugs-20261110/`. `D:\Current` is unchanged.

## Legend story-cycle reset

Report: [Doesn't reset tracks after beating game](https://discord.com/channels/1548169613206884355/1569178256265969776),
eVenom, November 8. After completing the rivals, subsequent races remain wet
instead of starting another story cycle while retaining the car.

The port recorded rival wins and played the ending, but omitted the progress
normalization performed when the arcade reads the card again. The source
`1AABE4..1AAD50` rebuilds the course selection defaults, then clears all 31
rival win/loss bytes once all nine courses are clear and Bunta's final encounter
has a result. It increments the completed-cycle counter to a maximum of seven
and retains the unlocked rivals. This is a card-load operation, not an ending
animation or per-race mutation. The original condition does not require two
wins against every rival; the final Bunta result can be a win or loss.

`refreshOriginalLegendCardProgress` now implements that source block. The App
calls it when accepting a saved driver or confirming a changed car. It does
not run during save-list/car previews, raw profile reads, online lookups, or
Full Tune selection. Existing completed profiles qualify without a save-file
format change. All fields outside the original progress block are retained.

The added 1,100 source-reference cases execute the complete block and its
original callees with zero hooks and compare all 1,228 profile bytes. The full
progress suite passes 36,422 cases, 11,260,590 exact comparisons and 17,169,522
original instructions.

The native App passes 44,265 checks across 67 saved-driver cases, plus the
confirmed-change-car path. These cover cycles 0/1/6/7, one/two/saturated wins,
Bunta wins/losses, each of the 31 incomplete-rival boundaries, persistence and
repeat loads, restored dry weather, byte-preserved car/tuning/points/rank,
unaffected other cars and slots, and read-only preview/online lookups. No
window, audio device or online connection is opened by this test.

The ending application regression also passes: 64 checks, all 31 settled
Legend wins and nine course clears through the real post-result owners,
credits and title return. Rendering and skipping the ending leave the profile
unchanged; the new cycle remains owned by the subsequent saved-driver load.
These runs use isolated fixture saves, not the user's saves. Evidence:
`Verification/bugs-20261110/legend-progress-reference.log`, `legend-cycle-app/`
and `ending-app/`.

The private Unity integration test now passes 3,061 checks. It prepares a
completed story in an isolated save, then uses the ordinary Continue menu
twice. The first load persists one cycle increment and clears all 31 rival
results; the second load does not increment again. The test compares every
profile byte outside the source progress fields, plus the other cars and
save slots, then selects Legend/Myogi/Iggy and reaches an actual dry race.
The rival menu and race captures were inspected: both show DAY/DRY and the
reset rival selection is Race 1. Evidence: `unity-local/legend-cycle-v3/`.

Two earlier integration attempts are retained. The first test moved left from
the source menu's default Legend selection into Bunta Challenge. The second
tried to capture rival selection during its black entry fade. Correcting the
diagnostic navigation and waiting for the existing fade required no additional
production changes.

## Complete private Unity build and menu regression

The full Windows Unity build succeeded with a clean build cache. Its staging
manifest preserves all 44 handling-source hashes and excludes ordinary saves.
Subsequent scripts-only rebuilds update only diagnostic C# code; inventories
confirm unchanged StreamingAssets metadata and unchanged content for native
DLLs, player data and other assemblies. Assembly-CSharp and the generated
boot build GUID are the only content changes in the later Legend rebuilds.
The native DLL used by those Unity runs has SHA-256
`653fd6cd39171c086edbd74df908f046d88f7dc612c90d6ca07fab7e600466c6`.

- Saved Data/Change Car: 3,655 checks, 44 captures. Ordinary pointer/controller
  input covers all 35 cars and seven manufacturers, per-car transmission and
  tuning retention, preview/cancel, and isolated save deletion behavior.
- Finish/mode flow: 78 checks, 16 captures. Actual native finish/timeout and
  finish-music routing, plus controlled original post-race fixtures.
- Pre-race RPM and Legend rollover pass as detailed above. Every run reports
  clean shutdown and muted output before scene initialization.

All players ran on unshown private Windows desktops with isolated fixture
saves and no public online session. These results do not establish physical
wheel compatibility, real VPN behavior, two-PC EOS connectivity or FPS gains.
The package contains a locally supplied game image for private testing and is
not a distributable release. Evidence: `Verification/bugs-20261110/unity-local/`.

## Tsuchisaka dust investigation

[Report](https://discord.com/channels/1548169613206884355/1566454363650523288):
missing roadside dust on dry Tsuchisaka, outside the tunnel. The reported build
had host tire smoke but no implementation of this distinct brown dust effect.

The original `060B38..060C76` branch is now lifted into
`original_roadside_dust.h`. It requires course 7, day and dry; the wheel contact
byte's `0x20` bit admits dust. Source effect indices 0..3 read physical wheels
3,2,1,0. Allocation starts a kind-2 emitter with ARGB `ff8b5439`.
The parent supplies display gain 1 for the player or 0.2 for the rival;
this is separate from the motion scale computed by attachment. The original
isolated admission test covered only the rival gain; both are now verified.
An existing emitter is retained; clearing the wheel flag stops kind 2 and
clears its slot. Allocation failure still suppresses the tire-smoke branch.
The low-nibble 6/7 test at `060964` belongs to a different smoke condition.

The later dust update at `060F00..060FC2` is also verified: the actor's source
displacement magnitude must exceed 0.1 to give particles a nonzero lifetime.
Effect indices 0/1 receive lifetime scale 0.3, indices 2/3 receive 0.5; slower
movement receives zero. This threshold is in source tick units, not host km/h.
There is no slip requirement in these dust branches.

`original_smoke_particle.h` lifts particle initialization (`13D780`) and motion
(`13D800`), including all three authored parameter tables, random force,
gravity, drag, signed lifetime, size limits and one-frame size overshoot.
`original_smoke_emitter.h` now lifts the `13E240` emitter controller: updates
existing particles before emission, returns removed particles to the shared
budget, preserves the source phase and modulo-5 schedule, and drains after
`13E940` requests a stop. It also reproduces the particle-relevant attachment
outputs from `13E160`/`156340`: displacement velocity, magnitude and retained
basis below the source threshold. It does not yet port the complete matrix.

Particle expiry deliberately clears the phase without immediately clearing
its live marker. The source display preparation (`13FCAE..13FCCC`) clears that
marker, and the following emitter tick removes the particle. Tests cover this
handoff over complete multi-tick runs; moving removal into the motion routine
would change the original lifetime behavior.

The admission, particle and emitter reference suites pass through CTest:

- Admission/update: 589,856 cases, 2,375,872 comparisons, 10,744,904 original
  instructions. Allocation, start/stop/update calls and the matrix copy are
  explicitly observed dependency boundaries.
- Particle: 28,288 cases, 311,635 bitwise checks, 1,104,232 original instructions.
  Only random values are supplied at a dependency boundary.
- Emitter/attachment/display cleanup: 43,144 states, 1,478,992 bitwise checks,
  7,817,133 original instructions. Hooks cover random values, exception TLS,
  allocation/object setup, intrusive-list bookkeeping and display-block exits.
  Attachment math executes the original helpers without hooks.

The canonical image SHA-256 is
`efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`.
These are instruction-reference comparisons under the existing finite-F32
reference model, not a claim of a complete hardware or visual comparison.

The course checker samples 21 lateral positions at every route point in both
collision variants: 78,309 successful contacts per variant. Both variants pass
2,751 lane probes through interior route indices 2420..2550 without a dust
flag, with four dust lanes at each outside sample (2380 and 2600). The generic
roof classifier also reports some dusty contacts under other geometry and near
the portals; that is not a reason to add a blanket shelter prohibition.
The original collision flags already distinguish the tested tunnel interior.

Evidence is under `Verification/bugs-20261110/`: `roadside-dust-ctest.log`,
`roadside-dust-cmake-build-final.log`, `smoke-emitter-attachment-reference.log`,
`roadside-dust-contacts-verified.csv/.log`, and `ground-effect-*`/
`roadside-dust-*.asm`. The source RNG at `1F9EA0` consumes the shared `37C778`
LCG; integration still needs an explicit decision about effect scheduling and
RNG ownership without changing unrelated driving behavior.

The display stages are now lifted and verified separately:

- `original_smoke_display.h`: source `13FC00` projection, clipping, overlapping
  particle accumulation into the shared density grid, and density clamp.
  **864 cases / 343,240 comparisons / 12,612,877 original instructions** cover
  the original 128x128 kernel, all three parameter records, 16/20/32 grids,
  expiry, offscreen/behind/near particles and overlapping clouds. Camera
  transforms, sphere visibility, footprint depth and final UV application are
  explicit boundaries in this suite.
- `original_smoke_depth.h`: `140BC0` selects near/far cloud depth by the four
  transformed car-footprint corners and the source wheel ordering. **4,096
  cases / 122,881 checks / 4,899,735 original instructions** include rotations,
  sloped/scaled bases, zero-size footprints and both outcomes. Only global
  matrix-stack storage is hooked; source transforms and arithmetic execute.
- `original_smoke_mesh.h`: `0D1640`, `0D1E60`, `0D19A0` and `1401E0` grid
  construction, UV-derived vertex mapping, density application, draw placement
  and material/packet mutations. The source writes **U=0.9, V=density**, using
  original texture 3; density is not direct vertex alpha. The native copy
  retains the authored geometry, strip topology, normals and palette binding.
  **384 draws / 2,982,417 checks / 23,482,360 original instructions** compare
  real HOSTFS grids, exported runtime models, every original kernel nibble,
  negative UV origins and white/brown/zero/random color masks. Source packet
  iteration executes. Hooks cover allocation/base-object setup, exception TLS,
  matrix submission, model-handle lookup and GPU submission.

`export_original_smoke_grids.py` exports the three source grids to
`effects/smoke/smoke_grids.idasmesh` (257,048 bytes), with an accompanying hash
manifest. The 32x32 source includes small fractional V residues; export keeps
them intact, and runtime mapping uses the source's truncation. The density
kernel is recovered exactly from the existing texture bank's four-bit green
channel; no extra texture copy or texture-bank change is needed.

All **six** suites pass together in `roadside-dust-display-ctest.log`. Additional
evidence is `smoke-mesh-assets-reference.log`, `smoke-depth-reference.log`,
`smoke-grids-export.log` and `smoke-grid-preview/comparison.png`. The optional
preview run adds three visible-pixel checks (2,982,420 total). Its original
material software previews were inspected: all three grids produce a bounded,
faint brown cloud over the background. These are not packaged race captures
or proof of GPU/filtering/occlusion parity.

The car-owner boundary is now implemented in `original_roadside_dust_owner.h`:

- `06088C..060964` selects the player/rival, its four-slot base and display
  gain, and stops the opposite owner's slots. Tests cover all slot occupancy
  masks, both gain paths, nonboolean flag bytes and unsigned mode boundaries.
- `028940` uses authored wheel translations 14/15, mirrored in X for the
  second wheel of each axle. The new `CarPresentation::originalEffectOrigins`
  exposes those exact points, retaining authored Y and excluding cosmetic
  wheel offsets. Existing host tire-smoke collision probes remain separate.
- `0609CC..060AA0` computes actor displacement magnitude, copies the car's
  footprint, then transforms each wheel using `T(0,0.1,0) * carWorld`.
  **The lift is world-up, not car-local up, and the footprint stays unlifted.**
  Earlier investigation notes had misread `1F64A0` as a matrix store; it
  multiplies XF. The new test executes the actual getter, stack and math.
- `startOriginalCarSmokeEmitter` reproduces `140660`'s particle-relevant
  start stores, including phase 3, zero frame/stop/velocity and the initial
  basis. Display gain does not overwrite motion scale/force. The same test
  then executes `140980` and compares attachment fields plus the 64-byte
  unlifted footprint copy. View allocation is excluded from this boundary.

The combined owner suite passes **6,720 wheel/start/attachment chains, 640
owner selections, 662,080 bitwise checks and 5,425,032 original instructions**.
Geometry and matrix math have no hooks; parent stop requests and start TLS
are observed boundaries. All 35 authored cars are tested on identity,
rotated/sloping/scaled bases, stationary and moving actors.

The car-presentation regression also passes all 181 factory appearances,
31,856 dynamic frames and 1,914 authored tuned profiles, including exact mount
checks through the real profile/legacy loaders. Seven dust suites pass after
the start helper change. Evidence: `dust-owner-start-reference.log`,
`dust-owner-integration-tests.log`, `dust-owner-final-ctest.log`,
`dust-start-build.log`, and `roadside-dust-owner-verification.json` under
`Verification/bugs-20261110/`. The presentation build retains two pre-existing
local-variable shadow warnings in `car_presentation.cpp`; none is in the new
mount accessor. No game executable was launched for these native checks.

### Race integration and visible-pixel verification

The dust effect is now wired into accepted offline and authoritative online
simulation ticks. It uses published source wheel flags/displacements, the
authored wheel mounts and raw ACar matrix. Profile offset 1180 bit 7 supplies
the player-owner preference (`05BC8C`); solo and two-car presentation modes use
the original owner selection. Legacy remote poses without contact data do not
invent dust. Only admitted wheels suppress the existing generic tire smoke.

`OriginalRoadsideDustSystem` retains the original four-emitter/60-particle
budgets, stop/drain/reuse ordering and once-per-tick display cleanup. Its full
parent/manager reference suite executes **3,840 ticks, 481,640 bitwise checks
and 16,690,581 original instructions**, including allocation and list boundary
hooks. Camera setup/frustum checks execute **65,536 visibility cases, 73,216
checks and 6,222,664 original instructions**. Geometry and particle math remain
source-derived; the host uses an independent deterministic presentation RNG
so adding a visual effect cannot consume driving/audio randomness. This does
not establish original shared-RNG trajectory parity.

The real renderer receives the recovered density grid, palette UVs and source
source-alpha/destination-alpha blending, with separate main/rear view meshes.
An explicit horizontal-clip adaptation supports wider host screens. The
renderer suite passes **10,959 checks**, including 12,689 changed pixels and
11,330 brown-contribution pixels against its controlled backdrop. Ten dust
reference/presentation suites pass (`dust-integration-ctest.log`).

The actual App race test now passes **2,193 checks** using solver-produced
contacts in both directions and excluded night/rain conditions. At tick 60,
the full race comparison changes 2,057 pixels forward and 501 reverse; 1,539
and 430 respectively shift toward the source brown tint. Repaints preserve
particle counts and the driving rollback digest. Source destination-alpha
blending can darken a green shoulder while adding brown, so an all-positive
RGB-delta assertion is inappropriate on real scenery.

The earlier failed race attempts are retained. They placed the car at path
100, which is a barrier in the forward race's collision file. Tsuchisaka
intentionally reverses collision-file suffixes. Path 500 is open in both
directions and supplies the source dust contacts; no physics or collision
data was changed. An early low-speed capture contained no visible contribution,
so the application check now requires visible pixels after particle growth,
rather than treating allocation as successful rendering.

Evidence includes `dust-app-run-{a..j}`, `dust-app-shared-fixture`, their logs,
`dust-renderer/`, `dust-plugin-build.log` and the shared diagnostic fixture
`Native/tests/roadside_dust_race_fixture.inl`. Private Unity build/staging
evidence is under `unity-local/dust-*`.

The private packaged Unity player now passes **61 checks** and exits with
code 0 after complete shutdown. Forward/reverse captures contain 2,059/500
changed pixels, including 1,572/427 pixels shifted toward brown. Night and
rain each contain zero changed pixels. Restoring the same native dust state
reproduces identical Unity RGB pixels in all four cases. The captures were
visually inspected. The first run caught a diagnostic-helper mistake: capture
resumed the race, invalidating the paired comparison. The dust-only diagnostic
now stays frozen across captures; ordinary gameplay and inspection are not
changed by that correction. Failed evidence remains under `dust-runtime-check`;
the passing run is `dust-runtime-check-v2`.

This was a private scripts-only build refresh, new native DLL and two smoke
grid files, with previous boot configuration restored. The inventory confirms
only the managed assembly and boot configuration changed during each script
build; explicit native/data staging is separately recorded. Native DLL SHA256:
`701cf681d07e8720560ddb9038d59cf6c90aaa39c371099d9b67b0d30c9859a0`.
All game tests were muted on an unshown desktop or rendered offscreen without
an audio device. This is not a fresh release build or desktop update.

**Remaining fidelity boundaries:** the generic host tire-smoke system does
not yet share the source dust manager's budget/RNG; live two-PC online visual
behavior and replay playback are unverified. Inspection remains unchanged.
No files under `D:\Current` were changed.

### Existing replay dust data audit

The read-only `replay_dust_contact_audit` ran 5,728 frames across eight arranged
Tsuchisaka positions in both directions. Re-querying wheel surfaces from the
current recorded pose disagrees with source dust contacts on 73 frames;
using the previous pose disagrees on two. Using the previous position/yaw
with the earlier pitch/roll matches these sampled contacts. However,
recorded-position differences disagree bitwise with the source pre-contact
displacement on 5,649 frames. The source publishes lagged contact flags and
computes displacement before road-height correction; IDR2 stores the resulting
pose, not those source inputs or the smoke manager's state/RNG.

This does not prove pose reconstruction works on every surface, and bitwise
differences alone do not establish the visual size of an error. It does show
that simply subtracting recorded positions is not exact source replay data.
No guessed contacts, new replay format, playback behavior or race physics
were introduced. Faithful replay dust remains open. Evidence:
`replay-dust-contact-audit/result.txt`, its per-frame CSV, and build/run logs.

### Recorded roadside dust and replay seeking (November 11)

The earlier data audit led to an explicit recording extension rather than
guessing contacts from poses. Newly recorded dry-day Tsuchisaka races now
retain the actual dust owner's wheel flags, pre-contact displacement magnitude,
attachment positions and footprint matrix for every accepted effect tick.
The source-owner prefix before the first running replay frame is retained too.
These values reproduce the current live dust presentation; recording never
queries a different surface or advances the driving solver.

Only enabled local archives on this course/weather use IDR2 revision 2. Its
96-byte header carries the dust-input count at byte 92; each 164-byte pose ends
with its consumed dust-input count. The appended records are 124 bytes each.
Other recordings retain revision 1's 160-byte frames. The reader keeps IDR1 and
IDR2 revision 1 support, validates revision 2's owner, finite inputs, sizes and
monotonic clock, and keeps the existing compressed wrapper. Existing recordings
have no exact source dust data and are not retroactively given invented dust.
Community Times still submits times only; replay uploads remain disabled.

Playback reconstructs every recorded effect tick and keeps particle/RNG
checkpoints for backward or distant seeks. The timeline belongs to the recorded
world when switching drivers. Ordinary repaints do not spawn particles, and the
viewer reuses its existing pose samples instead of adding another per-frame
sample allocation. The explicit replay render exclusion was removed.

Evidence lives in `Verification/replay-dust-20261111`:

- `before-run.log` reproduces the missing timeline before the change.
- `after-run-04` captures 720 running frames plus 60 owner-prefix ticks in each
  direction, compares live particle/manager/RNG state with decoded playback
  across 1,942 sequential/random/backward samples, rejects invalid native
  timeline loads/seeks, and checks excluded night/rain and disabled recording.
  Its replay render has 1,583 changed pixels; restored source/playback pixels
  match, and the driving-session digest is unchanged.
- `native-regressions.log`: race, original dust owner, original dust system,
  and dust renderer tests all pass.
- `managed-03.json`: 11,434 checks using the actual Unity reader/codec, including
  legacy and compressed data, both battle tracks, render-rate sampling and
  malformed/truncated input. The full managed project compiles.
- `viewer-02-forward` and `viewer-02-reverse` pass six actual packaged-viewer
  captures each, including 1x/4x MSAA and backward/repeated seeks. Visible
  contributions reach 2,111 and 3,811 pixels respectively; restored RGB matches.
- `viewer-03-pov` uses two explicitly constructed telemetry tracks to exercise
  both driver perspectives: 12 capture comparisons pass, all restored RGB
  matches. This verifies viewer perspective switching, not live online capture.
- `viewer-04-legacy` opens a revision 1 recording with the same current native
  and managed binaries and renders four samples successfully.

The initial prefix fixture incorrectly reset its visible clock while retaining
running HUD time; the final fixture advances only the effect owner before race
clock capture. The first two-car fixture also named an AI preset incompatible
with its recorded car. Those failed attempts remain in the evidence directory;
the final custom-car/online-telemetry fixture uses valid metadata. Neither
failure was resolved by weakening the production reader's validation.

Tests use offscreen native rendering or muted players/editors on isolated
Windows desktops. The existing private player was rebuilt with the new scripts
and native library; its resource archive is unchanged. `D:\Current` and inspection
remain unchanged. No release, upload or Discord action was performed. Real
two-PC capture and the original shared tire-smoke/dust manager's budget/RNG
remain separate fidelity boundaries; this change preserves the current live
dust behavior, not an unverified claim of complete cabinet-effect parity.

### Shared tire-smoke and dust controller recovery (November 11)

The original `060860` controller uses one four-emitter/60-particle manager for
white tire smoke and brown roadside dust. At the start of this stage, the game combined
the recovered dust controller with the older host tire-smoke approximation.
That approximation uses absolute slip and a render-time emission interval;
the original consumes the driving actor's four intensity bytes at `+84..87`.
It also has a wet-road branch, whereas the host effect is disabled in rain.

`original_car_smoke_system.h` now recovers the complete shared controller,
including source intensity, all nine course multipliers, rear-only rain
emission, snow lifetime selection, surface exclusions, owner switching and
white/brown transitions. Clouds that are still draining retain their budget.
The order matters: a live white cloud can prevent dust allocation, then be
stopped later in the same parent update. The new model preserves that delay
instead of immediately recoloring or replacing the cloud.

At this recovery milestone, the model was **not yet connected to normal gameplay or replay recording**.
Its renderer shares the existing authored density-grid path through
`RoadsideDustPresentation::appendSystem`; the existing dust-only entry point
delegates to that same path. The legacy IDR2 revision 2 behavior is preserved.

Evidence under `Verification/car-smoke-20261111/`:

- `run-02.log`: **69,120** full original-parent/manager ticks, **11,840,329**
  bitwise checks and **375,359,686** original instructions. The original RNG
  executes without a replacement hook. The comparisons include deliberately
  interleaved other-owner RNG calls, dry/wet/night/snow cases, both particle
  parameter modes, mixed white/brown clouds, full pools and complete drain.
  Allocation, generic object/list infrastructure, TLS and display bookkeeping
  remain explicit fixture boundaries. This does not prove full-scene RNG
  scheduling or hardware behavior.
- `application-03`: **2,400** actual driving ticks and **6,115** checks. The
  candidate controller reads the published actor without changing its contact
  or intensity values. Driving digests remain unchanged. Four offscreen images
  show forward/reverse brown dust, night braking smoke and wet-road spray,
  with **3,370 / 2,765 / 370 / 2,663** changed RGB pixels. Twenty repeated draws
  per image produce identical geometry without consuming simulation/RNG state.
  These use an isolated effect backdrop, not a full-scene cabinet comparison.
- `regressions.log`: all four targeted particle/emitter/controller tests pass.
  `renderer-regression.log` retains **10,959** checks and **11,330** brown
  contribution pixels. `replay-regression.log` retains **693,180** checks and
  **1,583** changed pixels with exact source/seeked restoration.

The first application fixture tried to use an uninitialized game texture bank;
the isolated renderer now explicitly loads its own original smoke textures.
The second reused the dust-only shoulder drive at night, which produced zero
source tire-smoke intensity. The final night case follows the road and brakes
through actual input. Neither failure was addressed by forcing effect inputs
or weakening the visibility checks. Failed evidence is retained.

Next integration work must move the full controller into accepted race ticks,
preserve shared state through online rollback, record the additional source
intensity/scene inputs, and retain compatibility with older local recordings.
The complete scene's shared RNG schedule still needs verification. Tests were
offscreen with no audio device. No player package, desktop installation,
release, inspection flow or Discord report state changed in this stage.

### Shared smoke integration and rollback (November 11)

The normal original-course race path now uses the recovered combined controller
for tire smoke, wet-road white effects and Tsuchisaka dust. `CarSmokeState`
owns its bounded particles, private visual RNG and accepted-tick count. The
source intensity/contact bytes and authored wheel origins are bound directly
from published actors. Host interpolation and peer correction offsets do not
feed the effect simulation. The older host slip-triggered smoke is suppressed
in these live races; its existing road marks remain separate.

`OnlineRaceSimulation` now keeps both observer views inside its checkpoint.
Each view preserves the source player/rival selector and gain. Rollback restores
particles, draining emitters, budgets, owner slots and RNG together. The App
renders the local view without advancing it again. The copied dynamic memory
is included in checkpoint/history accounting. Numerical network digests and
physics/audio RNG are unchanged; this stage does not claim the complete arcade
scene's shared-RNG schedule has been recovered.

Evidence under `Verification/car-smoke-integration-20261111/`:

- `physics-before.txt` was captured by `baseline.exe` linked against the
  pre-integration native library, before rebuilding it. `online-01.log` compares
  **3,600** subsequent frames against those digests. All match, including engine
  and contact/audio state. **39,565** checks cover source pools, day/night/rain/
  snow, view ownership, immediate checkpoint restoration, replayed ticks and
  **321** late-input rollbacks. Peak measured two-peer history is **5,113,664**
  bytes, including the new visual state.
- `network.log` / `network.csv`: the existing production wire/timeline test
  passes **172,800** verified peer frames across all 18 original course
  directions, 50/100/200/250 ms latency and jitter/loss variants. The suite also
  checks packet outages, duplicate/reordered packets, confirmed audio/contact
  dispatch and race cleanup. This is a virtual network, not an EOS field test.
- `application-04`: **2,400** offline live-input ticks plus **960** online App
  ticks and bounded stalls. Offline state matches independently bound source
  actors on every tick. Full-scene on/off/restored comparisons show
  **2,485 / 1,341 / 26 / 119** changed RGB pixels for forward dust, reverse
  dust, night cornering smoke and rain. Actual online App views show
  **251 / 1,151** changed pixels for local slots 0 and 1. Restored images are
  identical. Rendering consumes no smoke RNG/ticks; speculative corrections
  match a complete-input baseline; stopped packet delivery and repeated draws
  cannot keep emitting smoke. Leaving the room releases the online owner.
- `legacy-replay.log`: **693,180** existing revision-2 replay checks and
  **1,583** visible pixels still pass, including backward/exact seeking.
  `dust-regression.log`: **2,193** existing App dust checks pass.

The first integrated fixture incorrectly required the earliest tiny braking
particles to be visible through the car. Waiting longer under the same mild
braking inputs still produced no visible chase-view cloud. The final test adds
an ordinary 20-tick sharp steering input and observes actual source intensity;
no contacts, intensity bytes, depth settings, cloud sizes or brightness were
forced. Failed runs remain as `application-01` and `application-02`.

**At that milestone:** new recordings retained revision 2's separate
dust-only timeline. They did not yet record/reconstruct the combined controller,
its extra intensity/scene inputs or corrected online history. Existing legacy
playback deliberately retained its previous renderer behavior. The complete
scene's shared RNG ordering, packaged-player verification of this integration,
and real two-PC EOS capture remain open. Imported tracks retain the previous
host effect path. Tests used offscreen native rendering without an audio device.
The private Unity player, `D:\Current`, inspection flow and Discord were not
changed; no release was published.

### Combined smoke recording and replay seeking (November 11)

Ordinary offline/Legend recordings on original courses now save the combined
source smoke/dust controller's inputs. IDR2 revision 3 retains the existing
detailed pose layout and adds a 136-byte effect input for each accepted source
tick: the selected owner's geometry/contacts, four intensity bytes, source scene
decisions and RNG immediately before the tick. The pre-driving prefix is retained.
Checkpoints every 300 effect ticks restore the entire controller when seeking;
repeated painting does not advance it. The world effect timeline remains attached
to the root recording when switching opponent view.

Revision 1 pose-only and revision 2 dust-only recordings retain their previous
playback paths. Revision 3 rejects malformed input, impossible scene/owner
combinations, mismatched race metadata, inconsistent clocks and duplicate world
timelines. The maximum allowed detailed recording including its source prefix is
32,539,360 raw bytes; the bounded codec allowance is 34,000,000 bytes. This affects
local recordings only. Leaderboards still receive times, not replay uploads.

At this checkpoint, `OnlineRaceFrame` exposed both views' smoke inputs, with
rollback regenerating them and confirmed callbacks delivering them once. Online
App recording still used presentation poses. The next section addresses that
remaining boundary; saving predicted poses with confirmed particles would
reproduce the wrong world.

Evidence under `Verification/replay-car-smoke-20261111/`:

- `native-01.log`: **173,271 checks**, **72 scenarios / 56,232 source ticks**,
  all nine original courses, both weather/day-night settings and both parameter
  presets. Exact state comparisons cover owner changes, draining, RNG inputs,
  pre-driving prefixes, checkpoints, repeated/backward/random seeks, validation
  failure atomicity and legacy writers. **1,406** ticks contain both white and
  brown particles.
- `online-01.log`: **79,723 checks**, **3,600 canonical frames**, **321 rollbacks**.
  Confirmed output on both peers contains exactly one corrected input per frame;
  reconstruction matches the source particles and RNG. A full prediction window
  with missing remote packets does not add confirmed recording inputs. Existing
  pre-integration physics/audio digests remain unchanged. Peak measured combined
  history for two peers is **5,149,504 bytes**. This proves the simulation output
  boundary, not saved online App recordings or real EOS connectivity.
- `application-04.log`: **2,767,793 checks**, **3,120 actual race source ticks**.
  Four ordinary captures cover day forward/reverse, night cornering and wet-road
  effects. Native source/restored full-scene images are identical; removing the
  timeline changes **1,889 / 2,241 / 114 / 865** RGB pixels, independently checked
  from the saved bitmaps. The native log's 32-bit comparison includes a few
  additional alpha-only pixels (**1,897 / 2,249 / 119 / 865**). The tests verify
  recording, serialization, every source state, seeking and no host-smoke double
  rendering. `legacy-native-02.log` adds **693,180** revision-2 checks.
- `managed-05.json`: **206,001 checks**, **36 generated scene combinations**
  plus the **four actual race captures**, including compressed parsing, 30/60/
  144/240 Hz sampling, malformed input and opponent-view world ownership.
  `managed-legacy-05.json` passes **11,434** older-format checks.
- `viewers-02.json`: **42 packaged Unity captures** across day forward/reverse,
  night, wet road, opponent-view switching and old revision-2 playback, with
  one- and four-sample rendering. Every restored image matches its original
  pixels. Each scenario has a visible effect contribution. Night and wet tests
  use the native fixture's unobstructed camera through an explicit diagnostic
  flag; normal viewer cameras are unchanged. These are self-consistency checks,
  not a claim of cross-renderer or cabinet pixel identity.

Earlier failed harness attempts are retained. `managed-01` used outdated legacy
fixtures, and `managed-02` pointed to a nonexistent input directory; current
audits use freshly generated legacy recordings. The first actual night capture
did not exercise emitting cornering, so the fixture now uses ordinary steering
and brake inputs. `application-02`/`application-03` exposed an obstructed test
view, including a camera inside the hillside. The final native and Unity tests
use the live fixture camera. No effect intensity, contact, brightness, size or
depth rules were altered to make the images pass.

`findings.json` records scope and file identities. `private-player-02.json`
identifies the private updated player; its resource archive is unchanged. The
temporary Editor plugin swap was restored to its exact preceding bytes. All
player runs were muted before scene load on hidden desktops. `D:\Current`,
inspection, Discord report state and GitHub releases remain unchanged.

**Still required after the following online integration:** complete original
scene shared-RNG ordering and real two-PC EOS capture. Imported tracks retain
their previous host effect path. These results do not close the broader smoke
fidelity or networking reports.

### Confirmed online recording (November 11)

Version-2 online recordings now save numerical poses and telemetry from each
confirmed simulation frame. A bounded 65-entry history retains the matching
local presentation lamp state until confirmation. Private body queries and
feedback timers reconstruct body placement and HUD state without feeding back
into physics. Original courses also retain the complete confirmed smoke/dust
prefix before GO. Imported courses keep their pose-only format and existing
presentation treatment. Finished/timeout recordings close on the confirmed
result frame, and the losing car is not given a fabricated finish timestamp.

The tests exposed a separate lamp defect: the old remote-pose adapter replaced
a fixed headlight's full-brightness fraction of 1 with 1/40. Shared-input online
races now retain the full source lamp state; the legacy adapter remains for
legacy connections. Showcase lamp sampling uses a pure next-state preview and
does not advance the live motor twice.

Evidence: `Verification/online-replay-confirmed-20261111/`.

- `application-07.log`: **1,481,325 checks**, **12 actual App races**,
  **414 rollbacks** and **105,050 repeated pose checks**. Both player slots cover
  original day/reverse-night/wet cases and Sadamine, Gunsai and Enna. Tests include
  the actual 241-frame showcase caller, all 35 cars' lamp previews plus the FD
  fixed-headlight variant, delayed/duplicate/reordered packets, a 64-tick outage,
  prediction stalls, recovery, publication and room cleanup. **1,460 displayed
  positions differed from canonical positions** while saved poses remained exact.
  Four seeded source-rule boundaries cover local/remote wins and timeout; these
  are finish-boundary tests, not full-lap or Internet-match tests. Sadamine body
  placement is checked against its road plane; Gunsai checks finite presentation
  and unchanged horizontal placement, not full filtered-motion equivalence.
- `physics-regression-01.log`: **79,723 checks**, **321 rollbacks**, unchanged
  prior physics/audio digests. Combined two-peer history is **5,192,512 bytes**
  after including immutable replay poses. `offline-regression-01.log` retains
  **2,767,793 checks** and four exact source/restored native scenes.
- `managed-01/report.json`: **120,925 checks**, **12 actual archives / 24 tracks**
  through production compression, atomic library save and reload. Both raw
  streams remain byte-identical; sampling at 30/60/144/240 Hz is unchanged.
- `viewers-03.json`: **72 private Unity captures** across six paired original-
  course recordings, both viewpoints, and one/four samples. Forward/backward
  seeking restores identical pixels; every recording has a visible effect
  contribution. Some opponent views obscure the root car's particles. The
  source timeline remains root-owned. Captured prefixes were independently
  checked byte-identical against the final `application-07` files.

Failed attempts are retained. `application-01/02` exposed the lamp defect;
`application-03` did not enable the intended showcase fixture and was replaced.
The first viewer envelope omitted the opponent byte count normally added by the
managed library; the fixture writer was corrected without weakening the parser.
`viewer-02` sampled straight-driving/drained-effect moments. The explicit audit
now accepts validated sample times selected from recorded tire intensity, while
the original recordings, normal cameras and particle parameters remain unchanged.

`findings.json` records hashes, limits and evidence. `private-player-03.json`
identifies the private candidate; its resource archive is unchanged and the
temporary Editor plugin swap was restored exactly. Tests were muted and hidden
or used an offscreen renderer with no audio device. No desktop build, release,
inspection behavior or Discord report state was changed. Complete original
scene shared-RNG ordering and real two-PC EOS capture remain unverified.

## Imported analysis maps mirrored against course cards

[Report](https://discord.com/channels/1548169613206884355/1562988497281876038),
Seven, October 22, with an Enna analysis/card comparison and a forwarded
Sadamine example. Both images were read in Discord. The current analysis
projection still used +X/-Z for both courses, while the existing course-card
exporters use -X/-Z for Sadamine and +X/+Z for Enna. Myogi Special Stage, Usui
Special Stage and Momiji use the same card convention as Enna and had the
same vertical mismatch. Tsubaki's earlier correction was already present.

`ImportedCourse::analysisMaps` now uses each existing card's orientation for
the road, all four section zooms, driving traces, wall contacts and ditch
markers. This only changes analysis presentation, not the world geometry,
driving physics, checkpoints, route IDs, records or minimap.

The new regression reproduces 50 orientation failures before the correction
and passes 3,366 checks afterward across nine courses, both directions and
five pages. It checks trace/road agreement, event-marker positions, section
filtering and clipping. Actual map polylines from every installed course pack
were exported and compared visually beside the unchanged course-card images.
All five pages in both directions remain byte-identical for Hakone, Tsubaki,
Gunsai and Odawara. Evidence: `map-before/`, `map-after/`, their logs and
`map-card-comparison.png` under `Verification/bugs-20261110/`.

The rebuilt game DLL succeeds and has SHA-256
`de04ec1687da34187cbe45423660ca71d7c5aaab841931d81256ef9539bec15d`.
An extended run passes 3,468 checks and renders 34 complete native analysis
screens: all nine course overviews in both directions, plus every Enna and
Sadamine section. Representative overview/section screens were inspected.
The existing Time Attack visit regression passes 644 checks in English and
644 in Japanese; telemetry adds 96 passing checks. Logs and renders are in
`map-render/`, `map-ta-en/`, `map-ta-ja/` and the adjacent log files.

The earlier Unity runs above predate this additional map correction. The new
map DLL has not been copied into that private Unity package or `D:\Current`.

## Leaderboard website loses Japanese and symbol names

[Report](https://discord.com/channels/1548169613206884355/1563013392615084032),
Icey?, October 22. The report and attached ranking screenshot were read in
Discord. The screenshot includes names partially or wholly replaced by
diamonds. The website's `nameOf` function only decoded A–Z, digits and space;
all other valid name glyphs became the same placeholder. Submitted and stored
numeric glyph IDs were intact.

The local website now decodes all 221 original name-entry characters, with
the same fullwidth-ASCII/space folding as `OriginalVsBanner::decodeName`.
Padding ID 221 remains omitted. Public and admin run tables share this
decoder and continue inserting names through `textContent`, including angle
brackets. No score, identity, database schema or caching changes are needed.

The regression fails against the old website and passes after the correction.
All 221 characters match both the original EUC-JP name-entry records and the
game's exported Unicode table. Tests also cover Japanese, symbol-only and
mixed names, existing numeric order, padding, and both table render paths.
All 133 backend tests pass. Evidence: `name-glyphs-before.log`,
`name-glyphs-backend.log` and `name-glyphs-verification.json` under
`Verification/bugs-20261110/`.

A local fixture server was prepared for a visual browser check, but local
browser use is disabled in Work settings. No visual-browser pass is claimed;
the temporary server was stopped. The website correction is local and has
not been deployed.

## Car inspection / changing an existing tune

**Historical implementation, superseded below by "Inspection screens only."
The owner explicitly rejected changes to the car or saved data.**

[Report](https://discord.com/channels/1548169613206884355/1560410321779294338),
JDR99, October 15. The opening post and all 15 replies were read. The request
is to expose the original inspection flow for changing an existing car's
transmission/tuning package; the reporter additionally proposes applying all
new parts through the existing Full Tune convenience. Replies explicitly
retain this as a missing-original-feature report. The linked demonstration
video, `6qHW-1yGBZg` (155 seconds), has now been downloaded without audio and
inspected through sampled frames. It shows the inspection notice/confirmation,
transmission, current-versus-changed tuning option, a short Saved Data summary
with an Inspection Complete overlay, then Mode. The second pass changes A to
D and subsequently buys parts again after a race. The ordinary eight-page
Saved Data review is not the same as this inspection-completion page.

The initial source audit identified an integration gap: `acceptOriginalDriverCard` was
only called by tests, not the game App. Normal accepted profiles explicitly
clear byte 1192 (renewal state), and `needsFullTuneCourseSelection` only
permits package selection before any parts/progress exist. The original
tuning-course state machine already implements renewal changes when byte
1192 is 1: changing a package resets the relevant body/basic-tuning fields
while retaining earned engine level. Existing source-reference and Full Tune
tests exercise that operation, but do not make it reachable from the player
menu.

The normal `iSelRenewCardCheck::Main` path at `10ECC0..10EE16` is now ported
beside the existing card-check transition. It waits for the card writer's
acknowledgment, blocks confirmation during the first 60 completion ticks,
permits it on tick 61, and clears the completion overlay after tick 150. The
source can still time out while awaiting a physical card write, so the native
host must enter this completion screen only after a durable save succeeds.
Cabinet service-abort input is explicitly outside this port. Executing the
derived owner and common transition against the canonical image passes 42,332
card/configuration/renewal cases, 14,113,080 comparisons and 7,022,289 source
instructions. This tests transitions, not rendering.

`LocalCarInspection` now provides a transaction bound to one saved car and
driver directory. Preparation applies the source tuning commit to the exact
captured disk profile, preserving transient timer/renewal bytes. It writes
only on commit, uses the existing conditional atomic save, rejects stale
profiles, allows retry after I/O failure, and prevents cancelled or completed
visits from later overwriting progress. It does not emulate physical-card
replacement or replenish the physical-card use counter. The original tuning
controller still passes 2,389,468 source comparisons.

The transaction passes 1,549,201 persistence checks over all 35 cars and 1,260
old/new-package and transmission combinations. Tests reopen actual profile
files after saving and check every unrelated profile byte, unchanged-tune
progress, other-car/other-driver isolation, cancellation, invalid selections,
stale-profile conflicts, failed writes and retry, duplicate completion after
a later race, and backup recovery. Filesystem failures are confined to newly
created fixture directories; no real save was used. Evidence:
`inspection-persistence.log` and `inspection-540326381669800/`.

The tuning screen now draws the original CURRENT/CHANGE CUSTOMIZATION OPTION
banner. Tracing `11E630` identified the separately loaded `optcrs` model:
English chunks 0/1, Japanese chunks 1/2. The owner draws it at
`12BB16..12BB74`, translated by -1.28 in Y. The exporter verifies the original
resource pointers, and the Japanese import maps the two reviewed labels.
The source-reference test now covers both inspection kind 1 and imported-card
kind 2, across all 35 cars and 144 animation frames. It executes the actual
parent banner branch as well as the existing child draw routine: **1,452,254
comparisons and 32,919,072 source instructions pass**. Both localized renders
were inspected; these are 2D compositions, not a full Unity showroom test.

`OriginalSavedData` now has a separate inspection-completion visit using the
ported renewal transition. English uses the original six-row panel27;
Japanese uses its authored `cardcheck5` panel because that regional
`v3sS10conf` bank ends at26. The banner is `cardcheck89`. The short page shows
customization instead of points/use-count, uses the source field offsets,
and takes its 879-tick timer from `10EBC0 ->192800 ->1346A0`. Visual review
caught and corrected use of the tuning-page option glyphs, which placed the
package above the name; the inspection-specific glyphs are chunks0..4.
A regression now confines changes to the customization row.

The frontend completion entry returns to Mode after confirmation/fade or
timeout, without queuing another profile write. Its paint cache uses the
presentation's animation frame because the source renewal owner does not
increment ordinary card-check frame448. Native tests pass **2,748 English**
and **2,747 Japanese** checks, including the 60-tick confirmation gate,
150-tick overlay lifetime, exact timeout/fades, read-only navigation,
duplicate-save prevention, ordinary Saved Data behavior and wide-screen
margins. The English/Japanese summary and banner renders were inspected.

The local frontend/App now connects these components. An occupied save has
an **Inspect Car** action beside Delete Save, targeting the last-used car
shown in the panel. Existing Change Car selects another owned car first.
The confirmation defaults to No and explains that changing the package
resets its related parts. Keyboard/controller navigation and pointer targets
use the same frontend state, with a latched request to prevent duplicate clicks.
The new confirmation/error strings and glyphs cover English, Japanese and
Simplified Chinese; original arcade artwork retains its English/Japanese
language selection.

Opening inspection flushes pending progress before rebinding the driver
store. It rejects missing, unreadable, unfinished or invalid profiles and
captures the exact on-disk car. Kind1 and temporary setup flags are confined
to selection. The transmission owner enters the package selector, with the
source single-package GC8V exception. A completed selection commits through
`LocalCarInspection` before entering the short completion page. Cancelling
restores the saved car; conflicts reload current progress, and failed writes
return to the confirmation with No selected. General autosaves cannot receive
an inspection draft. Full Tune, Quick Race and online entry cannot interrupt
the transaction, and online profile lookup returns its captured saved state.

Visual review of the full offscreen composition caught a car preview drawing
over the confirmation modal; the preview is now suppressed while it is open.
A successful retry also clears stale save-error text. Comparison with the
reference video exposed the missing transmission inspection badge/countdown.
Tracing `1B75AC..1B7686 ->1BE2E0 ->1BE520` recovered the same pulsing common
chunks21/23 used by the tuning screen. The source constructor chain
`124496 ->1927C0 ->1346A0` supplies a fresh879-tick timer, so an old profile's
remaining timer cannot immediately confirm the new visit. Both are now drawn
in transmission, with paint-cache and read-only pulse checks.

The application test passes **116,427 checks across70 car/transmission/package
cases** in each of three locale runs. It reopens actual isolated save files,
checks every unrelated profile byte, unchanged-package parts, other cars and
slots, cancellation at both selectors, stale writes, locked writes, explicit
retry, pending writes bound to the previous slot, invalid/unreadable saves,
and old timer values. Its final sequence uses real `renderMenu` routing and
native keyboard/pointer input, then captures confirmation, transmission,
current/changed package and completion screens with a private offscreen GPU
target. No audio device, desktop window, online service or real save is used.
English/Japanese completion artwork and the three translated confirmations
were inspected; the bright/dim inspection pulse is also captured.

Regression checks pass: existing frontend navigation/rendering, **1,517 Saved
Data application checks** (including keyboard/controller/host-mapped controls),
and **44,265 Legend application checks across67 saved-driver cases**. The
native Unity plugin builds successfully. Unity pointer confirmation cue
tracking includes the new modal and pending request. A packaged Unity runtime
inspection pass is still required; the plugin build alone does not establish
that result. The user's `D:\Current` installation is unchanged.

Integration invariants: flush any pending writes before capturing the
inspection transaction; keep the captured store bound to the chosen slot;
use profile kind 1 only in the menu workspace; consume ordinary profile-commit
events inside this transaction so cancellation cannot queue a later autosave;
and show completion only for `Saved`/`AlreadySaved`. A conflict must return to
fresh selection rather than force-overwriting the changed profile. Existing
Full Tune remains an explicit, separate convenience, not an automatic reward
for changing packages. The ordinary native application path is now tested;
an end-to-end packaged Unity pass is not yet claimed.

Evidence is under `Verification/bugs-20261110/`: the silent reference video,
`inspection-video-contact.png`, bounded `inspection-*-disasm.txt` files,
`inspection-card-check.log`, `inspection-tuning-controller.log`,
`inspection-tuning-presentation.log`, `inspection-tuning-ja.log`,
`inspection-saved-en.log`, `inspection-saved-ja.log`, and their rendered
inspection images. Integration evidence adds `inspection-app-{en,ja,zh}-c/`
and their logs, `inspection-locales-final.png`, the bounded transmission
disassembly files, `inspection-frontend-final.log`,
`inspection-saved-data-regression.log`, `inspection-legend-regression.log`,
`inspection-unity-plugin-build.log`, and `inspection-integration-verification.json`.
All owned test/build processes finished; no game window or audio was opened
for these checks.

## Inspection runtime check and corrected scope

The private packaged Unity run now passes 22,855 checks in each of English,
Japanese and Simplified Chinese. It uses the real Save Select pointer path
and normal native input frames for both selectors, cancellation, durable save,
completion and Mode. Locked-file failure and a concurrent profile writer are
exercised against isolated files, followed by successful retries. Other cars
and driver slots are compared byte for byte. Actual Unity render captures were
inspected in `unity-local/inspection-unity-en-contact.png` and
`unity-local/inspection-unity-locales-contact.png`. All runs were muted on an
unshown desktop and exited successfully. The private player was updated using
a scripts-only build plus the current native DLL and 21 inspection asset
files; this is runtime integration evidence, not a new release-build claim.

**The owner subsequently corrected the requirement: Inspection must be purely
cosmetic.** The above passing tests describe the original renewal behavior,
which is now superseded and must not be treated as acceptance of the requested
feature. That earlier implementation changed transmission/package and reset
basic/body tuning progress. No desktop or published build contains this local feature.

The owner resolved the clarification: **"Play the inspection screens only;
change nothing."** This is a presentation-only sequence, not visual parts
customization. In particular, simply preserving engine
byte164 while changing package152 is insufficient: `applyOriginalProfilePhysicsSelection`
uses package152 to select the AE86's engine override. Transmission68,
package152, progress153..155, engine164 and flags1180 must not be modified by
a cosmetic-only implementation. Rendering already has a separate
`OriginalCarAppearanceConfig` and `loadConfiguredAppearance` boundary; that
is the appropriate place to separate appearance from saved performance if
visual customization were ever requested separately. The renewal transaction
has now been removed from the feature rather than retained behind a no-op save.

Runtime evidence: `unity-local/inspection-{en,ja,zh}/report.json`, their player
logs and captures, `inspection-staging.json`, script before/after inventories,
and `inspection-runtime-verification.json`. Those reports explicitly predate
the cosmetic-only correction.

## Inspection screens only

**Historical manual entry point, superseded by the automatic zero-play renewal correction below.**

Inspect Car now plays the original inspection summary and completion banner
directly, with the original hold, overlay lifetime, timeout and transition to
Mode. It does not enter Transmission or Tuning Course, change visual parts,
renew the card or call a profile save. Back returns to the save actions.
The former `LocalCarInspection` writer and its renewal-specific tests were
removed. The confirmation now says "PLAY INSPECTION SCREENS" and "YOUR CAR AND
SAVE WILL NOT CHANGE" in English, Japanese and Simplified Chinese.

Playback uses a captured profile for presentation and online identity lookup.
Normal race/setup saves cannot receive a playback draft. On exit, the host
reloads the latest existing profile so legitimate concurrent progress remains
current; it does not repair a damaged primary merely because a valid backup
was displayed. Existing pending race progress must still flush to its original
driver before switching slots. Inspection playback is excluded from play-time
statistics.

The replacement native application check covers all 35 cars, all authored
packages and both transmissions (292 combinations), using full save-file byte
and modification-time comparisons. It checks navigation cannot edit the car,
early confirmation, finish, cancel, timeout, a locked writer, a concurrent
writer, backup-only display, driver isolation, pending-write ownership and
normal pointer/renderMenu routing. The first English/Japanese passes each
reported 41,186 checks; the final native English pass adds two play-time checks
and passes 41,188. Frontend regression and 2,748 Saved Data checks pass.

The updated private Unity player passes **52,455 checks in each of English,
Japanese and Simplified Chinese**, including actual pointer/controller input,
normal completion, cancellation, timeout, locked-writer playback, concurrent
progress and repeated visits. All isolated save files retain their bytes and
modification timestamps except the explicitly simulated concurrent writer.
The original summary/banner, translated confirmation and Mode captures were
visually inspected. Each muted run used an unshown desktop, completed shutdown
and exited with code 0. The first script build caught a test-input int/uint
mismatch; it was corrected and the v2 build passed. This remains a private
scripts-only build refresh plus native DLL and two font atlases, not a fresh
release-build claim. No desktop or release was updated.

Evidence: `inspection-playback-build.log`, `inspection-playback-app-*`,
`inspection-playback-native-en.png`, `inspection-playback-saved-en.log`,
`inspection-playback-frontend.log` under `Verification/bugs-20261110/`.
Packaged evidence adds `inspection-playback-verification.json`,
`unity-local/inspection-playback-{en,ja,zh}/report.json`,
`unity-local/inspection-playback-unity-{en,locales}.png`, the v2 script-build
inventories and `unity-local/inspection-playback-staging.json`.

After the dust runtime was staged, the unchanged inspection flow passed another
52,455 checks in the private muted player, with complete shutdown and exit 0:
`unity-local/inspection-after-dust-20261110/report.json`.

## Repeated stalls after reliable input recovery

Follow-up to the Happogahara control-loss report and the existing .53 watchdog.
The current private build first passed local recovery and neutral-timeout
checks in both directions. However, both peers entered recovery five times in
about nine seconds when the unreliable input stream stayed unavailable. The
watchdog treated any newly confirmed frame as proof that reliable retries were
no longer needed. A single recovered burst therefore returned the race to the
failed delivery mode until the next one-second recovery delay expired.

An extended two-player test reproduced this defect with the old production
session. Pings and room traffic continued while all unreliable race inputs were
withheld. During the six-second sustained window, the host confirmed only 96
new frames and the guest 64, with repeated native stalls. The new session-level
regression also fails against the old code: after the first recovered burst it
sends no further reliable history despite the unresolved delivery failure.

The session now retains bounded reliable history for the affected race, using
reliable delivery for at most four input sends per second. Ordinary input
traffic continues between those sends. Confirmation progress still resets the
15-second no-progress deadline, but no longer abandons the working delivery
mode. Loading/the next race clears the fallback; a continuously healthy race
keeps its ordinary stream. Packet contents, native input validation, rollback,
physics and result ownership are unchanged. This adds no ping limit.

Validation in the actual muted private Unity player:

- Happogahara outbound and inbound: the unreliable stream remains blocked
  through a six-second steady window. Both peers show zero native stalls, no
  repeated recovery entry, and 360–363 newly confirmed simulation frames.
- Outbound with 250 ms simulated round-trip delay: both peers again show zero
  stalls and no repeated recovery entry, with 382/386 newly confirmed frames
  including catch-up. These are simulation-progress counts, not rendered FPS.
- Both directions with reliable inputs also withheld: the existing watchdog
  ends the race with no result, battle points or tuning points. Pings remain
  alive, so a route heartbeat cannot conceal frozen simulation.
- All five passing pairs also exercise the old menu-ownership latch and verify
  that mapped throttle reaches the native race. Recovered cars move again,
  confirmed peer digests advance, both clients leave cleanly, and all ten
  processes exit successfully.
- The production session harness passes 27 progress/fallback checks and 75
  lifecycle checks. Coverage includes packet-size limits, bounded retry rate,
  repeated same-time pumps, normal races, next-race reset, duplicate packets,
  timeout boundaries and no repeated terminal effects.

The first extended inbound attempt passed its network window but failed the
later one-metre movement assertion after long unguided full-throttle driving.
Its traces and failure are retained. The final extended diagnostic uses normal
mapped throttle/brake to keep the cars near the opening straight; it does not
change native speed or position and retains the original movement assertion.
This diagnostic adjustment does not alter ordinary gameplay.

Evidence is in `unity-local/happo-progress-*`, including the deliberately
failing `sustained-before` run. `reliable-fallback-verification.json` binds the
passing reports, before/after source, and private managed/native hashes;
`reliable-fallback-session.diff` isolates the production change. Scripts-only
build inventories show only the managed assembly and generated boot config
changed; boot config was restored and the native DLL/assets stayed unchanged.

This reproduces and fixes a specific repeated-stall defect under controlled
input loss. It does not establish the cause of the reporter's exact incident
or verify two-PC EOS/Internet behavior. No Discord reply, desktop update or
release was made. Inspection remains presentation-only.

## Wall/gutter contact audio — source queue corrected, loudness still open

Read-only report: [wall hit and gutter sounds needs to be louder](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1560241101258891304),
posted by me3 on October 15. The report distinguishes contact sounds from the
turbo effects raised by the same Effects slider. No reply or reaction was sent.

The investigation found a separate source-fidelity defect: the game played
every raw `142460` contact request. The original `141FC0`/`1435C0` PACK22 manager
accepts a non-forced cue only when its cooldown is nonpositive, consumes the
shared gameplay RNG to set a 10–29 frame cooldown, and decrements it at the
outer `1415E0` sound boundary. Forced misfire cues bypass that gate.

The correction keeps the contact queue in the driving session's checkpointed
state. Body/wheel requests run before engine control, and completion cue4 runs
after engine control but before the completion RNG/steering-mask update. The
outer sound update ticks the queue after skid selection. Local audio observes
accepted cues at those boundaries; online audio consumes accepted cues only
when the numerical frame is confirmed. Raw requests remain available for
diagnostics. Mute and volume do not enable/disable this simulation state.

Verification in `Verification/wall-audio-audit-20261110`:

- Original PACK22 wrapper probe: 9,000 requests, 29,875 checks and 480,045 original
  instructions. Includes missing/disabled managers, forced cues, repeated
  requests, shared RNG and signed cooldown wrap.
- Player integration: 2,400 original-reference frames across two Akina datasets,
  with the contact queue enabled/absent; exact physics/RNG/steering state,
  checkpoint replay, and bound audio-callback order.
- Player/rival integration: 24 source setups and 5,760 frames across both
  Akina/Tsuchisaka directions, including 84 body-contact frames. Exact source
  state and accepted/suppressed cues, including reset cases.
- Combined sound integration: 10,800 original-reference frames interleaving
  wheel/gutter contacts, engine control, turbo, misfire and skid selection.
- Online timeline: 518,400 confirmed peer frames across all 18 original course
  conditions at 50/100/200/250 ms, with clean and jitter/loss scenarios. Cue
  sequences are included in the comparison as well as simulation hashes.
- Production mixer: 88,200 exact stereo frames comparing confirmed contact
  playback with direct source-ordered presentation, plus existing mixer tests.
  All PCM was rendered without opening an audio output device.

Two expanded reference fixtures initially failed because the new gutter path
needed its history pointers initialized and the rival reset fixture retained
old memory writes beneath a zero-backed region. The failures are retained;
explicit source-buffer/queue initialization corrected the fixtures, and both
passed without weakening their comparisons. The isolated native DLL built
successfully. Its changed hash is included in the existing multiplayer
compatibility identity, preventing a mixed-old/new numerical session.

This does **not** resolve the loudness report. Ungated retriggering was not
shown to cause low volume: the dry waveform experiment generally measured
higher aggregate energy without the gate. The unchanged desktop mix already
clipped one sample in the four-car default-gain stress fixture. A proposed
6.02 dB contact boost added three clipped samples in the Trueno fixture; a
3.52 dB trial still added one. Both gain experiments were removed from game
code and retained as diagnostic evidence. Contact volume still needs source
mix/cabinet comparison; arbitrary amplification is not presented as parity.

No Unity player was launched, `D:\Current` was not updated, and no release was
published. This correction does change the simulation's previously omitted
shared-RNG consumption; the original-instruction and rollback comparisons
above are its required evidence. Physical-cabinet loudness and two-PC EOS
validation remain outside those checks.

## Wall/gutter contact audio — targeted mix correction

The follow-up retained the reporter's 3.7-second MP4 and decoded it without
opening an audio device. A waveform comparison matched PACK22 cue2 near
1.288 seconds (absolute normalized correlation 0.557 in the 400–1800 Hz band,
duration ratio 1.001). The recording's gain, compression and provenance are
not independently calibrated; this identifies supporting sound evidence,
not the cabinet's exact mix level. A weaker cue0 correlation is not treated
as proof of a missing cue or used to change source scheduling.

The host mixer now gives accepted PACK22 contact cues1/2/4/6 a +6.02 dB
presentation adjustment. PACK25 turbo release and other one-shot gains are
unchanged, including the previously adjusted misfire cue. The original
dry/effects routing and the user's Effects control still apply. This is an
audibility correction, not a claim of cabinet-volume parity.

The earlier failed amplification trials exposed insufficient output headroom.
The final stereo mix now has a linked sample-peak limiter after all user gains:
0.98 full-scale ceiling, immediate attack and 50 ms exponential release at
44100 Hz. It adds no lookahead delay. Below threshold and after recovery,
samples pass unchanged. Pause freezes its envelope; mute advances it along
with the sound clocks. It does not claim intersample/true-peak protection.

Verification in `Verification/wall-audio-mix-20261110`:

- All17 race cues compared against independent original voice/DSP components.
  The four contact cues measured +6.022 dB; unrelated cue output stayed exact.
- Four ten-second actual-mixer fixtures (Trueno, MR2, EvoIII and FC3S), with
  music/engine/skid/contact/turbo overlap: zero clipped samples, peak32111/32767.
- Overloaded200% output, stereo balance, instantaneous attack, recovery,
  pause/mute/resume, Effects mute and race-effect reset passed.
- Existing one-shot, engine, music-loudness and tire-volume checks passed.
  The initial tire test failure is retained: its17692 peak cannot double
  inside16-bit output. Its linear200% check now has master headroom, with a
  separate overload test requiring both increased volume and a safe peak.
- The isolated Windows native DLL built successfully. A standalone quiet-mix
  microbenchmark measured0.70–0.79 ns added per stereo frame on this PC; this
  is not an end-to-end game performance measurement.

No gameplay/RNG code changed in this mix step. All output was rendered into
memory/files; no listening session or Unity player launch was performed.
The private DLL is not installed in `D:\Current`, and nothing was published.
Subjective listening and a calibrated cabinet comparison remain outstanding.

## Imported courses disconnecting at the start — presentation readiness

Read the replies in Discord's "Added courses are extremely unstable online"
thread (1559275252381392996). The reported workaround is for both players to
preload the exact course, direction, weather and time of day. Replies mention
Odawara, Tsubaki and Gunsai, including one player loading before the other.

The guest previously sent `Loaded` as soon as native race initialization
returned. Unity's scenery construction happens afterwards, including
synchronous `LateUpdate` loaders. A host could therefore schedule its start
while the guest was still loading; the guest would later reject that already
expired start time. The preserved pre-fix production session reproduces this
with a three-second presentation delay.

The scene owner now reports readiness on the Update following its first
submitted race frame. This gives that frame's LateUpdate scenery loaders time
to complete. The guest sends `Loaded` only after that boundary; the host also
waits for its own presentation before proposing a start. Readiness belongs to
the race ID, is cleared during retirement/reset/load, and cannot carry over
from the preceding race. Existing connection/loading deadlines remain bounded;
this does not remove a timeout or allow gameplay to start against unfinished
scenery.

Evidence is retained in `Verification/online-loading-20261110`:

- The original production packet flow reproduces the early acknowledgement
  and expired-start failure. The corrected flow passes 25 assertions covering
  either slow peer, stale race IDs, duplicate readiness, rematch reset and the
  existing neutral loading timeout.
- The existing session suite passes 75 assertions; the sustained reliable
  input recovery subset passes five. These exercise managed production code
  with fake native/clock/transport boundaries.
- A scripts-only Unity Windows build succeeded. The private player's only
  content change was `Assembly-CSharp.dll`; its prior native DLL, boot settings
  and scenery were preserved.
- Two real private TCP players passed Odawara night/dry with an added
  20-second guest LateUpdate delay, and Gunsai inbound night/wet with an added
  20-second host delay plus simulated 250 ms RTT. Both peers loaded the selected
  scenery, released the race and moved over 19 metres.
- Tsubaki reverse/day/dry also passed from fresh player processes without
  course preloading or an injected delay.
- An original Happogahara pair passed its ordinary start and the existing
  input-outage/recovery check with the new managed build. All eight private
  player processes exited successfully and shut their native sessions down.

`Verification/online-loading-20261110/verify.py` checks those reports against
their tested source hashes, compares the private build inventories and records
the consolidated results in `findings.json`.

These were muted, isolated-desktop runs, with separate diagnostic saves and
no public room. This establishes the local Unity loading correction, not
real EOS/firewall traversal or the reporter's connection. The separate report
of FPS falling after repeated imported-course preloading remains unverified.
No desktop installation, release or Discord thread was changed.

## Repeated imported-course preloading — current local regression check

The same Discord thread reports roughly halved FPS on Akina/Akagi after
repeatedly preloading Gunsai/Odawara. The inactive-owner cleanup already in
this checkout dates to commit `ed8b3cfe` (October 25), after those replies.
It releases the Stage 8/Zero and Special Stage scenery when a different course
owner takes over, while retaining it during menus/results that may return to
the same race. This step inspected and exercised that existing behavior;
it made no new gameplay, graphics or cleanup changes.

The current private Windows player completed two fresh-process runs of the
existing transition diagnostic: three cycles each, 39 race visits per run,
cycling Akina, Gunsai, Odawara and Enna through the normal course menus and
pause-return commands. Requested conditions were dry/day and wet/night;
Enna retains its mandatory night setting. Both players exited successfully.
All tests used isolated saves, muted audio and an unshown Windows desktop.

Across all 42 measured Akina revisits:

- Both imported owners retained zero meshes, materials, textures and measured
  owned-resource bytes. Imported visits loaded the expected owner and released
  the other owner's resources.
- The recorded original geometry/range hashes, camera, projection and mirror
  setting matched the first Akina revisit within each condition.
- Mesh/material/texture counts were unchanged at the ends of cycles 1, 2 and 3.
  Day counts were 1535/1577/328; wet/night counts were 1539/1560/333.
- No progressive render-cost increase appeared. Median render times per Akina
  visit ranged from 1.544–1.592 ms in dry/day and 1.368–1.455 ms in wet/night.

| Akina setting | Cycle 1 | Cycle 2 | Cycle 3 |
| --- | ---: | ---: | ---: |
| Day/dry, median of visit render medians (ms) | 1.557 | 1.555 | 1.560 |
| Night/wet, median of visit render medians (ms) | 1.388 | 1.389 | 1.390 |

These are manually completed offscreen 2560×1080 AA4 renders with fixed native
ticks, not foreground FPS. The first 30 frames of each driving section and
checkpoint frames are excluded from the timing summaries. No forced garbage
collection or unused-asset unloading was used to obtain the resource counts.
Unity allocated-memory estimates are retained separately from Windows process
memory: sampled process peaks were 2.11/2.02 GiB private committed memory and
1.42/1.29 GiB working set for day/night respectively, including startup,
loading, screenshots and diagnostics. The smaller Unity counters must not be
presented as the game's total RAM usage.

`Verification/repeated-imports-20261110` retains the launcher, live-process
memory samples, full frame traces, 160 checkpoints, source/binary hashes,
matched-state comparisons and analysis. This reproduces the repeated course
switches without reproducing the reported FPS loss on this PC. The exact
online preload/lobby sequence, Akagi, reporter hardware and normal foreground
frame pacing remain outside this fixture. Keep the report open for those
checks; this is confirmation of an existing cleanup, not a new FPS-gain claim.

## Imported preloads followed by a private online lobby and race

The follow-up now exercises the offline-preload → lobby → original-track
multiplayer sequence with two real private Windows players. Akagi wet/night
uses two preload cycles per player, and Akina dry/day uses three. Each cycle
loads Gunsai and Odawara through the ordinary course/route/conditions menus;
cycles alternate dry/day and wet/night. Each setting has a separate fresh-start
baseline pair. All eight processes passed, moved both cars over 170 metres,
left the private room and shut down successfully. All runs were muted and used
unshown Windows desktops and isolated saves; the connection was local TCP.

The twenty imported-course visits constructed the requested scenery and
variant. The last imported owner remains cached through the connected lobby,
as expected for a menu that can return to that track. At both the beginning
and end of all eight original-course race samples, the imported and Special
Stage owners retained zero meshes, materials, textures and measured resource
bytes. The original scene texture count also matched the corresponding
fresh-start baseline. Global Unity object counts are not identical: the
preloaded runs retain 65/66 additional UI textures, plus some mesh/material
objects. Those observations are retained rather than claiming all memory
returned to its startup state.

| Setting / peer | Fresh render median (ms) | After preloads (ms) |
| --- | ---: | ---: |
| Akagi wet/night, host | 1.4681 | 1.5168 |
| Akagi wet/night, guest | 1.6445 | 1.7123 |
| Akina dry/day, host | 2.0571 | 2.1350 |
| Akina dry/day, guest | 2.0413 | 2.1478 |

These are six-second manual offscreen 2560×1080 AA4 samples with GPU completion
readback, excluding the first 30 frames and checkpoint frames. Both clients
share this PC/GPU. The original online simulation runs on its real elapsed
time clock; it is not the fixed-tick offline transition fixture. The generic
scope sentence inherited by `transitions.json` is explicitly corrected in
the consolidated findings using the verified `frameDeltaMode` from each
client's final report.

Render medians were 3.3–5.2% higher after preloading, and wall medians were
3.1–4.6% higher. One pair per condition does not establish that small
difference as a regression, and these samples are not foreground FPS. The
reported halving was not reproduced. No forced collection or unused-asset
unloading was applied. Unity counters are not total process RAM; managed heap
capacity and cached UI resources differ after loading courses.

`Verification/online-preload-20261110/analyze.py` validates the requested
preload order/variants, lobby/race observations, loopback room, real-time
clock mode, peer movement, camera counts, successful process exits, build
inventory and binary hashes. `findings.json` retains the per-peer measurements
and source/report hashes. This step added opt-in diagnostic coverage; no new
scenery cleanup, performance adjustment, desktop update or release was made.

Still open: actual EOS with independent identities, loading while an EOS
client is already authenticated, the reporter's hardware/settings, longer
sessions and ordinary foreground frame pacing. Keep the Discord report open.

## LAN entry during sign-in and race-start packet delivery

Read-only Discord report: **Bring back LAN races**, J:D:R | 煮毛球 Fan,
October 15:
https://discord.com/channels/1548169613206884355/1560492032777134090
The reporter described two players remaining alone after joining a Wi-Fi
address, and using an online private room instead. The fixes below address
two independently reproduced LAN defects; they do not establish the cause of
that reporter's missing-peer symptom.

1. **LAN entry depended on online sign-in finishing.** The menu disabled both
   transport selectors whenever the session was busy, including initial EOS
   sign-in. The session rejected the selection too. `CanSelectTransport` now
   permits the ordinary LAN selection while initial EOS sign-in is pending.
   Queue, room creation/join, lobby, race and disconnected-finish guards remain.
   Switching disposes the previous provider and detaches its callbacks. The
   production menu uses the same guard as the session.
2. **TCP held the race's Load packet until after native loading.** `StartRace`
   sends Load, then initializes the native race synchronously. TCP previously
   only wrote queued data in Poll, so a slow host left its guest in Lobby with
   the shorter inactivity timeout. TCP now makes a bounded, nonblocking attempt
   to send queued bytes immediately. Partial writes remain queued for Poll.
   Send does not read incoming messages or dispatch their callbacks, preserving
   the native initialization boundary before handling the guest's Loaded reply.

Evidence is retained in `Verification/lan-entry-20261110`:

- `before` fails the pending-sign-in LAN-selection regression. Final
  `session-after-tcp-fix` passes **81** lifecycle checks, including cancellation,
  late retired-provider callbacks, queue restrictions and switching back online.
- `tcp-before` fails delivery without a host Poll; `tcp-after` passes **7**
  real loopback socket checks for immediate delivery, deferred incoming
  callbacks, packet framing/order and disconnect.
- `loading-before-tcp-fix` uses the archived old TCP implementation with the
  current production session. It cannot deliver Load while the host remains
  inside native initialization. `loading-after-tcp-fix` passes **31** checks:
  real TCP/session Hello and Ready, a simulated 20-second host load, the guest
  remaining in Loading, deferred Loaded processing and release of both peers.
  Native calls and the clock are test boundaries; this is not a 20-second
  wall-clock Unity load or a live EOS outage.
- Unity scripts-only build `unity-tcp-build.log` succeeded. Comparison of the
  private player's inventoried files found only `Assembly-CSharp.dll` content
  changed; native DLL and boot configuration were preserved, and StreamingAssets
  metadata was unchanged.
- `player-normal-lan-tcp-fixed` passes in two hidden, muted Unity players using
  **normal `SelectTransport(1)`**, the default LAN listener/port and the host's
  advertised address. Both complete Hello, lobby/Ready, race presentation,
  authority driving and clean shutdown. Both cars move over 177 metres.
  This does not substitute `ConfigureLocalTest` for normal LAN entry.
- `verify.py` checks the final source/build hashes and these results and writes
  `findings.json`.

Keep failed runs: `player-normal-lan` stopped at a missing diagnostic launch
argument before connection; its launcher was corrected. `player-normal-lan-b`
reached lobby/Ready but its guest timed out at startup. A loopback control and
`player-normal-lan-c` then passed with the unchanged pre-TCP-fix build. The
earlier host stall was not captured; attributing that specific run to queued
Load delivery remains an inference, separate from the deterministic repro.

No real two-PC Wi-Fi/firewall traversal or pixel/input click test was performed.
Pending EOS sign-in was tested with the service-boundary fake. The reporter's
missing-peer issue remains open. No Discord reply/status, desktop installation
or public release was changed.

## Challenge received animation: original banner motion and mesh restored locally

Read [Improve the Challenge received pop up animation](https://discord.com/channels/1548169613206884355/1560418885071667310),
JDR99, October 15. The original post describes a jarring banner and a car that
coasts/stops before the notice instead of freezing with it. Its two replies also
require race/menu music and other sounds to cut when the notice appears. The
linked references start at `ftUYvLP3BlU?t=133` and `MVThbCUF_G0?t=826`. Read-only
Discord access was restored after the earlier firewall prompt disappeared.
Opening/downloading the GIF was interrupted by user-input/coordinate-geometry
errors; no local GIF was obtained. Do not claim a recorded-GIF comparison.

Evidence: `Verification/challenger-animation-20261110`.

- The original instruction trace completed successfully: 182 source frames,
  1,737,158 original instructions, one cue at frame 0 and the finish callback at
  frame 181. It executes `16CBA0` banner math (including original trig), `16C980`
  background-strip generation and `16C860` owner lifetime. Integer quotient
  helpers, model parsing and draw/audio submission are explicit boundaries.
  The complete 100-vertex backdrop strip is recorded for each frame, but is not
  yet used by production rendering.
- The source has a damped horizontal arrival during frames 0..39, one flash
  envelope during frames 41..63 (peak 255 at frame 48), then a steady banner.
  The previous Unity overlay used a 0.2-second smooth slide, repeating 8Hz
  flashes, enlarged rectangles and a guessed 2.8-second duration.
- Separate execution of the original default camera (`1D0940`/`1D0880`) and
  bank draw transform (`145EC0`) established the source screen placement.
  `Tools/Import-ChallengerGeometry.py` extracts all 26 vertices in four banner
  batches, including their original depth, indices and UVs. The background
  texture repeats twice. No artwork was redrawn.
- `Idas3ChallengerAnimation`, `Idas3ChallengerBanner` and
  `Idas3ChallengerGraphic` now drive the overlay from those source positions,
  the one-shot flash and the projected source mesh. The unscaled host clock
  samples the 182 source draw frames at 60Hz before releasing the lobby.
  The independent search badge and existing immediate native physics hold
  remain owned by the existing overlay/session flow.
- `check-animation.py`: **601 comparisons pass**, covering every source X
  and flash sample, the 60Hz clock, and 52 vertices projected through the
  separately executed original bank/camera matrices (0.002px tolerance).
- `Idas3ChallengerAnimationChecks.Run`: **18 production Unity renders pass**
  at 640x480 and 1280x720. The Editor ran muted on an unshown desktop and
  exited 0; no player was built or installed. The first render exposed an
  inverted source-V mapping. Its images/log were retained as `gpu-before-uv`
  and `editor-before-uv.log`; the corrected mapping was rendered again.
  `check-gpu.py` checks source texture interiors using the independently
  executed projection: the retained wrong orientation fails 280/433 patches,
  and the correction passes **433/433**. The settled and peak-flash renders
  were also viewed. These are focused layout/texture checks, not cabinet
  rasterization or a complete online acceptance test.

### Challenge interruption: audio cutoff and frozen native owner

The preceding implementation played PACK20 cue3 over the running scene mixer
and did not invalidate queued Unity PCM. It also re-entered the previous
menu/race owner with dt=0; this did not protect against pending owner actions
or input commands that do not depend on elapsed time.

- `EngineAudio::beginChallenger/endChallenger` now bracket the interruption.
  The only newly played sound during the notice is original PACK20 cue3. Its private voice
  loads PACK20 alone, retains the currently selected source DSP program/routes,
  and initially started without the preceding scene's accumulated reverb/limiter
  tail. The later source shutdown trace below corrects the reverb reset; the
  host-only limiter still starts fresh.
  Music, engine, tires and preceding effects keep their clocks frozen. Cancel
  resumes them; commit retires the old scene through the existing course flow.
  This is host interruption routing, not a claim that every original scene
  manager's destruction and DSP-preset selection has been reproduced.
- The cue probe found **114,695 nonzero effect sends on bus0**. The correction
  therefore preserves its wet return instead of dropping DSP or muting the
  entire output. Existing user gains and muted-clock behavior still apply.
- Begin/end invalidate queued PCM. The native frame loop now preserves the
  last scene/UI capture and services notification audio only, without running
  old owner commands. Cancel and commit restore normal frame processing.
- `challenger_audio_mixer_tests` passes **556,444** independent source-cue/DSP
  stereo comparisons and **48,000** exact resumed-scene frames. Cases cover
  menu music/effects, race stream/engine/effects, held showcase, development
  engine, full 182-frame notice, early cancellation, repeat begin/end, user
  gains and mute. The run contains 538,377 nonzero wet-return frames.
  Existing one-shot and contact mixer regressions also pass.
- `native-owner-check.cpp` loads the actual private DLL and exercises the
  exported scene/challenger/audio APIs with private userdata and no audio
  output device. **1,493 checks pass**: menu and race captures/UI remain frozen
  for 182 frames despite input/focus changes; notification PCM remains audible
  in the sample buffer; old queued audio is discarded at begin and cancel;
  cancellation resumes both owners; commit reaches course selection.
  The retained preceding wall-audio DLL fails this same check with
  `Queued previous-scene audio survived notification start`. Logs and hashes
  are in `audio-findings.json`, `native-owner-before.log` and
  `native-owner-final.log` under the evidence directory.
- A new private native DLL was built under this evidence directory. Neither
  the private Unity player nor `D:\Current` has been updated in this step.

**Still open:** replace the uniform shade with the original captured-background
effect after establishing its complete material/composition state; compare
the full original scene audio shutdown/preset behavior; verify live
admission-to-banner freeze and disconnect recovery in an updated private
Unity player. The native checks start at the exported notification action,
so they do not alone disprove the reporter's pre-banner coasting symptom.
Keep the report open until the full transition, including menus and gameplay,
is verified. No Discord message/status, `D:\Current` files or public release
was changed.

### Challenge popup: packaged transition and texture-copy correction

The new full private development player exposed a player-only defect that the
earlier Editor captures did not catch. `Instantiate(Texture2D)` logged
`No texture data available` for the non-readable banner background. The eyes
and lettering appeared, but the silver backing was missing. Both peers in
`admission-full-02` passed their session checks while showing this rendering
defect; those logic passes are not visual passes.

`Idas3ChallengerBanner` now copies that texture on the GPU into its own repeat-U
RenderTexture. It preserves the shared asset's sampler, restores the prior
render target after the copy, and releases the owned texture on destruction.
The updated packaged capture visibly includes the silver backing, and neither
peer logs the missing-texture warning. The scripts-only correction was applied
to the newly built complete private player; its resource archive hash stayed
unchanged. Both native DLLs were restored from the verified private native
build and hash-checked after Unity packaging.

Verified in hidden, muted game processes with separate test saves:

- `admission-gpu-copy-03`: search during offline driving, four aspect ratios,
  cancellation/restart of search, admitted-peer banner, stable native simulation
  ticks during the notice, one announcement, safe course/lobby transition,
  readiness and a real two-client race. Both clients travelled over 230 metres.
- `admission-drop-04`: peer leaves during the notice; pending/modal/search
  state clears, and the host resumes its offline simulation.

The first package launch (`admission-full-01`) stopped at the real ROM gate:
the new fixture lacked the ROM already present in the earlier private build.
That instance was stopped, the existing user-provided ROM was copied and
hash-verified, and the tests were rerun. No ROM-gate bypass was added.

These are real TCP game-process checks on one PC, using private diagnostic
discovery. They do not establish EOS Internet discovery, two-PC timing,
audible cabinet parity, or pixel-identical reference animation. The original
captured-background effect is still outstanding. A separate 1,212-instruction
probe now establishes its RGB565 strided capture format and material compilation
under explicitly listed test globals; it does not establish the original
per-frame extra material masks or the background underneath the capture.

Evidence: `Verification/challenger-animation-20261110/packaged-findings.json`,
`background-material-notes.json`, the paired player logs/reports and before/after
`challenge-received.png` captures. The report remains open. Nothing was copied
to `D:\Current`, published, or changed in Discord.

### Challenge popup: captured background and recorded-reference comparison

The report author's linked public video is now available as private reference
evidence. The 128–142 s and 140–147 s excerpts from
<https://www.youtube.com/watch?v=ftUYvLP3BlU> were downloaded and decoded without
audio playback. Discord's GIF could not be opened: native window input returned
`coordinate input geometry is unavailable`, including after window recovery.

The left player's frozen image settles to a median 55.38% brightness relative
to the initial captured frame. Original vertex colour plus source-alpha blending
over black predicts 56.69%; blending over the previous scene predicts 81.39%.
Across seven frames and 2,799 sampled pixels per frame, the original slanted
strip over black fits with median channel error 1.45/255, versus 10.48/255 for
the retained-scene alternative. These are comparisons to compressed video,
not a lossless cabinet framebuffer or proof of every renderer state bit.
See `recorded-reference-findings.json` for alignment, crop and file hashes.

Production now replaces the uniform shade with the original 100-vertex strip.
Its per-vertex colour intentionally preserves the source's OR-with-192 bands.
A GPU command captures the composed scene once, before the challenger layer is
drawn. The captured image then remains frozen independently of later scene/UI
renders. The shader reproduces the black-underlay composition and ignores Unity
scene alpha; cancelling or finishing removes the command and releases the
temporary texture. The effect fills the viewport at 4:3 and widescreen.

The numeric check passes 18,200 exact vertex/colour comparisons against the
original-instruction trace. The isolated muted Unity GPU check passes 16
renders and 8,616 sampled pixel comparisons, including a source with zero alpha,
a changing source after capture, and cancellation before the first render.
The first GPU attempt found a missing CanvasRenderer; the component now declares
that dependency, and the corrected check passes. See `backdrop-managed.log`
and `backdrop-gpu/PASS.txt`.

A new full development player includes the shader resource. Its native copy
and hashes completed; staging initially rejected Unity's already-created ROM
README directory. Recovery preserved/verified that README, copied/hashed the
ROM, and restored/verified both native DLLs. No game-code failure was involved.
`backdrop-package/player-build-staging.json` records the completed staging.

Packaged normal admission passes in `admission-backdrop-05`: 32,234 host and
31,689 join checks, one notice each, with both cars travelling over 230 m.
The active capture and packaged shader-support checks pass, and the resulting
host screenshot was inspected. Explicit cancellation passes in
`admission-backdrop-drop-07` (`drop: true`): 59 host/16 join checks and native
offline ticks resume. Case `admission-backdrop-drop-06` was mistakenly launched
without the drop flag: it is a second normal race, not cancellation evidence.
The evidence summary asserts the requested drop mode to prevent that naming
mistake being counted as a passing disconnect check. All players were muted,
hidden, local TCP fixtures; no Internet/cabinet parity is claimed.

One visual follow-up is now directly visible in the recording: the original
frozen image retains the accepting-challengers badge. The production capture
currently includes the world/source-UI cameras but excludes that separate
overlay. Include it in the snapshot without continuously capturing during normal
racing. Full audio-preset/listening and real Internet-service admission remain
open as before. See `backdrop-findings.json`; no desktop/release/Discord change.

## Challenge popup: retain the old search badge in the frozen picture

The reference-video follow-up above is now implemented. The one-shot snapshot
includes the last displayed accepting-challengers badge. The live badge then
disappears, and the captured lettering receives the same moving shade as the
rest of the old scene. This preserves the original effect without continuously
copying the framebuffer during racing.

The failing-before GPU check expected a green overlay pixel and instead found
the scene beneath it. The completed version passes 16 rendered cases and 8,616
pixel checks, including the previous overlay, snapshot opacity, freezing and
capture cancellation at both 4:3 and widescreen sizes. Capture uses the overlay
camera's final target; command-buffer cleanup is deferred until the next render
because Unity's post-render callback runs before that final buffer executes.

Private packaged case `admission-frozen-badge-09` passed normal admission and
driving. Its lossless screenshots retain 1,303 tested green-lettering pixels;
their median maximum-channel error against the original shade formula is
0.248/255, and the maximum is 0.599/255. The scene comparison also passes. This
is a packaged-render comparison, not a new cabinet capture.

Cancellation case `admission-frozen-badge-drop-10` failed an existing assertion
that waited only 15 Unity frames for the race clock to advance. It did not log
enough state to prove the exact cause retrospectively. Added instrumentation in
cases 11 and 12 showed successful cancellation and recovery, with that 15-frame
wait lasting only about 10 milliseconds, less than a native 60 Hz tick. The
diagnostic now requires three actual native simulation ticks within two seconds
and verifies the running offline race owner and closed lobby. It also checks
that the offline tick remains unchanged throughout a 0.45-second notice hold.
Explicit `--drop` case `admission-frozen-badge-drop-13` passes those checks.
No production disconnect behavior was changed on the basis of the old failure.

`frozen-badge-findings.json` checks the production source hashes against the
GPU and normal packaged runs, the private binary hashes against the final
cancellation run, and records the timing observations. The scripts-only private
rebuild preserved the previously verified resource archive and native DLLs.
All player tests were muted and ran on isolated hidden Windows desktops.
The desktop installation, GitHub release and Discord were not changed.

### Challenge interruption: preserve the original effect memory

The original parent transition at `0C050E00` requests the interrupt child and
selects sound set3 through `0C1416A0`. That set has no replacement sound banks,
score manager or stream manager. The previous managers are destroyed before
PACK20 cue3 is queued. This does **not** imply a DSP reset.

- The final `audio-transition-trace-04.json` executes **14** original SH4
  transitions, including the SDK bank unload, command packing and stereo-stream
  stop/release routines: **15,160 instructions**. Fixtures cover sound sets0–4,
  playing/stopped streams and loaded/unloaded menu scores. Only hardware
  transfer/FIFO/acknowledgement, allocation and unrelated debug/network edges
  are hooked. Earlier traces used incomplete synthetic manager fixtures; use
  trace04 for the final evidence.
- `audio-dsp-transition-trace.py` uses the verified original ARM sound driver
  and real bank bytes. The race bank names are read from the original scene
  descriptor, including its two PACK23 slots. Across those 14 transitions,
  stop commands, bank removal, stream shutdown and the challenger dispatch
  preserve the loaded program, return routes, working registers and delay RAM.
  The explicit preset-7F clear changes the seeded state in every positive
  control. Total: **1,060,524 original ARM instructions** including those
  controls. Bank compaction and transfer acknowledgements are fixture work;
  this is not a full hardware/IRQ timing or active-voice-envelope comparison.
- `EngineAudio::beginChallenger` now copies the live effect processor instead
  of reconfiguring an empty one. Its private copy continues the inherited
  reverb under the challenger cue. The suspended scene retains its own state
  for cancellation; its music, engine and effect clocks remain frozen.
- The updated mixer test fails against the preceding implementation on the
  first race sample (`expected=-1,-1`, `actual=0,0`). With the correction,
  **556,444** cue/DSP comparisons and **48,000** exact resumed-scene frames pass.
  The reset-state negative control differs on 308,517 tested frames, with a
  maximum difference of **31 / 32,768** PCM units. This is a small fidelity
  correction, not evidence of a large audible improvement.
- All four targeted audio regressions pass. The rebuilt private DLL also
  passes **1,493** native scene/audio integration checks, with no audio device.
  The private player's normal two-client TCP admission/race test and explicit
  challenger-disconnect test both pass with the new DLL. The latter verifies
  that the frozen offline race resumes for three actual simulation ticks.

Evidence is under `Verification/challenger-animation-20261110/`, indexed by
`reverb-tail-findings.json`. The updated player is private and muted.
`D:\Current`, GitHub releases and Discord report state remain unchanged.
Calibrated listening, active-voice release/timing, a complete cabinet comparison
and real Internet admission remain open.

### Challenge interruption: active-voice shutdown trace (November 11)

The earlier sound-bank trace did not contain active voices. The new bounded
ARM test starts actual authored A9 sequences, the TYPE score, and the original
continuous engine programs before applying the SH4 teardown events. It observes
the real termination routines and hardware-register stores without replacing
their bodies. Source queues and bank transfers are serviced explicitly in
private RAM; the test does not claim an emulated sound-chip clock or DMA timing.

The results distinguish two shutdown paths:

- All 32 nonpersistent A9 cues and the tested TYPE score voice are force-muted
  by `2E24`: total attenuation becomes 255 and playback registers are cleared.
- Both engine programs in all 36 car/engine families, at four control values,
  and the auxiliary PACK10 instrument receive `2DF4` key-off. Their total level
  and authored release envelope survive. The voices are not immediately
  force-muted. The 36 family selections were independently reconstructed from
  the canonical SH4 image and match the imported table byte-for-byte.
- **181 cases, 383 active voice slots and 5,310,395 original ARM instructions**
  pass. Suppressing either the original force-mute stores or key-off stores is
  detected by the corresponding fault control. The inherited DSP memory is
  preserved, and the challenger cue starts in a new slot in every case.

The existing native ICS player already supports those release envelopes. A
separate silent probe runs all 36 families and the auxiliary instrument at four
control values: **148 cases**. Following key-off, voices retire after **137–159
samples (3.11–3.61 ms at 44.1 kHz)**. Their dry output remains nonzero for
107–134 samples; a hard-reset control is immediately silent. Releasing a copy
leaves the suspended original's sample stream unchanged. These are native
sampler measurements, not a recording of the original AICA chip.

This source-only pass identified an unresolved distinction in the challenger mixer:
it inherits the DSP tail but bypasses every old voice. It does not establish
whether the original engine release survives until the first visible banner
frame. That depends on the source command/DMA/IRQ ordering; the offline native
engine can also be ahead of its output queue, so copying its current state
without checking that boundary could jump sample phase. Do not replace this
uncertainty with an arbitrary fade or claim complete audio parity.

Evidence: `Verification/challenger-voices-20261111/findings.json`, original
register traces, detected-fault controls and native release CSV. Exploratory
probe/build failures are retained separately. That source-only pass changed no
game code, desktop files, Discord state or release, and launched no game or
audio-output process.

### Challenge interruption: engine release at the mixer boundary (November 11)

The subsequent implementation preserves the continuous engine's authored key-off
release when starting the challenger cue. It releases a private copy of the
sample voices into the inherited DSP, while the suspended race's stream,
controls, sample voices, queued PCM and DSP stay untouched. Cancellation resumes
that suspended state. Score and old one-shot voices remain cut off as established
by the original force-mute trace; no arbitrary fade was added.

The offline engine synthesizes ahead of its mixer output. One bounded checkpoint
per 735-sample block now retains the state at the queue head. Interruption
reconstructs the next mixer sample from that checkpoint instead of copying the
future synthesis state. Presentation and confirmed-online engines already run on
the mixer clock and can be copied directly. Initial silent padding, queue
underflow, inactive/hidden vehicles and paused output cannot introduce a phantom
engine release. Queue resets discard both PCM and its sample history together.

The first reference comparison fails on sample zero before this correction
(`expected=10,10`, `actual=-1,-1`). The final mixer suite passes **650,652 exact
stereo comparisons** and **95,104 exact resumed-scene frames**. This includes
39 offline queue cases across three cars (block boundaries, priming, underflow,
overflow and DSP-send clearing), three confirmed-online cases with delayed
controls, and five reset/pause/silence cases. Twenty-one cases explicitly
distinguish the output sample phase from the future synthesis phase. Gains and
mute are changed during the short release itself, and pausing partway through
the release resumes the exact cue/voice/DSP sample sequence.

All nine targeted audio regressions pass. The rebuilt private DLL also passes
**1,493 scene/audio API checks**: menus and races freeze for 182 notice frames,
prior host PCM is flushed, cue PCM is delivered, focus/input cannot escape the
notice, cancellation resumes the prior owner, and acceptance reaches selection.
Those checks write PCM to memory; they never open an audio device.

The same DLL is staged only in the marked private Unity player. The muted,
hidden TCP admission check passes on both roles (32,200 host / 31,684 guest
checks), then reaches driving with both cars moving. A separate explicit
disconnect-during-banner run passes (62 host / 17 guest checks): the host's
offline race resumes, the notice and search badge clear, and the original race
owner remains active. Its captured state advances from tick 479 before the
drop to 484 on cancellation and 487 at the verified resumed milestone.
The run named `admission-engine-release-drop-17` accidentally omitted the drop
flag and is retained as an additional normal-admission run; only the explicit
`--drop` run `admission-engine-release-drop-18` supports disconnect recovery.
These are local TCP tests, not two-PC EOS/firewall validation. All owned player
processes exited. `D:\Current` and the Editor asset plugin remain unchanged.

This establishes the source termination distinction at the native mixer
boundary. It does **not** measure original AICA IRQ/DMA latency, physical host
device latency, or the exact sound-versus-visible-banner timing on a cabinet.
Full listening/cabinet parity remains unverified. Evidence is recorded under
`Verification/challenger-voices-20261111/`; the original source-only findings
remain separate from `release-integration-findings.json`.

## Smoke/dust scheduling: shared driving and tire-sound RNG (November 11)

The previous combined effect implementation deliberately retained a private
random stream while the original race-owner ordering was unresolved. The
canonical executable now supplies a tested ordering boundary:
`056EC0 -> 023480 -> 05FDC0 -> 062DE0 -> 063CE0 -> 066840/060860`,
followed by the outer `056ECC -> 1415E0 -> 142120` tire-sound update.
Dry-only `0610C0` follows smoke inside `066840`; lighting and network publication
follow that wrapper before the race owner returns.

The expanded original-instruction smoke test executes that actual outer
dispatch, actual ARace owner, full smoke/particle controller, actual outer
sound service and tire-sound selection. It passes **4,800 coupled frames**,
**4,824 ordered audio commands**, and a deliberately reversed-order comparison
that distinguishes **16 frames**. Synthetic solver callbacks consume varying
numbers of original RNG calls and issue the original tire request. They are
explicit dependency boundaries, not a claim that this fixture executes full
driving, all course effects, or the whole arcade game. HUD/rules, pose
publication, dry marks, lighting and stream-device callbacks also remain
boundaries. The source LCG itself is not replaced.

Live offline and original-course online smoke now take the driving owner's
current random state, advance after current pose publication, return that
state, then let tire sound consume it. Both online car poses are available
before either view advances its effect owner. Each view still has its own
rollback state. Replay stores the actual pre-effect shared seed, so it can
reconstruct the effect without consuming the live driving stream.

Because emitter/particle lifetime now affects future shared RNG consumption,
the online synchronization digest includes the complete numerical smoke state.
The online test deliberately resets only that state and verifies immediate
digest rejection and checkpoint restoration for both views in all six cases.
The existing managed compatibility identity hashes the native DLL and managed
assembly, so this changed private simulation does not share its compatibility
identity with an earlier native build.

Validation with the new private DLL:

- Full smoke source suite: **73,920 ARace/SmokeMgr ticks**, **13,142,826 bitwise
  comparisons**, **423,615,632 original instructions**, including the new outer
  scheduler cases. Original tire audio and the virtual network timeline also
  pass; the latter covers **518,400 peer frames**.
- Online smoke integration: **90,545 checks**, **321 rollback corrections**,
  **2,339 view ticks consuming shared smoke RNG**, and **12 digest mutations**.
  Tire commands and the returned driving seed are checked from the post-smoke
  boundary. The prior private-stream digest baseline remains historical
  evidence; unchanged digests are no longer the correct requirement for this
  source-RNG correction.
- Replay codec: **72 scenarios**, **56,232 source ticks**, **173,271 checks**,
  exact seeking and malformed-input rejection, with legacy writer coverage.
- Actual App: **2,400 offline and 960 online frames**, four offline and two
  online offscreen comparisons. Rendering/stalled repaints preserve simulation
  state, and restored effects reproduce identical pixels.
- Offline replay: **3,120 recorded ticks**, **2,767,793 checks**, four restored
  full-scene comparisons. The old fixed night snapshot at tick 312 had its
  corrected particles occluded; the independent live camera test found a
  visible cloud at tick 318 (wet at 65). The replay test uses those observed
  frames without changing depth, intensity, or graphics to force visibility.
- Confirmed online App recording: **666,789 checks**, **46,046 pose checks**,
  **196 rollbacks**, **639 predicted-pose differences**, and four finish cases.
  This invocation covers the original-course scenarios, not imported packs.
- Packaged private Unity viewer: a new shared-seed archive and an untouched
  historical archive both pass twelve captures, alternate POVs, rewind and
  exact image restoration. Both runs verify muting before scene load and exit.

The checked native DLL is staged only in the marked private player under
`Verification/challenger-animation-20261110/backdrop-package/player`.
`D:\Current`, the Editor asset plugin and GitHub are unchanged. Tests use
offscreen rendering or an unshown desktop and no audible output. Evidence,
original disassembly, failed setup/snapshot attempts, successful logs and file
hashes are retained in `Verification/smoke-schedule-20261111/findings.json`.

This establishes the live smoke-to-tire shared-state boundary and rollback
integration. It does not establish the complete boot/menu RNG history, every
course-effect callback's interleaving, full Flycast/cabinet equivalence, or
real two-PC EOS/firewall behavior. Those wider comparisons remain open.

## Evo III exhaust flashes missing from recorded playback

The October 4 misfiring-system report's live local/remote fix still had a
documented replay limitation: files did not contain the accepted flash phase,
and replay rendering excluded the effect. A real App capture driven by the
original engine controller reproduced this: ten live flash frames in a
180-frame recording, none represented in the old format.

IDR2 revision 4 now records each car's accepted two-frame flash state. Online
archives advance it only from confirmed engine commands, retaining countdown
history without serializing speculative effects. Playback uses discrete states
through interpolation, pause, rewind and viewpoint changes. Legacy revisions
1-3 still load; missing historical or AI-opponent timing is not guessed.
Leaderboard upload behavior remains times-only.

The native replay test also exposed a second defect: ReplayStart initialized a
solo light snapshot, so an added opponent had no proper car-light scope.
Playback now composes both scopes and spot positions from recorded poses and
lamp flags before adding the short-lived flame lights. Repaints cannot accumulate
lights or advance simulation. Current recorded lamp flags approximate the
unrecorded prior-request lighting state; full historical lighting parity is not
claimed. Original per-draw flame-length jitter also remains outside this change.

Validation retained in `Verification/replay-backfire-20261111/findings.json`:

- Ordinary recording: 184 checks, 180 frames and ten accepted flash frames.
- Actual App confirmed-online recording: 762,029 checks, 46,046 pose checks,
  197 rollbacks, 643 predicted/confirmed pose differences, four finish cases,
  and 80 flash frames across both slots, day/night/wet and showcase histories.
- Combined smoke recording remains valid: 2,767,793 checks, 3,120 ticks and
  four native full-scene comparisons with exact seek restoration.
- Managed parser/sampling: 1,804 checks, plus 206,001 smoke compatibility
  checks and 11,434 legacy dust checks; malformed phase/layout data is rejected.
- Actual DLL: 1,190 checks for independent local/remote effects, three fitted
  variants, non-Evo exclusion, expiry, invalid calls, rewind and unchanged saves.
- Existing Discord regressions: 2,897 checks. Their old Tsubaki test fixture
  incorrectly expected forward endpoint order during reverse traversal; the
  fixture now follows the reverse route. The map implementation was unchanged.
- Private Unity viewer: 30 on/off comparisons at 1x/4x MSAA, offline and both
  online viewpoints, with exact image restoration after disabling/re-enabling
  the recorded state. Representative offline and online captures were inspected.

The private player contains native DLL
`1dc862b1e3415bcd4f0b0ac15ac3b98c896a9785202a1d0f5870ae51087d2fff`.
All tests were muted and hidden/offscreen. The desktop and Editor asset DLL
hashes are unchanged. No release, inspection/physics change, Discord reply or
thread-resolution action was made. Local rollback fixtures do not replace
real two-PC EOS or full cabinet/Flycast verification.

## Legend ending camera framing and motion

The [Legend ending report](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1554605215767076905)
has a further local correction to the October 7 driving backdrop. The recording
matrix omitted the original actor's final half-turn and height offset, putting
the camera ahead of the car. It also reconstructed wrapped angles instead of
using the source actor angles. The original matrix boundary now produces the
camera anchors, and kind 13's authored target spring/noise runs at the original
tick rate. Its private presentation RNG leaves the completed race unchanged.

An independent test executes the original actor matrix, full camera initializer
and camera update, with only the exception-context provider hooked. The prior
matrix fails its first anchor comparison; the corrected implementation passes
306,027 comparisons over 18,000 frames and 32,437,911 original instructions.
Moving, stationary and discontinuous poses, spring velocity, RNG state and
phase-rounded FOV are covered. This test is registered in CTest alongside the
existing ending timeline test; both pass.

The actual App passes 43,219 checks: live two-car recording with optional archive
saving disabled, consistent camera targets at 30/60/144/240 FPS, pause/repaint
ownership, held-input guard, skip, final card/music lifecycle and cleanup. The
completed race digest and profile remain unchanged. Four offscreen captures
were inspected, including the corrected rear-facing framing and thank-you card.
Evidence and hashes: `Verification/ending-fidelity-20261111/findings.json`.

At this camera-only milestone, the last-3600-frame recording still held its
final pose. The subsequent capture/playback correction is documented below.
Ending lighting and independent course animation remain open. The result-flow
fixture does not drive all 31 races, and these native checks do not establish
packaged Unity or full Flycast/cabinet visual parity.
All tests were muted and hidden/offscreen. The DLL is staged only in this
verification directory; the desktop and Editor plugin hashes are unchanged.

## Legend ending highlight recording and second playback pass

The November 11 follow-up found that the initial playback investigation had
read the **base replay vtable**. The derived ending vtable at `0C387EBC` overrides
the buffer getters: `0EC000..0EC080` select a separate 5,000-frame highlight.
Only the secondary cursor uses the 3,600-frame recent-race ring. The previous
same-buffer conclusion in `ending-fidelity-20261111/playback-investigation.json`
is explicitly marked superseded, with its original claims retained for audit.

The game now records both bounded buffers. `0E1200` starts the highlight when
the raw HUD course-distance fraction is strictly greater than the authored
float 0.43 and the truncated accumulated-path lap number is zero. Once started,
it continues until full. The capture occurs at the source pre-solver boundary
(`05C13A`), including the final running update, independently of archive saving.
It does not advance the solver, change progression, or write an archive.

The ending plays the highlight first, then the source secondary cursor, and
preserves the final cursor reset instead of holding the last captured pose or
endlessly looping footage. The original second-pass setup adds 143 to the ring
end without wrapping positive overflow. For an out-of-ring or unrecorded slot,
the native renderer retains a valid highlight pose for that sample while
preserving the source cursor timing; it does not access unrelated memory.

Evidence: `Verification/ending-playback-20261111/findings.json`.

- The new registered source test passes **590,143 comparisons** over
  **19,403,155 original instructions**. It executes the derived getters,
  recorder controllers, descriptor bookkeeping, secondary setup, actor-copy
  selection and stage transitions. Only opaque actor-payload copying is
  represented by marker words. Empty, short, full and wrapped captures,
  threshold boundaries, lap gating, both cars and both playback passes are
  covered. All three ending CTest targets pass.
- The hidden, muted application test naturally crosses the finish in an Itsuki
  race after **12,134 running ticks**, fills the highlight, and passes
  **55,355 checks**, including pre-solver capture, movement at late credits,
  30/60/144/240 FPS, pause, repaint, skip, cleanup and unchanged race/profile
  state during the ending. Four inspected renders show different driving
  locations through the late credits and the final thank-you card.
- The separate settled-result fixture still traverses all 31 rival result
  owners into the ending and back to the title. It does not drive those races.
  Earlier final-rival driving attempts timed out before the highlight trigger;
  those failed logs are retained. The passing clip fixture does not establish
  a naturally completed final-rival race or full Flycast/cabinet parity.

The native DLL builds into the private verification directory. Desktop and
Editor plugin hashes remain unchanged. Packaged Unity verification, complete
ending car/scene lighting, independent course animation, and full source-owner
camera/actor ordering still require further work; the Discord report remains
open.

## Legend ending lighting and course animation

The next native ending pass now owns separate course, player and opponent light
sets. Replay cars use the course header ambient directly, as `0E2008..0E2032`
does, rather than inheriting the race parent's ambient adjustment. The recorded
ACar world matrix drives the source headlights and relative course lights; the
road-light gain is evaluated in both day and night. Course fog is bound for the
driving backdrop, then cleared for the final artwork.

At night, `0E4FC0` routes each car's headlight to the course and opposite car and
adds a camera spotlight to all three owners. The native ending now follows that
routing. Its camera light uses the original inverse-view/rotation/translation
sequence, dry/wet intensities, distances and equal cone angles. Akagi gets a fresh
course-animation owner, advanced once per original tick and never by a repaint.
Ending lighting, camera and animation are released on normal completion, skip,
new-race start and return to selection; the renderer drops its pointers first.

Evidence: `Verification/ending-lighting-20261111/findings.json`.

- The new source test passes **31,012 checks / 1,563,449 original instructions**:
  replay ambient setters, cross-car/camera registration, 256 dry/wet camera-light
  transforms and full eight-word light packets. The initial packet mismatch was
  a fixture error: it omitted source `1CEFE0`/`1CF180`, which initializes the raw
  diffuse flag. The test now executes that reset; production packet flags were
  not changed to accommodate the incomplete fixture.
- All seven targeted CTest tests pass: ending timeline, camera, playback,
  ending lighting, car lighting, course lighting and course animation.
- The muted, hidden App check passes **55,406 checks** (`app-09`), including the
  natural Itsuki finish and all prior frame-rate/skip/save/physics invariants.
  Separate 600-frame Akagi night dry/wet clips use simulated driving poses and
  a deliberately seeded highlight trigger. They verify camera-light intensity,
  fog binding, isolated animation clocks, repaint behavior and restart cleanup;
  they are not completed additional Legend races.
- Six offscreen captures were inspected: four day credits/final-card frames and
  both night conditions. The small scrolling photographs on the right are the
  original ending texture-14 artwork, not incorrect roadside-light textures.

This establishes the native lighting/animation milestone, not full ending
fidelity. Complete source actor/camera update ordering, replay-specific effects
(including projected road headlights/shadows/weather), and the packaged Unity
ending still require verification or implementation. The independent lighting
tests are bounded original-instruction checks, not a full Flycast/cabinet run.
The Discord report remains open. The private DLL is build 19; desktop and
Editor-plugin hashes are unchanged. Nothing was published or sent to Discord.

## Legend ending projected headlights

The common replay callback `0E3700..0E374A` draws each car's projected road
headlights at night. The native ending had omitted that pass. Ending playback
now retains two independent projection owners and road-contact queries. It
loads the original headlight texture bank after the car/plate banks and submits
the original emissive projected mesh with its source material.

The update keeps the original publication boundary: `034840` publishes the
previous working geometry, `E31A0` applies recorded blind-attack changes, then
`034E60` draws that published mesh and calculates the next projection. These
updates run on original ticks; repainting does not advance road queries or
republish geometry. Restart/exit clears the ending owners without touching
the live race headlights or physics.

Evidence: `Verification/ending-headlights-20261111/findings.json`.

- Extended original-instruction scheduling checks pass **1,664 cases / 11,108
  checks / 128,336 instructions**, including the actual ending vtable callback,
  night gate, independent car/projection presence, recorded actor pointers,
  and on/off transitions. Projection math and material submission remain
  independently covered by their existing source tests. All seven targeted
  CTest tests pass.
- Muted, hidden App verification passes **55,528 checks** (`app-11`), including
  bit-identical ending projection/query state at 30/60/144/240 FPS, the prior
  publication delay, repaint/pause stability, both blind-attack edges, texture
  offsets, separate live-race state, and restart cleanup. Night clips contain
  real simulated Akagi poses with seeded highlights, not full completed races.
- Dry and wet night captures were visually inspected. The dry capture was
  compared with `app-09`; the projected road illumination is now present.
- `app-10` failed because its assertion counted12 source strip vertices per
  light, while the renderer expands indices into triangles. The corrected test
  verifies the complete indexed geometry; production was not changed for it.

This closes the projected-road-headlight omission in native ending playback.
Full ending fidelity remains open: the complete actor/camera update sequence,
other replay effects (including shadows/weather/glare), and packaged Unity
verification are outstanding. The private DLL is build20; the desktop and
Editor plugin are unchanged. No release or Discord response was sent.

## Legend ending replay smoke and wet tyre spray

The native ending now replays the original rear-wheel smoke controller rather
than dropping the effect or borrowing live-race emission rules. The recovered
`0C0E3900` owner processes effects 2/3 of both cars, supplies display gain .05,
uses actor intensity without the live race's course multiplier, and has its own
wet/snow and Shomaru daytime dust branches. Admission, stop/drain/reuse and the
shared four-emitter/60-particle budgets retain source ordering.

The private ending recording now retains actor displacement, smoke intensity
and contact bytes at the existing pre-solver capture boundary. Playback uses
the recorded visual matrix and original wheel anchors. Camera and smoke consume
one private ending random stream; they do not advance the completed race.
Original density meshes and textures are appended before projected headlights.
Pause/repaint leaves particle state and RNG unchanged, and ending exit clears
the particle owner. No archive saving is required.

Evidence: `Verification/ending-smoke-20261111/findings.json`.

- Actual `E3900`, wheel-anchor/matrix routines, CarSmokeEvt start/attachment,
  SmokeMgr, emitter scheduling and particle motion: **24,640 ticks / 3,491,477
  bitwise checks / 97,106,160 original instructions**. Coverage includes both
  actor pointer bindings, zero/one/two/capped car counts, rigid transforms,
  wet/dry and dust/white transitions, stationary/moving threshold, alternate
  particle parameters and a fully drained pool. Allocation/list/TLS and the
  separate display boundary remain explicit interpreter boundaries.
- Eleven targeted CTest groups pass, including the existing live-race smoke
  controller, source particle/display/material/depth/view checks and ending
  timeline/camera/replay/lighting checks.
- Hidden, muted App14 passes **232,170 checks**. A natural Itsuki finish with
  replay saving disabled supplies the main recording. Akagi night fixtures
  use real moving poses with a seeded highlight trigger. Smoke state and RNG
  match at 30/60/144/240 FPS; recorded input bytes, pause/repaint, lifecycle and
  completed-race physics/profile invariants pass.
- The wet capture has 18 particles, four submitted clouds and 5,400 expanded
  mesh vertices. Suppressing only the private smoke owner changes 46 pixels
  (largest RGB-channel difference 9) in the matched frame. The effect is subtle;
  activity counters alone were not used as proof of visible rendering. The
  sampled natural dry ending frames had no submitted smoke, although smoke
  was active elsewhere in that recording.

Private plugin build23 contains the production change; build24/25 only extend
the application verifier. The desktop build and repository Editor plugin are
unchanged. No release, Discord message or resolved-report marking was made.

Full ending fidelity remains open. This does not establish the complete outer
actor/camera sequence or RNG interleaving with remaining weather/effect owners.
Rain/snow fields, other optical/shadow effects, cabinet/Flycast sequence matching
and packaged Unity ending validation still require work.

## Legend ending rain recovery — 2026-11-11

The ending still omits the original rain owner. Reusing `wet_weather.h` would
substitute its host particle placement/timing for the original behavior, so a
separate source-backed implementation is in progress. It is not connected to
the ending renderer yet and does not close the Legend ending report.

`original_rain.h` now retains the original 25 rain streaks, nine ground
splashes and 72 rain wheel-mist records, distinct from the existing smoke
manager's wet tyre smoke. First-free admission, bounded pools, shared random
consumption, birth geometry and lifetime updates match the recovered code.
Rain/splashes draw after advancing, including their final expired frame;
wheel mist draws before advancing. These are separate presentation snapshots.
Rain strips preserve source UVs, day/night alpha and strip control. Splash
and wheel-mist matrices preserve the original billboard/scale operations.

The rain birth code requires a general inverse view matrix, so
`invertOriginalMatrix` ports `1FC0A0` / `1FC620` rather than assuming a rigid
transpose. Its scaled pivoting, single-precision operations, determinant and
partial-matrix result for singular input are compared with original execution.

`original_rain_owner.h` now implements the complete `17F7E0` update schedule.
Motion uses raw actor XYZ separately from the visual matrix. The four wheel
contact values share one suppression flag per car: low nibble 6 **or** 7
disables both weather and glare. These numeric gates are verified; their road
material meanings have not been established. Wheel attachment points execute
through the source's actual part-transform helper in the reference test.

`original_rain_glare.h` now implements `17F500` and `17F260`. It retains the
source's differing primary/rival anchor-height factors, inverse-view eye,
single-plane culling, swept-ray endpoints, collision flag rejection, camera
offset, facing-based opacity, braking/blind-attack levels and random scale.
It consumes a random value for issued zero-alpha draws too. Material edits
preserve authored words and update both TSP/diffuse banks. The reference runs
the original material walker, matrix routines and random generator unchanged.
All 35 rain anchor records at `32437C` were compared with the existing replay
table `33C5F4`: their values and complete coordinate lists are identical.

Evidence: `Verification/ending-weather-20261111/findings.json` and
`ctest-03.log`. Private build47 passes **6,349 cases / 8,324,104 checks /
164,289,417 original instructions**. The full rain owner executes its actual
glare pass across 2,160 sequential updates; a previous synthetic one-draw RNG
boundary has been removed. Comparisons cover particle state, prior poses,
admission, wheel origins, ray endpoints, ordered glare draws, matrices,
material edits, caller restoration and final shared RNG. Separate glare tests
exercise 370 issued draws (113 zero-alpha), 398 rejections, 651 queries, and
1,588 rear-pass draws across all 35 cars. General/singular matrix tests,
saturated/recycled pools, unusual activity flags and birth/expiry checks remain.
Source collisions are supplied as explicit hit/flag fixtures; texture/model
lookup and final GPU publication are intercepted. This is bounded code-level
evidence, not a full Flycast/cabinet or integrated visual comparison. Build37's
earlier four-group regression result remains recorded in `ctest-01.log`.

Verification caught and corrected an initially misread brightness literal
(the source level-one factor is 70), and a reference-fixture memory overlap
between the dynamic rain strip and temporary glare model. The final test uses
separate regions and executes the combined routines without a glare hook.

Constructor and allocation verification now executes the successful original
allocator tail and all of `17E0E0`, with allocation/resource/publication services
as boundaries. The allocator zeroes the payload, establishing initial raw actor
history and motion as zero. The constructor copies each available car's current
visual matrix, initializes inactive particle records, and edits the authored
wheel-mist material to alpha 56 and rear-glare materials to alpha 80. Splash
material is not changed by the constructor. Construction consumes no RNG.

The ending call site `E2CA8..E2CDA`, effect-manager constructor `17D560`, and
rain admission block `17D8EA..17D90E` also execute in the verifier. They bind
the embedded replay view and pass **preserveStreakMotion=1**. The latter is
manager+92, not a field inside the view: it preserves the constructor's zero
streak motion rather than deriving a streak velocity from the car's position.
Both day/night and optional-rival bindings are covered. Resource admission and
the rain constructor are explicit hook boundaries for this call-site check;
the full rain constructor is tested separately above.

The view helpers now recover `1565A0/1564C0`'s upright look-at basis and
`1F66A0`'s rigid camera inverse. The general rain inverse and this camera
inverse are deliberately separate. The extended ending-camera reference test
executes original camera initialization/update, rigid inverse, projection-plane
setup and the rain glare's single-plane depth test together: **954,027 bitwise
comparisons across 18,000 frames / 39,115,911 original instructions** pass in
`camera-view-01.log`. Only the exception-context provider is hooked. These
helpers were initially verified separately. The runtime now uses them for
ending lighting and rain. The extended `camera-view-02.log` also compares the
runtime camera method: **1,242,027 comparisons across 18,000 frames**, with the
same 39,115,911 original instructions.

### Integrated rain and packaged ending verification

`OriginalRainPresentation` now binds the source owner to actual course collision
queries, all 11 authored model chunks/textures, and the original rear-lamp
anchors. Source ticks retain streak, splash, wheel-mist and rear-glare draw
snapshots. Draws convert the source view-space models through the inverse view;
they use the original unlit materials and still receive course fog. Repainting
does not issue road queries or consume randomness. The ending uses the recovered
`preserveStreakMotion=1` constructor mode; dry and snow-course endings do not
construct this rain owner. Snow remains separate unfinished work.

Private build49's native application test passes **272,754 checks** in
`Verification/ending-fidelity-20261111/application-15.log`. A genuine Myogi
Legend finish records 5,000 moving highlight frames. Separate Akagi night
dry/wet fixtures use real source-driven poses with an arranged early highlight
trigger; they are not claimed as another completed race. Rain, smoke, glare,
collision scratch and shared RNG match across 30/60/144/240 FPS. Pause, repeat
renders, recorded raw actor positions and cleanup pass. At the wet sample,
25 streaks, nine splashes, 72 mist quads and four glare draws are submitted;
2,217 glare draws are accepted by actual road queries over 600 ticks.

The native wet/rain-disabled/restored images were inspected. Removing rain
changes 9,520 RGB pixels; restoration produces identical pixels. The packaged
Unity player now also passes **441 checks and 12 captures** in
`Verification/ending-weather-20261111/unity-01`. Both selected wet views change
9,371 pixels when only rain is removed; both dry controls remain identical,
and all restored images match exactly. Native range counts, texture slots,
original/unlit/main-view flags and shared RNG are checked. This is a private
scripts-only build plus native build50, not a fresh shipping build. All runs
are muted before scene load and use an unshown Windows desktop.

The extended packaged pass `unity-02` adds per-family renderer comparisons:
**687 checks and 28 captures** pass. All native draws have matching Unity
renderers: one coalesced streak range, nine splash ranges, 72 wheel-mist ranges
and four rear-glare ranges in each wet sample. Hiding each family separately
changes visible pixels in both views: streaks1,069/449, splashes90/86,
mist4,903/5,852 and glare4,366/3,803. Every restoration is pixel-identical;
native frame generation, rendered-frame count and simulation ticks stay fixed.
The original shader/material/texture mapping is retained. Representative wet
two-car and later credits captures were inspected.

The first private staging attempt stopped because it compared packed and
unpacked asset hashes. The corrected verifier decodes the existing LZ4 blocks,
checks their CRCs, and compares decoded bytes with the source assets. No game
asset change was needed. The script-build helper restores the Editor plugin
and preserves the private player's packed scene data and boot configuration.

Visual review initially suspected two floating road/shrine pictures were a
scenery artifact. They are the original credits photo strip: ending model
chunk4, texture14, drawn separately from the course. The source scroll table
at262738 selects chunks3/4, and `OriginalEnding::paint` draws that photo strip.
The original atlas and source draw trace confirm its identity. No scenery or
photo removal was made; the prior artifact suspicion is withdrawn.

The separate snow owner was subsequently integrated and verified below.
Remaining work includes complete outer effect/RNG interleaving and full
Flycast/cabinet presentation comparison. The integrated
rain checks do not establish every ending condition or the whole final-rival
sequence. No desktop update, release, Discord message or report-status change
was performed; protected `D:\Current` and Editor-plugin hashes remain unchanged.

## Legend ending snow — 2026-11-11

The original snow owner (`1AD820`, update `1ADFE0`) is now separate from rain
and connected to ending playback. It reuses the original rain bank's texture0
(32x32), with its own source material: PCW `8A00072E`, ISP `93800000`, TSP
`941024D2`, diffuse `FF888888` and flags `6AA`. No new artwork or generic snow
emitter was substituted.

There are320 particle records and four admission attempts per source tick.
Birth, velocity, lifetime, the shared camera-facing quad basis and odd-slot
replay reflection match the recovered routines. Activity is sampled before
the update, preserving the last visible frame on expiry. Inactive source
quads collapse to zero; the host omits those degenerate triangles. Active
quads coalesce into one unlit main-view range with course fog. Rendering
consumes no randomness and does not advance particles.

Verification in `Verification/ending-snow-20261111`:

- `ctest-07.log` passes3,867 cases,43,281,719 bitwise checks and360,153,073
  original instructions. This includes constructor record initialization,
  the original snow/weather admission gate and replay-mode bindings,
  material/UV initialization, all320 particle updates, prepared vertices,
  published vertex copies, moving views, pool exhaustion/recycling and expiry.
  Allocation/resource/exception services, bounded integer division and final
  texture/GPU publication are explicit test boundaries. Constructor draw
  initialization is intercepted in its binding test and executed separately.
- Native build57 compiles the application and private Unity plugin. The
  application test `ending-fidelity-20261111/application-16.log` passes272,777
  checks, including the existing genuine Myogi finish and dry/wet regression.
  The added Akina Snow fixture uses real source-driven car poses with an
  arranged course/highlight condition; it does not add a Legend course or
  claim a natural snow-course Legend ending. All particle fields, shared RNG
  and update counts agree at30/60/144/240 FPS. Pause/repaint do not advance
  state, restart clears it, and race physics/profile bytes remain unchanged.
- At600 ticks the native capture has292 flakes in one range (1,752 vertices).
  Removing only snow changes1,806 RGB pixels; restoring it is pixel-identical.
  The visible scene was inspected.
- The private packaged Unity test `ending-weather-20261111/unity-03` passes
  **741 checks and34 captures** across dry, rain and snow. Snow samples at180
  and600 ticks contain290/297 flakes in one range each. Removing snow changes
  2,129/1,750 pixels; every restoration is identical. Both dry controls are
  unchanged and the prior rain-family comparisons still pass. A snow capture
  was visually inspected. This is a scripts-only diagnostic build with the
  new native plugin; no fresh shipping-build claim is made.

The build helper restores the Editor plugin; protected `D:\Current` binaries
remain unchanged. All rendering tests use an unshown Windows desktop and mute
output before scene load. No release, desktop update or Discord action occurred.
Complete outer effect/RNG interleaving and full final-rival/Flycast/cabinet
presentation parity remain open; these local fixtures do not prove them.

## Remaining ending effect identification — 2026-11-11

The Unity license retry succeeded in private script builds 06 and 07. The
latest muted, hidden Unity run remains `ending-weather-20261111/unity-05`:
892 checks and 58 captures, including the independently isolated rear ribbons.
No desktop or release update was performed.

The original manager's two remaining conditional effects are now identified
by their resource loads: constructor `17BCC0` loads `effect/bkfire`, while
`180520` loads `effect/flare`. The latter is daytime lens flare, not dust or
another road-contact effect. Its original visibility function is `180B00`.

Added the isolated `OriginalBackfireFields` state translation and an original
instruction reference test. Across 4,800 owner frames, 61,335 checks and
3,303,715 original instructions, the host matches the source's two-car trigger
order, actor-buffer selection, actor +5C bit1 admission, one-in-five trigger,
two-frame flame lifetime, random variation, shared RNG state and primary-only
sound requests. Coverage includes 854 flame submissions and 28 trigger requests
while the primary flame was already active. The source still requests sound
in that latter case without restarting the animation. Each enabled car also
consumes a variation random draw while its flame is inactive; omitting those
draws would change subsequent ending effect/camera randomness.

The test executes `17C5E0`, `17CAC0`, `17CB00` and the original RNG. Explicit
boundaries remain render-state setup, light-list preparation, actor matrix
getters, model lookup, GPU submission, sound dispatch and positive integer
division. Its final passing log is
`Verification/ending-conditional-effects-20261111/backfire-02.log`; the initial
attempt stopped on the interpreter's unsupported FPSCR division helper, then
used the same explicit integer-division boundary as other source tests.

That initial preparation is superseded by the integrations below and the
private lens-flare checks in `Verification/ending-flare-20261111/`. The complete
outer effect sequence still has untranslated calls; do not mark the overall
report resolved or describe these private changes as shipped.

## Legend ending backfire integration — 2026-11-11

The ending now captures the source actor +5C flags at the existing pre-solver
recording boundary and runs a private backfire owner after weather and before
lamp trails. It uses the actual player/rival appearance words: the Evo III
needs its source misfire flag and one of the three authored exhaust mounts.
Ordinary replay archives, driving RNG and saves are not involved.

The flame uses its original two-frame animation, shared ending RNG decisions,
per-frame length variation, mount correction and published ACar world matrix.
The separate point light is appended to copies of the ending lighting scopes,
so repainting cannot accumulate lights. Original primary-only sound requests
pass through the existing sound-set guard; ending sound set0 has no race SFX
bank. Live-race/replay audio-driven backfire presentation remains unchanged.

Validation:

- `backfire-04.log`: 4,800 source-owner frames, 74,999 checks and 3,303,715
  original instructions. All 16 submitted transform words match the original
  for 854 flame submissions, including translated/rotated cars and all mounts.
- Private native build74/application23 passed 320,785 ending checks. Real
  source-driven Evo III/rival recordings preserve the actual actor flags;
  30/60/144/240FPS produce identical effect state and RNG after600 ticks.
  Pause, repaint, light restoration, profile/physics isolation, admission
  exclusions and restart cleanup pass.
- Private Unity build11/run04 passed the actual native publication, material
  and pixel comparisons for three exhaust mounts plus the rival. Twenty
  captures include full-owner and independently isolated flame removals; all
  restorations are pixel-identical. The rival view deliberately moves only
  the diagnostic camera to a real recorded rival trigger, because that rival
  can be outside the ordinary player-following ending camera. Production
  camera behavior is unchanged.
- The existing weather/trail Unity suite passed again in run06, with all58
  previous captures byte-identical. Subsequent changes only extended the
  backfire diagnostic footage/camera; the production runtime is unchanged.

Initial checks exposed two fixture assumptions, not production corrections:
the old lighting assertion expected pointers to the base scopes instead of
the new draw copies, and the new actor assertion compared a pre-solver
recording with the next solver tick. Both now check the actual boundary.
The first rival Unity capture was outside the camera and proved no visible
flame; it is superseded by the explicitly labeled rival-view diagnostic.

Evidence is in `Verification/ending-conditional-effects-20261111`. This is a
private fix, not deployed to `D:\Current` or GitHub. Daytime lens flare, full
outer effect ordering and a complete original-hardware ending comparison
remain unfinished.

## Legend ending rear lamp trails — 2026-11-11

The ending also constructs replay lamp owner `1A8AE0`. Its omitted rear lamp
sprites and projected nine-sample ribbons now run after weather updates and
before projected headlights, using the original assets and recorded ACar
matrix. The ending retains only one current sample plus its bounded history;
rendering cannot advance collision queries, history or random state.

The camera-cut signal is based on the selected camera-list index, not the
recorded replay cursor. The authored ending file contains one kind13 camera.
Original `0A9DA0` single-entry wrap and `0E36A4` change detection pass 6,000
cycles without a cut. No invented trail reset is added at the highlight/recent
recording transition. Explicit camera changes still use the original nine-tick
suppression. Night, recorded lights-off, visibility and occlusion gates are
retained; the replay owner's front-trail loop remains disabled.

Evidence: `Verification/ending-trails-20261111/findings.json`.

- Original effect manager construction, linked-list dispatch and camera-cut
  checks: 72 admission cases, 12,628 checks, 1,565,124 original instructions.
  Weather precedes conditional engine and dry-day effects, then the lamp
  owner. Child resource constructors/updates are explicit test boundaries;
  this does not establish the entire outer ending update order.
- Existing lamp component comparisons remain passing: 3,976 cases, 259,390
  checks and 3,692,213 original instructions.
- Native App18 passes 315,190 checks, including the earlier genuine finish,
  ending rain/snow and lighting regressions. Lamp-specific checks cover both
  cars, identical 30/60/144/240 FPS history, cached/streamed geometry agreement,
  recorded lights-off, nine-frame cut suppression, daytime admission, paused
  repaint, unchanged race/profile and restart cleanup. A moving Akagi clip
  submits two ribbons and 120 lamp vertices at tick121. Hiding the lamp owner
  changes 357 pixels; restoring it produces zero differences. This clip uses
  real driven poses with an arranged early highlight trigger.
- Private Unity build07/run05 passes 892 checks and 58 captures across dry,
  wet and snow ending views. Material-specific suppression isolates the
  ribbons from their lamp sprites: two ribbons change 58 pixels in the dry
  tick600 view, and one changes 17 in wet. Frames with no ribbons change zero
  pixels. All restored comparisons are exact, and weather checks still pass.
  The earlier run04 texture-only filter also hid sprites and is not evidence
  of ribbon-only rendering; run05 supersedes that claim.
- The updated shared attract cache passes its full native check. All 24
  comparable App04 screenshots are byte-identical to App03. Reused ending
  samples explicitly clear old visibility/opacity before preparation.

All player checks ran muted and hidden. The first Unity attempt was rejected
by an expired licensing session; after the owner refreshed it, builds06/07
succeeded. `D:\Current`, the Editor plugin and GitHub remain unchanged.
Complete original visibility-owner execution, the other conditional ending
effects, full outer RNG ordering and a cabinet/Flycast optical comparison
remain open.

## Attract rear lamp trails — 2026-11-11

The reported missing rear-light streaks were absent from
`OriginalDemoPresentation`. The opening's effect manager constructs the replay
lamp owner (`1A8AE0` / `1A9CE0`), whose rear gate differs from the ordinary
driving owner. Its front-lamp loop explicitly supplies a disabled flag at
`1A9C64`; the existing separate front headlight glow is retained.

The private implementation now exports the unchanged taillight textures,
sprite chunks and all 35 car anchor records. Replay table `33C5F4` and driving
table `337AD0` have identical record contents. The renderer prepares the
nine-sample projected history at recorded-frame cadence, clears it through
camera-cut/light-off/visibility boundaries, and submits the original ribbon
UV/alpha progression plus the lamp sprite. Collision queries use private
presentation state during loading, not per repaint. The cache is approximately
1.07 MiB. Rendering and seeking cannot change race physics or save data.

Verification in `Verification/attract-trails-20261111`:

- Original-instruction comparisons pass **3,976 cases, 259,390 checks and
  3,692,213 instructions** across driving/replay gates, cut suppression,
  history, projection, strip expansion/normalization, UVs, opacity, controls
  and dynamic material initialization. Final publication/draw leaves are
  intercepted; this is not a complete emulated owner or cabinet comparison.
- Silent, isolated offscreen App03 checks all **5,837** recorded frames:
  **1,142** visible lamp samples and **993** eligible ribbons. Both cars,
  cut/light-off boundaries, finite captured geometry, backward seeking,
  repaint stability, D3D rendering and Unity scene-packet texture/light/camera
  bindings pass. The existing full attract-owner loop and Start behavior pass.
- Initial visual review caught an incorrect double ride-height translation.
  Trails now use the stored `ACar+964` matrix from `034840`, before `034C20`'s
  separate body translation, with the source owner's own 0.3Y anchor lift.
- In the final captures, removing only ribbons changes **12,072 pixels** at
  frame 2567 and **31,005 pixels** at frame 3036. Removing glow and ribbons
  changes 12,203 and 31,825 respectively. Representative distant and close
  views were inspected; activity counters alone are not the rendering proof.
- The packaged Unity player now passes `unity-02`: **392,037 checks** and
  **30 GPU captures** at ten recorded frames, including the light-off interval,
  both cars' rear views and later camera sections. Hiding only ribbon renderers
  changes 1,019/394 pixels in distant views and 27,821/71,693 in the two close
  views at 2560x1080. Restoring them reproduces identical pixels in every case;
  native geometry, lighting, fog and simulation clocks remain unchanged. All
  native ribbons have matching Unity renderers and retain the original unlit
  material. The test was muted before scene load and used an unshown desktop.
  The first launcher attempt was stopped before verification; its output path
  incorrectly existed already. The corrected run uses a new capture directory.

Private plugin build33 and native build32 compile. The three protected files
in `D:\Current` and the repository Editor plugin retain their previous hashes.
No desktop update, release, Discord message or resolved marking was performed.
Packaged Unity playback now passes. Full Flycast/cabinet optical matching and
the complete source visibility-owner trace remain unverified; component checks
do not establish complete sequence parity or every host camera adaptation.

## Legend ending road-contact leaves — 2026-11-11

Post-race playback now advances and draws the original replay leaf owner using
recorded primary-car wheel contacts, actor displacement, visual matrix and
authored wheel positions. It uses the replay constructor's 300-particle pool,
three original model chunks, source lifetime/wake/floor/rotation behavior and
the shared ending RNG at `E4320`, after smoke update and before weather/backfire.
Rendering and pausing do not advance the owner. Active particles are bounded;
slot allocation uses a fixed occupancy table to avoid nested list searches.
This changes ending playback only, not the live-race host leaf adapter.

The collision assets admit these leaves only on Usui and Happogahara (low
material nibble 1, 2 or 11). Myogi and Akina have no eligible triangles. The
initial application fixtures on those two courses correctly produced no leaves
and failed the positive-coverage check; they are not passing visual evidence.
The corrected fixture records ordinary Usui Legend driving and only arranges
the early highlight threshold. Its 5,000-frame clip contains 1,317 eligible
wheel contacts. At replay tick 1,305 it has 12 leaves, 12 ranges and 72 vertices.

Reference verification executes the original replay caller, particle pool,
allocation/recycling, wake, particle update, matrices and RNG. The remaining
hooks are exception TLS, profiling, model lookup and GPU submission. Across
2,514 cases, 10,251,021 bitwise checks and 242,960,920 original instructions,
the lifecycle matches, including pool exhaustion, draining and reuse.

Native application checks verify visible leaf contribution (71 pixels), exact
removal/restoration, pause/cleanup, unchanged live physics and identical active
leaf state/RNG at 30/60/144/240 FPS. The frame-rate comparison reaches tick
1,320 and requires active leaves; the earlier tick-600 comparison was before
the first eligible contact and is superseded. A private Unity check also
verifies actual leaf geometry, texture binding and main-view ownership, with
91 changed pixels and zero restored differences across three captures. Both
native and Unity captures were visually inspected.

Final evidence is under `Verification/ending-outer-order-20261111/`:
`reference-06`, `application-06` (364,292 checks), `unity-leaf-02` (47 checks,
three captures), and `findings-02.json`. These tests use
isolated saves, disabled audio and offscreen/unshown rendering. No ordinary
save, `D:\Current`, repository Editor plugin or GitHub release was changed.
This establishes the leaf owner and its integration, not complete ending
visual parity or complete shared-RNG order while other outer calls remain.

## 2026-11-11 — original ending tyre marks

Continued the ending-screen work after the user asked to move on from mirror
smoke. The ending recording preserved actor+84 tyre-smoke intensity but omitted
the independent actor+88 tyre-mark bytes. Its outer playback also omitted
E4000 and the 151700 mark draw pass. Both are now present in source and the
private player; ordinary live-race presentation is unchanged.

`original_tire_marks.h` translates the two-rear-wheel admission loop (at most
two cars), its world-Y contact offset and original wheel normals, the 0.2-unit
ribbon width, five-update sampling, ten pairs per strip, twenty strips per
stroke, and the ending's fifty-stroke recycling budget. It retains the original
black/brown packed colour rules, bounds, camera culling and newest-first draw
order. Material words are checked against the executed original model builder.
There is one explicit finite-data adaptation: a coincident/vertical movement
that leaves the original output coordinates uninitialized does not admit a new
pair. Previous valid data stays intact.

`OriginalEndingTireMarks` consumes the recorded bytes at the fixed ending tick,
between smoke update and leaves; its draw pass follows smoke and precedes the
weather/effect list. Wet endings skip it. Reset, pause, independent playback RNG
and completed-race physics remain covered. No host slip threshold or random
mark generation substitutes for the original signal.

Evidence under `Verification/ending-outer-order-20261111/`:

- `tire-reference-05`: 2,267 cases, 2,196,603 bitwise checks and 3,437,808 executed
  original instructions. Full stroke/strip construction, vertex pools, geometry,
  sampling, colours, bounds, material construction and culling execute. Lifecycle
  boundaries are heap allocation, exception TLS and renderer cache/texture
  binding; the separate admission test observes the acquire/submit boundary.
- `application-09`: 364,317 combined ending checks. Genuine Usui capture carries
  nonzero rear-wheel mark data; playback tick 510 draws two strips. Removing
  them changes 134 RGB pixels, and restoration is exact. State agrees at
  30/60/144/240 FPS, pause freezes it, wet playback admits none, cleanup removes
  it, and the original live-physics digest is unchanged. `application-08` counted
  framebuffer alpha too; its 4,616 count is superseded by the RGB-only result.
- `unity-tire-01`: 25 checks, three muted hidden captures, two strips / twelve
  vertices through the packaged Unity scene. Removing marks changes 985 RGB
  pixels; restoration has zero differences. Native and Unity captures were
  visually inspected. The fixture arranges an early highlight, retaining real
  recorded poses and effect bytes; it is not a full final-rival cabinet capture.

`findings-03.json` records hashes and scope. `unity-build-02` is private only.
`D:\Current`, the repository Editor plugin and GitHub are unchanged. Full
ending visual equivalence and complete shared course-owner ordering remain
separate verification work.

## 2026-11-11 — ending optical headlight integration

The live Discord forum review still showed the November 8 story-cycle report
as its newest thread. Continued the unfinished ending-presentation coverage
after the user asked to move on from stopped mirror smoke. The ending drew
road projections but did not submit the separate optical lens sprites. The
original outer routine gates this pass on night at E3700 and calls D36A0;
the host ending now loads and draws the same lens adapter as live races.

`OriginalEndingHeadlightGlare` reads each recorded car's visibility, light bit,
pop-up fraction and body pose, with independent rival admission. It is read-only
and main-view-only, resets with the ending and uses a separate texture-bank
offset after leaves. It retains the existing live adapter's close-camera lens
attachment and opacity correction. This is an integration fix, not a new
bitwise translation of the original glare owner; the normal ending camera and
live-race glare implementation are unchanged.

Evidence under `Verification/ending-outer-order-20261111/`:

- `application-11`: all 364,383 ending checks pass, including both recorded cars,
  independent light-off and closed-pop-up admission, invisible/rival-hidden
  suppression, day/reset exclusion, material scope and texture-bank bounds.
  Matched dry/wet native captures change 4,500/4,721 RGB pixels with exact
  restoration and unchanged ending RNG, timeline and completed-race physics.
- `unity-glare-01`: 29 checks, three muted hidden packaged captures; 4,769 RGB
  pixels change, restoration differs by zero pixels. The native geometry,
  textures, emissive flags and main-camera ownership reach Unity correctly.
- `application-10` deliberately remains as failed evidence: the normal sample
  600 ending camera sees the cars from behind and correctly submits no glow.
  The final pixel comparisons arrange a front-facing inspection camera over
  genuine Akagi poses. They verify lamp visibility, not original ending camera
  equivalence or a naturally occurring frontal shot. Both native and Unity
  images were visually inspected.

`findings-04.json` records hashes and limits. `unity-build-03` is private only;
the installed `D:\Current`, repository Editor plugin and GitHub are unchanged.
No Discord messages or resolutions were posted. Full ending visual equivalence
and original outer-owner interleaving remain separate work.

## Remaining reports

The Tsuchisaka dirt effect now passes the private native and Unity race checks
above. New recorded dust and seeking also pass the native and packaged-viewer
checks above; live two-PC capture and shared-manager fidelity remain open.
The combined original smoke/dust controller now also runs in normal original-
course races and the online rollback checkpoints, with native full-scene and
both-role App checks. Ordinary original-course combined recording, exact seeking
and packaged replay-viewer checks now pass as detailed above. Confirmed-frame
online App recording now also has both-slot native tests, actual compressed
archive checks and packaged viewer checks. The source outer smoke/tire order,
shared driving seed, rollback digest and historical playback now pass the
additional checks above. Complete boot/menu and other course-owner RNG
interleaving, and real two-PC EOS capture, remain open; local fixtures do not
establish field parity.
The reporter's real VPN stutter and physical wheel validation remain open.
LAN now has the two local corrections and tests above; the reported Wi-Fi
missing-peer behavior and real two-PC LAN/firewall checks remain unverified.
The challenge banner now has source motion/mesh corrections and private GPU
checks, plus native audio cutoff/owner-freeze corrections with a failing-before
and passing-after DLL check. The packaged TCP admission, race transition and
disconnect recovery now pass, including the player-only silver-backing fix.
Its captured background now has exact source-strip checks, GPU capture/freeze
checks, a comparison to the report author's recorded video, and passing packaged
admission/disconnect tests. The accepting-challengers badge is now included
and shaded inside the frozen picture, with the checks above. The source sound
shutdown trace also exposed and corrected the DSP-memory reset. The new active-
voice trace distinguishes source force-mutes from engine key-off tails. The
native mixer now preserves those tails with exact output-clock and cancellation
checks; source hardware/device timing, calibrated listening, the original GIF/physical cabinet
comparison and real Internet-service admission remain open.
The TIME EXTENSION sprite now passes the packaged replay-viewer checks above.
The Happogahara follow-up now has the local reproduction and correction above;
the reporter's connection and two-PC EOS behavior remain unverified.
Imported-course startup now has a reproduced premature readiness signal and
passing private Unity checks above; real EOS/cold-loading confirmation and the
reported repeated-preload performance loss remain open. The latter did not
reproduce in the 78-switch offline check or the two-client TCP preload/lobby
checks above. Real EOS, longer sessions, reporter hardware and foreground
frame pacing still need verification.
The wall/gutter contact queue and targeted volume adjustment have the local
checks above; listening confirmation and calibrated cabinet comparison remain open.
Inspection's corrected presentation-only flow
has passed the local native and packaged checks above. No Discord thread has been marked resolved
or replied to, and no release or desktop installation was updated.

## 2026-11-11 — automatic card renewal at zero plays

The owner corrected the entry condition: renewal must appear only when the
remaining play count is zero, never as a Save Select action. They additionally
confirmed that races should consume a play and renewal should restore 50,
while tuning and other stats stay unchanged. This supersedes the manual
Inspect Car entry point and its no-counter-change behavior.

Save Select now contains Continue, Change Car and Delete Save only. The old
inspection confirmation, navigation index and pointer target are removed.
Continue reads the selected card's stored counter. A zero-play card receives
an atomic counter-only reset to 50 before the original completion banner is
shown. The six-row summary stays read-only; there is no transmission/package
renewal editor and no reset of parts, tuning, points, records, identity, options
or aura. Exiting or timing out of the completed presentation cannot renew twice.
Each garage car retains its own profile counter; other cars and drivers are
untouched. Ordinary new host profiles start with 50; existing positive counters
are retained. The source reference profile and arcade acceptance functions are
unchanged.

An actual solo race arms one consumption. Finish or timeout decrements once,
saturating at zero, alongside normal result persistence. Repeated result/tuning
callbacks and post-finish simulation frames do not consume another play. Menus,
previews, Full Tune and replay playback do not count. Renewal is checked on
returning to the mode/course menu and before the next solo race starts, so
continuing a session cannot bypass an exhausted card. Failed renewal writes
preserve the card and return to Save Select, including failure during loading.

Online results include the absolute counter change in the existing recoverable
result journal. A win/loss retains its existing reward; a double timeout uses
one play without adding points or battle history. Reapplying/recovering the
same receipt cannot decrement again. The stored garage card supplies the
counter, not the normalized race-physics profile. The result snapshot restored
when leaving online play retains the updated counter. Renewal presentation is
deferred while the online lobby/race covers the menu.

Native evidence: `Verification/card-renewal-20261111/native-03/` passes 39,651
App checks across 292 car/package/transmission combinations, frontend
navigation/rendering regression, and 33 online reward checks. A real source
Time Attack race naturally timed out after 5,587 updates with one play left:
its saved count reached zero once, stayed zero through another 180 simulation
updates, then automatically renewed to 50 on menu return. No timer or result
was forced. Writer-lock failure, retry, concurrent progress, reopen, timeout,
old button coordinates, exact unrelated profile bytes and other-driver
isolation are checked using private saves only.

Managed settlement passes 52 checks. Journal interruption/recovery passes 48
checks both with the simulated persistence boundary and the actual native
recovery DLL, including the new double-timeout counter case. These are local
persistence tests, not a claim of live two-PC EOS validation. Final packaged
locale reports and protected-build hashes are indexed in the verification
summary in the same folder. All player tests are muted and hidden. Nothing
was installed into `D:\Current` or published.

Final packaged verification: private player `unity-build-05` passed 40,332
checks in each of English, Japanese and Simplified Chinese, with clean shutdown
and exit 0. Save-action removal and the original renewal banner were visually
reviewed. Reports: `unity-0-02`, `unity-1-01`, `unity-2-01`; exact sources,
protected-file hashes and outcomes: `verification-summary.json`. The earlier
English `unity-0-01` passed behavior but retained an obsolete report scope string;
the final build/report corrects that description.

## 2026-11-11: minimap scale, artwork and colour follow-up

User supplied a Flycast reference and a widescreen remake capture at maximum
zoom-out, and requested this after the plays-left fix. The previous minimap
derived its coverage from road width and drew a generated centreline over an
estimated grey-green field. Replaced the stock-course map with its original
navigation model, texture bank, five-component navigation path and per-course
view extent. Restored the 96-pixel source-canvas frame, the source marker art,
day/night material tint and texture transparency. Akina Snow shares Akina's
navigation bank. Source path heading governs the stock map's orientation.

Evidence comes from the local original program (SHA-256
`efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`),
the 36-byte records at `0C2FB118`, navigation initialization at `0C0C24C0`,
update at `0C0C3240`, and the user's HOSTFS navigation assets. Extracted table,
disassembly, asset input/output hashes and verification results are under
`Verification/minimap-fidelity-20261111`. All 64 input/output file checks pass.

The 100% setting now uses the arcade extent; 75% and 50% retain additional
zoom-out without changing marker size. Saved option values are preserved.
Imported courses use the same field colours and a fixed 384-metre base window,
matching Akina's navigation coverage instead of depending on road width.
Their route is generated because those courses have no original navigation
bank. Existing opponent edge-clamping and blind-attack suppression remain.

Added opt-in source texture filtering for navigation. Both the native preview
and Unity draw stream honour the navigation material's bilinear sampling;
other UI retains its existing sampling. Native verification passes 515 map
cases, including a four-texel colour check, original-track directions,
day/night, road-width independence and viewport containment. The HUD suite
passes 1,060,275 layout/name/code/gap/map/timing checks plus saved-profile,
entrance-animation and advantage checks. UI capture retains its command hash
and zero warmed copy allocations, and checks that filtering stays opt-in.

These source settings do not establish pixel-identical whole-scene colour:
the transparent map also shows the underlying road, and the two supplied
screenshots differ in camera position, aspect ratio and scene rendering.

Final checks also cover wrapping multi-lap Myogi/Usui opponent positions. The
packaged native loader passes the same 515 map cases. Muted, hidden Unity
captures cover 640x480 and 1280x720, 100% and 50% zoom, Akina day/night/reverse,
and Irohazaka's autumn course. Imported Hakone verifies rendering at its
native spawn, which is beyond the diagnostic's 10%/17% thresholds; this is
not full-route coverage. The final Irohazaka run passed 68,912 checks, exited
cleanly, and its eight PNGs are byte-identical to the clean-core comparison.

The Unity engine/shader build succeeded, but post-build asset compression
encountered MAX_PATH: a 252-character filename plus `.packing` exceeded the
limit. Moved the private output to `Verification/nav-20261111`, resumed the
lossless packer and verified all 17,321 runtime files. All 761 packed assets
passed decoded-byte verification. The final native DLL includes the lap fix;
its hash, full runtime inventory, source hashes, diagnostic corrections and
all outcomes are in `Verification/minimap-fidelity-20261111/verification-summary.json`.
This is a private staged build, not an installation into `D:\Current` or a
GitHub release. Protected project/current binaries remain unchanged.

## Two-car renderer allocation spikes — November 11 follow-up

Re-read the live Discord `FPS Drop Issues` thread
(`1552613619269640242`) and the visible `Akagi frame fluctuations` report.
The reported scope remains Akagi/Akina, two-car online races, uphill, night,
and Akagi CP3/CP4 fluctuations. No messages, reactions or issue status changed.

Split renderer diagnostics into buffer resizing, complete material setup and
queue-only changes. A controlled full Akagi night Legend traversal showed
113 mesh-buffer resizes and 120 complete material setups at the 95th
percentile. Moving ranges lost their previous allocations when a scenery
window changed their source indices, even when suitable buffers/materials
were still available.

The renderer now reserves all exact geometry matches first, retains compatible
moving allocations, and reuses remaining allocations with identical material
state and vertex count when at least eight ranges need them. It still compares
all changing vertex bytes, uploads changes, updates visibility/light ownership,
and assigns the original per-range draw order. This does not reduce source
geometry, drawing distance, lighting quality, resolution or source draw counts.
`-idas3-range-allocation-baseline` disables this allocation reuse for comparison.

The accepted guarded comparison covers 10,286 matched Akagi night Legend
frames. Mesh resizes at the 95th percentile fall from 113 to 3; complete
material setups fall from 120 to 3. GPU-completed offscreen wall time changes
from 6.4025 ms to 5.4091 ms at the 95th percentile (15.5% lower), and from
4.1614 ms to 4.0533 ms on average (2.6% lower). Renderer median increases
slightly, from 0.9191 to 0.9413 ms. This is a reduction in allocation spikes,
not the requested 30% overall FPS gain or proof that the online report is fixed.

The final default-enabled build also passes 10,286 Legend frames. Its native
progression and three captured images match the accepted candidate exactly.
A separate final-build uphill Time Attack baseline/default comparison covers
10,858 matched frames per run. Its average/95th-percentile offscreen times
are effectively unchanged (3.2481/4.4384 ms versus 3.2578/4.4623 ms); its
three images also match exactly. Mesh/index bytes, material ownership, unique
range ownership and draw queues are checked against native source data at
captures, outside measured frames. Together the five accepted runs cover
52,574 measured frames and pass 735,418 checks with clean shutdowns.

All tests used muted private desktops, a complete main/mirror/HUD camera
stack, 2560x1080 AA4, and synchronous GPU completion. These timings include
readback/synchronization and are not displayed FPS, pure GPU timings, a live
EOS race, or reporter-hardware verification. Two earlier experiments remain
recorded: position-only reuse did not improve averages; unconditional pool
construction increased median cost. Prototype-02 also overlapped a filesystem
enumeration and is excluded from final timing claims.

Final evidence: `Verification/two-car-performance-20261111/verification-summary.json`.
The latest private player remains `Verification/nav-20261111`, with its current
hashes in `Verification/two-car-performance-20261111/unity-build-04/private-player.json`.
This supersedes the managed-assembly hash in the earlier navigation package
manifest; the verified navigation/card native DLL and player data are unchanged.
No installation into `D:\Current`, GitHub publication, or Discord post occurred.
The broader bugs-channel goal and real online/performance verification remain open.

## Linux auto-updater revalidation — November 12 follow-up

Read the live Discord report `Auto Updater does not work on Linux again`,
posted by Tootis on September 25. The report says the updater failed again
across the author's Linux devices; it supplies no error log, distribution or
Wine/Proton version. No Discord message, reaction or issue status changed.
The September 27 release commit `8092319` subsequently added canonical
Steam-library and temporary-folder handling. This pass checks the current
implementation rather than assuming the older report still reproduces.

The existing Wine test runner had drifted: it selected the obsolete
`Builds/MenuFontCheck` diagnostic and wrote its configuration outside the
marked disposable fixture now required by the diagnostic. Its native suite
also selected an October 7 helper build instead of the newer embedded helper.
The runners now use an explicitly selected private diagnostic, test the exact
embedded installer by default, retain build/helper hashes, preserve separate
evidence folders, and explicitly mute the player. The isolated diagnostic's
ownership check compares canonical directories, so a genuine alias can be
tested while the game remains a direct child of its marked fixture. Missing
markers and foreign game directories are still rejected before downloading.
Production download, staging, cache and native installer code was unchanged.

A fresh Unity 6000.6.0f1 diagnostic build passed in a hidden desktop. The
exact embedded helper passed all ten native Wine cases: clean application,
linked-file/tampered-stage/concurrent-change rejection, parent exit and
restart, C:/Z: aliases, and both overlapping-folder rejections. The final
real Unity downloader/Mono staging/native handoff suite passed fifteen cases
under Wine 9.0 in WSL with Xvfb. Seven cases installed and restarted:
full, Steam-library alias, patch, damaged-retained-file fallback, split
core/course download, split patch-to-full fallback, and temporary-folder
alias. Their installed hashes, embedded helper and payload cleanup were
verified. Eight cases rejected invalid or interrupted input with old files
intact: internal file/directory links, corrupt core/course downloads,
interrupted course transfer, linked cache root, unmarked fixture and foreign
game folder. All cases preserved the private save and original-data sentinels.

Evidence is indexed in
`Verification/linux-updater-20261111/verification-summary.json`; the fresh
diagnostic lives in `Verification/lu-20261111`. The initial eleven-case
`unity-flow-01` run is retained; `unity-flow-02` adds the current split-course
format and exact helper/status/cleanup verification. Both are isolated
loopback tests with inert replacement executables, not a public GitHub upgrade
or live game session. The report was not reproduced in this environment;
reporter-specific Linux/Proton and Steam Deck verification remain open.

All players/builds were muted and hidden. The project native DLL, `D:\Current`
and the staged navigation/card/performance player retain their recorded hashes.
No current installation, GitHub release or Discord post changed. The broader
bugs-channel goal remains active.

## Tsubaki fork island collision — November 12 follow-up

Read the live Discord report `Fix the physics at the fork on Tsubaki Line`
(thread `1558534007170400306`), its six replies and the attached 12.63-second
video. No Discord content or status changed. The silent extracted video
frames show a narrow planted island separating two paved branches, with
bollards and turn/no-entry signs. The report attachment and its hash are
retained under `Verification/tsubaki-fork-20261112`.

The importer converted authored driving-limit paths into a continuous road
ribbon. Those paths enclose both branches and the island, so the resulting
contact data incorrectly made the island drivable. The original Stage 8
collision asset contains a separate blocked component at this fork: triangles
24296–24321, bounded by 28 vertices, with an area of 31.316571 square metres.
Source collision SHA-256 is
`83f7a18310ec392c9fcc9b751e1fb88798396625387c3a657979bb6dd16dcd93`.
The source contour and provenance are exported to `fork_island.json` during
import. The converter inserts cross sections at every contour corner and
preserves that blocked region with connected RCL1 neighbours; pavement on
both sides remains available. It does not replace the original D3 solver.

The independently surveyed native boundary fixture failed on the old data:
all 252 attempted curb crossings passed through. The corrected data passes
all 252 crossing/long-step cases and all 84 clearance cases. Six Python
geometry/guardrail tests verify closed reciprocal adjacency, source boundary
vertices and area, 16-bit indices, unchanged road paths and weather rails.
The complete imported-course suite passes 10,050 cold lookups, 29,232 per-tick
edge steps, 87,744 route projections, 14,616 outward sweeps, 21,918 interior
lane sweeps, cached traversals in both directions, race gates and 6,240 D3
solver ticks. The original 14-course collision reference still passes
2,794,116 exact comparisons against 32,618,528 original instructions.

Race paths, checkpoints, original road data and scenery retain their previous
bytes. Outside the fork cross sections, 19,712 complete collision vertex
records and 29,553 triangle geometry/material/flag records remain identical;
only their indices shift where local rows were inserted. Collision size grows
by 28,352 bytes. Repeating the conversion produces identical output hashes.
No handling parameters, graphics, course scale or player controls changed.

The first hidden Unity coverage run demonstrated contact at the island but
failed full-route completion: its simple centreline driver kept aiming into
the new barrier at source point 2966. The failure remains recorded as
`solo-01`, not a passing traversal. The opt-in, isolated-save diagnostic now
slows and follows the driver's left branch around this surveyed fork;
forward and reverse runs therefore exercise opposite physical branches.
This steering code is gated by the existing diagnostic mode and never drives
a normal player's car. Subsequent complete traversal results are recorded
below after verification.

The private navigation/card/performance player received the collision data
and diagnostic update. Its current manifest is
`Verification/tsubaki-fork-20261112/private-player.json`. The preceding native
DLL is retained as `before-diagnostic-driver.dll`. The older native/map
manifest remains historical evidence; use this newer manifest for the current
private player. `D:\Current`, the project plugin and public releases remain
unchanged.

Final hidden Unity runs both finish: 13258 downhill frames and 13955 uphill frames, with 82223 checks and clean shutdowns. Both open branches pass the fork without any wall-contact frames (288 downhill and 237 uphill frames in the surveyed interval). Neither run needs a diagnostic timer extension. The complete main/mirror/HUD stack is rendered every measured frame; these are offline coverage checks, not displayed FPS measurements or live online tests. Final evidence is `Verification/tsubaki-fork-20261112/verification-summary.json`. All protected current/project hashes remain unchanged. The broader bugs-channel goal remains active.


### Momiji roadside-tree report investigated (November 12)

Read-only Discord report: https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1558002348700340224
("momiji line", October 9). The sole attachment is a cropped blue-tinted
photograph of a large trunk beside an inside curb. It does not identify the
route position or show the car passing through the trunk.

The current pack retains the source archive hash, both original collision
files byte-for-byte, all three road paths, and all 2,303 authored tree
transforms. The staged player's 171 Momiji files match the runtime pack.
A geometric survey of 31,477 steep static scenery triangles found no
crossings through the inner 90 percent of road width at 0.8 or 1.5 metres
above its authored cross sections. That survey does not validate tree-card
transparency, the precise photographed corner, or all possible positions.
No scenery or collision was changed, and this report is not marked fixed.
Evidence: `Verification/momiji-trees-20261112/verification-summary.json`.


### Xbox controller discovery across input backends (November 12)

Read-only Discord report: "Controller stop responding. when starting on steam",
October 14, takumi padorucore, in the bugs forum. The report says an Xbox pad
works when launching the executable directly but ignores all controls when
launched through Steam. The exact Steam/hardware setup is not reproduced.

A related deterministic discovery defect is reproduced locally: any connected
native XInput slot caused every Unity XInput device to be excluded, even when
the native slot was idle and the Unity device was receiving fresh input. The
installed Input System has no exposed XInput user-index association; connection
of one native port cannot establish identity with all Unity devices.
`Idas3ControllerDevices` now retains those devices as selectable inputs. Native
slots remain preferred on startup. Existing neutral/held-source protection
prevents delayed duplicate input from repeatedly replacing the active source.

The new regression uses the actual Windows XInput layout in Unity with a
private synthetic device and an injected native reader. The unmodified
production implementation fails both discovery and fresh-input takeover
(`baseline-01`, 506 checks, two failures). The correction passes all 509 checks
(`candidate-01`), including explicit selection, native unplug/reconnect,
120 frames of delayed duplicate input, and the pre-existing slot/release-guard
checks. The fixture creates/removes only its own synthetic device and writes
only isolated preferences; no hardware commands or Steam settings are changed.
The Windows-specific fixture is excluded from non-Windows builds.

Evidence: `Verification/controller-backends-20261112`. The initial scripts-only
build (`unity-build-01`) failed because Unity's cached scene list belonged to a
different diagnostic scene, not due to licensing or a compiler failure. The
previous private player hashes were checked unchanged before starting a fresh
full player. Packaged verification results follow after completion. This is a
controller discovery correction, not confirmation of the exact Steam report.

The fresh full Windows build completed successfully (`full-build-01`). Its
17,321 native asset records match the preceding navigation build, and both
the Unity data archive and accepted native plugin retain their previous
hashes. The Tsubaki fork collision changes are retained separately. The first
packaged device run passed 378 checks.

The packaged online-menu check then caught another existing input-routing
defect: closing Online with Escape left the offline race paused after the
two-frame transition guard expired (`online-01`, failure after 45 checks).
The online-only closing latch was not arming the shared pause/native key
mask. `OnlineClosed` now arms that shared mask until the closing input is
released; held throttle and steering remain available after the transition.
A new standalone regression fails before this correction and passes after
it, covering keyboard Escape and controller Start through the transition,
continued steering, release and a fresh pause press.

The final scripts rebuild succeeds (`unity-build-02`). Final verification:
509 backend/reconnect checks, 223 production menu-routing checks, 377
packaged device/profile/binding checks, and 132 packaged online-menu checks.
The latter exercises F1, Escape and programmatic closing, held/fresh driving
controls, replacement devices, keyboard recovery, wheel navigation, and
creating/leaving an empty private LAN room. Both final player runs shut down
cleanly and were muted on unshown private Windows desktops. They do not
establish live EOS connectivity or physical Steam-controller acceptance.

Final evidence and hashes:
`Verification/controller-backends-20261112/verification-summary.json` and
`Verification/controller-backends-20261112/private-player.json`.
The final private assembly is
`53d63c2bddd5842b31a5766a5f3d82ec4073970cb434fca5518e91b2820592c0`.
The project plugin and both protected `D:\Current` DLLs remain unchanged.
No release was published, no current installation was replaced, and the
broader bugs-channel goal remains active.

## Irohazaka online overlap investigation — November 12

Re-read [YUI!?'s October 17 overlap report](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1560988693827813467)
in Discord without posting, reacting or changing its state. The sole image
is Replay Viewer at 2:56.901 on Irohazaka forward/night/dry, with a BNR32 and
CT9A. It shows overlapping cars but does not expose the collision setting,
game version, original input history or a downloadable replay. The report
says blocking is impossible. The historical cause remains unverified.

Added `online_overlap_audit`, an opt-in developer diagnostic using the actual
App race, prediction, display correction and confirmed replay recorder, a
numerical second peer, and an independent unpredicted run of the same inputs.
Ordinary steering/throttle drives both cars from the grid to a natural finish;
no car is relocated, clock extended or finish state forced. This changes no
game behavior. The automatic driver is not a reference for skilled driving.

Four accepted runs total **51,463 ticks and 211,750 checks**. Collision-on
runs cover both local roles with input exchanged every 12 ticks and one role
at 32 ticks. Each reaches the 2,558 course goal index after 12,877 updates,
with 7,278 contact frames and 7,277 nonzero physical-response frames. Both
peers converge to the unpredicted numerical state. Every saved position and
yaw in both replay streams equals its confirmed source frame. Contact-on
minimum car-centre separation is 1.3584 metres; side contact and some body
clipping remain visible in the selected native captures. This is not a
general mesh-intersection or visual-fidelity pass.

The collision-off control run reaches a natural finish after 12,832 updates,
has zero contact/response frames, and spends 2,429 racing frames with car
centres less than one metre apart (minimum about 3 cm). Its captured bodies
visibly interpenetrate through the centre. This demonstrates the diagnostic
can expose pass-through behavior, but does not establish that the reporter
had collisions disabled. The current lobby code transmits the collision flag
and passes it to the native shared simulation; this audit does not exercise
that path over a real two-PC EOS connection.

Separately reran the independent original-instruction body-contact reference:
**1,020 cases, 522,625 exact comparisons, 8,355,592 original instructions,
zero hooks**, covering all 35 car geometry rows. It confirms the tested
contact routine's source parity, not the entire online/rendering pipeline.

Evidence: `Verification/online-overlap-20261112/verification-summary.json`,
per-tick position/contact CSVs, paired replay streams, and twenty native
offscreen 3D captures. No audio device or visible game window was opened.
The early long-run fixtures exhausted voices because their silent mixer
omitted the normal scene/PCM clock; final runs include both. Native software
HUD composition also rejected a source transparent blend; final captures
use the production UI command path and omit HUD pixels from their BMPs.
Those failed diagnostic attempts are retained and not counted as passes.

**Status: investigated, not resolved.** Current local runs did not reproduce
loss of physical blocking with collisions enabled. The affected replay or a
new live reproduction is still needed to distinguish a historical recording
artifact, disabled collisions and an untested network trigger. Physics and
presentation behavior remain unchanged; only the diagnostic and its CMake
target were added. The project plugin, accepted private player and
`D:\Current` were not replaced, and nothing was published.

## 2026-11-12 — LAN host address selection

Revisited the live “Bring back LAN races” report (October 15, no replies at
review). Its two-PC Wi-Fi failure remains unverified. Independently reproduced
a host-address defect: the transport excluded every adapter without a default
gateway and fell back to advertising `127.0.0.1`. That address cannot connect a
second computer to the host. The previous enumeration could also choose a
PPP/tunnel adapter before an ordinary local network, and one unreadable adapter
could hide subsequent working adapters.

Hosting now accepts usable IPv4 addresses without requiring an Internet gateway,
prefers ordinary connected interfaces over PPP/tunnel interfaces, and rejects
loopback, unspecified, multicast and reserved address ranges. A missing usable
address produces an explicit connection error before opening the listener.
Explicit loopback-only diagnostics retain their previous behavior. The listener
still binds all local IPv4 interfaces. This does not identify VPN adapters that
present themselves as Ethernet or guarantee the intended network on every
multi-homed machine; no firewall or adapter settings are changed.

The production address selector was exercised through simulated operating-system
adapter records. Nine cases failed against the old implementation; all twelve
adapter cases and the existing seven real TCP delivery checks pass after the
fix. These are distinct test scopes, not nineteen real network configurations.
The initial fixture compile error is preserved separately and is not a product
failure. The test runner's report description now makes the scopes explicit.

The private Unity scripts build passed. Two muted, hidden game processes used
normal LAN selection and the actual advertised host address, completed the
Hello/lobby/Ready/race path, moved both cars over 177 metres and shut down with
exit 0. Both reported the same room. This is same-PC coverage, not verification
of two-PC Wi-Fi, firewall traversal or the reporter's exact failure.

Evidence: `Verification/lan-address-20261112/verification-summary.json`,
`before-02`, `after`, `unity-build-01`, and `player-normal-lan`. The private
candidate retains the earlier renewal, navigation, controller and Tsubaki
corrections. Project plugin and `D:\Current` hashes remain unchanged. Nothing
was installed or published.

## 2026-11-12 — leaderboard refresh work during racing

Continued the read-only “Game stuttering caused by VPN” report. Its author
describes approximately half-second freezes accompanying network spikes in
solo and online play. The actual VPN connection remains unverified. The live
forum still contains the report and two replies. A Windows firewall prompt
left by the preceding LAN test covered Discord; no security action was
automated. The owner was asked to dismiss it while local investigation continued.

Found and measured an independent game-thread stall: the HTTP request yielded
asynchronously, but UTF-8 decoding, JSON parsing, full snapshot validation,
serialization and atomic disk replacement then ran on the game thread.
An offline Unity Editor fixture using 2,000 server-shaped entries (678,313
bytes) measured 30.17–34.95 ms for parsing, validation and writing. A 200-entry
fixture measured 7.65–10.68 ms. These measurements do not reproduce the author's
half-second VPN freeze or measure displayed FPS.

Response decoding, validation, serialization and the snapshot cache write now
run on a background task using detached plain data. The coroutine yields until
completion and publishes only a validated, successfully committed snapshot.
Only the main thread accesses settings, scene/native state and Unity time.
The shutdown callback cancels pending work without waiting. Switching sharing
off while a response is being prepared cannot re-enable community records.
Bad data, old seasons, incompatible builds and failed writes retain the prior
cache and select personal records. No authentication, upload cadence or server
configuration changed.

The response handler also enforces the existing two-megabyte limit while
receiving bytes, rather than allocating/decoding an unbounded response before
checking its string length. A partial HTTP 200 is not treated as success.
The string-based JSON operations used here support background threads; see
[Unity's JSON serialization documentation](https://docs.unity.com/en-us/engine/6000.6/manual/programming-environment/code-reload-serialization/script-serialization/json-serialization).

The final Editor checks exercise the production response coroutine, actual
atomic replacement failure with a locked cache, malformed/old data, bounded
buffering, settings changes, an empty current board, and cancellation. The
2000-entry payload is the exact retained baseline cache. Its measured maximum
coroutine-step times were 0.0084–0.0201 ms; processing still takes time on the
worker and background allocations can still contribute to garbage collection.
The first candidate fixture incorrectly expected edit-mode GameObjects to
receive the destruction callback. Its failure is retained; the corrected
Editor fixture invokes that callback explicitly, and the player check below
tests actual destruction.

The scripts build passed, retaining the accepted native/data assets and earlier
fixes. A hidden, muted packaged player drove the real Akina race while eight
2,000-entry responses were prepared. Each operation yielded across 25–67
simulation updates; every native ranking field matched the input. The first
main-thread step peaked at 0.329 ms and subsequent per-operation maxima were
0.0077–0.0106 ms. Actual component destruction also cancelled late publication.
The run passed 224,338 checks and verified clean native shutdown. Rendering
presentation/FPS, HTTP traffic, VPNs and two-PC online play are outside this
fixture; it is a response-processing correction, not a claim that the reported
VPN issue is resolved.

The investigation also found a separate capacity mismatch: a server-shaped
3,080-entry board is rejected by the managed client's old 2,112-entry cap;
the native bridge has the same obsolete cap. That requires coordinated client
and native correction and is still open. The snapshot background change does
not conceal that failure or silently truncate the board.

Evidence: `Verification/community-refresh-20261112/verification-summary.json`,
`baseline`, `candidate-02`, `unity-build-01`, and `player-01`. No public service
was contacted by these tests, no personal save was used, and `D:\Current` and
the project plugin remain unchanged. Nothing was published.

## 2026-11-12 — full-course leaderboard and pending queue capacity

Corrected the coordinated capacity mismatch identified above. The managed
snapshot validator and native shared-record bridge both retained a 2,112-row
limit from the smaller course catalog. The pending-upload queue also used
2,112, including a destructive trim when restoring an otherwise valid queue.
An offline fixture with all current server-valid course/direction/weather
boards reproduces both failures: the 3,080-row snapshot is rejected, and
2,450 pending personal bests are restored as only 2,112. A packaged baseline
separately confirms the native bridge rejects the full snapshot.

Snapshot limits now follow the supported condition count, two weather choices,
and retention of the course top ten plus the other 34 models' bests. Pending
capacity allows one personal best per condition/weather/model. With 18 courses
the structural limits are 3,168 snapshot entries and 2,520 pending entries;
excluding unsupported dry Akina Snow boards gives 3,080 and 2,450 respectively.
This does not relax per-run validation, build/season checks, the bounded response
size, or the server's ranking rules. The native board remains atomically
replaced only after every entry is accepted.

The Editor check passes 2,457 assertions, including exact pending-run recovery,
faster replacement, refusal of a slower replacement, adding and persisting a
new key past the obsolete limit, and exact/over-capacity snapshot validation.
The native time-attack record suite passes, including full retention across
all supported courses and file reload. A hidden, muted packaged player passes
129,376 checks: all 3,080 native entries and their 14 fields match the managed
payload; invalid counts, null input and an invalid final row preserve the old
board; an empty valid board clears it. Clean native shutdown is verified.

The measured native application took 8.67 ms for 2,000 rows and 23.05 ms for
3,080 rows in this run. This capacity correction is not an FPS improvement or
a claim to resolve the VPN report. Bulk native record construction remains a
possible follow-up to the separate background-response correction. No public
service was contacted, and no personal saves were used or recovered by these
fixtures; already-truncated real queues cannot be reconstructed by this fix.

Evidence: `Verification/community-capacity-20261112/verification-summary.json`,
`before-editor`, `after-editor`, `before-player`, `after-player`, `native-tests.log`,
and the final `unity-build-03` scripts build. The final scripts build corrects
the diagnostic scope text to distinguish Editor queue checks from packaged
native-board checks. Final private candidate hashes and protected-file checks
are recorded in `private-player.json`. The plays-left and original minimap
corrections remain included. `D:\Current` and the project plugin are unchanged;
nothing was installed or published.

## 2026-11-12 — batch construction of downloaded rankings

Followed up the measured native application cost from the capacity check.
`Idas3SharedSetRecords` previously called `TimeAttackRecords::record` for each
row, repeatedly sorting and trimming the growing board while holding the
render mutex. It now parses a detached vector and applies one validated batch
with the same stable tie ordering, course top ten and per-model best retention.
An invalid row leaves the active board unchanged. Local CSV loading uses the
same batch operation after its existing complete validation and revision checks;
atomic save replacement and migration backups are unchanged.

The new operation is compared against a frozen copy of the previous sequential
algorithm over 24 deterministic 4,800-row streams, including ties, arbitrary
batch boundaries, mixed individual finishes, every metadata field and aliased
input. These perform 160,760 exact row comparisons. Ten invalid-final-row cases
verify atomic rejection. The complete native record persistence/migration suite
passes, including the full 3,168-row retention/reload case and Gunsai/Odawara
backups, future revisions and failure preservation.

A single scripts build added optional repeated application timing. Before and
after runs use that same assembly, player data and 3,080-row fixture, changing
only the private native plugin. Both hidden, muted packaged runs pass 129,392
checks, exact native readback, atomic failures, empty-board clearing and clean
shutdown. Across eight warmed repetitions, the 2,000-row median drops from
8.3169 ms to 0.02605 ms and the 3,080-row median from 20.6491 ms to 0.04045 ms.
The full-board maximum drops from 22.39 ms to 0.0904 ms. First calls and all
samples are retained, not omitted from the evidence.

This operation is applied only while the native frontend is open; races retain
the records selected at their start. These results establish reduced menu
ranking work, not a driving FPS improvement or resolution of the VPN report.
Combined personal/community display-cache rebuilding and external networking
remain separate from the timed bridge operation. The live Discord forum was
re-read and the same VPN report is still visible; the Windows firewall dialog
still prevents further UI navigation. No security prompt or Discord interaction
was submitted.

Evidence: `Verification/community-apply-20261112/verification-summary.json`,
`native-test-details.log`, `before-player`, `after-player` and `unity-build-01`.
Protected project and `D:\Current` hashes are unchanged. The private candidate
includes the earlier card-renewal, minimap and leaderboard-capacity corrections.
Nothing was installed or published.

## 2026-11-12 — personal/community display cache stalls

Measured the remaining ranking-display stage separately from the native
snapshot application above. `App::displayedTimeAttackRecords` rebuilt personal
records with one sort/trim per row, then repeatedly scanned the remote board
and sorted the combined board for each unpublished personal result. An actual
packaged App fixture with a complete 35-car garage reproduced a 43.159 ms
warmed median when both caches were dirty, including 12.99105 ms for personal
reconstruction alone. The same combined operation took 4.55105 ms with five
cars and 1.06015 ms with one car. Unchanged cache reads were already cheap.

Personal records are now staged and retained as one batch. Community merging
indexes remote identities once, using the same condition/weather/car/time/name
comparison as before, and appends unpublished personal rows in one batch.
The identity index contains pointers and does not reorder the ranking. Remote
metadata still wins for a published personal best; different names remain
distinct, and stable equal-time ordering is retained. Existing dirty flags,
sharing behavior, card ownership and source record extraction are unchanged.

Before/after hidden, muted packaged runs use identical managed/data builds.
Each passes 301,165 native checks against the frozen sequential display
construction, covering 1/5/35-car fixtures, all original/imported directions
and supported weather, 3,080 remote rows, all metadata, dirty/cached paths,
sharing toggles, remote removals and changed/empty profiles. Each also passes
the existing 27,670 checks using actual private five-slot/legacy profile files,
backup recovery, source frontend record targets and race starts. The CSV row
artifact is the reference table; actual displayed caches are compared to it
field by field inside the fixture. Both runs shut down cleanly.

Across eight warmed samples, the full 35-car reconstruction/merge median falls
from 43.159 ms to 0.2354 ms (maximum 45.5536 to 0.2458 ms). Five cars fall from
4.55105 to 0.06805 ms and one car from 1.06015 to 0.05865 ms. Personal-only
reconstruction for 35 cars falls from 12.99105 to 0.03775 ms. These are measured
cache-operation times, not whole-game FPS or validation of the VPN-driving
report. First samples and all warmed samples are retained.

The initial diagnostic builds failed for a missing managed native import and
a non-span-compatible test initializer. The first runtime fixture also omitted
the accepted-card flag and failed before timing. Those failures are retained;
the corrected baseline passes before production changes are applied.
Evidence: `Verification/community-display-20261112/verification-summary.json`,
`baseline-player-02.json`, `before-player-02`, `after-player`, and
`native-test-details.log`. Source/package and protected-build hashes pass.
`D:\Current` and the project plugin are unchanged; nothing was published.

## 2026-11-12 — pending leaderboard writes leave the race thread

The response/cache work above still left a synchronous path: every successful
upload rewrote the complete pending-time file, and collecting a new finish or
switching sharing off did the same. A private 2,450-row baseline measured the
actual serialization and atomic file replacement at 14.46–91.44 ms on the
Editor main thread. This establishes a possible local hitch, not the cause of
the Discord VPN report or a whole-game FPS measurement.

Pending-time mutations now pass through one ordered background writer. It
copies submitted data, retains one best time per condition/weather/car, and
publishes a new queue only after its file replacement succeeds. Upload replies
remove their exact submission ID, so a late reply cannot delete a newer faster
run. Publication revisions prevent coroutines resuming out of order from
restoring an old queue. Sharing-off clears use the same ordering. Failed writes
retain the previous file/state and can retry. Native finishes are acknowledged
only after persistence and only if the pending native payload still matches
the one captured for that write. Teardown cancels pending work and prevents
late publication. Initialization remains synchronous before racing.

Evidence is in `Verification/community-queue-20261112`. The detached writer
passes 32 Editor checks for coalescing, durable replacement, late replies,
publication ordering, clear ordering, caller mutation, times-only metadata,
locked-file failures, retry and cancellation. Complete snapshot/pending
capacity recovery still passes 2,457 checks; time-only client checks pass 22.
The private scripts build succeeds without changing its native DLL or data.

The hidden, muted packaged player passes 1,409 managed checks and the actual
native finish fixture's 37,223 checks. Nine full-queue replacements occur while
Akina simulation advances by 11–95 updates per write. The scheduling/publication
main-thread steps are 0.9861 ms cold and 0.0119–0.0151 ms thereafter; worker
elapsed time is 14.70–83.94 ms and remains off the race thread. These timings
exclude verification-file reads and are not rendering FPS. The native finish
remains unacknowledged while the destination is locked, then is acknowledged
after a successful retry and verified durable time. Actual component destruction
leaves a complete JSON file and prevents late publication. The player shuts
down cleanly with exit zero. No public HTTP or two-PC/VPN test was performed.

`verification-summary.json` records all results, timing samples, source/package
hashes and protected-file checks. The accepted private player is
`Verification/cb-20261112`; `D:\Current` and the project plugin remain unchanged.
Nothing was published.

## 2026-11-12 — EOS peer backlog disconnect and dispatch bursts

The live read-only bugs forum still shows the VPN-associated freezes and
imported-course disconnect reports. The Windows firewall prompt still covers
Discord; no input or security action was taken. Continued with the actual
current transport implementation rather than marking either report resolved.

`Idas3EosP2P.Poll` first drained up to 4,096 SDK packets into a queue limited
to 1,024 messages, then delivered everything in that queue without a frame
budget. An offline fixture using the production route/framing implementation
and an in-memory SDK reproduced a disconnect from 1,100 correctly framed
reliable messages: zero messages reached the owner before "The reliable online
receive buffer is full." This is a transport-level reproduction, not a full
race, live EOS or the reporter's actual VPN connection. Test payloads and the
application owner are synthetic.

The route now stops taking game-channel data when its queue cannot accommodate
another maximum-sized message, leaving the remainder in the already bounded
SDK queue. It continues reading channel zero, so handshake, heartbeat and leave
controls are not blocked by the game backlog during native loading. Channel
selection follows the installed official SDK's `ReceivePacketOptions` contract
in `ThirdParty/EOS/com.initiald.eos-sdk/Runtime/EOS_SDK/Core/P2P/ReceivePacketOptions.cs`.
Game delivery keeps FIFO order and shares a 64-message / 2-ms budget across
all polls in one Unity frame. The time budget is checked between callbacks;
one long native operation cannot be interrupted. Recursive delivery is guarded,
and disposal still stops the queue immediately. Wire protocol, game inputs,
rollback, sanctions, routing and SDK queue-size limits are unchanged.

`Verification/eos-p2p-burst-20261112/before-02` records the old behavior and
`before-checks-02` fails the new backlog requirement before production changes.
An additional intentionally slow callback probe drained all 16 callbacks in
253.22 ms before the fix; the final candidate dispatched one in 14.07 ms.
These callbacks deliberately sleep and advance the fake time source. They
demonstrate yielding and cannot establish game-frame performance gains.

The final `after-02` run passes 3,850 checks: both host/guest backlog recovery,
exact once-only FIFO payload delivery, maximum-sized fragmentation and byte
backpressure, same-frame count/time budgets, nested pumps, disposal, mixed
channels, an ordinary stream without added delivery delay, and 60 seconds of
keepalive-only loading with a full queue. A queued leave still takes effect
before game delivery. Existing P2P handshake/access/disposal tests pass 59,
packet framing tests pass 163, and transport lifecycle tests pass 261. Those
tests retain their documented SDK/service boundaries; no public players or
online identities were contacted.

Two initial diagnostic compilations caught an uninitialized test callback under
warnings-as-errors. The first backpressure candidate also failed the long-load
heartbeat test (`control-before`), exposing the need to continue channel-zero
reads; it was corrected before acceptance. These failures are retained.

The private Unity scripts build passes against the actual installed EOS/Unity
APIs, preserving its native DLL, player data and other managed assemblies.
It is staged in `Verification/cb-20261112`; no runtime Internet verification is
claimed by that build. Source/package hashes, reports and protected-build
verification are in `verification-summary.json`. `D:\Current` and the repository
Editor plugin are unchanged, and nothing was published. Actual VPN/two-PC EOS
verification remains open.

## 2026-11-12 — optional trace for the remaining VPN stutter report

The live Discord snapshot still shows the VPN, LAN and imported-course reports.
A Windows firewall permission dialog still blocks further Discord navigation;
no input was sent to it, and no reports were replied to or marked resolved.
The report describes intermittent freezes in solo as well as online play.
The current session-only `NetworkMilliseconds` measurement omits SDK/callback
work invoked separately by `Idas3EosPlatformPump.Update`. Existing automated
performance modes also suppress ordinary online presence and community service
startup, so they cannot by themselves reproduce the reporter's normal session.

Added optional `-idas3-stutter-trace` observation for normal play. This flag
does not enter diagnostic gameplay mode, inject controls, redirect ordinary
saves, disable online services, change audio or replace rendering. It enables
the existing host timing probes and records SDK Tick and deferred-callback
durations from every platform pump in the same Unity frame. Host timings are
cleared at the beginning of each measured update so an early return cannot
report the previous frame's work again. No transport, authentication, callback
budget or physics behavior was changed.

The trace holds at most 36,000 rows in memory (about ten minutes at 60 FPS).
Normal quit writes one `Diagnostics/stutter-*.csv` under the game's persistent
data directory; automated tests use their isolated save directory instead.
The CSV contains frame/timing counters, scene state and GC collection counts,
not names, account IDs, IP addresses, credentials, messages or packet contents.
There are no per-frame file writes. A forced process termination cannot
guarantee a report. The file is local and is not uploaded automatically.

`precedingFrameMs` is Unity's previous-frame interval, while the other timing
columns belong to `unityFrame`; `elapsedSeconds` is sampled in LateUpdate.
SDK/callback and session columns can overlap and must not be added together.
Scene/UI columns measure CPU submission, not GPU completion. Native `course`
and `flags` are retained together so imported-course flags remain available.
An unattributed stall still requires a system/driver trace; this is not a
claim that all sources of stutter are now instrumented or that the VPN is fixed.

Evidence: `Verification/stutter-trace-20261112/`. The production platform passes
63 checks, including artificial SDK delays, callback delays, same-frame repeated
pumps, empty following frames, exceptions and trace shutdown. These use fake
SDK boundaries; they do not connect to EOS. The production trace passes ten
retention/chronology/output tests over 40,001 simulated frames, including ring
wrap, locale-independent numbers, idempotent quit and an unwritable destination.

The private scripts build passes with the installed Unity/EOS APIs. A hidden,
muted Akina run passes its 13,344 existing gameplay checks and writes 1,635 trace
rows, including 447 racing frames with advancing original simulation ticks
and populated native/scene/UI measurements. Its absent EOS counters correctly
remain zero in this isolated offline test. This validates packaged recording,
not real VPN behavior, public matchmaking or display FPS. The current accepted
private player is still `Verification/cb-20261112`; its updated manifest is
`Verification/stutter-trace-20261112/private-player.json`. The native DLL and
player data, repository Editor plugin and `D:\Current` remain unchanged.

Next field evidence needed: run the affected connection with this trace flag,
reproduce a freeze, quit normally shortly afterward, and inspect the local CSV
alongside the exact game build and whether the VPN was enabled. No report was
requested from Discord users or transmitted on their behalf.

## 2026-11-12 - Arcade card acceptance and functional renewal (supersedes per-race/cosmetic implementation)

The user explicitly requested correcting the per-race countdown after learning the arcade behavior, and adding the inspection functionality described. This supersedes the earlier cosmetic-only inspection preference and the 2026-11-11 per-race implementation; historical evidence above is retained as history, not current behavior.

Primary reference: Sega Initial D 3 kit manual, printed p.18 / PDF p.22, https://arcarc.xmission.com/PDF_Arcade_Manuals_and_Schematics/Initial%20D%203%20Kit%20Manual.pdf . Continued races do not count as further card uses. Recovered `acceptOriginalDriverCard` (0FEC40..0FED00) decrements at acceptance; recovered transmission/tuning owners provide the renewal choices and reset rules. Same-version renewal is distinct from a Version 1/2-to-3 transfer.

Implemented behavior:

- Continue in Save Select accepts the chosen card and durably subtracts one use. Browsing, profile enumeration and transmission previews do not consume uses. Change Car accepts the committed car; a new card starts at the source's 51 before its first acceptance, leaving 50.
- Continue races and online rematches retain the accepted card session. A final accepted use remains valid with zero displayed; renewal opens at the next card entry, not at the next race. Returning to Title/Save Select ends the previous session. Explicit save selection starts a new one.
- Online saved-slot selection uses the same acceptance gate. Results, wins, losses and double timeouts retain the persisted counter; existing result journal recovery remains absolute/idempotent. An expired unaccepted online card must be renewed through Saved Data. Guest/legacy selections without an issued save-slot card do not acquire a counter-based online restriction.
- At zero, the original transmission selector leads into the original tuning-package selector (the single-package GC8V follows its source skip route). The original owner supplies CURRENT/CHANGE customization artwork and exact package-change behavior.
- Keeping the package retains tuning. Changing it applies the source's body/basic progress resets; engine level, points, identity, records and unrelated saved state survive. The NORMAL-route branch retains/reconstructs its basic level according to the original flags. No broad 'reset to stock' substitute is used.
- Selectors edit a draft. Cancellation before final confirmation changes nothing. The chosen transmission/package, source resets and 50 remaining uses are saved together with a compare-and-save transaction. Only a durable save opens INSPECTION COMPLETE and its summary. Writer failure or conflicting newer progress returns safely to save selection. Completion does not consume another use.
- Removed manual inspection action remains absent. No retroactive refund of previously consumed uses is inferred from missing historical session data.

Validation lives in `Verification/arcade-renewal-20261112/`. Native all-package sweep: 638,060 checks / 1,260 car-package-transmission transitions, plus original driver-entry/card-check/tuning-menu reference tests. Follow-up boundary checks cover new-card acceptance, NORMAL's inherited-level branch, guest isolation and a naturally timed-out source race (5,587 source ticks): zero remained valid for Continue, then a new entry required inspection. Online reward tests (34), managed settlement (52) and journal recovery (48) passed. Muted hidden Unity inspection input/save/capture checks passed in English, Japanese and Chinese menu settings (25,986 checks each). Captures were inspected for selector/current-option/completion/summary placement. Subsequent evidence records final native/script hashes and adjacent Save/Change Car regression checks.

This is a private diagnostic build. No release, server change, ordinary player save, project plugin replacement or D:\Current deployment is part of this task. Live two-PC online matchmaking was not tested.

Final adjacent regression: the complete Unity Save/Change Car/Delete Save input path passed after separating card-acceptance bookkeeping from transmission-preview assertions. The final `verification-summary.json` records immutable evidence for each run and verifies the final private player hashes plus the unchanged D:\Current/project-plugin hashes. A further 2,973 boundary checks include fresh-card issuance and guest/legacy isolation. All diagnostic processes exited normally.


## 2026-11-12: scalable minimap road artwork

The original navigation banks enlarge small sections of 512-pixel route
textures. Rebuild all eight road images from their authored navigation paths
into 1024-pixel distance fields, generated once when the bank loads. Native
previews and Unity reconstruct antialiased coverage at the final display size.
The source materials retain the field tint, road/field transparency, UV view
extent and orientation. Frame and player/opponent marker artwork is unchanged.
No per-frame texture generation or gameplay/physics changes are involved.
Original asset files remain intact on disk.

Width measurements use the nearest connected road at half coverage; summing
nearby hairpins made the first Irohazaka candidate too heavy. The final width
also passes a source-versus-rebuilt road-area regression on all eight banks.
Irohazaka's out-of-range U coordinates remain clamped rather than wrapping an
individual path vertex and drawing a false cross-map chord.

Evidence: `Verification/minimap-art-20261112/verification-summary.json`.
Course-map, UI capture/reload and multiplayer-HUD regressions pass. Final Unity
captures cover Akina, Irohazaka and reverse/night Akina through 3840x2160. Eight
matched Akina screenshots have zero changed pixels outside the minimap.
Initial diagnostic staging/overweight-artwork runs are retained but excluded
from acceptance; `after3-*` identifies the accepted final native binary.

Installed 11 code/player-data files into `D:\Current`, with reversible backups.
Existing game assets were retained; all 25 navigation inputs match the complete
candidate. The installed game passed its own muted, unshown-desktop check
(68,928 assertions, clean exit). Saves, settings, personal music and ROM paths
are preserved. Final native SHA-256:
`4bd462a32ddaabbe0b9f9ba5a258784b21f53b798edd7aae7cc705e43ce13b81`.
GitHub publication remains held.
