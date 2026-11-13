# Original feedback translation — November 13, 2026

The original Arcade Stage 3 force, damping and vibration calculations now have
a production C++ translation in `Native/src/original_feedback.h/.cpp`. The live
game path reads the original vehicle and transmission state, calculates once
per accepted local simulation step, and passes completed output to the wheel
and controller adapters. This replaces the previous heading-error estimate and
collision-only vibration envelope in that path.

This work is staged in source and private build outputs. It does not update
`D:\Current`, change saved settings, enable a device, or publish a release.

## Implementation

`original_feedback_presentation.h` maps raw original fields without changing
their units or steering sign. `App::projectOriginalPose(true)` advances the
producer for solo play and the presented local slot in authoritative online
play. It does not modify the physics session or run for replay viewing,
showcases, menus, pause, results, disconnects or a stalled authority. Duplicate
reads do not advance the original startup ramp.

The port preserves the original operation order, fused multiply-add location,
integer truncation, lookup tables, weather/oil branches, wall-corner response,
automatic-braking override, output caches and positive-rumble refresh behavior.
Its fixed per-car engine-reference table is copied from `0C270EB0`, including
the 12,000 entry for car 30; it is separate from an upgraded rev limit. Invalid
indices, nonfinite state and unrepresentable/invalid output fail closed.

The saved steering index comes from profile byte 1187. The other original
configuration index has an explicit host default of 5, separate from device
strength. Its original operator-menu caption remains unresolved; this change
does not pretend the user's strength slider is that setting.

The new `Idas3SceneGetFeedbackState` returns a versioned 40-byte snapshot.
The existing `Idas3SceneGetWheelState` ABI is unchanged. Unity reads one new
snapshot for both output destinations, including when output settings are off,
so old events cannot accumulate. Missing or invalid native output stops feedback
instead of using the old approximation.

Current vibration and the highest unconsumed short event are carried separately.
A peak survives multiple simulation ticks in one rendered frame, managed output
throttling and queued wheel requests. Bounded event histories give each event
its own expiry, so an old high peak cannot hide a newer smaller impact.
Undelivered peaks expire; they do not
receive the old quarter-second decay. Peak delivery is host scheduling policy,
not a claim that a 30 FPS renderer reproduces exact physical event timing.

The wheel adapter sends raw normalized output through `Idas3WheelSetCabinetState`.
Device strength is applied once as output gain. The damping coefficient now
uses the original varying second parameter, while saturation uses the first.
The legacy board adapter no longer scales its already-scaled output twice.
Foreground ownership, selected-device rules and finite 100 ms wheel/XInput
leases remain in place. Force feedback stays off unless previously enabled.

## Boundaries still requiring physical comparison

The host retains its existing steering polarity and Invert setting. Software
checks verify the conversion and signed driver payload; they do not establish
the physical direction or feel of a particular wheel. Force directions and
device axes are backend conventions, not interchangeable with an on-screen
steering sign. Relevant primary contracts: [SDL direction](https://wiki.libsdl.org/SDL2/SDL_HapticDirection)
and [DirectInput effects](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416616(v=vs.85)).

Rumble continues to use the existing Flycast v2.7-style amplitude conversion,
`max(0, amplitude - 1) / 80`, followed by the user's linear output gain. It is
not a claim to match Flycast v2.5's stronger controller scaling. Ordinary pads
receive equal motor intensity, not a guaranteed 31.5 Hz physical waveform.

Generic Unity gamepads still rely on the main thread to send a stop; unlike the
native wheel and XInput adapters, they do not have an independent expiry while
that thread is blocked. No unsafe background Unity device calls were added.

## Verification evidence

Private build and test evidence is in
`Verification/original-feedback-integration-20261113/`; original instruction
comparisons for the production code are also in
`Verification/original-feedback-producer-audit-20261113/forces/production-result.txt`.
The production translation passed 68,712 executions of the complete original
routine, with 481,265 checks of outputs, state and safe rejection boundaries.
All six selected native test suites passed, including 1,522 presentation checks
and 206 fake-device backend checks. The actual App probe passed 13,265 checks
across 900 solo steps and 660 shared-authority steps, including parallel physics
state comparisons, both local-player slots, and lifecycle gates. Managed
wheel/controller checks passed 1,081 assertions, including independent expiry
of repeated and smaller vibration events.
The complete managed assembly compiled from 195 source files with zero errors.
Fake-device checks cover wheel damping, gain, polarity conversion, vibration
events, queues, stale updates and shutdown. No physical output is used by any
of these tests. The final build manifest records the integration results and
staged binary hashes.

This change does not adjust acceleration, tire grip, cornering physics, car
tuning or track scale. It changes how their original state produces feedback.
