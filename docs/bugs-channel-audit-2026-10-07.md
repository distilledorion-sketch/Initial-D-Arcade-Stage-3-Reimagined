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
