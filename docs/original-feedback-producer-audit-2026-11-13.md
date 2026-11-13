# How original Arcade Stage 3 generates force feedback and vibration

Audited November 13, 2026. This translates the original game's decision-making,
before Flycast interprets its output. The current remake does not reproduce all
of these decisions: it substitutes heading error and collision pulses for several
original signals and omits the speed-change adjustment to steering resistance.

The readable C++ translations are private audit references, **not installed game
changes**. Reference execution used the original SH-4 instructions in a software CPU
with recording stubs at the three final output wrappers. No game, wheel, or
controller was started. The installed `D:\Current` native DLL remains SHA256
`13677460f1fc6ca5fb2599cdcb0453672c4ad1f26ba4061c5e571ff5b4f7b5cc`.

## What determines the feel

| Output | What the original game uses |
|---|---|
| Directional steering force | Steering input, an angular error signal, left/right road-contact differences, wall contact, a per-car table, two configuration indices, wet/snow/oil flags, and change in speed. |
| Damping | The angular error signal, two configuration tables, snow/oil flags, and an automatic-braking override. |
| Vibration | Wall correction, car-to-car correction, normalized speed, a filtered engine value relative to that car's reference RPM, selected road-surface codes, a special Akina road-entry countdown, and a gear-change signal. |

These are **outputs from the driving state**. The force producer does not change
the car's acceleration, tire grip, or cornering speed. Differences here can
change what the player feels; they are not evidence that the simulation can
physically take a corner faster.

## Readable translation and verification

Original routine: `0C15A5E0 .. 0C15AAD0`, in the user's original main image.
Image SHA256:
`efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`.
Offsets below refer to the original drive object at `0C900F00` unless a full
address is given.

Evidence lives in `Verification/original-feedback-producer-audit-20261113/`:

Readable C++: [steering force and damping](../Verification/original-feedback-producer-audit-20261113/forces/original_force_translation.h)
and [vibration](../Verification/original-feedback-producer-audit-20261113/rumble/original_rumble_translation.h).
These private files are excluded from release packages.

| Reference/probe | Result |
|---|---|
| `forces/original_force_translation.h`, `forces/force_oracle.cpp` | 28,218 cases; 422,217 checks; 10,622,140 original instructions. |
| `rumble/original_rumble_translation.h`, `rumble/rumble_oracle.cpp` | 6,973 controlled inputs, three previous-latch states, two successive calls: 41,838 executions; 191,180 checks; 15,304,901 original instructions. |
| `lifecycle/lifecycle_oracle.cpp` | 33 checks; 11,335 original instructions; reset, stop, disable and five dispatch modes. |
| `fields/field_producer_probe.cpp` | 728 cases; 1,486 exact checks; 13,409 original instructions; configuration, normalized speed, speed change, and angular-error field writers. |

All **614,916 checks passed**. This includes intermediate force limits, output
codes, the vibration sum's exact floating-point bits, integer truncation,
startup ramp, cached outputs, repeated vibration commands, and stopping.
The force/rumble probes together execute the complete original producer 70,056
times. They use the actual original lookup tables and per-car reference RPM.

The matrices cover all 35 cars, configuration indices, condition branches,
contact directions, boundaries and combinations. Some fixtures deliberately
exercise combinations or finite boundary values that normal driving may never
produce. This establishes parity for the tested calculations, not exhaustive
whole-game or physical-device equivalence. Original instruction order and
single-precision rounding matter; simplified algebra is explanatory only.

## Steering force: translated

The centering contribution is formed from steering times 100, doubled, then
multiplied by `1 - drive[27C]`, and truncated toward zero. The latter field is a
normalized angular error signal, not a direct measurement of tire grip.

Its writer includes steering, not merely the car's heading difference:

```text
error = fusedMultiplyAdd(steering1CC, approximately pi/6, unwrappedHeading110)
error = error - unwrappedHeading10C
factor27C = min(abs(error) / (pi/2), 1)
```

The exact original constants and operation order are in the field probe. This
writer does not wrap the difference before taking its magnitude. With equal
input directions and full steering, its force multiplier is 2/3; the current
heading-only proxy gives 1. That is another independent mismatch.

