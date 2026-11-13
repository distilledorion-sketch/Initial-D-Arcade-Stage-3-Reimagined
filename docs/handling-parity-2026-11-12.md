# Original Arcade Stage 3 handling re-audit — November 12, 2026

The target is the original GDS-0033 behavior, not a subjective grip or speed
rebalance. This pass found and corrected a wall-clock scheduling discrepancy.
It does **not** certify complete cabinet parity or establish a general clean-
corner speed advantage in the remake.

## Corrected difference

Driving previously used the general 60 Hz host clock. The archived original
game's Flycast capture contains 5,103 valid samples with SPG_LOAD `0x02110353`,
SPG_CONTROL `0` and FB_R_CTRL `0x00800005`: 852 pixels × 530 scanlines at
27 MHz. Its initialized elapsed-driving counter advances once per vblank;
the reviewed intervals contain 3,944 single-frame advances and 162 held frames.

The nominal video period is `852 × 530 / 27,000,000` seconds, or
59.7927185756 Hz. Rounding that to 60 runs the game 0.346667% faster in wall
time. Flycast's integer scanline-cycle scheduling produces 59.7937712828 Hz;
the new clock uses the unrounded original pixel-clock period. These are
nominal timing calculations from captured registers, not an oscillator
measurement on a physical cabinet.

`OriginalDrivingClock` now schedules the existing race owner. It retains
bounded catch-up, pause reset and render interpolation. Numerical `1/60`
frame units in the lifted equations, original timer increments, input
conditioning, car tables, tuning, collision and race tick numbering are
unchanged. Menu clocks are unchanged. Rendering may still run at the selected
display FPS. This small pacing correction does not explain a large reported
cornering advantage.

