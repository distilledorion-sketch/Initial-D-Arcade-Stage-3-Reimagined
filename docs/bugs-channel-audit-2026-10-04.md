# Bugs forum follow-up — October 4, 2026

Read the authenticated Discord bugs forum `1548170688857112637`, including the new reports and relevant replies. Scope is this forum only. No Discord messages, reactions, tags or report statuses were changed.

Work is isolated on `fixes/discord-bugs-20261004` in `D:/Codex/GitHub/Initial-D-Arcade-Stage-3-Discord-Bugs`, based on release .38 commit `80923197cb739fecc3e91da20992e19ebcc75882`. The original checkout's R35 work remains separate. These fixes have not been published, deployed to the leaderboard, or installed over a desktop build.

## Fixed locally

| Report | Finding and change | Verification |
| --- | --- | --- |
| [Xbox Controller Stops Responding briefly](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1556313764079472710) | Automatic selection could alternate between a physical controller and its delayed Steam Input copy, repeatedly resetting the binding state. Keep the connected active pad while it has held/recent input; permit another device after 0.5 seconds of neutral input. Explicit selection and unplug recovery remain available. | 380 synthetic-device reconnect/selection checks, including 360 frames of duplicated controller activity, neutral handover and unplug fallback. This addresses the reported switching case; no physical Xbox controller was connected for this run. |
| [Replay Viewer bug](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1555293566740201572), plus October 4's bumper-camera report | The viewer used an approximate elevated camera and default HUD layout. It now evaluates the recovered bumper transform and field of view from recorded position, yaw, pitch and roll, and loads the saved HUD size/placement options using their migration rules. | 30 native camera poses compared with the live bumper camera, including wrapped yaw and banking. Managed compilation passes. Imported-course surface offsets and an actual affected user's replay still need visual comparison; no claim that every gutter/camera case is resolved. |
| Replay Viewer keychain visibility/movement | Replay keychains now appear only in bumper view and react to recorded motion on a fixed 60 Hz timeline. Pause holds the pose; seeking resets and warms the motion history. | Parsed IDR2 turning/bumping fixture at 30, 60, 144 and 240 render FPS; identical final chain/pendant poses, pause stability and backward-seek reset. |
| [Altezza has no 5th gear icon on alternative huds.](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1555368852882456606), [Missing Gear 5 text on infinity meter](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1553955767768457267) | The previous .38 change incorrectly treated atlas cell 7 as a silver fifth-gear image. It is blank. Gear 5 now selects the actual fifth-gear cell, preserving its authored color. This corrects the September 27 audit's mistaken cell-7 assertion. | Real GPU output for all 26 styles using the affected atlas family, rendering gears 4, 5 and 6 for both five- and six-speed cars: 156 visible-digit checks. Saved fifth-gear images include Infinity, Hot Version, Miku and Touhou meters. |
| Bunta will be weak after beating 15+ level (September 29) | The saved completion marker 16 wrapped to zero in the original four-bit pace lookup. Clamp only the host's pace input to the highest authored tier, 15. Preserve the saved completion marker and progression. | Actual host sessions with levels 0, 14, 15 and 16, plus 1,196 existing progression/storage checks and 122 save/restart cycles. |
| You cannot select another option for tune. when changing a vehicle (September 28) | A stock car chosen through an existing save now gets its own A/B/C/D package screen after AT/MT and before mode selection. Selecting a package does not grant upgrades or points. Already upgraded cars retain their tune; single-package GC8V skips the extra choice. | Actual frontend-to-host save transitions for all four packages and both transmissions; preserved name, points and stock parts; cancellation and existing-tune/single-package checks. |
| Discord shows Akina DH for any other new course during TA (September 29) | The native presence payload used the imported course's physics donor ID. It now publishes the selected imported course ID. | All 16 courses in both directions, in menus and races. No Discord status transmission was used as a test. |
| Course order on Online Leaderboard is wrong (October 2) | Display order is now Myogi, Usui, Akagi, Akina, Irohazaka, Akina Snow, Happogahara, Shomaru and Tsuchisaka, followed by imported courses. Stable course IDs and existing times are unchanged. | Existing leaderboard suite: 41 tests pass. This local web change is not deployed. |
| [Tsubaki track map in SRA is mirrored](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1553852866656211075) | Tsubaki's full/section analysis maps now use the course card's +Z-down orientation. Road, driven trace and events share the same projection. | Both directions and all five analysis pages; an unrelated imported course retains its previous orientation. |
| [Auras not playing their SFX when they appear](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1553943998366879796) | Copied only the independently developed aura fix from the other checkout: submit the original PACK24 cue 5 once at the qualifying online showcase's source frame 1. No duplicate cue for two qualifying cars. | 4,334,512 aura assertions against original behavior, including 1,996,555 source instructions. Mixer checks verify the compound three-note cue, pause, Effects mute and cleanup. |
| Misfiring System Visual Bug (October 4) | Restored the local tuned Evo III's missing exhaust flash using the original `bkfire` mesh/texture bank. Its existing accepted misfire cue drives two 60 Hz effect frames. Body pose carries the flash; Effects mute does not hide it. All three tuned exhaust mounts are supported. | High-RPM throttle-release through the real engine audio controller, muted Effects, render-clock independence, confirmed online frame advancement, restart cleanup and stock/other-car exclusion. Nine rendered car captures cover all three exhausts with no flash and both effect frames; six before/after comparisons confirm visible pixels at the pipe. |

