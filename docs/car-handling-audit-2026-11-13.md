# Car handling and acceleration audit — November 13, 2026

The objective is original Arcade Stage 3 behavior, not a subjective handling
rebalance. This audit found a reproducible host-input defect, a narrow solo
oil-rule discrepancy and substantial remaining force-feedback differences.
It did **not** reproduce a general
clean-corner speed advantage or find a universal acceleration multiplier.
Passing the vehicle calculations does not certify the entire game as 1:1.

## Findings that matter to players

### Keyboard steering changed when an unused controller was connected

The installed game returned keyboard steering to centre instantly if a neutral
controller was connected. With the same keys and no controller, it followed
the existing short return ramp. After releasing a held steering key, the next
six original-conditioned steering samples were approximately
`0.8375, 0.6625, 0.5000, 0.3375, 0.1625, 0`; connecting an idle pad made all six
zero. This was reproduced through the installed native game interface, not
just a copy of the input formula.

The correction tracks which device supplied steering. A keyboard release keeps
its ramp; a real stick still takes control immediately and centres immediately.
It does not change the recovered tire, engine or steering calculations. This
can affect repeated keyboard steering taps, but is not established as the
cause of the reported general corner-speed advantage.

### Wheel feedback is still not original-game accurate

The board-command decoder and several original tables are present, but the
owner producing the commands substitutes other quantities for original car
state. It estimates the centring/damping factor from heading error instead of
using the original vehicle field, omits a dynamic torque-limit adjustment,
approximates the road term, and uses an impact/decay proxy for rumble.

In 10,507 captured Myogi observations above 108 km/h, the estimated force factor
differed from the original field by more than 0.1 in 6,846 observations. At one
observation the proxy was zero while the original factor was approximately
0.876. This is a comparison of force-feedback inputs, **not** a measurement of
tire grip or torque on a physical wheel.

A separate test then executed the actual original feedback owner at `0C15A5E0`
using 470 captured car states, with only the final three hardware-output
wrappers replaced by observers. It executed 173,457 original instructions.
Torque magnitude differed in 414/470 samples and the damper parameter differed
in 418/470. For a wall-free sample at approximately 180.47 km/h, the original
requested torque magnitude 45 board units and damper 20; the remake requested
zero torque and damper 1. Both startup ramps were fully open, so startup timing
and torque-direction conventions do not explain that magnitude discrepancy.
Runtime state outside the captured vehicle was explicitly seeded; this proves
the feedback command producers differ under matched seeded conditions, not
physical-wheel equivalence or different vehicle motion.
The 470-state sample includes stopped and low-speed states; it is a different
population from the 10,507 moving observations used for the earlier proxy check.

The startup ramp also counts feedback sends instead of simulation updates.
The managed sender is throttled and its background queue keeps only the latest
request. A software reproduction shows different ramp timing at different
render rates. These results do not represent a physical-wheel measurement;
actual timing also depends on jitter and queued work. An accurate solution
needs the original feedback owner evaluated on each simulation tick, with
device delivery handled separately. No real wheel force was enabled in this
audit.

### Boost materially changes comparisons, as it does in the original

Actual original instructions confirm that trailing boost raises the car/course
speed coefficient and clears persistent accumulated loss. Immediate corner
loss and braking remain. For the tested stock AE86/Akina fixture, trailing
25 metres adds 7.5 to the coefficient; 100 metres adds 30, the cap. That is a
coefficient change, not a promise of 30 km/h or a fixed percentage increase.

In one fixed corner/loss stimulus, the boosted final loss was about 22% smaller
than the unboosted loss. This is verified original behavior and is **not** a
proposed nerf. It has not been shown to explain the user's report. Time Attack,
boost-off battles and boost-on battles must not be mixed in a comparison.

### Optional controls and imported courses are deliberate differences

- Keyboard ramps, optional steering smoothing, configurable controller curves
  and deadzones change the commands reaching the original solver. Default raw
  controller conversion matches the checked Flycast conversion, but physical
  cabinet wheel travel and pedal calibration have not been measured here.
- Optional H-pattern bindings allow an immediate multi-gear jump. The original
  cabinet has sequential shifts. A direct 6→2 jump and four sequential downshifts
  produce measurably different speed histories. Direct bindings are unbound by
  default; this is not evidence of a new default transmission defect.
- Imported courses use donor physics and new geometry. In particular Odawara
  intentionally combines Myogi speed with Tsuchisaka handling, as requested
  earlier. All current imported acceleration multipliers are 1, but donor speed
  and corner-loss tables differ. These courses cannot be certified against a
  corresponding original AS3 layout that does not exist.

## What was tested against original instructions

Reference image SHA256:
`efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`.
The reference executes instructions from the original image independently of
the native solver. Floating-point behavior follows the existing Flycast-based
reference; this is not a new physical SH-4 silicon test.

| Area | New coverage | Result |
|---|---|---|
| Complete player simulation | 35 cars × 18 original course/directions; 630 runs × 900 frames | 567,000 frames; 704,100,337 checked state words matched |
| Acceleration, lift, braking, AT/sequential MT | 47 long synthetic-road scenarios | 273,540 frames; 170,415,420 checked state words matched |
| Boost and accumulated loss | 210 cases, including equal/leading/trailing, braking and prior loss | 25,200 paired stages; 16,203,600 comparisons matched |
| Tuning and profile selection | 20,816 cases, including upgrade 4→5 and package 2→3 | 5,972,897 comparisons matched |
| Steering, angular response, road surface and loss | 22,680 cases, all 18 conditions; explicit wet/oil/shoulder branches | 8,008,560 comparisons matched |
| Wheel feedback command producer | 470 captured states, actual original owner instructions | Torque magnitude differed in 414; damper parameter differed in 418 |