The timing interpretation was cross-checked against version-matched primary
source: [Flycast v2.5 scanline scheduling](https://github.com/flyinghead/flycast/blob/v2.5/core/hw/pvr/spg.cpp)
and [register definitions](https://github.com/flyinghead/flycast/blob/v2.5/core/hw/pvr/pvr_regs.h).

## Current evidence

- The 21 original-code tests passed. They cover controls, tables, initialization,
  dynamics, transmission, collision, road/body contact, recovery, race start,
  complete player sessions and player/rival sessions. The reference executes
  instructions from the owner's original image; it is not a second invocation
  of the native solver.
- A new permanent test expands the full player-session comparison to 70 runs
  of 900 frames: all 35 cars and all 18 original course/direction conditions.
  Dry/automatic/stock and wet/manual/tuned cases are distributed across those
  runs; this is not their complete Cartesian product. It includes lift-off,
  partial pedals, hard braking, opposite lock, shifts, original road queries,
  impacts, contact-audio RNG, publication, recovery and rollback. All checked
  state words match. The test also requires moving-brake, wall-contact,
  gear-change and wall-free steering frames above 72 km/h to occur.
- Four host/controller/race/rollback checks passed. The default Flycast
  controller response matches the independently implemented ADC conversion
  across signed-axis values. Custom deadzones, smoothing, keyboard ramps and
  direct H-shifter bindings remain host options, not physical cabinet controls.
- A new clock test checks 1,793 driving updates in 30 seconds at seven display
  rates, 20–240 Hz. All 10,758 compared full-session state digests are identical
  at equal driving ticks. Pause, interpolation, stall limits and sliced online
  catch-up are covered.
- The rebuilt native library passed an actual private Unity run, muted on an
  unshown desktop: 18,507 checks, 1,793 driving updates across 1,799 submitted
  `1/60`-second host intervals. That matches the new cadence to within one
  fractional frame. This uses supplied host deltas; it is not a foreground FPS
  benchmark or a new two-PC online test.

## Recorded full-game Flycast comparison

The current source was freshly linked and fed the original captured raw ADC
sequence from the October 13 Myogi/stock AE86/AT/dry run. Only the initial pose
and selection are used to reset the solver; recorded speed/history/contact
state is not injected. It compares 3,174 observed rows / 3,232 solver frames.
Speed, XYZ position, yaw, steering, pedals, gear, grip/loss and wall-contact
fields match bit-for-bit, including the recorded wall-free steering pulses.

Remaining discrepancies are one startup RPM sample (5.1405 RPM at the
40-frame warmup observation) and 78 yaw-step values after stopping, with a
maximum absolute difference of `9.8951e-39`. No position, yaw or speed difference
was observed. Startup sampling and subnormal floating-point treatment require
separate investigation; neither discrepancy was hidden or patched by copying
the expected state. The comparator supplies observed gear-enable and finish-
brake boundaries, and does not include the captured AI opponent. It therefore
does not independently certify those owners or the whole live game.

## Geometry and remaining scope

A fresh comparison against original HOSTFS found the original 18 driving paths,
14 collision files (151,664 vertices), eight road guides and 16 main-track
banks (5,473,289 vertices) unchanged in scale. All 1,871 examined assemblies
have unit scale. This supplies no evidence of enlarged original tracks.
Imported courses remain adaptations and have no original AS3 counterpart;
Odawara's requested Myogi speed/Tsuchisaka handling combination is deliberate.

Complete 1:1 handling still requires matched skilled full-lap/corner input
traces, the relevant cabinet wheel/calibration and feedback, other game
revisions if supported, and the complete running host including online owners.
The correction was initially staged without changing `D:\Current`. Following
the owner's installation request, the tested native DLL was installed there
with a backup of the previous DLL and build information. The installed game
passed 18,507 checks muted on an unshown desktop, including 1,793 driving ticks
across 1,799 submitted `1/60`-second host intervals. Saves, settings, original
game data and custom music were verified unchanged. The test game exited.
That earlier installation's DLL SHA256 was
`cbc95f65c672084c2df8695a106fd9a7078bab03e4e284bec06ef99314a72265`.
GitHub publication remains held; a release must keep both online players on
the same simulation cadence. Installation evidence is in `installed.json`
and `installed-check/result.json`; `summary.json` retains the pre-install audit.

## Evidence

### Sustained cornering follow-up

`Verification/cornering-live-20261112/` contains a **new** input-only Flycast
v2.5 run, rather than another analysis of the October capture. A private copy
of the emulator ran muted on an unshown desktop. No original game memory was
patched. A full-throttle path-following input script drove stock AE86 / Myogi
CCW / dry / automatic, selected through the original menus. This was a Legend
race against Iggy, not a skilled Time Attack lap. The capture contains 11,113
observations across 11,171 solver frames, including sustained turning and wall
contact. Its complete data flushed before termination, but the emulator
shutdown was nonzero; this is not recorded as a clean emulator-exit check.

The new comparison reproduced a real floating-point mismatch. Under a host
using gradual underflow, the first difference was drive `+0x25c` at frame
1,642: zero in Flycast versus `1.1592512e-38` in the remake. It later propagated
to trajectory and contact differences. The full open-loop replay eventually
diverged by 72.37 km/h and roughly 653 metres in the horizontal plane. Those
late values compare cars on different paths and **do not establish a uniform
speed or grip advantage**. The speed difference first exceeded 0.1 km/h at
frame 5,400, when the wall-contact flags also differed.

Running the solver with nearest rounding and flush-to-zero, matching the
Flycast comparison, removes every measured movement/handling mismatch in
that trace. The production solver now selects this mode at initialization,
each driving step, and the shared two-player simulation, then restores the
caller's previous control register even on exceptions. It does not change
handling constants or a process-wide setting. The fresh capture's only
remaining difference among the 320 drive words is the RNG-initialized
`+0x21c` value; the live boot RNG was not reproduced. Observed gear-enable and
automatic-brake boundaries are still supplied, and the source replay excludes
the AI opponent. It is not certification of those owners or cabinet silicon.

A separate native-scene probe drove the DLL installed in `D:\Current` through
150 seconds of Akina downhill / stock AE86 / dry / automatic Time Attack.
It used private saves, the authored starting grid and full-throttle steering
commands, with no teleportation, collision bypass or extended race timer.
The car peaked at 144.933 km/h; the imperfect input script also hit walls.
Replaying those same 9,000 commands through the original SH-4 instructions
checked 9,240 complete solver steps including startup, 11,780,178 state-word
comparisons and 242,177,067 original instructions. The original and corrected
solver matched, including road contacts, histories and contact-audio RNG.
The old installed DLL differed by at most 0.087561 km/h in this particular run.
The corrected DLL's 9,180 observable post-warmup steps matched speed, yaw and
gear without that drift. This probe uses the native scene API and source
assets; it does not test a physical controller or Unity's managed input path.

Validation includes 29 focused native tests, the 70-scenario handling matrix,
and 4,800 single-car plus 3,600 two-player state digests under five caller
floating-point modes. The three changed solver translation units also compile
for Android ARM64; inspection of an optimized scope probe confirms arithmetic
occurs between the FPCR writes. No new Android runtime parity claim is made.
Private Unity validation stays muted and uses an unshown desktop.

The large reported corner-speed advantage has **not** been reproduced as a
systematic handling difference. The tests are stronger than the previous
short steering pulses, but do not replace a matched skilled corner/lap with
the same car, tune, conditions and controller calibration. Custom steering
smoothing, keyboard ramps and FOV remain separate host options; no personal
settings were changed and no arbitrary grip reduction was made.

The verified correction was subsequently installed in `D:\Current`, retaining
the earlier cadence correction and all preceding local fixes. The actual
installed Unity player passed 18,507 checks, remained muted on an unshown
desktop, and exited normally. Saves, settings, original game files and custom
music were verified unchanged. A separate backup preserves the previous
`cbc95f...` DLL and build information. The new DLL SHA256 is
`fac41477240023651e1685030aa02a043caef99fcb1254e6695b600b3ab371ab`.
`cornering-live-20261112/installed.json` records the completed installation;
its `summary.json` preserves the pre-install validation. GitHub remains held.
The existing multiplayer compatibility check hashes the native DLL and
managed assembly, so an older simulation build cannot silently join this one.

`Verification/handling-parity-20261112/` contains source/reference hashes,
the fresh build logs and DLL, JUnit results, complete trace metrics, geometry
comparison and the muted Unity result. `reference-provenance.json` identifies
the archived live captures and explicitly labels this pass as a reanalysis,
not a new Flycast run. `summary.json` records final verification and hashes.
The original ROM and full-game capture files remain private and are not added
to the repository.