### Exhaust effect limits

This restores the reported local-player visual, not every behavior of the original effect owner. The later follow-up below adds confirmed online-opponent flash timing. Replay flash timing, per-draw random flame-length jitter and the source scene-light pulse are not implemented. The original effect geometry/materials, mount table and rotations are retained, with a body-local placement adaptation verified against the rendered exhaust. No extra driving RNG consumption, physics changes or sound event was introduced.

## Reports still open

| Report | Evidence and remaining work |
| --- | --- |
| [Ending credits do not play after completing Legend of the Street](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1554605215767076905) | Credits and final artwork are now restored locally; see the later follow-up below. The original driving cinematic behind the roll remains unimplemented, so the complete original ending is still only partially restored. |
| [Can't setup my Logitech G923 Wheel in the Options Menu](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1552980823815495680) | Replies include another Xbox 360 binding failure and a working G29. Previous synthetic capture fixes are already in the base release. No G923/Fanatec hardware or affected-player input log is available here; the duplicate-controller fix is not evidence that wheel binding is repaired. Need the device/control diagnostics from a failed binding attempt. |
| [Auto Updater does not work on Linux again](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1553197675703312535) | No new diagnostic details in the report. Earlier Windows/Wine alias tests and fixes do not establish Steam-managed Proton behavior. Need the affected player's updater log, install path and runtime; no speculative updater edit in this batch. |
| [Cannot submit lap times in China without VPN](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1553327616239345714) | Still requires affected-network evidence and a reachable, authorized service hostname. Local service tests do not prove reachability from that network. |
| [FPS Drop Issues](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1552613619269640242) | Read the reports of two-car online drops, especially Akagi uphill. Earlier local two-peer measurements did not reproduce the reported slowdown. No new representative profile was available; it remains unverified. |

The September 26/27 audits remain the record for older topics. Existing fixes and prior passing tests are not blanket confirmation that every player-reported case is resolved.

## Afternoon follow-up

Read the new topics and their replies after the user restored the browser login. No Discord messages were sent or report statuses changed.

| New topic | Finding and status |
| --- | --- |
| [The rear-view in mirror is higher than the original D3](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1556382717090988173) | Fixed locally. The host supplied the raised body position to the rear camera, adding the car's body-only ride height. The source bumper-view camera uses the live actor matrix with its -0.02 vertical offset and local +0.8 camera offset. The host now uses that actor anchor, retaining imported-course smoothing and online presentation offsets. All driving camera choices use the same corrected mirror. |
| [Random Crash](https://discord.com/channels/1548169613206884355/1548170688857112637/threads/1556376861888487484) | Still open. The player reports failure after time trials or entering story mode, even after reinstalling. Their screenshot shows `Idas3SceneGame.Update` and a path containing the .27 release folder, but the actual exception and the rest of the path are clipped. This does not establish the running binary's version or the cause. Need the full exception from that player's `Player.log`, their current version and install path before claiming a fix. |
| Community Leaderboard not Getting Updated with New Times | The original message had already been deleted when opened; no replies or reproduction details remained. The topic itself subsequently disappeared from the live forum during this read. No evidence supports a specific leaderboard change from this report. |

Mirror evidence is in `Verification/discord-bugs-20261004/mirror-followup/`:

- `original_rear_view_tests`: 720 isolated matrices plus 720 actual source actor-to-car-to-camera sequences, 18,004 comparisons and 326,207 executed source instructions. Eye and up vectors are checked bit for bit; facing accounts only for the source model half-turn. The earlier isolated callback test alone could not detect the incorrect host anchor.
- `discord_bug_application_tests`: 2,324 passing checks, including all 35 body ride heights across Bumper, Chase and Natural. Body placement must not alter the mirror anchor or banking.
- The arranged Akina downhill night/Bunta capture uses FD3S Type R and AE86 at a 4.3 m center-to-center offset. `akina-mirror-before.bmp` and `akina-mirror-after.bmp` keep the scene and rival placement identical. The corrected view includes both headlights instead of cutting off the car below its bonnet. The measured height correction is 0.319946 m. This is a native renderer comparison, not a recreation of the player's exact race.
- `rear_view_renderer_tests` passes mirrored side order, viewport containment, repeated rendering and HUD restoration at three resolutions. The native Unity plugin builds successfully. No full Unity player build, desktop installation or GitHub publication was performed for this follow-up.

## Reproducible checks and local evidence

Evidence directory: `Verification/discord-bugs-20261004/` (local, ignored by Git).

- `Native/tools/check_discord_bugs.cpp`, built as `discord_bug_application_tests`, exercises the actual host. `host-final/PASS.txt` records 2,102 passing checks for bumper camera, course presence, saved-car packages, Bunta, Tsubaki and backfire. It requires a fresh output directory and uses isolated save files.
- `Assets/Scripts/Idas3ControllerReconnectChecks.cs`: `controller-retained/report.json`, 380 passing checks.
- `Assets/Scripts/Idas3ReplayOrnamentChecks.cs` and the GPU `GearPixels` check in `Idas3BugMeterChecks.cs`: `managed-retained/report.txt`, 172 assertions, zero failed suites. The diagnostic harness ran in a separate copy of the existing Windows player; no normal game session or live saves were used.
- Native `Idas3Unity` builds successfully; production managed scripts also compile successfully against the existing Unity references. The Unity editor license was unavailable, so this is not a claim that a new full Unity player build was produced. The separate diagnostic player must never be deployed.
- `aura.txt`, `audio-backfire.txt`, `bunta.txt`, `frontend.txt`, `leaderboard.txt` retain their corresponding test results. Audio compares 324,108 exact stereo frames. Existing frontend checks cover all nine screens, 35 cars and nine base courses.
- `host-backfire-anchor/visible-flashes.json` and the car captures document the body-local exhaust alignment. The first visual inspection caught a source-owner height offset being incorrectly applied a second time; this was corrected, and a tighter geometry assertion now catches that error.
- `backfire-import.json`/`backfire-reimport/` preserve extraction evidence; a backfire-only reimport produced byte-identical mesh and texture packs. Runtime source/output hashes and source-owner references are in `Native/data/original_assets/effects/bkfire/manifest.json` and its README. No CHD or executable ROM image was added.
- `ending-owner.txt` records the incomplete ending investigation for the next pass.

## Remaining-report implementation follow-up

### Legend credits and final artwork

`finishLegendVisit` now starts the ending owner after the final Legend result. The original staff names, photo strips, Takumi/AE86 final card and stream 12 play before returning to Title. Source scroll ranges and fade/phase arithmetic run at 60 Hz independently of render FPS. Start/Escape skips the roll, and a fresh press skips the final card; a held dialogue skip does not skip the credits. Presentation does not award points again or modify the completed profile.

The 3D driving cinematic behind the original credits is **not** restored. This implementation displays the roll over black, followed by the original final card. The earlier blanket “ending not implemented” status is superseded only for these restored portions.

Evidence: `Verification/discord-bugs-20261004/ending-final/`.

- `original_ending_tests`: 160,944 checks and 822,622 executed original instructions across normal playback and five skip boundaries. Draw, integer conversion/division, audio and parent-notification dependencies are hooked; this is not an original full-cinematic render comparison.
- `ending_application_tests`: 46 checks. Normal playback completes at exactly 5,460 source ticks at 30/60/144/240 render FPS. Audio starts/stops, pause, held input, both skip stages and profile stability pass. The full 31-rival **result-flow fixture** reaches Ending and then Title with all 31 completion markers retained; it does not play 31 full races.
- Captured real native-renderer frames at ticks 600, 2400, 4600 and 5100 were visually inspected: readable, correctly oriented staff/photo strips and final card. No Unity full-player build or desktop deployment is claimed.
- Assets are reproducible through `Native/tools/extract_original_ending.py`; runtime files and source/output hashes live in `Native/data/original_assets/ending`.

### Online opponent exhaust flashes

The shared online simulation already emitted the remote tuned Evo III's misfire command, but the host consumed only the local car's commands. The host now consumes confirmed remote cue 7 to drive the same two-frame exhaust effect, with the remote car's saved exhaust assembly. It is visible in both the driving view and rear-view mirror. Rendering does not tick the effect or alter simulation RNG; disconnect and rematch clear it. This does not synthesize an extra opponent sound.

Evidence: `remote-backfire-final/PASS.txt`: 2,517 actual-host regression checks. Coverage includes original high-RPM/throttle-release controller output for all three exhausts and either local slot; local/remote cue isolation; no advancement on repaint; two confirmed frames; restart/disconnect cleanup; actual online renderer submission, mirror view mask and unchanged simulation digest. Replay flash timing remains open because existing recordings do not store the accepted cue.

### Readable crash diagnostics

The crash panel previously clipped long file paths and stack traces in a fixed 220-pixel box. It now scrolls, offers **Copy error details**, and writes `last-error.txt` beside `Player.log`, including version, platform, graphics, scene stage and the full exception. Cleanup exceptions are logged separately so they cannot replace the original failure. Managed production scripts compile successfully.

This is a diagnostic improvement, **not a root-cause fix** for the “Random Crash” report. The affected player's full exception remains unavailable.

### Sadamine wet-corner investigation

Extended the previous short wet-drive fixture to full runs in both directions using the report's AE86 Levin. The stronger cornering fixture reaches 131.1/131.6 km/h and completes both 6.83 km routes. Across 50,167 actual driving ticks there are zero lost ground anchors. The full road survey checks 81,942 lane samples, and all three camera render paths leave physics words unchanged. `sadamine-wet-before/imported-road-presentation.txt` records 611,059 passing assertions.

No new camera/handling change was made: this fixture still does not reproduce the player's sharp-turn bouncing. An affected replay and the camera setting remain necessary to reproduce that exact case. These results do not establish that every curb, collision or player-driven line is smooth.

### Other open reports

Linux/Steam Proton updater failure, physical G923/Fanatec binding failure, the exact post-race controller failure, public online FPS/collision intermittency, China service reachability, and remaining replay/gutter-camera cases are still unconfirmed. No new affected-machine logs or reproducible network/hardware cases were supplied during this follow-up. Prior synthetic/local passes are not treated as proof of fixes on those players' systems.

The native Unity plugin builds and production managed scripts compile. Work remains local on the bugs branch; no GitHub publication, leaderboard deployment, Discord post, desktop replacement or R35 changes were performed.
