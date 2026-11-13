# Flycast force feedback and controller vibration audit — November 13, 2026

This audit traces original game commands through Flycast's emulated drive board
and device backends, then compares the remake at each boundary. It found active
differences in damping, strength controls, event delivery and rumble generation.
No production code or installed game was changed. All executable probes used
recording stubs; no game, physical wheel or controller output was started.

## Versions matter

The executable used in the earlier comparison reports version **v2.5**; its
SHA256 is `ce000cd21b404d81aa5885da9baa6685e283bd5a09fc6c56721e09312008dc2d`.
This audit compares the source of that version with **v2.7**, identified as the
latest stable release by the official release page when checked. It does not
assume that the older local source checkout is either version.

| Reference | Pinned Git commit | Bundled SDL commit |
|---|---|---|
| Flycast v2.5 | `5f4eefab5852d1c832ad8ffdf514fef8c217f1ac` | `2359383fc187386204c3bb22de89655a494cd128` |
| Flycast v2.7 | `5aa091fde632fb332c8d8c34e280d62dc951954c` | `00d4f1c2c4ad2dc95d26b18743541f57b1dca56d` |

Primary sources: [v2.5 board decoder](https://github.com/flyinghead/flycast/blob/5f4eefab5852d1c832ad8ffdf514fef8c217f1ac/core/hw/naomi/naomi.cpp),
[v2.7 board decoder](https://github.com/flyinghead/flycast/blob/5aa091fde632fb332c8d8c34e280d62dc951954c/core/hw/naomi/midiffb.cpp),
and [official releases](https://github.com/flyinghead/flycast/releases).

## How the signal reaches the device

The original AS3 executable generates commands for the cabinet's drive board.
It writes them through AICA MIDI. Flycast recognizes Initial D, attaches its
drive-board receiver, checks the four-byte packet, interprets the effect, and
sends it to the appropriate host backend.

| Output | Device behavior |
|---|---|
| Constant torque | A capable wheel supplies directional resistance. An ordinary gamepad does not receive this as motor rumble. |
| Damper | A capable wheel applies resistance depending on movement, with separate coefficient and saturation parameters. |
| Spring | Supported by the emulator/driver; the previous original-AS3 caller audit found no normal game caller of the spring command. Do not add an extra centring spring over the game's torque. |
| Rumble | A capable wheel receives a periodic effect. Ordinary SDL gamepads receive equal intensity on their low- and high-frequency motors. |

The gamepad motor API accepts intensity and duration, not the cabinet's exact
wave frequency or steering direction. Equal motor commands do not mean equal
felt vibration across different controller models. SDL replaces a previous
rumble effect when a new one arrives and stops it on zero intensity.
[SDL motor API](https://wiki.libsdl.org/SDL2/SDL_JoystickRumble)

On Windows, Flycast uses SDL; SDL routes suitable wheels through DirectInput
and Xbox-compatible rumble through XInput/HID paths as supported. The remake
uses its own DirectInput wheel backend plus Unity gamepad or XInput vibration.
Flycast routes to devices assigned to the target emulated controller port;
the remake deliberately selects a particular active device.

## Controller rumble changed between versions

The following is **one isolated command**, amplitude byte 25, frequency byte
63, board power 1, with no later refresh or stop command. Live game commands
normally replace or cancel the effect, so the duration column is not a claim
that every in-game impact lasts that long.

| Decoder/output | Pad intensity at 100% setting | Pad intensity at 50% setting | Requested duration |
|---|---:|---:|---:|
| Flycast v2.5 | 65,535 / 65,535 | 3,557 / 3,557 | 17 ms |
| Flycast v2.7 | 19,660 / 19,660 | 1,067 / 1,067 | 1,000 ms |

The older decoder divides amplitude-minus-one by 24; the newer decoder divides
by 80 and clamps it. Both SDL pad paths use a nonlinear strength curve:
50% gives roughly 5.43% of the incoming power before saturation, not half.
The remake uses a linear controller-strength setting, so matching percentage
labels is not a matching test condition.

Wheel rumble also differs. v2.5 uses a fixed 25 Hz sine and softens its magnitude.
v2.7 uses the command frequency and a linear wheel-strength setting. Its integer
millisecond period makes the AS3 request of 31.5 Hz approximately 32.26 Hz at
the SDL effect boundary. The remake requests approximately 31.5 Hz using
microsecond periods. These are different translations; none alone proves the
physical cabinet's exact motor behavior.
[v2.5 SDL methods](https://github.com/flyinghead/flycast/blob/5f4eefab5852d1c832ad8ffdf514fef8c217f1ac/core/sdl/sdl_gamepad.h),
[v2.7 SDL methods](https://github.com/flyinghead/flycast/blob/5aa091fde632fb332c8d8c34e280d62dc951954c/core/sdl/sdl_gamepad.cpp)

## Confirmed differences in the remake

### 1. The original feedback producer has not been fully reproduced

The earlier audit already showed the wheel command producer substituting a
heading-error estimate for an original force factor, dropping a torque-limit
adjustment, and approximating road effects. This audit confirms that original
rumble also depends on multiple car/contact/surface/transmission quantities,
not just one impact value. Positive original rumble commands are sent every
physics frame, even if amplitude is unchanged; the remake owner suppresses
unchanged amplitudes.

In the previous 470-state original-instruction probe, 197 samples requested
nonzero rumble, including 62 with the wall-contact flag clear. Runtime globals
outside captured car state were zeroed and the remake's synthetic impact was
zero; this illustrates missing inputs, not a complete live rumble recording.
Do not interpret those counts as the percentage of real driving time lost.

### 2. The wheel damper parameters are mapped incorrectly

The remake drops the varying damping parameter, treats the small saturation
value as the coefficient, and sets full saturation. Actual code tested against
both Flycast versions produces these DirectInput payloads at board power 1,
100% reference gain and original first parameter 2:

| Original second parameter | Remake coefficient / saturation | Flycast coefficient / saturation |
|---:|---:|---:|
| 20 | 157 / 10000 | 1574 / 157 |
| 44 | 157 / 10000 | 3464 / 157 |
| 120 | 157 / 10000 | 9448 / 157 |

This changes the damping curve; it is not a universal percentage weaker or
stronger. It is active during ordinary driving. Existing backend tests only
advertise constant force on their fake device, so they never exercise the
damper or other optional effect payloads. The new private audit probe does.

### 3. Wheel strength is not currently a percentage gain

The remake converts the setting into an original cabinet/operator dial and
commands full board power. It does not apply a final percentage gain. The
source formula for car 0, saved steering 4, fully open startup ramp, force
factor 1 and steering 0.1 gives identical torque at 35% and 100%, because
neither hits the selected limit. Flycast's wheel strength scales the force
payload directly. Cabinet settings, saved-card steering and device gain should
remain separate if the controls are to have comparable meanings.

### 4. Controller vibration can miss events and changes with render rate

The managed vibration class reads only the latest car state and creates an
impact envelope lasting up to about a quarter second. An actual-class probe
with a fake motor target found:

- An impact on simulation tick 2 was missed entirely at a 30 FPS sampling
  schedule, but detected at 60, 120 and 144 FPS. There is no intervening event
  history to consume.
- With a constant synthetic impact, output stayed at full strength at 30 FPS
  but dipped to 0.9375 at 60/120 FPS because render samples without a new
  simulation tick decay the envelope.
- An observed isolated full impact lasted 250–266.7 ms depending on sampling.

These are software reproductions, not physical controller measurements. They
prove the current envelope and event delivery are not independent of rendering.

Expiry also differs by adapter. The native Xbox target has an independent
100 ms watchdog, but the Unity target sends only motor speeds and has no
independent deadline. With both actual adapters using fake outputs, Xbox
stopped without another update while Unity retained its last requested value.
This identifies a missing adapter guarantee during a stalled update; it does
not measure how every physical controller or driver behaves after a crash.

### 5. Wheel ramp timing follows output delivery

The original ramp advances with the game simulation. The remake advances it
when the managed feedback sender delivers an update; that sender is throttled
and its worker keeps only the latest request. Exact-schedule software tests
reproduce different startup timing at different render rates. Real scheduling
jitter and coalesced requests can change the observed values further. The
original producer needs to run on simulation ticks, independently of delivery.

### 6. Board scalar parity is not full event parity

The remake's board decoder matches v2.7's normal numeric effect formulas in
the new exhaustive checks, but retaining a snapshot loses some command/event
semantics. For example, a power command does not immediately resend every
Flycast effect; reading the remake snapshot immediately rescales retained
values. Enable, calibration, repeated rumble and effect expiry need separate
verification before routing a recovered original command stream through it.

The native wheel backend also applies board power twice. This is latent in
normal driving because the present owner first sets power to 1. At power 0.8,
it produces 0.64 scaling instead of 0.8. It should be corrected before adding
other power/gain policies, but it does not explain ordinary weak feedback by
itself.

## Platform and lifecycle boundaries

The Windows/SDL findings do not automatically apply to Android or RetroArch.
In standard v2.5 builds, board rumble goes through the general vibration
dispatcher, which has Android/Libretro routes. In v2.7, board rumble goes through
`haptic::setSine`, whose body is empty without `USE_SDL`; standard CMake excludes
SDL on Android/iOS and Libretro. This is a source-routing limitation for this
particular arcade board, not a claim that all vibration is broken on those
platforms. No phone or RetroArch runtime was tested.
[Haptic routing](https://github.com/flyinghead/flycast/blob/5aa091fde632fb332c8d8c34e280d62dc951954c/core/input/haptic.h),
[build configuration](https://github.com/flyinghead/flycast/blob/5aa091fde632fb332c8d8c34e280d62dc951954c/CMakeLists.txt)

The remake's DirectInput wheel path intentionally requires foreground device
ownership and 100 ms effect leases, and stops feedback on pause/focus loss/device
change. Those safeguards should remain. Xbox vibration has its own watchdog;
Unity controller vibration lacks the independent expiry described above.
DirectInput wheel driver gain, actuator selection, firmware and
real motor response remain hardware-validation boundaries. Flycast itself
notes uncertainty about AS3's low damper saturation; matching an emulator is
not a physical-cabinet torque certification.

## Recommended correction sequence

1. Correct the active damper mapping and add reference tests that create all
   supported effects, not just constant force.
2. Recover the original force/rumble producer, advance it each physics tick,
   and preserve its events across rendering and output-thread boundaries.
3. Separate device strength from cabinet/operator and card settings; apply
   power once. Preserve bounded device ownership and stop behavior.
4. Specify the target Flycast version for rumble comparisons, then compare
   complete command streams, durations and stop events. Do not combine v2.5
   amplitude/duration with v2.7 frequency and call it either version.
5. Validate on actual wheels/gamepads before claiming identical felt output.

## Evidence

Private evidence is under `Verification/flycast-feedback-audit-20261113/`:

- `board/`: actual pinned Flycast decoder bodies against the native board,
  recording output callbacks; 2,146,328 successful checks plus event traces.
- `wheel/`: actual native backend against fake DirectInput devices and pinned
  Flycast/SDL translations; 51 successful checks and captured effect payloads.
- `input/`: extracted pinned SDL methods and actual managed vibration/sender
  classes with fake targets; amplitude, lifetime, strength and render-schedule
  results plus source fingerprints.

All probes are output-only. These findings do not change simulated tire grip,
acceleration or car trajectory. `D:\Current` remains unchanged by this audit.
