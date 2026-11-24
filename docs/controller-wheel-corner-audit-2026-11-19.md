# Gamepad vs. wheel in a corner — November 19, 2026

**Finding:** Release `.56` has a reproducible gamepad steering difference from Flycast v2.5. On the default response, moving the stick vertically while holding it halfway right changes steering from 70% to 75% in `.56`. Flycast retains 70% until another horizontal event. A private Windows build now matches this sequence. There is no measured exit-speed advantage for the corner in RPM.S’s report: the available stills compare different runs, and the player says they used TST once.

## Device travel vs. car behavior

The release takes the steering response from the game settings. The active binding profile and the saved steering-response setting are separate. For this comparison I explicitly selected each response and used its default deadzone (gamepad: 10% radial; wheel: 0%). Smoothing was off. Values below are steering magnitude after the arcade calibration, with vertical travel at zero. Half travel means half of the reported axis range, not half of the same physical angle or effort.

| Horizontal travel | Gamepad response | Wheel response |
| --- | ---: | ---: |
| 25% | 26.25% | 40% |
| 50% | 70% | 80% |
| 75% | 100% | 100% |
| Full | 100% | 100% |

The gamepad curve delivers *less* steering in the middle of this horizontal sweep. Both reach the same lock. That tells us nothing by itself about which driver takes a particular corner faster: turn-in, unwinding the steering, using the pedals and the time taken to reach each steering angle all matter.

I built `Native/tools/audit_pad_wheel.cpp` against the archived `.56` driving library and headers from commit `5a02ea97bcad6c468b7cc77bf404990fadfba220`. Three independent stock AE86 Myogi dry manual sessions started on the same grid. The signed displacement was constructed from the retained no-repeat Flycast input tape; pedals were chosen to reproduce the conditioned levels and the seven shift edges were preserved. These runs deliberately start at the native grid and advance the numerical session, without attempting to duplicate an arcade menu/countdown history. The tape is not input recorded from RPM.S or a physical wheel.

- At the same normalized travel, the pad and wheel response delivered different steering on **3,304 of 3,940 updates**. The first difference in car pose was on frame 484. This is a positive control: different steering actually moved the cars differently. Once lines diverge, comparing speeds at a common tick is not an exit-speed comparison.
- In a third session, the wheel travel was chosen to give the car the exact steering, pedal and shift values produced by the gamepad on each update. **Zero differences on 3,940 updates** in the drive-state words, the actor words or the simulation’s state digest. This isolates the numerical car behavior for this case; it does not measure physical device latency, certify every car or tell us what inputs the two drivers used.
- The separate `.56` controller-response sweep passed **2,752,512** channel comparisons. It tests static X/Y combinations and endpoints. It cannot catch a bug where the *order* of motion events matters.

## The event sequence that differs

A process-local SDL 2.30.10 virtual joystick generated real axis-motion events. Those events went through the unmodified Flycast v2.5 `GamepadDevice::gamepad_axis_input` function body, its opposite-axis handler, a fixture mapping, and arcade calibration. The same raw snapshots went through the `.56` input code. This tests Flycast’s input implementation; it is not a physical cabinet test or a new full emulator race.

| Event (signed raw units) | Flycast | Release .56 | Corrected app input |
| --- | ---: | ---: | ---: |
| X moves to 16,384; Y is zero | 70% | 70% | 70% |
| Only Y moves to 24,576 | 70% | 75% | 70% |
| X moves one count to 16,385 | 75% | 75% | 75% |
| Only Y returns to zero | 75% | 70% | 75% |
| X returns to center | 0% | 0% | 0% |

Flycast’s SDL dispatch passes one axis per event (`core/sdl/sdl.cpp`, `SDL_JOYAXISMOTION`). The input handler updates the axis named by that event (`core/input/gamepad_device.cpp`, `gamepad_axis_input`). The remake recomputed horizontal steering from both current stick axes on every driving update. Its new default gamepad path retains the vertical value from the last observed horizontal change. It samples on host frames even when no driving update is due. Wheel response, legacy response, custom deadzones, selected smoothing, and car physics are unchanged. A disconnect, suspension or disabled mode clears the retained value. Input snapshots still cannot reconstruct the order of two physical events inside a single poll, or a repeated X event that carries exactly the same numerical value. This change does not claim parity for those cases.

The actual native game input test exercised the table above, including an X movement followed by a Y-only movement before the first solver tick. The wheel remained at 80% in the half-travel check; a nondefault deadzone retained its existing behavior. The Windows game plugin built in private staging. The application input, host steering, Flycast keyboard, and controller-response tests passed. No installed game, personal save, online service, or GitHub release was changed.

## What would establish the reported speed difference

Use RPM.S’s full run (preferably the game replay) and the matching segment from Lugo, with the course, direction, car, tune, conditions, game version and steering settings identified. Compare the cars at fixed track landmarks for turn-in, apex and exit; record entry speed, line, steering including the unwind, throttle, brake, gear and shift frames. Count the one TST on each run independently. If the inputs differ, compare both sets of inputs through the same car and track before changing grip or acceleration. No confirmed evidence here calls for a blanket controller penalty or a TST cooldown.

## Reproducing the local comparison

Build the `audit_pad_wheel` CMake target and pass the native asset root, an accepted-input CSV, and a new output CSV. The exact `.56` headers, archived-library binding, build commands, raw joystick events, frame measurements, candidate test outputs and file hashes are in `Verification/driving-parity-next-20261113/owners/controller-wheel-20261119/`. `result.txt` and `measures.csv` are the release comparison; `events.json` is the virtual joystick capture. No original game files are distributed by the probe.