The full-session matrix distributes weather, AT/MT, upgrade 0/4/5/63/75 and
package independently across car/course cases. It is **not** every combination
of all those settings. It includes 23,642 wall-free steering frames above
72 km/h, 54,161 moving braking frames, 5,080 gear changes and 394,896 wall-contact
frames. Much of this scripted run is contact/recovery testing; it is not 630
expert clean laps. It executes 14,752,183,574 original instructions.

The powertrain suite independently initializes original force/transmission
parameters and runs complete original vehicle updates without service hooks.
Its flat-road acceleration figures are component-test results, not real-road
cabinet 0–100 times or measured terminal speeds. Both stock and upgraded curves
matched. The AE86 special engine changes the shape of acceleration rather than
uniformly increasing it at every speed.

The full-session fixture retains explicit engine/diagnostic/reset service
boundaries and an inactive rival-pair boundary. Contact queue/RNG instructions
execute, with the final sound sink and integer quotient services hooked. The
tuning mutation suite observes some child-object and appearance rebuild calls
rather than executing their entire bodies. These limits are recorded in the
private evidence, not counted as proven whole-game behavior.

## Findings requiring care before changing code

The original retains previous opponent information in the tested Time Attack
initialization and condition-selection paths. It can select a rare steering-
memory gate. A long repeated-input test produced later path divergence when
that retained information changed, but the dependency also exists in the
original. Clearing it merely because it looks suspicious would break parity.

There is also a narrow discrepancy in the solo oil gate: the native owner
forced the road's opponent selector to zero, while original setup retains it.
Retained opponent 24/25 suppresses the original dry oil latch; the native
override enabled it. The correction retains the original road selector for
solo play and keeps the existing online/imported adaptation explicit.
The prior discrepancy made affected oil sections **more** slippery, so it does
not explain generally faster corners.

Reachability was checked through original fresh-profile initialization, normal
Legend opponent selection, complete Mode initialization and 155 consecutive
Mode updates selecting Time Attack. These preserve the fields without direct
injection of the opponent ID or oil-exclusion flag. Combined with complete TA
initialization, condition-selection and race-owner checks, this establishes a
normal source path. The final Mode probe executes 221,144 original instructions
and explicitly hooks supporting allocation/TLS/graphics/input/audio services.

Appearance slots for spoilers/rims do not directly select the inspected engine
or force tables. Performance tuning and the special-engine package do. The
audit does not justify a blanket claim that every accessory has no possible
whole-game effect: all appearance rebuild bodies and a controlled original
spoiler/rim A/B driving run have not been verified.

## Previous corrections retained

The earlier audit corrected the nominal driving cadence from 60 to
59.7927185756 updates per second, removing about 0.35% wall-time acceleration.
It also corrected floating-point mode inheritance, eliminating reproduced
long-run state divergence. Those are established corrections, not an
explanation for a large clean-corner advantage.

The previous fresh Flycast run matched 11,113 observed rows across 11,171
simulation frames for measured speed, position, yaw, controls, transmission
and loss/contact fields. A retained boot RNG word differed. Previous geometry
checks found original-course scale unchanged. See
[the November 12 audit](handling-parity-2026-11-12.md) for boundaries and evidence.

## Implemented and installed

The keyboard ownership and original solo oil-rule corrections are installed
in `D:\Current`. Online and imported road rules are preserved, including the
older online startup path. Wheel-feedback approximations remain unchanged.

The combined build passed 13 relevant native/reference suites, including
original player/rival sessions, controls, cadence, floating-point behavior,
rollback and online finish/timeline checks. The oil suite passed 538,269 checks,
including 42,300 original owner instructions. The actual application passed
82 oil checks, including both online paths. Five direct native-game input
scenarios confirmed unchanged keyboard-only motion, corrected idle-pad release,
and preserved stick takeover, centring and blocked-input reset.

Both the private Unity copy and the installed Current copy passed 18,507
integration checks. Each recorded 1,793 driving ticks across 1,799 supplied
60 Hz host intervals, consistent with the original nominal cadence. Tests ran
muted on an unshown desktop and exited afterward. Protected saves/settings,
community records, ROM contents and personal music were unchanged. This was
not a real-network test or FPS benchmark.

Installed native SHA256:
`13677460f1fc6ca5fb2599cdcb0453672c4ad1f26ba4061c5e571ff5b4f7b5cc`.
The previous native library and build metadata are backed up under
`Verification/car-handling-deep-20261113/current-backup/`.
GitHub was not published.

## Remaining verification

The strongest next comparison is an original-course expert corner with the
same car, exact tuning/package, direction, weather, boost mode, calibrated
control sequence and starting state. Record speed/yaw/position and contact
state; camera/FOV impressions alone cannot establish a physics difference.
The ordinary Unity integration and isolated native checks do not replace a
physical cabinet comparison or a two-PC online test. The player's personal
settings are not silently changed to manufacture a match.

Oil visibility is a separate presentation boundary. The original renderer uses
a retained profile flag, while the remake derives visibility from the road's
opponent gate. A retained normal opponent 30 can distinguish these: the original
flag hides the drawn oil while the physical latch remains active. The narrow
physical-gate correction does not rewrite that rendering policy.

Private reproducible sources, logs, metrics and hashes are under
`Verification/car-handling-deep-20261113/`, including the `acceleration`,
`steering` and `input` subdirectories. GitHub publication remains held.