The original adds a signed road contribution. It compares the two front contact
values (`3B4/3B8`) to choose direction, and uses a separate normalized pair
(`3C4/3C8`) for magnitude, capped at 40 command units. The branches matter:
inconsistent signs between the two pairs can suppress the term.

A valid wall contact replaces that road contribution. Its magnitude depends on
`158`, the per-car table, and the saved steering setting. Corners 0/2 give one
sign and 1/3 the other. The wall contribution is capped at 24 units, or 16 on
snow.

The candidate torque code is:

```text
128 + road contribution - truncated centering contribution
```

Code 128 is neutral. The game then limits this code in the following order:

1. Add the per-car limit and the two configuration-table adjustments, clamped
   to 1..127.
2. If wet, divide the limit by 1.5 and truncate. If snow, multiply by 0.5 and
   truncate. If oil, divide by four using two integer arithmetic shifts.
3. Apply the speed-change adjustment below and clamp again to 1..127.
4. Limit torque around 128 by that result, then by the startup counter / 2.

The speed-change signal is:

```text
A = 2 * clamp(10 * (original speed scalar238 - previous scalar23C), -0.5, +0.5)
trim = truncate(truncate(limit * 0.9) * (A + 1) * 0.5)
limit = clamp(limit - trim, 1, 127)
```

This is a per-update change in the original motion scalar, without division by
elapsed time. Do not substitute displayed km/h or a measured acceleration.
**Steady speed gives A = 0.** The current producer's comment calling -1 the
neutral value is incorrect. -1 is the maximum-deceleration end of this signal.

An isolated comparison demonstrates the consequence. Both versions use car 0,
configuration indices 5 and 4, dry conditions, fully settled startup, steering
0.8, no angular attenuation, and no contact. The current owner receives the
corresponding steering input and strength 0.5. These are command magnitudes
relative to neutral, **not percentages of physical wheel force**:

| Speed-change signal | Original force limit/output magnitude | Current owner output magnitude |
|---|---:|---:|
| -1, deceleration end | 90 | 90 |
| 0, constant speed | 50 | 90 |
| +1, acceleration end | 9 | 90 |

The exact original cases were instruction-checked. The current values came
from the actual existing `Owner::update`, not a second handwritten model.
See `forces/current-owner-comparison.csv`.

## Configuration and input provenance

The original saved-card steering index is the unsigned byte at profile offset
1187 (`0x4A3`), copied to `0C9015EC`. The current saved steering source is correct.
The other index, `0C9015D8`, comes from the low nibble of the operator
configuration structure's word at offset 12, copied through `0C4004DC` and
clamped to 0..10. Its original menu caption has **not** been established by this
audit. Calling it difficulty or a generic motor-gain slider is not justified.

The speed term `248` is the original motion scalar divided by the product of
the original `0.278` constant and a coefficient. The speed-change term `24C` is the bounded difference described
above. The wall X/Z corrections are `258/25C`; car-contact X/Z corrections are
`260/264`. The road-response pairs combine geometry and previous response state;
they should not be described as directly measured tire loads.

The engine and transmission terms are internal states, not guaranteed physical
measurements: `EA0` is filtered/coupled engine state and `EA8` is a normalized
transmission value. The per-car RPM table is used as their original reference.
The decoded instructions, source offsets and remaining naming limits are
recorded in the `fields` evidence directory.

## Damping: translated

The game sends command 6 with first parameter **2**. Its second parameter is
the truncated product of the global-index damper table, the angular factor
twice, and the saved-steering damper table. Snow halves this integer; oil
quarters it. The automatic-brake flag overrides the result with 90. Finally
it is clamped to 1..120. Wet alone does not apply the snow/oil damper reduction.

The previous backend audit found a separate downstream mismatch: the current
DirectInput backend treats the first parameter as the damping coefficient and
ignores the second. Flycast uses them as saturation and coefficient,
respectively. Correcting the producer alone will therefore not restore the
right damping. See [the backend audit](flycast-feedback-audit-2026-11-13.md).

## Vibration: translated

The game adds the following terms, preserving the original floating-point order:

