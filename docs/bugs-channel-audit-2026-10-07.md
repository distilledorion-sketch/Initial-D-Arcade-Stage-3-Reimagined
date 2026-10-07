# Bugs forum follow-up — 2026-10-07

Scope: the authenticated Discord bugs forum only. Read reports and replies without posting, reacting, retagging or closing threads. Changes are local to the bugs branch. No GitHub release, leaderboard deployment, Desktop installation or R35 changes are part of this batch.

## Implemented locally

### Shomaru section maps

[Report](https://discord.com/channels/1548169613206884355/1557050375578845255): the detailed Time Attack analysis maps are inverted.

Shomaru's three section artwork pieces face the opposite direction to its overview and driving telemetry. Rotate those pieces 180 degrees about the original map centre. The overview, driving trace, impact coordinates, routes and other courses retain their existing orientation.

The regression check now overlays the actual authored course centreline instead of only comparing the selected image with itself. Before the fix, the first section matched 93/415 road samples. After the fix, the three sections match 415/415, 474/474 and 402/402 samples in both driving directions (two-pixel tolerance). The full visit suite passes 630 checks, including page cycling and normal exit.

Evidence: `Verification/bugs-20261007/maps-before/shomaru.png` and `maps-after/shomaru.png`.

### Original multiplayer battle-stat artwork

[Report](https://discord.com/channels/1548169613206884355/1557048440939225208): restore the original battle/win/win-rate sprites before online races.

The banner now uses the existing `start2d` labels and gradient number atlas, replacing the generic alphabet and synthetic slant. Source spacing, decimal/percent artwork, entrance timing and safe-area fitting are preserved. Long counters extend their slots without overlapping adjacent labels. Player names and the course header retain their existing owners.

The banner suite passes 1,351 checks, including gradient artwork, integer percentage calculation, overflow, aspect ratios and unchanged offline rendering. Visually inspected `Verification/bugs-20261007/battle-stats.png`.

### Map / Water Cup / Off

[Report](https://discord.com/channels/1548169613206884355/1557058116108161225): missing ability to replace or hide the minimap.

Added **Settings → HUD → Minimap Display: Map / Water Cup / Off**. Map remains the default for existing settings. The selection participates in normalization, draft/apply comparison, saved options, replay options and HUD-layout preview. Cup uses the minimap's existing editable position and scale; Off removes that HUD layer. The previous native preview entry point remains ABI compatible, with a separate extended entry point for the new choice.

Recovered the original 70-piece `cupwater` bank, including 60 water frames and six splash frames. The original 17D2A0 state machine advances on accepted 60Hz race updates, using motion and the existing wall/gutter contact cues. Ordinary motion produces ripples; gutter contact and stronger impacts use their original curves and splashes. Repainting does not advance it or alter physics/RNG. Replay playback uses known movement only: old recordings do not contain the contact cues, so replay wall/gutter splashes are not reconstructed. Seeking resets the effect.

Verification:

- 9,106 state/artwork checks, including 128,924 original instructions compared frame by frame over stopped, moving, gutter, both impact directions, strengths and cue precedence. Diagnostic timer, vector magnitude and division hooks are explicit.
- Actual HUD compositor verifies all three display choices, group 5 capture, marker removal and repeat-paint stability. The broader HUD suite passes 196,214 online checks, 60 saved-profile/RPM comparisons and intro/restart checks at three aspect ratios.
- Production managed scripts compile against installed Unity assemblies. This is a compile check, not a full Unity player build or interactive settings test.

Evidence: `Verification/bugs-20261007/cup/`, `hud/` and `hud-full/`.

### Opponent headlight glare, including rearview

[Report](https://discord.com/channels/1548169613206884355/1556748556696551507): the mirror shows lamps without the original headlight halo.

The current renderer submitted the lamp meshes and road beams but omitted the separate original `hilight` owner. Restored its source sprites and all 35 cars' lamp anchors. The host adapter uses the original 1.5m sprite size, 0.3m forward offset, 100m horizontal cutoff and quantized sixth-power facing attenuation. Each camera receives its own billboard and visibility mask. The local car's glow stays out of its own mirror.

The effect follows actual lamp enable state and popup opening. Depth testing occludes it behind scenery; it does not write depth. It adds no simulation queries, random draws, light timers or extra bloom. This restores the visible halo; it does not claim an instruction-for-instruction port of every original glare/occlusion branch or its screen-wide exposure accumulator.

Verification: 219 native/renderer checks cover all 35 anchor records, angle/range, disabled lights, closed popups, camera masks, repeat rendering and wall occlusion. Actual assembled AE86, R34, Evo III and Evo IV bodies were rendered to check lamp placement. Native Unity plugin builds successfully. Evidence: `Verification/bugs-20261007/glare/`.

## Still open or awaiting affected-machine evidence

- [Akagi frame fluctuations](https://discord.com/channels/1548169613206884355/1556898650742067250) and new replies in [FPS Drop Issues](https://discord.com/channels/1548169613206884355/1552613619269640242): reports identify Akagi CP3/CP4, Akina and worse night performance. No matching affected-machine CPU/GPU profile was available. Existing near-start projector benchmarks do not establish the cause of these later-sector drops; no performance fix is claimed.
- Physical G923/Fanatec binding failures, Linux/Proton updater failure, intermittent online collision/disconnection reports, China service reachability and the exact random crash still need the relevant hardware, logs or reproduction. Prior synthetic passes do not close those cases.
- The Xbox controller report inspected in this pass includes the reporter's confirmation that explicitly selecting the controller resolved their case; no further change was made based on that thread.
- The leaderboard delay thread's inspected messages predate the R2/model-record recovery already shipped in `.42`; they supplied no new failure after that recovery. No leaderboard data or service settings were changed.
- Previously documented Sadamine wet-corner/replay camera and ending-cinematic limitations remain as described in the October 4 audit.

## Build and provenance

Native plugin build and production managed compilation pass; existing compiler warnings remain. `git diff --check` passes. Visual evidence and local diagnostics remain under the ignored `Verification/bugs-20261007` directory.

The two asset exporters write source hashes and extraction manifests. The reference executable is used only for local extraction/validation and is not added to the repository or any build. No ROM/CHD is included. The changes require a future packaged update before players receive them.

## Resumed unfinished reports

The user requested resuming unfinished bugs after the Japanese texture download stalled. The Discord browser session requested a new login, so this pass uses the reports recorded above and in the October 4 audit; it does not claim to have inspected new messages.

### Imported-track replay bumper height

Reproduced a separate camera error in the existing Replay Viewer report. Detailed replays reconstructed their camera anchor by subtracting a flat ride height from the recorded displayed body. Live Hakone, Sadamine and Tsubaki instead raise the body along the road normal, with a 2 cm model-origin offset. The old reconstruction also incorrectly adjusted Special Stage replays, whose live camera uses the recorded actor directly.

The viewer now reverses the slope-aware height conversion for the three Stage 8 imports and uses the recorded actor for Special Stage. A separate contact-query cache preserves the live solver/body state. Missing contact falls back to the recorded actor; legacy IDR1 recordings keep their existing path. No replay format, physics, tuning, save or leaderboard change is involved.

`imported_replay_camera_tests` compares real host driving poses against replay evaluation on all seven imported courses plus Akina, in both directions and dry/wet conditions. The pre-fix fixture failed 2,520 pose comparisons, with a maximum 25.2 mm camera error. After correction, the expanded suite passes 14,496 checks, including backward/forward seeking, repeated paused paints, legacy recordings and unchanged solver/contact state. The largest remaining numerical difference is 0.111 mm (2 mm tolerance). The original chase/bumper instruction-reference test also passes. Native Unity plugin rebuilt successfully; existing compiler warnings remain.

Evidence: `Verification/bugs-resumed-20261007/replay-camera-before/`, `replay-camera-final/` and `build-final.log`. This fixes the reproduced replay-height mismatch. It does **not** establish a fix for the separately reported live Sadamine sharp-turn bouncing, which remains unreproduced.

### Later-sector frame-rate investigation

Added `late_race_benchmark` to sample the production native scene-publication path at 5%, 55%, 75% and 90% of Akagi and Akina uphill, with one/two cars, day/night and the rear-view camera. It uses arranged frozen poses, private saves and the normal Unity UI capture path. It checks that rendering leaves the simulation digest unchanged. This expands the earlier near-start checks, but does not measure Unity GPU/managed rendering, network reconciliation, moving sector transitions or playable frame rate.

Native scene preparation measures 0.078–0.386 ms median across the 32 local cases (1,920 measured frames after warmup); this alone does not explain or resolve the reported drops to 40 FPS. No speculative performance or networking change was made. Evidence: `Verification/bugs-resumed-20261007/late-race-verified/`. Earlier diagnostic attempts omitted UI capture or the rear-view submission and are not the reported benchmark.

### Remaining limits

Physical G923/Fanatec failures, Steam-managed Proton updates, intermittent public online collisions/disconnects, China reachability and the clipped random-crash report still lack the affected hardware/network, complete logs or reproduction. The original driving cinematic behind the restored ending credits also remains unfinished. These reports remain open. No Desktop installation, GitHub publication or Discord message was performed in this pass.

## Ending driving backdrop follow-up

Restored a driving backdrop behind the Legend ending credits locally. The old transition called `returnToCourseSelection()` before starting the ending, replacing the completed scene; it now keeps that scene until the credits finish. A separate 3,600-frame buffer records both cars during Legend racing, including wheel/body pose, lamps, braking and course-selection index. It works with optional replay saving disabled, cannot grow beyond one minute, and holds its final pose when its recorded footage ends. Playback never runs the race solver, awards points or saves another time. Its recording and private car presentation copies are cleared on exit or the next race.

The camera uses the car-relative eye and target from `o_ending_camera_00` (kind 13) and its original FOV. This restores the missing visible backdrop; it is **not a complete source-identical cinematic port**. Original camera shake/smoothing, source highlight selection, special ending-scene lighting and independently animated track effects remain outside this change. Credits, photographs, final card, music, fades and skip timing retain the earlier source-verified owner.

Validation: `Verification/ending-driving-20261007/application-final/` contains 64 passing application checks and actual rendered frames. This drives 1,800 native ticks against the final Legend rival with replay saving disabled, verifies two-car capture and movement, checks ring overwrite/seal/hold behavior, and exercises the ending at 30/60/144/240 FPS. Physics digest, race tick count and profile remain unchanged through playback. Pause, held-input protection, both skip stages, audio stop, buffer cleanup and the existing 31-rival result-flow fixture pass. That fixture settles results rather than driving 31 complete races. The independent `original_ending` reference test also passes. Native software-rendered frames were visually inspected; this is not a new Unity GPU performance measurement or an original-hardware cinematic comparison.

This pass does not resolve the hardware/network-dependent reports or the later-sector FPS report. Changes remain local; no Desktop installation, GitHub publication or Discord messages.

## Private matches and direct gear bindings (2026-10-07)

- Online Battle now offers **Host Public Battle** and **Host Private Battle**. Private rooms use the existing shareable room code. Both peers see **PRIVATE** in the room header. LAN Direct retains its existing address-based hosting.
- Private Steam rooms use an invisible lobby and a separate game namespace. Public discovery filters that namespace at Steam's query boundary, including on older clients, and validates it again in returned results. Quick Match also rejects private room records. Direct code joins accept either namespace after the same game/build/owner/capacity checks. The room code is a join link, not a password or protection against someone sharing it.
- Steam's private lobby type requires invitations, so it is not used for code-based rooms. See [Steam matchmaking lobby types](https://partner.steamgames.com/doc/api/isteammatchmaking#ELobbyType).
- Settings > Controls now contains Gear 1 through Gear 6 in a scrollable binding list. Keyboard, gamepad buttons, and separate generic wheel/H-shifter devices use the existing capture/profile system. Keyboard/controller navigation follows the selected row through the list.
- Direct selections apply only to Manual transmission and only to gears the selected car has. Empty/overlapping shifter positions retain the engaged arcade gear; no clutch, neutral, or reverse model is added. Sequential shift bindings remain supported. A direct downshift retains the original downshift steering signal, and all shifts still use the original RPM/coupling calculations.
- Existing controls formats migrate to version 4, preserving all previous bindings and adding six unbound gear actions to each device. Disconnect, reconnect-release, focus-loss, and menu-capture guards cover direct gears.
- The native frame ABI remains 88 bytes (gear in flag bits 8–10). Authoritative online input packets use codec version 2 and carry the direct gear in the existing input byte; rollback compares it and predicts the held state. Build matching continues to require identical builds.

Validation: `Verification/private-shifter-20261007/`. Native host-input, source-reference transmission, and source-reference vehicle tests pass. A four-case online simulation checks 57,600 peer frames at 50–250 ms, with jitter, loss, duplication, reordering, and outage recovery. Unity checks pass for 62 direct gear/privacy assertions, 19 multi-device rig checks, existing headlight migration, and 112 controller/menu checks. The first hidden-player screenshot attempts did not repaint; they are preserved as failed evidence and do not establish visual verification. Physical H-shifter hardware and two separate Steam accounts have not been tested. No release or desktop installation is part of this change.
The final hidden Unity player input run passed **136 checks**, including navigating to Gear 6, capturing and saving its binding, delivering the requested gear in the actual native frame, and clearing it on release (`player-input/report.json`). Visual repaint and physical-device/network limitations above still apply.

## Game optimization pass (2026-10-07)

Optimized the production original-menu/HUD submission path. It now uploads a single interleaved vertex stream, retains a capacity-sized sequential index buffer, and publishes changed submesh ranges together. Changing digit counts no longer clears and reuploads every draw's indices. Original background/foreground commands are reused until their draw state, clip, canvas size or order changes. Custom tachometers, ornaments and fades retain a separate animated command buffer in the same display order. HUD editor bounds are merged once per source draw instead of reconstructing a rectangle for every triangle vertex.

The performance harness also now renders the complete six-camera stack explicitly when requested and checks exact render counts. The earlier manual mode allowed `ApplyFrame` to re-enable the main camera and duplicate renders. CPU-only hidden-window measurements are still labelled as such; they are not display FPS measurements.

Evidence: `Verification/optimization-20261007/comparison.json`, `managed-build.log`, `ui-final-full/`, `legend-day-render-{baseline,optimized}/`, and `ui-final-pixels-v2/`.

- Akagi uphill/night full-course traversal: 10,858 measured source ticks, actual finish without timeout (diagnostic timer grace enabled). Median HUD submission time fell **7.0%** against the original implementation. This run measures CPU submission; the hidden window did not automatically render cameras.
- Akagi two-car Legend race: 1,800 measured frames per variant, 1,800 main-camera and 10,800 total camera render events each. Median HUD submission time fell **23.1%** against the forced-rebuild comparison. Both paths in this comparison include the bounds optimization. Overall wall time changed only slightly; no overall FPS percentage is claimed.
- The final full-course run uploaded no HUD indices after warmup and rebuilt original HUD commands on approximately 38% of frames, instead of every frame. Both scenarios measured zero main-thread allocated bytes per frame after warmup.
- Driving samples match exactly before/after for simulation ticks, speed, course distance, course length, race progress, wall contacts and travel. No physics or networking code was changed in this pass.
- **45 byte-for-byte rendered-image comparisons across 15 views pass**: independent HUD sizes/offsets, two custom tachometers with an ornament, course/mode menu transitions, and 1280x960, 1920x1080 and 3440x1440 menu targets. Comparisons force index/command rebuilding, then exercise reuse on the same frozen source frame. Output was also visually inspected. Final runs contain no mesh overlap warnings or exceptions.

Limits: Windows/RX 9070 XT private diagnostic player; its existing serialized assets/shaders were retained for matched tests. GPU timing was unavailable. The two-car fixture is local Legend AI, not a public online race. Scenery transitions still sometimes update more than 1,800 material queues; their exact source ordering was preserved. This pass does not establish a fix for the reported 40 FPS online/later-sector drops. Changes remain local; no Desktop installation, GitHub publication or Discord messages.

Superseded diagnostics are retained: the first manual-render attempt double-rendered, the first Legend/night request disagreed with the rival's authored daytime conditions, individual submesh updates produced transient overlap warnings before being changed to a bulk update, and a menu capture initially used a world-geometry assertion. None of those runs is used as passing final evidence.