| Term | Original calculation |
|---|---|
| Wall and car contact | `80 * (abs(258) + abs(25C)) + abs(260) + abs(264)`; the wall multiplication/addition is one original fused operation. |
| Basic motion/engine term | `normalizedSpeed248 + filteredEngineEA0 / carReferenceRPM`. |
| Surface code 4 | Add `35 * normalizedSpeed248` when either checked contact's low surface nibble is 4; suppressed by the snow flag. |
| Special Akina road entry | On condition 6 or 7, add `80 * normalizedSpeed248` while the countdown `14C` is nonzero. |
| Gear change | When current gear differs from its snapshot and normalized speed is strictly greater than 0.01, add `60 * transmissionNormalizedEA8`. |

The total is truncated toward zero below 47 and capped at 47 otherwise. For
normal nonnegative inputs this produces amplitude 0..47. Small nonzero terms
do not guarantee perceptible vibration: truncation can produce zero, and
Flycast subtracts one from the command amplitude.

Every nonzero amplitude sends command 5 again, even when unchanged, with
frequency byte **63**. This represents a nominal 31.5 Hz command in Flycast's
newer decoder; ordinary gamepads receive motor intensity rather than that exact
waveform. When amplitude becomes zero, the game sends an explicit zero command
if its previous-on latch is 1, then clears that latch. It has no generic
quarter-second impact decay in this routine.

The low-nibble surface code 4's physical material name remains unproven. The
Akina countdown is triggered on entry to a selected surface whose high nibble
is `0x20`: its writer initializes 10 and decrements to 9 in the same update.
These labels should not be replaced with guessed descriptions such as gravel.

Two tempting branches are inactive: the code computes terms involving surface
codes 1 and `0x20` and then multiplies them by zero in executed branch delay slots.
A later RPM calculation also does not control the outgoing amplitude. The
translation preserves this behavior instead of inventing extra effects.

The current native producer replaces this calculation with a held collision
value decaying at 4 per second, quantizes up to amplitude 81, and suppresses
unchanged amplitude commands. The current controller path likewise uses its
own impact envelope. Neither is the original multi-input vibration decision.

The original command stream and its interpretation are separate parity targets:
Flycast v2.5 and v2.7 scale the same amplitude differently. The earlier audit
documents both. Matching command decisions alone cannot guarantee identical
felt vibration across those versions or across controller hardware.

## When decisions run and stop

The original input dispatcher `0C057720` selects:

| Global feedback mode at `0C2F4B88` | Behavior |
|---|---|
| 1 | The separate centering routine `0C15A440`. |
| 2 | The driving producer `0C15A5E0`. |
| Other tested values | Neither routine. |

At `0C062124` the original resets the module and then selects mode 2. The reset
sets the counter to zero and cached torque to 128. Each driving-producer call
increments the startup counter until 256; the torque window grows as counter/2.
At one call per normal 59.7927 Hz game tick, 256 calls would take about 4.28
seconds. This audit verifies the counter and dispatcher, **not an independent
wall-clock measurement of the original input callback's cadence**.

Torque and damper commands are sent when their respective output changes;
positive vibration is refreshed every call. The disable helper `0C15A3C0`
disables the board and clears the ramp. The stop helper `0C15A400` explicitly
sends zero damping and zero rumble and clears the ramp. Those helpers and the
module reset leave the rumble latch itself unchanged; a direct translation
must not assume that reset clears every cached field.

## What a faithful implementation needs

The dynamic values already exist in the active original driving session's
drive and transmission state. The current feedback bridge exports a much
smaller set of steering, speed, heading, lateral-wall and impact values. Also
expose the verified 35-entry reference-engine table at `0C270EB0`; it is not
the currently available transmission `workingBase` and must not be replaced
with that value. The original global configuration index needs an explicit
configuration policy rather than substituting the host device-strength slider.
This work requires carrying state and original constants through the feedback
system, not inventing additional vehicle physics.

Use the original drive and transmission fields at the producer's update point,
rather than estimating them from rendered steering, heading error and a generic
impact. Preserve command order, truncation, lookup tables, conditional updates
and positive-rumble refreshes. Deliver short events without losing them between
rendered frames, and retain the host's foreground ownership and bounded output
lifetimes. Fix the downstream damper mapping and treat the device-strength
slider separately from the original game's configuration indices.

These corrections should be validated first as command streams against the
private translations, then with controlled device tests. The audit does not
claim that a wheel or controller has already been made physically identical
to an arcade cabinet.
