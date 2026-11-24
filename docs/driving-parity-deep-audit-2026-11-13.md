# Driving parity deep audit — 2026-11-13

## Conclusion and scope

The new source-instruction comparisons found no additional mismatch in the
tested original acceleration, transmission, braking/loss, steering-memory or
road/body-contact calculations. Independent Flycast runs cover 6,171 automatic
and 6,577 manual solver steps, including actual body contact in the automatic
run and repeated shift/countersteer sequences in the manual run. This is
substantial evidence, **not a certificate that every
input device, state, course and gameplay situation is identical**.

The subsequent natural application comparison exposed an integration mismatch
outside those calculations: the real showcase-to-countdown handoff omitted
one initialization update and engaged first gear one solver step early.
Original dispatcher execution confirms that missing update. After correcting
the handoff, shared showcase cadence, actor-light publication and recording
progress ownership, three controlled natural application continuations each
match all 36 named measurements and all 84 published actor words over 3,941
rows. The unresolved route-search word and the declared initial/input
boundaries remain visible. Whole-game parity is not inferred from these runs.

Two online integration issues were corrected: boost was supplied with the
preceding tick's progress, and input batches could contain consecutive
new-press edges that the current normal host adapter cannot produce. These
changes do not establish macro detection and do not explain an offline,
boost-off second-TST complaint.

The latest controlled repeated-shift experiment removes only four accepted
shift edges, retaining every steering and pedal input. Both fresh Flycast
branches and their independent native continuations cover 3,941 measured
frames. Each native continuation matches all 36 named fields and all 84
published actor words; 319/320 driving words agree, with the known `+0x118`
route-search difference retained. The small speed and rotation change from
the additional shifts occurs in both versions. This case supplies no evidence
of a remake-only advantage and is not an expert TST reproduction.

The route-state follow-up is now complete at its declared ownership boundary.
Actual original initialization and cleanup instructions pass 165 checks across
four race histories. The original retains a shared route pointer after its
PATH storage is freed, even on same-course restarts. Later active-AI setup
replaces that pointer and resets the search index. This establishes the
lifetime distinction, not the bytes that a later Time Attack search reads.
No additional human-motion or active-AI speed mismatch was demonstrated.
Preserving a guessed previous route would not reproduce the original, so no
handling change was made from this finding.

The latest combined native candidate, `combined-check8`, passes all 63
regression suites and five broader application suites. Ordinary offline Finish
and cabinet Start are integrated. Forced Quit, countdown View handling and
host input neutralization also pass their native and real Unity input checks.
The full paired local result/rematch runtime route also passed in runtime5.
A source camera
ordering difference is documented, but a camera-only phase change is held:
the original draws camera and attached car together before physics, while
native draws both after physics. Order alone does not prove a visible mismatch.

The confirmed keyboard-response difference is now corrected in source and
passes direct Flycast-code, original ADC and actual application-owner checks.
The higher-level RNG seed/history investigation has identified and corrected
specific omitted or forked consumers. A retained platform-clock owner and a
finish-audio boundary pass source and application checks, with potential
consequences for later shared steering-gate state. Optional H-pattern gears,
custom controller conditioning, FOV, imported-course overrides and online
prediction are also distinct from an unmodified physical cabinet. They must
not be hidden inside a blanket “1:1” claim.

This audit does not install `D:\Current`, publish a release, or modify online
services, leaderboards or personal control settings. Private game and emulator
tests are muted and run on unshown desktops. The user's independently running
Current game can still update its own files; each later validation reports its
observed preservation results separately rather than assuming no other writer.

This report describes the tested candidate and supersedes intermediate
findings in the earlier [handling audit](car-handling-audit-2026-11-13.md).
That earlier document retains its historical build/install record; it should
not be used as a claim about this candidate's remaining keyboard or feedback
command approximations. Physical feedback hardware is still unverified.

## Reference and method

Baseline repository commit: `5a02ea97bcad6c468b7cc77bf404990fadfba220`.
Canonical Export AS3 image SHA-256:
`efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335`.
Live emulator: the local Flycast v2.5 executable, SHA-256
`ce000cd21b404d81aa5885da9baa6685e283bd5a09fc6c56721e09312008dc2d`.

The reference tests execute original SH4 instructions using the bounded
development-only decoder. They do not compare the translated C++ code with
another call to itself. Unsupported instructions/missing memory fail the
tests; each fixture states its substituted boundaries. Independent raw image,
HOSTFS and Flycast sine-table checks guard against comparing two copies of the
same incorrect extracted data. Synthetic fixtures and genuine emulator
captures are reported separately.

Prior clock/float corrections are retained: driving is scheduled at
`27,000,000 / (852 × 530) = 59.7927185756 Hz`; original equation units are not
arbitrarily rescaled. Flycast v2.5's integer scanline scheduling is approximately
59.7937713 Hz. The small scheduling difference is explicitly distinct from
running one driving tick at every 60 Hz host update. See
[prior clock audit](handling-parity-2026-11-12.md).

## New coverage

Counts below describe separate fixtures. They are not a claim that every
possible car/course/tuning/input combination was driven in a live emulator.

| Area | New comparison | Result |
| --- | --- | --- |
| Shifting and RPM boundaries | 111,744 threshold/table scenarios and 86,400 carried transmission frames; all 35 cars plus AE86 engine override | 6,935,040 exact comparisons; no mismatch |
| Acceleration and selected coefficients | All 35 cars × 18 directed conditions × 76 tuning levels: 47,880 selections | Original image and packaged coefficients agree |
| Saved profile, package and engine selection | 106,400 selections across all cars/tuning levels, five package-byte values and wet/snow boundaries | Original getters/initializer and native selections agree |
| Boost, braking and persistent speed loss | 226,800 carried frames; signed gaps, threshold-adjacent float values, braking, contact/lateral loss and reset flags | Combined profile/loss test: 165,223,800 exact comparisons; no mismatch |
| Repeated manual shifting with real road contact | 144 cases: four cars × 18 directions × dry/wet; 230,400 frames | 286,278,186 exact comparisons; no mismatch |
| Long steering history | Three 24,000-tick sessions, retained opponent selectors 0/1/2 | 92,401,429 exact comparisons; actual moving rare-gate activations matched |
| Active rival and body contact | All 35 cars, 105 complete setups, 25,200 frames, 1,469 body-contact frames | 52,013,721 exact comparisons; no mismatch |
| Exported data and geometry | Original HOSTFS/path/collision data and per-car tables | 130,146 comparisons; original-course scale/data unchanged |
| Digital input acquisition | Every previous/current held-byte pair: 65,536 cases | 524,288 checks; original pressed/released/ADC publication agrees |
| Online boost update order | 24 forward/reverse, dry/wet, boost on/off cases; 4,332 changing gaps | Regression fails the old order and passes the corrected order |
| Online input history | Reversed batches, late predecessors, duplicate/retried input, confirmed boundary and ring wrap | 422 assertions passed |
| Network simulation | All 18 directed conditions with 50/100/200/250 ms delay, jitter/loss and outages | 172,800 confirmed peer frames matched; virtual transport only |
| Dedicated surface and landing encounters | 72 fixtures, three cars, dry AT/wet tuned MT, actual oil/Akina roadside/Irohazaka material 28/road triangles and three starting elevations; 64,800 frames | 80,627,024 exact comparisons; no mismatch |
| Recovery boundary | 1,024 actor/publication/recovery and 1,600 body-response cases, including 20 actual restores and 64 backup writes | 1,431,808 exact comparisons; no mismatch |
| Driven oil and jump approaches | Six input-only runs, three stock cars, 5,400 frames; upstream road starts, no subsequent state injection | 6,886,910 exact comparisons; no mismatch |

The expanded original-instruction powertrain tests execute 129,357,281
instructions with zero call hooks. The full road fixture executes the original
vehicle/road/contact/recovery functions, but has explicitly declared engine,
diagnostic and inactive-rival boundaries. Its proof is correspondingly scoped.

The repeated-road fixture includes 21,600 sequence frames, 7,648 wall-free
sequence frames above 72 km/h, 2,592 downshift edges, 288 nonzero history values
at immediate repeat boundaries, 82,762 wall-contact frames and 7,492 moving
brake frames. Only 111 of 144 cases had a clean sequence frame above 72 km/h.
It would be incorrect to call all cases expert high-speed TST executions.

The dedicated surface suite counts **951 oil-effect frames, 162 Akina roadside
branch frames, 1,459 frames with all four wheel-above-surface flags and valid
road queries, and 66 subsequent landing transitions**. Dry oil activates and
wet oil is suppressed in every corresponding fixture. It executes
1,711,477,105 original instructions with the same explicitly declared platform
boundaries as the full session test. Elevated starting poses are constructed
initial conditions on real geometry, not recordings of expert Irohazaka jumps.
These trajectories had zero recovery restores; the separately counted
original-instruction boundary fixture supplies recovery coverage instead.

The additional upstream approaches enter Tsuchisaka oil at **114.70–115.90
km/h**, cover 87 active oil frames above 72 km/h, and complete **five driven
takeoff/fully-grounded landing cycles** on Irohazaka. Each starts on an authored
road point at normal height, settles, and proceeds only through accelerator
and steering inputs. Three stock cars are covered; two become stuck farther
along the hairpin, so these are useful bounded jump fixtures, not successful
expert laps. The full original source execution matched 146,741,521
instructions' observed results, with the same declared platform boundaries.

## Fresh live Flycast comparison

The private emulator runs stock AE86, Myogi condition 0, dry, automatic,
Legend battle against Iggy. Lua reads telemetry and supplies ordinary input;
it never writes guest memory. Native initialization receives the selected
car/course/tuning/weather and six starting pose values. It does not receive
the captured player's speed, steering history, grip, contact impulse or whole
physics state.

The first comparison omitted the AI car. It matched until game frame 2051,
where the original body-contact latch and response became active. The later
speed/road divergence followed that contact. The failed comparison is retained;
no grip adjustment was made to conceal it.

A second fresh capture supplies **only the previous opponent publication** as
an external boundary. The native player's own pose remains independently
simulated, and the original translated body-contact producer calculates the
response. Across 6,171 solver frames and 6,113 independently observed rows:

- All 32 named measurements match bit-for-bit, including XYZ, speed, RPM,
  gears, steering, pending/active steering memory, opposed steering, loss and
  wall-contact response.
- Seven moving body-contact steps match, as do 1,209 wall-contact rows and
  60 brake rows. Peak recorded speed is 177.021 km/h.
- **319 of 320 drive words match.** The remaining `+21C` field is initialized
  from different pre-race RNG histories: original `0x4281D78D`, native
  `0x43286189`. This audit has not established every consumer of that field.
  It is not silently excluded or claimed to be harmless in all situations.
- Original gear-enable/automatic-brake state and the opponent trajectory are
  explicit external inputs; their independent owners are not certified by
  this capture. The source-instruction owner tests cover separate boundaries.

The capture is AT, **not an expert manual double-TST recording**. All 7,000
telemetry rows flushed before the private emulator exited with `0x80000003`;
that shutdown code is retained rather than called a clean emulator exit.
The independent numerical comparator completed successfully.

### Additional manual run

An uninterrupted cold boot and ordinary menu inputs selected stock AE86,
manual transmission, Myogi dry Legend against Iggy. Card-handshake reload
attempts that stalled were retained, not described as successful races. The
successful capture verifies the original profile's manual byte and the
original Legend session value. It is not a Time Attack capture.

Across **6,577 solver frames and 6,519 observed rows, all 320 player drive
words and all 32 named driving measurements match bit-for-bit**. There are
62 real original up/down pressed edges, gears 0–4, repeated paired shifts and
countersteering at five scheduled intervals, 435 wall-contact rows and 60
brake rows. Peak speed is 126.963 km/h. Pending, active and opposed steering
memory are nonzero for 601, 5,723 and 1,305 rows respectively and match exactly.
This run adds no new moving body-contact case; the AT capture above supplies
that separate coverage.

The pre-player initializer seed is directly observed before any player pose
exists: 223169661. The native original initializer independently consumes one
draw and produces source seed 484824946 and player `+21C=0x4293F127`. The
capture requires one further observed draw before warmup. A follow-up review
found that its attribution to an opponent initializer was not established by
the cited source test: the private comparator explicitly advances the LCG
once, but that consumer's real caller is still under investigation. This is
an observed initialization boundary, not an independently verified owner or
a per-frame correction. No speculative extra draw was added to production.
The warmup join matches all 320 drive and 84 publication words. As in the AT
test, the opponent's previous publication and original gear/automatic-brake
boundary are supplied externally; the native player body producer and
driving calculations run independently.

The initial stripped comparator omitted outer engine/tire-audio owners and
therefore disagrees with global RNG from frame 61, despite matching the car
state. Its first divergence is exactly one additional original draw. A
mechanical source trace locates that draw in the original low-idle engine
callback. Adding the already-existing native engine/tire owners extends the
shared-RNG match through frame 5,980. A subsequent read-only Flycast capture
locates the actual engine-disable byte at frame 5,969, after the prior
automatic-brake request at 5,882. The player stopped flag appears at 5,967;
rules consume it and select the finish child at 5,968; the child's initialization
disables the engine at 5,969. The difference becomes observable in RNG at
5,981, when RPM crosses the idle-randomness threshold.

Using that explicitly supplied source phase boundary, the extended comparator
matches **6,042 recorded rows across 6,100 solver frames**: all 320 drive words,
all 32 metrics, shared RNG and the steering counter are exact. That is
1,933,440 drive-word and 193,344 metric comparisons. It supplies the captured
phase and opponent boundaries; it never overwrites per-frame RNG or player
driving state. It is not an independently replayed complete frontend/race
state machine. The original stripped comparator's mismatches remain preserved.

The captured eligibility mask is 65535. Neither side takes the rare random-
mask steering branch while moving in this live trace. The long-history
original-instruction tests cover actual moving rare gates separately; that
coverage must not be relabeled as live Flycast coverage. The expert second-
TST complaint remains unreproduced.

## Concrete corrections

### Current-tick online boost

Previously `OnlineRaceSimulation::step` calculated advantage before updating
both cars' path/race progress. Each solver therefore received stale progress.
It now projects both current poses and runs their rule updates, calculates the
shared signed gap, and then advances both solvers. Original outer-owner
instruction execution verifies this ordering; an old-source regression fails
at the first changing boost gap. Boost-off still supplies zero.

This changes deterministic boost-on simulation. Existing matchmaking and the
connection handshake include the complete native DLL and managed assembly
identity. The actual old/new binaries produce different compatibility keys;
mixed membership is rejected. This is compatibility checking, not cryptographic
attestation against a dishonest modified client.

### Validate received shift edges

The wire input carries newly pressed bits, not held buttons. In the current
producer, a particular shift bit cannot be newly pressed on two consecutive
simulation samples without an intervening release sample. Both local and
received histories now enforce that contract.

Validation considers known predecessors/successors and the entire proposed
batch before committing any record. It preserves out-of-order delivery,
retries, alternating up/down, simultaneous `0x30` edges and optional direct
gears. It also checks the retained confirmed sample after the ring wraps.

Original digital acquisition permits simultaneous up/down bits. Their
mechanical reachability has not been proved here, so they were not removed.
Plausibly timed macros still produce valid input; this validation is not a
macro detector or grounds for an automatic player ban.

### Finish engine owner

An actual application probe confirmed that the previous local finish path
continued ticking engine control during the finish announcement. The original
finish child `05C280` calls `061A80/1427E0/0C4200`: it latches the engine-disable
flag and issues four continuous-voice `A6=0` commands. `05C040` first marks the
racing child complete; its final solver still has engine control enabled.
The next child initializes before the following solver. This is a source
state transition, not a guessed banner delay. The special challenger-interruption
branch is separate: its finish initialization skips the stop, and its Main
waits for actor bit 14. Earlier artifacts incorrectly called this retirement;
the source producer and its debug string now establish that `outer+0x64C` is
`challengerflag`. The host's retirement action is a separate extension, not
proved equivalent by injecting that source flag.

The initial source regression executes **1,366 original instructions** across those
handoffs, including expiry-before-stop, stopped timeout, child selection,
next-update initialization and the injected special interruption flag. It declares rendering, allocation,
stream output, unrelated numerical leaves and normal finish Main as boundaries.
The initial local App correction passes 23 focused checks and leaves online
authority untouched. Independent review then found two presentation regressions
in that correction: the backfire counter could remain active after the gate,
and queued engine removal prevented the authored key-off release from rendering.
Both fail against candidate B and are corrected in candidate C. The flash now
ages on accepted steps even when engine control is disabled. The audio cut
captures the actually audible voice cursor, applies key-off to those voices,
and renders the finite release on the output clock, retaining existing DSP
history. It neither advances gameplay RNG nor inserts another priming delay.
The private regression passes **13,866 assertions**, including exact dry PCM
at three audible cursors, release completion, pause, repeated stop and restart.
The actual App test now passes 28 checks, including a final racing cue and its
expiry.

The online simulation has its own checkpointed engine/tire owners. The same
source finish stop is mode-independent, but linked timeout waits for both
actors and has additional message handling. The authority counterpart must
use its agreed prior terminal state, retain the stop latch in rollback state,
and regenerate commands for confirmed playback. Candidate C implements that
boundary before the next solver, issues four stop commands once per player,
skips disabled engine control and passes the same gate to tire selection.
The latch is included in checkpoints and the state digest. A private test
passes **45,666 checks**, including both winning slots, before/after-stop
restoration, fresh reset, 5,611 naturally timed-out confirmed frames and 701
rollbacks with delayed/reordered/duplicate input. The mixer receives one
confirmed stop batch and no speculative audio. The extended source test
executes **1,732 instructions**, including the original linked both-stopped
predicate and common finish stop. No live linked-cabinet trace is claimed.

## Shared RNG initialization and lifecycle

The unmatched randomized `drive+21C` field is **not** the steering mask. A
dynamic watch over 2,400 complete original frames (63.4 million instructions)
observed only bulk recovery-copy reads of this field, with no arithmetic
consumer on those paths. That does not establish absence of all consumers.

Separately, actual original completion code `157E4E–157E5C` calls shared
`rand` and adds `rand & 3` to `0C900EBC`; original steering code
`15DA20–15DA2A` gates a steering-memory contribution with that counter and
`0CAA9CF0`. The original scene owner `056A20` seeds the shared generator from
a live indirect platform word returned by `2021E0`. Before candidate B, a
fresh native application started at seed 1; a reused native session retained
the previous seed. That omitted the source application's seed boundary.

The bounded platform trace identifies the seed word as
`uint32(AICA RTC seconds − BIOS reference + preserved backup-SRAM baseline)`.
The getter reads SRAM through `0CB11AF4`; the updater uses the AICA RTC and
BIOS-derived reference `0CB120AC`. RTTI and the normal application constructor
identify the seed owner as **AMiddleSeq**. The complete scene dispatcher
initializes it when its activation flag is clear; later updates do not reseed
it. An isolated dispatcher test covers two activations and 242 updates, not
every frontend transition. It must not be described as a per-race reset.
The RTC/SRAM calculation matches 1,728 source cases plus 24 developer-scene
seed cases: 8,664 comparisons and 100,488 original instructions, no hooks.
Cold-boot read-only Flycast telemetry independently observed a seed of 24,164
matching that instance's preserved clock baseline. That is one platform
history, not a universal correct seed.

Candidate B adds a separate source-derived platform counter and host storage
adapter. A fresh application seeds the shared stream **once, before menu
consumers**, from its committed powered-on counter. Subsequent menus, races,
pauses and periodic clock publications do not reseed it. Standalone numerical
sessions still default to 1 unless explicitly injected; agreed online driving
seeds are unaffected. Missing or invalid clock storage represents cleared
SRAM and starts at zero. The stored word is the platform counter, not the LCG
state. No Flycast user's SRAM or timestamp is imported.

The maintained source test executes 48 scheduler/gate cases, 36 batched-update
cases, 729 RTC histories, 17 real outer-dispatch frames and all four original
SRAM CRC/repair outcomes: **1,135 comparisons, 409,842 original instructions
and 87 declared service/scene hooks**. Original settings repair preserves the
clock words; only both invalid settings copies trigger the full SRAM clear.
The pending-settings-write branch `9900BC=1` is explicitly outside this test.
The host adapter uses the source ready/no-DMA branch at the original rational
rate; it does not emulate the machine's entire hardware scheduling.

Host persistence uses a single coalescing worker and atomic replacement.
Gameplay only queues changed snapshots and never waits for filesystem work.
Shutdown drains the latest snapshot. A second instance or unavailable folder
runs from committed/cleared history in memory without overwriting another
owner. Host tests passed **121,423 checks**, including 118,000 clock advances
while a fake writer is deliberately blocked, latest-value coalescing, bounded
failure retries, reopen, shutdown and 30/60/144/240 Hz schedules. Actual App
activation/reopen and online-shutdown integration passed **39 checks** in
candidate B. This host storage policy is not a claim to emulate physical
battery-backed SRAM timing or arbitrary operating-system clock changes.

Several **proven shared-state defects** around the numerical solver are now
corrected:

- The shared generator can be used before the first vehicle is created, so
  first-race dialogue no longer silently skips its draw. Reset carries those
  prior draws into the source player initializer.
- Spectator selection now uses original `rand() % 7`. The previous extra
  `% 1000` selected a different variant for 27,768 of 32,768 possible outputs.
- Solo Myogi dry/day course construction consumes its 19 crow draws before
  player initialization, matching the traced normal ARace call order. The
  previous presentation code reconstructed values without consuming the
  shared stream. Online decorative construction retains its existing separate
  policy; local menu history is not inserted into agreed peer simulation.
- Accepted ending ticks retain their camera/smoke/weather/effect draws in the
  same shared state used by the next race. The previous ending copy was
  discarded. Repainting, pausing and restarting the ending do not rewind it.
- A callback that throws after consuming a draw now synchronizes the reset
  mirror before rethrowing, preventing accidental rewind on the next reset.

The independent regression exhausts the original spectator selector and
composes four source menu/course/player/ending/reset histories. It explicitly
tests checkpoint epochs, moves, failed resets and callback exceptions.
Completed-car diagnostics compare all numerical state while normalizing only
the two shared-RNG mirror words; the expected shared sequence is checked
separately. Ordinary rollback digests and checkpoint contents still include
those words. This resolves specific callers, not every random consumer in
the full game. The native FullTune extension and isolated replay tools retain
their distinct policies.

Actual completion-step sensitivity tests with mask `0x1FFF` demonstrate that
different seeds change rare-gate timing. Example gate frames are
`0, 5428, 21798, 21799` for seed 1 versus `0, 1, 2, 5461, 21730` for seed 12345.
These isolated sequences omit other whole-game RNG consumers; they are not
predictions of a player's lap. This establishes a possible handling-state
difference, not systematic extra grip or a reproduced double-TST cause.
No arbitrary wall-time seed or compensating steering penalty was substituted.

## Input, presentation and intentional differences

With a modeled 59.7927 Hz sample schedule and the actual native calibration:

| Keyboard action | Flycast v2.5 | Previous host |
| --- | --- | --- |
| Center to calibrated full lock | 4 samples, about 66.9 ms | 10 samples, about 167.2 ms |
| Full right to opposite full lock | 10 samples, about 167.2 ms | 15 samples, about 250.9 ms |
| Release shape | Holds calibrated full lock initially because raw axis extends beyond it | Starts returning immediately |

Flycast keeps the full signed raw-axis range and a 100 ms raw ramp; the previous
host used a normalized 6/10/12-per-second response. The corrected original-
handling path now retains Flycast's signed axis and integer-millisecond clock,
advances the preceding held-key interval before accepting current key states,
and preserves the complete JVS steering word through the calibrated input
adapter. It samples in the host commands owner even on zero-simulation-tick
frames. Catch-up ticks and stalled attempts read that sample without advancing
the keyboard clock again. Menu, pause, focus, loading and blocked-input
boundaries clear the retained state. Custom smoothing/controller conditioning
and the intentional experimental handling path retain their existing policy;
no preference file was rewritten.

Direct comparison with mechanically extracted, unmodified Flycast v2.5
`buttonToAnalogInput/getTargetArray/rampAnalog` bodies passed **98,304 host
samples, 196,608 comparisons and 312 resets** across eight rates and irregular
0–250 ms schedules. Exhaustive raw JVS conversion and actual original steering
conditioning passed **133,344 checks and 67,134 original instructions**. The
actual application commands/driver/lifecycle methods passed **49 checks**,
including no-tick frames, catch-up, raw ADC retention, controller arbitration
and suspension. These are controlled input-owner tests, not measurement of
physical USB latency or the emulator's live thread scheduling.

Controllers/wheels with default conditioning match the tested calibrated ADC
mapping. Custom deadzones, curves and smoothing intentionally change input.
Unity samples device state per rendered Update and holds it through catch-up
ticks. The adapter emits a held shift's edge only once; stalled online steps
commit input history only when advancement succeeds. Varied render-cadence
tests cover 20/30/60/144/240 Hz, including 597 multi-tick updates and 280 failed
advance attempts, without duplicating or consuming a shift edge incorrectly.
Physical device latency and the cabinet's complete polling schedule remain
unverified.

Optional H-pattern direct gears are not the cabinet's sequential lever: a
multi-gear jump currently creates one downshift-memory event. Default direct
gear bindings are unbound. A competitive input-policy decision should be
separate from rewriting the original car equations.

Original bumper/chase camera transforms and FOV have a separate original-code
regression. At 100% FOV the source lens is retained; widescreen exposes more
horizontal road, and optional FOV/Natural view/render interpolation change
presentation. This can affect perceived speed or risk; it is not proof that a
car can physically negotiate a corner faster.

Imported courses have intentional geometry and handling choices, including
the requested Odawara speed/handling combination. They have no matching
original AS3 course on which universal 1:1 course behavior can be certified.

### Spoilers, rims and collision geometry

The complete original appearance rebuild callback was executed for all 35
cars and 1,470 spoiler/rim combinations, with varied body-fit selectors. The
strict run clears the reference hook map before each complete callback:
1,974,315,117 callback instructions, 12,292,712 comparisons across constructor
and rebuild states, and 1,056,853 preserved authored vertices passed.
Observed callback writes stayed within the car presentation object, material
bank and stack; none changed driving globals or collision-geometry tables.
Constructor allocation/upload/classification boundaries remain explicit hooks
outside the fully executed callback.

The road/body probes use fixed per-car dimensions. All 385 words in the source
dimension record match the two copied tables used by the relevant native
paths. Visual wheel-fit offsets have separate presentation consumers. This
supports cosmetic spoilers/rims for the inspected code and states; it does
not claim a full original-game driving A/B experiment for every accessory.
Performance tuning and the special AE86 engine remain separate, noncosmetic
selectors and are included in the profile/powertrain tests above.

## TST finding and remaining acceptance gaps

Pending steering correction `+288`, active correction `+284`, opposing
steering `+28C`, contact contributions and their original bounds/drain persist
through repeated manoeuvres. No “one TST only” rule or arbitrary cooldown was
found or added. The same input can produce different outcomes depending on
entry speed, gear, steering direction, throttle/brake, contact and retained
state. The player's specific second-TST complaint remains unreproduced.

Before calling the complete experience 1:1, remaining work includes:

1. Reproduce the reported manual sequence with a specified car, corner, entry
   gear/speed and input trace, then compare matched original/native state.
   The controlled MT Time Attack A/B/C experiment below now has a common
   initialization/input prefix and defined no-sequence, single-sequence and
   immediate-repeat branches. It has not reproduced the
   reported unwanted understeer/oversteer in Flycast. Compare raw accepted
   controls, entry line, retained steering state, yaw/drift, contact and exit
   speed without forcing RNG/mask gates or injecting expected vehicle state.
   An exploratory timing sweep is not a substitute for an expert reproduction.
2. Capture actual keyboard/controller/wheel polling and calibrations. The
   source-derived keyboard owner is corrected and separately verified;
   physical sampling and display latency are not established by those tests.
3. Investigate untested scene transitions and RNG history before the declared
   pre-loading seed. The corrected natural runs establish the tested startup
   and driving owners from that boundary, not every boot/menu path. Same-input
   equality requires comparable starting state. The remaining route-search
   word and original recorder cold initialization also need explicit limits.
4. Add dedicated live airborne/landing, gutter and oil-patch encounters.
   Constructed real-geometry encounters now count and match those numerical
   paths; recovery restores are separately covered at the original boundary.
   They are not a substitute for input-only live expert-jump captures.
5. Test real two-PC EOS matches across networks and physical force-feedback
   hardware. Local transport, source feedback-command and muted application
   tests do not establish those properties.
6. Combined application and private Unity player validation is complete. The
   earlier blocked online controls and 300-tick/manual-skip sequence have now
   been corrected from original counter/camera/fade ownership, including all
   256 accepted input byte values. Source, managed and isolated App checks
   pass, and runtime-check5 completes natural results, Continue, the connected
   lobby and a second race using check8/build7. This closes the local player
   validation item, not the external-network test in item 5. A measured
   2.35-second screen-transition pause and an earlier unproved stall cause
   remain performance/reliability limitations. The EOS handoff after the
   source sequence remains a declared multiplayer adaptation.

## Evidence and reproduction

Private evidence root: `Verification/driving-parity-deep-20261113/`.
The original image, emulator states, captured player data and private binaries
remain outside release assets.

- `powertrain/FINDINGS.md`: source boundaries, table/profile/boost tests and
  accessory callback investigation.
- `steering/FINDINGS.md`: repeated shifts, retained history, body/road contact,
  geometry identity and precisely bounded surface/recovery coverage.
- `flycast-contact-live/FINDINGS.md`: live method, raw-state differences,
  contact boundary and emulator exit limitation.
- `flycast-live/compare-at/`: retained initial omitted-opponent mismatch.
- `input-online/`: input timing, packet-validation, boost-order and
  compatibility evidence.
- `ctest.log`, `results.xml`, `camera-test.log`: combined candidate regressions.
- `runtime/summary.json`: private Unity integration and preservation checks.

New maintained tests are registered in `Native/CMakeLists.txt`, including
`original_transmission_boundary_reference`,
`original_speed_loss_boundary_reference`, `original_shift_input_reference`,
`original_repeated_shift_reference`, `online_input_edges`, `online_boost_owner`,
and the extended `original_driving_repeated_shifts` and
`original_driving_steering_history` modes. Canonical-image and independent
Flycast sine-table paths are supplied explicitly at configure time.

## Previous integrated candidate verification

The fresh Windows Release candidate built successfully and passed all **28**
selected CTest cases, including both new extended real-road modes. The
separate original bumper/chase-camera CTest also passed. Actual application
force-feedback integration passed **13,265** checks over 900 solo and 660
online-authority steps; it compares source commands and observer invariance,
without opening audio or driving physical feedback hardware.

The candidate native DLL was then temporarily installed into the existing
private `.56` test player, never `D:\Current`:

- Solo Unity scene/clock: **18,507 checks**. 1,799 submitted 60 Hz host intervals
  produced 1,793 original driving ticks; expected 1,792.785 within the
  accumulator's one-tick bound. This is not an FPS benchmark.
- Two actual Unity LAN clients: **21,539 host and 21,591 guest checks**.
  Boost-on synchronized driving, manual keyboard/remapped controller shifting,
  held-button nonrepeat, prior-menu input release, original countdown, natural
  source timeout/results, connected lobby return, fresh race identity, second
  race, rematch input release and leaving from the second result passed.
  The fixture uses synthetic device inputs through production bindings,
  isolated profiles and local TCP; it is not a physical controller or EOS test.
- Both players shut down cleanly. Test launches stayed muted on unshown
  desktops. The private binary was restored; the installed Current binary and
  ordinary user-data fingerprints were unchanged.

Candidate native SHA-256:
`95b983f6b15ee4a719fdaccaabe7b82e50afe9d6a8fc0f9c5de70c9ce07b49a8`.
`summary.json`, `candidate-source-manifest.json`, `native-test-results.json`
and `runtime/summary.json` bind these results to the tested files. The summary
explicitly records `completeOneToOneParityCertified: false` and
`installedCurrent: false`, `published: false`.

Two initial build invocations named nonexistent test targets and stopped at
target lookup; their logs are retained. The corrected target list was checked
against generated projects before the successful full build. This was a build
invocation correction, not a waived physics or runtime failure.

## Continuation verification

Evidence for the keyboard correction, source RNG investigation and dedicated
surface coverage is under `Verification/driving-parity-continue-20261113/`.
`surface/full-test.log` retains all 72 case counters; `surface/summary.json`
states the constructed initial conditions and separate recovery coverage.
`input/primary-ramp.log`, `input/keyboard-unit.log` and `input/app.log` retain
the independent source and application-input checks. Seven freshly compiled
input, shift-history, boost and timeline CTests also passed in
`input-root-results.xml`. The previous candidate DLL/hash and runtime results
above are historical; they do not certify subsequent source changes.
`surface-approach/root-reference.log` preserves the six driven approaches;
`rng/FINDINGS.txt` records the source ownership chain and remaining clock
boundary. The first RNG fixture failure was an uninitialized authored crow-
group pointer; its failed log is retained and the corrected fixture binds the
original group/count rather than bypassing its code.

Continuation candidate A built and passed **36/36** selected CTests plus
**13,265** application feedback checks. Its ending application check failed:
`Backfire comparison did not change submission`. The diagnostic had searched
forward for a backfire frame and then restarted playback assuming production
would rewind shared RNG. That assumption became invalid when ending RNG was
correctly retained. The fixture now explicitly restores its captured
diagnostic input seed before reconstruction and asserts the selected frame's
seed and every backfire field agree. Production playback is not made to
rewind. The corrected fixture passes the candidate B and C application runs;
candidate A is not reported as an overall integration pass.

Candidate A DLL SHA-256:
`f2c2f9f2281cc8abfa85bf257f1641e41bff75afe73c402afc416918c8057b70`.
Its exact source, patch, manifest and DLL were archived under `candidate-a/`
before further changes. `candidate-results.xml`, `candidate-full-test.log`,
`candidate-feedback.log` and the failed `candidate-ending.log` remain intact.
Candidate B adds the separately proven platform-clock boundary and passed
**41/41 CTests in 357 seconds**, the 13,265 feedback checks and the complete
ending application check, with source unchanged during validation. Its DLL
SHA-256 is `7a83ec48f275abefa66f850c1f30b536aa7a302ea255b86a627662fc0952bb56`;
exact source and binary are archived under `candidate-b/`. Passing these tests
did not hide the two independently found finish-presentation regressions
described above. Their separate failing reproductions and hashes remain under
`rng/finish-audio-review/`.

Candidate C includes those reviewed corrections and the authority finish gate.
Only eight source/build/test paths differ from B, recorded by the two source
manifests; the underlying acceleration/steering/contact code is unchanged.
It passed **24/24 affected audio, online, driving and source-owner CTests in
49 seconds**, plus 13,265 feedback checks and the complete ending application
check. Source hashes remained unchanged throughout validation. Together B
and C cover 52 unique registered suites; overlapping reruns are not counted
as independent coverage. The earlier broad matrix remains evidence for the
unchanged implementations, not a claim that its binary hash equals C.

Candidate C DLL SHA-256:
`ba231790980205c9d783ef750697946a59126c5a067c2da68c6b7cb681a8cffa`.
Its exact source and binary are archived under `candidate-c/`.

The first C Unity run passed 18,507 solo checks and both local clients reached
verified driving, then the diagnostic save fingerprint failed while attempting
to read the new, deliberately exclusive zero-byte platform-clock lock. Those
clients did **not** complete their finish/rematch checks; the failure remains
under `runtime/`. The native binary was restored and installed Current and
ordinary player data were unchanged.

A diagnostic-only managed correction classifies exactly three root clock
metadata files without opening their streams. It retains size checks and
strict content hashing for profiles, stats, leaderboards, nested files and
lookalike names. **574 focused filesystem checks** cover the exclusive lock,
concurrent atomic clock publication and detection of real save changes. Native
clock integrity and persistence remain covered by the separate owner tests.
An initial stream-based diagnostic attempt that interfered with atomic writes
is retained alongside the corrected metadata-only implementation.

The refreshed player assembly was built using Unity **6000.6.0f1**, scripts
only, in an ordinary copied private player. Its SHA-256 is
`27b58cd02786729752bb770399066287bf5b8fcd1a4d30a4efbd83c2cb2c85c5`.
Build inventories verify that source/configuration/plugins and the original
private player were unchanged. Only this validated assembly and the same
native C binary are used for the integration rerun under `runtime2/`.
No candidate in this continuation has been installed to Current or published.

Attribution correction from the subsequent review: the manual comparator's
additional pre-warmup draw was previously called a source-proven opponent
initializer. The tests prove one player initializer and the preceding course
draws; they do not prove that additional caller. The observed numerical match
is unchanged, but it is conditional on that explicitly supplied extra draw.
The earlier report and summary are preserved under
`Verification/driving-parity-next-20261113/previous-report/`; follow-up source
tracing is under that next evidence root's `owners/` directory.

### Final private Unity integration

The rerun completed successfully with the native and managed hashes above:

- **Solo: 18,507 checks.** Across 1,799 supplied 60 Hz host intervals, the
  actual Unity/native bridge produced 1,793 driving ticks (expected 1,792.785,
  within the accumulator's one-tick bound). This is a scheduling check, not an
  FPS benchmark or measured USB latency.
- **Two actual Unity clients: 20,802 host and 21,123 guest checks.** The run
  covered boost-on synchronized driving, manual/remapped controls, held-shift
  nonrepeat and prior-menu release, original countdown, natural timeout and
  results, connected lobby return, a fresh race identity, the second countdown
  and race, and exit from the second result. It uses synthetic device input
  through production bindings, private profiles and local TCP, not live EOS.
- Both clients shut down cleanly. Startup logs confirm muting before scene
  load; all test windows remained on unshown desktops. The private native DLL
  and managed assembly were restored. Installed Current and ordinary player
  data fingerprints remained unchanged.

`runtime2/summary.json` binds those checks and preservation results to the
tested binary hashes. `summary.json`, `final-source-manifest.json` and
`native-test-results.json` bind the complete continuation evidence to the
final source. Native C differs from its archived manifest only through the
separately recorded diagnostic managed refresh; the native source is unchanged.
The final summary explicitly records `completeOneToOneParityCertified: false`,
`installedCurrent: false` and `published: false`. Passing this integration does
not close the specific expert TST, whole-session history, physical hardware
or real-network acceptance gaps listed above.

## Subsequent integration audit

The following sections retain the sequence of intermediate findings and
failed attempts. References to a test being pending describe that stage of
the investigation; the conclusion above and final check8/runtime5 closeout
give the current status. Later passing results do not erase earlier failures
or establish their causes unless explicitly demonstrated.

Evidence below is under `Verification/driving-parity-next-20261113/`. These
source changes postdate candidate C. Its archived binary and runtime pass do
not certify them; the combined rebuild and integration are recorded separately.

### Precise pedal input

The previous managed path rounded higher-resolution HID pedals to a byte
before the native Flycast/JVS conversion. Across all 32,768 positive source
positions, that changes the final original pedal value at 1,454 positions,
by at most one original conditioner step (1/107). It does not create additional
resolution in physical XInput triggers, which already supply bytes.

An additive 16-byte, versioned sidecar now retains precise pedals through
bindings, calibration, accessory rigs, explicit stick-direction pedal mappings,
native submission and the original conditioner. The existing 88-byte input
layout and online wire format remain unchanged. Keyboard precedence is
independent for each pedal; Previous response and nonoriginal handling retain
their former mapping. Focus loss, disconnect, blocked controls and diagnostic
replacement clear optional values. Malformed sidecars fail before changing or
destroying the existing native scene. An older native DLL uses the legacy entry
point; online build identity still prevents mismatched builds joining.

- `input/precision/bridge4.log`: **164,349 checks**, including exported API,
  actual App input, ordinary startup and **34 running solo solver frames**.
  Single and three-tick submissions, simultaneous pedals, keyboard precedence
  and the following legacy frame are checked at the real solver conditioner.
- `input/precision/production-wire.log`: **1,204,109 checks** through production
  packet/authority/rollback/replay code, including 3,600 confirmed peer frames.
  This is virtual transport, not EOS or external-network validation.
- `managed-check3/`: **294,934 actual managed checks** with synthetic HID/rig
  devices. Unity compilation and functional checks passed; no physical device
  input was polled by the fixture. Editor initialization enumerates devices.

The first managed check passed functionally but did not retain enough file
detail to attribute its preservation failure. The second failed compilation
because a newly added test local shadowed another variable; that fixture name
was corrected. The third passed functionally, with source/configuration and
Current DLL unchanged, but detected a changed
`community-times/snapshot.json` while the user's Current process was running.
Before/after hashes are retained. It is **not** reported as complete user-data
preservation, and no user file was restored to hide the difference.

### Finish input ownership

The real App previously forced neutral, released throttle, full physical
brake and a throttle-suppression word after finishing. The source retains GO
gear engagement and accepted physical pedals; its finish owner requests
automatic braking, and the solver cuts propulsion itself. The crossing's final
racing step precedes the normal finish owner's brake request. Timeout may
already have requested braking before that point.

App and online finish handling now preserve accepted inputs and distinguish
automatic braking from a synthetic pressed brake pedal. Evidence includes the
retained failing App reproduction, **3,635 corrected actual-App checks**, a
**2,526-instruction original parent/finish trace**, and four input-only complete
Myogi finishes. Those cover both a crossing with rule-owned timeout brake and
a positive-time crossing without it. Full details and declared fixture
boundaries are in `finish-controls/FINDINGS.md`.

### Pre-race dialogue history and ownership

Original normal and Bunta dialogue initialization both construct ten spectator
records and 200 snow records, consuming **430 random draws even on nonsnow
courses**. Normal initialization selects cheer before those records; Bunta
does not. Eligible nonsnow Draw updates the ten spectator records; snow Draw
advances retained particles and consumes a draw only for an actual respawn.
Those explicit state records and accepted-tick updates are now translated and
connected to the App's shared random stream. Host repaint is read-only.

The normal pre-race owner `051EE0` also establishes three corrections: held
Start is applied before child Main; Ready closes the configured intro rather
than advancing to another post-race dialogue kind; and a closed child must not
continue consuming random state in the rest of a host catch-up batch. The last
legitimate closing Draw is retained. The actual App update is factored so the
production owner can be tested without starting a renderer or another race.

The earlier private record oracle passed **1,535,490 comparisons over
42,827,158 original instructions**; private scene/visit integration passed
**292,971 checks**. Maintained source-dispatch and actual-App regressions were
extended for the normal pre-race parent before the combined build. Rendering
of these spectator/snow meshes remains absent from the existing painter; this
change restores state/history, not that visual feature. A zero-time initial
repaint and the visit's existing 60 Hz clock remain explicit timing gaps.

### Controlled original manual Time Attack

The original game was entered through ordinary menu inputs into stock AE86,
manual, dry Myogi Time Attack. No guest state was written and no opponent was
injected. The early first-gear pilots peaked around 42–45 km/h; they are not
high-speed cornering evidence. A later pilot had mismatched intro input and is
retained as uncontrolled evidence rather than used for an A/B conclusion.

The corrected A/B/C runs use the same original inputs, all 320 vehicle words,
powertrain and random state through solver frame **1,986**. Their first intended
input difference is frame 1,987. Each runs 4,000 solver steps, with 3,941 retained
comparison rows. Each independent native replay matches **319/320 vehicle
words and all 32 reported movement metrics**. At frame 1,984 the original raw
speed is 44.9150658 m/s, approximately **161.69 km/h**. Original code
`15ECE0–15ED04` and its `3.6f` literal independently establish that conversion.

The mismatches are not filtered away:

- Vehicle `+0x118` is a nearest-path index. In these actual original TA runs,
  the path pointer is zero and the original search reads mapped boot/BIOS
  words; native searches its exported route. Read-only captured memory and
  9,496 original instructions reproduce that index difference. No player-motion
  consequence was observed in these runs; exhaustive independence is unproved.
- Shared RNG and steering-mask EBC differ. At the time of this comparison one
  delayed pre-warmup draw lacked a proven caller; the later resource-owner
  investigation below identifies it. The comparator supplies no guessed extra draw or
  per-frame resynchronization. No moving rare-gate activation occurred in this
  particular controlled set.
- These are scripted shift/countersteer probes, not a supplied expert TST
  recording. Subsequent route-following inputs and wall contacts respond to
  each trajectory. They do not prove a dedicated second-TST penalty or resolve
  the reported player complaint.

The precise inputs, source speed fields, prefix checks and comparator limits
are retained in `flycast-ta/high-speed/controlled/summary.json`. The input
publication investigation separately traces the original graphics callback
registration to the edge/ADC publisher, but has not yet measured the callback
against each solver update. The current online edge guard remains a contract
of the native held-state producer, not proof of what every cabinet sampling
schedule permits. No arbitrary shift cooldown or macro-detection claim follows.

### Identified loading and race-overlay initialization

The delayed TA draw is now tied to a specific original resource constructor,
not an invented additional opponent. Original `16B640` initializes the race
overlay and, in profile modes 0/1/2, calls `1F9E60` before requesting the fixed
`interrupt02` model and texture. The random return value is discarded; original
format calls explicitly pass 2. Mode 3 follows a different resource branch
without that draw. A semantic resource-plan fixture executes the original
constructor's mode/RNG branches; the parent-order fixture traces `061B00`
through its 60 warmup steps with explicit child/asset boundaries.

The independent read-only capture `flycast-20261113-221245` corroborates the
owner's timing: the exact 56-byte overlay appears at `0DF97AA0` between VBlank
2600 and 2601, while the seed changes from `E130BAA2` to `35E90333`, one LCG
advance. The same object gains its resource pointer by VBlank 2634. All 18
manifested evidence files were independently hash-checked. This is object and
seed observation plus independent original-instruction/caller proof, **not a
live CPU call trace**. The pointed-to resource metadata lies outside the
captured ranges; its `interrupt02` identity comes from original code.

A separate omission is the loading artwork chooser `16CD20`. The live TA
chooser executes before player initialization, not at the delayed draw.
Its original parent branches and all 32,768 rand15 outcomes have been checked.
Normal Legend enemy 29 selects authored artwork 12 without a random draw.
Both integrations remain separate from whole-session RNG parity:
bootstrap state, other omitted presentation owners and dialogue cadence still
require explicit accounting. Their current integration status is recorded in
the private proposal/validation evidence, not assumed from these source proofs.

The reviewed startup integration is now present in the working source. Loading
selection runs after card acceptance and before course/player initialization;
the overlay owner runs before the sixty solver/pose warmups. Direct starts,
loading redraws, failed private saves and canceled renewals have explicit App
checks. Imported, online and replay behavior is excluded from these new owners.
The overlay resource plan currently has no rendering consumer; this restores
its state and random draw, not a missing visual asset implementation.

`owners/captured-startup-comparison/run-02` then starts the actual App using
only the live capture's pre-loading seed and ordinary stock AE86/Myogi dry/day
manual settings. Captured vehicle/pose words are compared, never assigned.
Using maintained build objects, **822 of 824 words/scalars match bit-for-bit**:
all 320 vehicle and 84 published actor words before warmups; 319/320 and
83/84 afterward; all 16 compared scalar values. Loading artwork, pre/post
warmup seeds, EBC98, initial21C, pose, gear and RPM match. The mismatches remain
reported: the known nearest-path index and primary actor+50's bit16.
Source035220 identifies that bit as headlights-off, written from car byte81
by the pose/presentation path; its full native ownership correction remains
under review. Neutral throttle suppression also differs (source1/native0),
reported separately despite zero physical throttle in both. This is bounded
startup evidence, not whole-session or expert TST equivalence.

`cadence/retained-sampling-summary.json` records the three retained controlled
captures without running another emulator. Each contains 3,940 adjacent
snapshot intervals advancing one solver frame and 164 advancing none, within
the selected frame60..4000 span. None has consecutive nonzero same-shift
publication snapshots. These are Lua VBlank snapshots, not observations at
each original solver input read; no universal input-rate limit follows.

### Combined native validation and restart recovery

The first combined attempt retained three failures: two clock checks timed out
during host I/O stalls and passed unchanged on a serial retry; the dialogue
dispatcher fixture lacked the declared final model-render sink `C5200`. That
fixture boundary was corrected without bypassing its owner or random calls.

`combined-check2` then passed all **37 native CTest suites** in 73.82 seconds
and the **13,265-check actual App feedback suite**. A Windows restart at
22:22:10 local interrupted the later application tests and the separate
managed build preflight. Those interrupted stages are not counted as passes.
`combined-check2recovered` verified the exact source manifest, rebuilt the
four application artifacts byte-for-byte unchanged, and completed the missing
checks: **362,162 ending application checks** and **164,383 exported
API/bridge/App precision and ownership checks**. The latter includes 34 actual
running solo solver frames after 180 ordinary startup frames and the native
finish capability's actual status publication.

The recovered native DLL SHA-256 is
`5545f3cfe3f46eb98e73c0dd39c0e9511fd24cc5e1884ef248ab373d2947aba1`.
Its diff, untracked sources and source manifest are archived together. No
Current installation or publication occurred. This candidate includes the
local Unity finish-binding correction, but does not yet include the proposed
online finish or loading/overlay-owner follow-ups.

The managed continuation also retains every attempt. `managed-check4` stopped
at Unity's license check after the restart. Normal Hub startup restored the
existing entitlement. `managed-check5` compiled, but the new keyboard fixture
incorrectly injected raw I/L/U virtual keys that the real SceneGame collector
does not emit. The fixture now builds its raw keyboard frame using the actual
collector arrays; production mapping and its strict assertions were unchanged.
`managed-check6` passed **295,784 actual managed checks**, including the new
wheel/pad/keyboard finish routes and invalid physical trigger sanitization.
Source/configuration, Current and ordinary user-file hashes were unchanged;
no physical device was polled by the fixture and no game was launched. This is
an Editor binding test, not yet a new paired native/player runtime pass.

`combined-check3` includes the reviewed startup owners and passed **41/41
CTest suites** in 65.84 seconds, **13,265 actual App feedback checks**,
**362,930 ending application checks**, and **164,383 exported API/bridge/App
checks**. The source manifest remained unchanged throughout. Its DLL SHA-256
is `531cdfe0db34138c93a4df4c307e538bcb8e5a20e08de3a2e9b0fee4e1737bd0`;
the exact tracked diff, untracked source and manifest are archived alongside
it. No Current installation or publication occurred. Online finish follow-ups
and actor-light feedback are still private proposals, not covered by that
candidate's passing result.

The first retained low-speed driving rerun from actual App startup kept shared RNG
and EBC equal through frame569. Its first remaining RNG difference is frame570
and first EBC difference frame571. All 32 reported movement metrics and
319/320 drive words remain exact in the inspected low-speed A run through
frame2800. These are the earlier first-gear captures with prefix210206 and
tails210458/210805/211344, not the later high-speed controlled captures.
The inherited comparator omitted the existing App smoke owner. The subsequent
comparison below resolves that test omission without changing production smoke
or adding compensating random draws. The earlier failed results remain retained.

### Retained driving continuation with the existing smoke owner

`owners/captured-startup-comparison/startup-and-retained-traces-results.json`
binds the startup, joined source captures and completed reruns to their hashes.
The private continuation now calls the existing App pose, camera, headlights and
smoke owners in their actual order before tire audio. At low-speed frame570,
two smoke emissions consume the twelve missing random draws, independently of
the preceding contact draw. No production change was needed for this difference.

All three low-speed A/B/C runs match their 34 reported scalar fields over
2,741 retained rows each. All three controlled high-speed HCA/HCB/HCC runs
match those fields over 3,941 rows each, including shared random state and EBC.
All 320 vehicle and 84 published actor words are also compared: the nearest-path
index and headlight-state bit remain the only differing words. They have not
been hidden or overwritten to force equality. The older HA/HB captures retain
uncontrolled prefixes and are not used as controlled technique comparisons.

These are **actual App initialization followed by an explicitly assembled
continuation**, not natural App race dispatch. The 34 fields are not 34
independent behavioral outputs: automatic brake is itself an observed supplied
boundary. Controller ADC/calibration, accepted shift edges, throttle suppression,
GO/gear enable, recording-progress and engine/tire stop boundaries are supplied
from capture. Vehicle, pose, contact, particle and random states are not injected
after initialization. A later source trace identifies captured `platform_frame`
at `0C92DE30` as direction-normalized recording progress, rather than a clock;
the original column/API names remain in retained artifacts and must not be read
as proof of wall-clock parity. Its natural owner and consumers are under audit.

The next comparison uses actual App showcase, countdown, GO and race-rule owners
to remove those observed race boundaries. Neither the retained comparisons nor
their passing movement metrics certify arbitrary cars/courses, cabinet input
sampling, real-time Unity behavior, or the expert repeated-TST complaint.

The first natural HCA continuation, retained as
`owners/natural-app-continuation/HCA-natural-01`, finds a real owner-phase
difference: native engages first gear at solver frame481, while the source
capture does so at482. Solver-frame counts remain aligned. The first movement
difference begins at481 and later contacts/random state diverge; the passing
explicit-owner comparison had supplied GO timing and therefore could not expose
this. No delayed input or captured state was added to conceal the result.
Showcase-to-countdown-to-GO ownership is now being traced against original code
before a correction. The separate recording-progress field already differs at61;
its consumers are being investigated independently rather than assumed to cause
the one-step GO discrepancy.

The source handoff fixture then executes 257,818 original instructions with
4,430 checks in numeric modes 0/2. The original auxiliary pass precedes child
dispatch: StandBy selects Pre at outer update 241; PreInit creates its 240-count
child at 242, after that update's auxiliary pass; GO first occurs at 422, or
solver frame 482 including the 60 warmups. The private four-hunk correction retains
that creation update only after an actual original offline showcase. It does
not delay isolated direct/validation starts or extension paths. The actual
App fixture passed 742 checks and an independent review found no blocking defect.

All three natural HCA/HCB/HCC reruns now match 35 of 36 reported scalar fields
over 3,941 rows each; only the recording-progress alias differs. This includes
independently owned gear enable, automatic brake, all compared motion/contact
fields, RNG and EBC. Each first-GO index is 482. The test supplies conditioned
cabinet ADCs and accepted shift edges, plus virtual monotonic elapsed time,
not captured race gates or subsequent state. It still reports the known drive
and actor word differences. These immutable private runners predate the separate
actor-light integration, and use one source-period host update at a time.
`owners/natural-app-continuation/natural-go-results.json` records their exact
source, output and build-input hashes.

The real Unity showcase still had a separate rounded 60 Hz clock and discarded
fractional elapsed time when it exited. That is a distinct cadence defect which
the source-period natural runner does not certify; a private correction and
actual exported-bridge tests across host rates are in preparation. The
recording-progress audit meanwhile establishes only wall/ditch history consumers,
not grip/force/acceleration/RNG branches. Correcting its previous-update ordering
is an analysis-marker fix and is being tested separately.

The reviewed recording-progress correction is now in working source. Source
fixtures passed 1,320 checks over 40,140 original instructions; the actual App
fixture passed 1,084 checks. A session retains the source progress latch across
resets, uses it for contact stamps, and updates it after the mode 2 solver during
StandBy, countdown, driving and retained Finish updates. Current driving-path
samples still use current progress. Imported/online/authority/replay paths are
excluded; inactive source-latch fields do not change their existing checksums.
Checkpoints preserve the separate retained latch. Final combined validation is
still required after promotion and its repeat-safe fixture-directory adaptation.

The original global's cold initialization remains unproved: fresh native zero
is a declared boundary, not a claimed recovered reset. The existing goal-time
copy of the Time Attack result snapshot is also unchanged; the current oracle
does not establish whether the original captures those globals at goal time or
at later result entry. Neither limit is concealed by forcing captured state.

The showcase cadence correction has also been independently reviewed and
promoted over those changes. Original offline StandBy, PreInit and driving now
share one `OriginalDrivingClock` accumulator; rendering reads that showcase
without stepping it. The original creation update remains intact, including
when a single host update spans the handoff. Imported, online and replay paths
retain their existing contracts. Native diagnostic callers were adapted to use
the correct owner rather than relying on render to advance original driving.

The private exported-bridge fixture passed **43,796 checks** across eight
profiles: source rate, 30/60/144 Hz, jitter, pause/focus, hitches and a forced
handoff batch. A submission spanning source updates 239 through 243 preserves its quarter-tick
remainder. Additional positive-delta renders leave the full numerical digest,
RNG, solver count and showcase count unchanged. Compiler dependencies and
support objects are hashed, and the failed initial coverage-assumption attempt
is retained. This is offscreen virtual-time evidence, not physical display/input
timing. It intentionally retains the existing 100 ms/six-step offline catch-up
policy; it does not certify every elapsed original vblank during a long stall.
The combined 53-suite native candidate and five application suites are now being
built and checked before a fresh natural replay and actual Unity runtime test.

### Integrated online finish owner and managed runtime checks

Reviewed online finish proposal v4 is now in working source, after the archived
check3 candidate. It restores the original linked finish sequence: readiness,
602 hold calls and six exit calls, with same-update final unblanking. Linked
shift inputs cannot skip it. Precise driving controls remain available while
that owner runs; the results handoff clears already-mapped controls and requires
release before a menu action. Pending saves hold handoff separately from actual
modal/focus/recovery blocks. A settled finish can advance during a transport
stall without fabricating authority simulation frames. The eventual EOS results
handoff is explicitly an adaptation beyond the original SSNC wrapper.

The frozen private v4 source passed 31,533 actual App checks, 164,408 exported
bridge checks, and 19,424 presentation checks against 188,344 original
instructions. The final reviewed promotion bundle records all baseline and
installed hashes. This is in-memory authority testing, not a two-PC EOS test.

`managed-check7` then passed **295,844 actual managed checks**, including v4's
input and modal ownership route. It launched no game, polled no physical device,
and left source/configuration, Current and ordinary user files unchanged.
The silent `managed-build3` player scripts build also passed. Its retained
private player was restored byte-for-byte, and managed source/configuration,
Current and ordinary user files remained unchanged. The extracted assembly
SHA-256 is `00aae5cf84d91d17d2c4e8604e8ea5d5df0cb12df7da0ad48f889563d808912f`.
Combined native/player runtime validation remains pending; no Current
installation or publication is implied.

### Frozen combined candidate and natural continuation

The `combined-check4` build passed all **53 CTest suites** in 174.81 seconds.
Its first standalone application suite then failed with "Feedback observer
changed next solo physics step." The failed attempt and exact source manifest
are preserved. Field-level diagnosis shows that the App's initial pose now
runs the independently proven lamp-state owner, while the fixture's bare
control did not. The only initial difference is live actor `+0x50` bit 16;
the next solver publishes that same flag to the actor and recovery snapshots.
All other exposed driving, history, control and RNG fields match. Both recorder
latches are inactive. A private diagnostic supplies that same lamp owner to
the control and passes all 13,265 existing checks, without excluding a digest
word or changing production code. The minimal fixture correction and full
application validation were checked separately. The approved four-line fixture
correction preserves every previous line and assertion. The recovered candidate
then passed **13,265 feedback**, **362,930 ending**, **164,408 precise-input**
and **31,533 online-result** application checks, plus six 241-frame showcase
rev curves and their first-countdown transition. Production DLL and the four
other application binaries stayed byte-identical during the fixture rebuild;
the 53 passing CTests are reused with that exact dependency scope recorded.

The natural HCA/HCB/HCC runners were rebuilt against the frozen combined
source and support objects, with 477 input files sealed and compiler
dependencies checked. Each run compares 3,941 rows, from solver frame 60
through 4,000. All **36 named measurements**, **84 published actor words** and
**319 of 320 driving words** match bit-for-bit. GO first occurs at solver
frame 482 on both sides, with no solver-count difference. These runs include
actual startup, loading, player/HUD setup, warmup, showcase, creation,
countdown, driving rules, brake, engine/tire, recording and light owners.
Only the declared starting seed and observed cabinet ADC/accepted shift edges
are supplied. No later pose, speed, history, RNG or progress state is copied
from the source to force agreement.

The remaining word is `drive+0x118`, the retained nearest-path lookup index
already documented above. It remains in every comparison; its wider consumer
scope is still under investigation. The source `platform_frame` column is
compared to recording progress, not the host clock. The runs cover stock AE86,
Myogi dry/day/MT and controlled input sequences, not expert double-TST laps,
physical input latency, all cars/courses or live EOS. The trace success did
not override the failed application check; its cause and correction were
verified independently as described above. Exact source, executable
and result hashes are recorded in
`owners/natural-app-continuation/combined-check4/combined-natural-results.json`.

An independent read-only review verified the three metric sets, reconstructed
the raw capture joins, checked the input-only runner diff, and bound 451 source
files, 26 support inputs and 250 compiler includes to the candidate. It found
no circular state comparison or later state injection. The controlled capture
manifest is `flycast-ta/high-speed/controlled/manifest.json`. The source solver
delta schedules virtual elapsed time, so solver-count agreement in these
runs is partly alignment, not independent wall-clock proof. Accepted shift
edges bypass device acquisition. The runner's process-wide FTZ setting was
reported retrospectively; a contemporaneous MXCSR receipt was not retained.
The independent review records these limits rather than filling in missing
evidence. Solver reset/tick floating-point scopes are separate production
code and are not evidence of the runner's surrounding process state.

An additional original input-bank review corrects historical capture labels:
`held_byte00` at `0C92ED00` is the P1 rising-edge bank, not held input;
`pressed_byte40` at `0C92ED40` is the P2 rising-edge bank; `released_byte80`
at `0C92ED80` is a third-bank rising field, not P1 release. The original
producer is `1CEE80`; P1 current/previous/release are `ECE0/ECF0/ED10`, and
P2 current/previous/release are `ED20/ED30/ED50` within `0C92xxxx`.
All three 3,941-row natural comparisons consume only `shift_pressed8`, which
equals the correct P2 rising-edge byte in every row. They never consume the
two incorrectly named columns; `held_byte00` is zero in every retained row.
This attribution correction does not invalidate those numerical comparisons
or establish physical input/Start acquisition. Sealed raw captures remain
unchanged. The review and exact inspected hashes are retained in
`owners/offline-finish-duration/parent-reference/input-bank-attribution-review.json`.

The recovered native candidate is archived as `combined-check4recovered` with
unchanged source during validation. DLL SHA-256 is
`8b57bc50a646b9c0d79587a3a126c2a06d05b70f6f05be82a94f54e66a111c0b`.
Its exact working diff, untracked source and manifest are preserved together.
Muted private Unity runtime validation is the next acceptance step; no Current
installation or publication has occurred.

The first combined Unity runtime attempt (`runtime-check1`) passed **18,507
solo checks**. Its 1,799 synthetic host intervals produced 1,793 driving ticks,
against 1,792.785 expected. The subsequent real local TCP pair failed its
wall-clock assertion: the host observed 600 synchronized local/remote race
ticks (10 source timer seconds) across 17.2679812 elapsed seconds. The report
and screenshots are retained. The measurement includes diagnostic methods
that explicitly freeze native input submission while rendering/readback/PNG
capture runs. The trajectory is stable before and after those operations;
the interrupted interval is being isolated from an uninterrupted clock check,
without relaxing tolerances or subtracting unexplained stalls. This attempt
did not reach the later return/rematch acceptance checks. Both private
binaries were restored, Current and ordinary user files were unchanged, and
all test windows remained muted on unshown desktops.

The additional static `path118-consumer-audit` identifies the retained nearest
route index's two semantic readers: the next 128-point search and active rival
pacing. The observed cold-TA traces have rivals disabled. Normal AI startup
installs matching real shared paths, so the observed discrepancy is not
evidence of incorrect current AI pacing. Original TA can inherit a path from
an earlier AI race because the sparse rival reset leaves that global intact;
forcing all TA paths or indices to zero would be incorrect. No additional
direct human force/input/gear/rules reader was found in the audited owners,
but whole-program indirect aliases are not excluded. Recovery/checkpoint and
digest copies retain the word. The audit makes no production change or claim
of complete state equality.

### Analysis entry and isolated runtime timing correction

The result snapshot investigation found a concrete discrepancy: original
HLecture reads final published statistics at entry after ARace and its close
child, while the native analysis retained the goal/timeout snapshot. Original
Finish sets the actor automatic-brake flag before its first solver, but the
drive latch is updated after that solver's statistics publisher. An ordinary
goal can therefore admit one more pedal-average/steering sample; a timeout
with an existing brake latch need not. Subsequent contact counts and top-gear
use can also change. Each contact append follows the statistics publication,
so the final solver can leave one impact that is not yet published.

The reviewed correction refreshes stock solo analysis at HLecture entry from
the last actual published values, trims event histories to those published
counts, and converts their markers without adding a solver update. Imported,
online, replay and inactive/suppressed recording paths retain their prior
contracts. Completed times and profiles are unchanged. The capture itself is
const and consumes no RNG; corrected advice classification can legitimately
change post-race shared RNG and the seed inherited by a later race.

Private source checks passed **85 assertions over 3,114 original instructions**.
The actual App fixture passed **106 checks**, including goal and timeout,
four Finish solvers, analysis entry, the unpublished final impact, completed
times/profile, advice RNG and repeated/excluded entry. Terminal progress and
contact admission are declared synthetic inputs. The exact six-file promotion
is recorded in `owners/result-snapshot-timing/promotion/manifest.json`.
This does not prove whole offline Finish duration or naturally driven full-race
Flycast results. The existing 99-wall/98-ditch retained-history bounds remain
an explicit policy rather than emulating source reads beyond those buffers.

The runtime timing diagnostic now coordinates both peers after their early
captures, measures a fresh uninterrupted 600-tick interval, and waits for both
measurements before further captures or lifecycle actions. Its original
0.85–1.15 tolerance and raw lifetime trajectory remain intact; no stall duration
is subtracted. Small barrier-file I/O remains included. `managed-build4` passed
with assembly SHA-256
`33b04cebe20f4f8e4e0b3267d0b357233d160d6cec43d1f551b79fa2ee36d8ed`.
The private player, managed source/configuration, Current and ordinary user
files were preserved. This is a diagnostic-only change, not a driving-clock
or transport change. The revised combined native build and runtime validation
remain pending.

The bounded follow-up on offline Finish duration identified a further open
integration discrepancy. Existing source skip tests establish minimum 120,
maximum 600 and accepted digital bit 7 for offline modes; the maintained solo
path instead uses audio completion, separate record-panel render timing and
an immediate Start bypass. This determines the number of retained Finish
solver updates and consequently the final published analysis data. The late
snapshot correction does not resolve that duration difference. Full original
initialization/hold/exit/close ordering, accepted input and interruption paths
are being mapped before proposing a correction; the current combined build
remains frozen during that investigation.

### Combined check5 and second private Unity runtime

`combined-check5` passed all **56 CTest suites in 95.17 seconds**, followed by
all five application suites: 13,265 feedback checks, 362,930 ending checks,
164,408 precise-input checks, 31,533 online-result checks, and six 241-frame
showcase rev curves with their first countdown transition. Source manifests
were unchanged throughout validation. The DLL SHA-256 is
`9b4cdff96dc0527dee7bacc73cc31ecdc0debd4c9ac310a10c723ed1c2e243eb`.
The exact diff, untracked source, and six ignored LZ4 vendor source files are
archived alongside it. The small vendor and asset-storage libraries were
rebuilt before validation; this binds their current source and does not
retroactively establish the provenance of earlier support objects.

`runtime-check2`, using this DLL and managed-build4, passed the 18,507 solo
checks and both uninterrupted paired clock windows. The host advanced 600
local and remote ticks in 10.00893 seconds; the joiner advanced 600 in
10.0012861 seconds. Both retained the original 0.85–1.15 acceptance bounds,
made no captures inside their window, and accumulated zero additional
stall observations there. The joiner's lifetime observation counter was
already 195 at window entry; the host's was zero. This managed counter counts
render observations of a cached native last-step-stalled flag, rather than
195 distinct failed physics steps. Those values remain in the raw reports.

The overall attempt **failed**: the joiner's later lifetime assertion reported
"Authority stalled during ordinary network conditions" and shut down; the
host subsequently failed while waiting for that peer's first-race validation.
Return/rematch acceptance was not reached. The earlier 195 observations are
being attributed to their actual owners and phases before any diagnostic or game
change. Passing the fresh timing windows does not erase that failure or prove
that the earlier stalls are harmless. Both private binaries were restored,
Current and ordinary user files were unchanged, and all test players shut
down. Testing remained muted on unshown desktops, over local TCP rather than
live EOS or two separate Internet connections.

### Offline finish and cabinet Start integration under validation

The private ordinary offline Finish helper passed **320 cases / 775,526
checks**, executing **20,462,052 original instructions**. The separate source
parent oracle passed **12 route cases / 934 assertions / 44,008 instructions**
for Time Attack numeric2 and live Legend/Bunta numeric0. Its declared entry
is a ready Hold at counter120 or600; renderer, numerical and resource leaves
are hooked. It proves six subsequent Exit dispatches, a final solver before
the same-update unblank, one Close dispatch without a solver, then result
Init/Main on the following original dispatch. The child fixture separately
covers outcome/record combinations, card flags, accepted input and counter
boundaries. Neither fixture substitutes for naturally driving an entire race.

The application correction has now passed its private transition tests and
independent review and has been promoted to source for combined verification.
Its driving scope ends at source-timed result Init and
final-statistics capture. Subsequent non-driving HLecture/common-result Main
still uses the existing rendered-screen timer; full screen-cadence equivalence
is not claimed. Source `race+0x64C` is a challenger interruption flag, and the
native host-retire mapping is explicitly an extension.

The reviewed managed input route adds a dedicated held cabinet Start signal
in unused flag14 of the unchanged 88-byte input. It separates configured or
default Start sources from Pause/Escape and from accelerator/menu-confirm
aliases. Focus, capture, menu-closing and reconnect guards also clear it.
Native acceptance and rising-edge ownership are tested separately. The bridge
proposal deliberately has no ambiguous raw-button fallback: older managed
clients auto-end the new native Finish but cannot manually skip it.

`managed-check8` stopped in the new fixture because Unity does not accept an
individual bitfield ButtonControl through that InputState.Change overload.
The fixture now submits a complete GamepadState; its assertions and the failed
run are preserved. No production correction was needed for that failure.
`managed-check9` then passed **295,952 checks**, including **108 new cabinet
Start routing checks**, plus the separately reported **237 existing Start
binding/migration checks**. It used synthetic Unity devices and actual routing
methods, without hardware polling or a game launch. Source/configuration,
Current, and ordinary user files were unchanged during the run.

The runtime-stall diagnostic additionally records a bounded in-memory event
timeline for the next paired run. Its original assertion, lifetime counter,
waits, native calls and session behavior remain unchanged. No earlier stall
has been forgiven or attributed solely from the clean clock windows.

The sealed offline Finish package is
`34d77fb5a4abfa98b3be3a59eed56d341ad029042c4727261e709efadd3dfc3c`.
Its actual App suite passed **4,674 checks** across Time Attack, Legend and
Bunta, each with goal/timeout and automatic/minimum-Start endings. Automatic
completion retains 609 post-terminal solver updates; a fresh accepted Start
at the minimum retains 128, followed by Close and result Init on separate
accepted updates. The exported bridge passed **586 checks**, the existing
late-statistics regression **106**, and the finish audio/engine/RNG regression
**3,635**. The private App fixture disables its numerical engine callback;
the separate audio suite retains that callback. These results prove the
declared scheduling boundaries, not a new whole-race natural RNG capture.

An initial battle-result rendering failure was traced to the fixture omitting
the normal `Hud::loadOriginal` initialization. Loading the real asset banks
fixed it without bypassing result rendering. Earlier failed attempts remain
archived. All 16 maintained source baselines and proposed file hashes were
verified before promotion; the exact receipt is in
`owners/offline-finish-duration/promotion/receipt.json`. Four new CTest suites
raise the next combined validation to 60. Current remains unchanged.

The next source discrepancy is the original Forced Quit shortcut. Its Start
and View held counters must each be in their source acceptance window, and
card/operator gates apply. Acceptance consumes the counters, sets remaining
time to zero and profile bit17; ordinary braking and stopped-actor rules end
the race afterward. The current card shortcut calls immediate host retirement
instead. Source proof is sealed in `owners/forced-quit-source-audit`; a private
instruction-level comparison is being built before changing this behavior.
The separate Unity pause-menu Retire action remains an explicit extension.

`runtime-check3` used unchanged native check5 with managed build5's bounded
timeline. Solo again passed 18,507 checks. Both first-race reports passed
(host 23,837 checks, join 24,349), each with zero lifetime stall observations.
Their coordinated 600-tick windows took 10.0007933 and 10.0420348 seconds.
This does **not** establish that the earlier intermittent stall was fixed.

The attempt then failed a stale test ordering: `ReturnToLobbyCheck` demanded
acknowledgment within five seconds of the session entering Results, before
its subsequent existing wait for native results page1. The source-backed
Finish keeps page0 and input unarmed for about10.2seconds. The test needs to
verify that deliberate input lock, wait for page1, and then retain its
five-second acknowledgment check. No production result-input change is
justified by this failure. Host stopped at24,164checks; the join's final report
was empty when the owned processes were shut down, so only its earlier
first-race report is accepted. Private binaries were restored; Current and
ordinary user files remained unchanged. Full return/rematch acceptance is
still pending.

The private Forced Quit owner now passes **22,789 cases / 3,556,255 checks /
689,248 actual SH4 instructions**. It covers accepted-level counters and
32-bit wrap, card/configuration gates, entry-positive timer values that become
nonpositive after decrement, subsequent zero-timer braking/stopped rules, and
the actual original goal-handler prefix followed by the chord. A same-update
goal clears profile bit17 first; the later accepted chord sets it again while
preserving the finish result. The source proofs do not yet establish the
App's device acquisition, counter placement or braking distance. Those are
the next integration checks. Numeric configuration bits remain explicitly
named as bits, without inventing an operator-menu label.

The online scheduling review found no separate rounded60Hz driving clock:
managed real unscaled deltas reach the same `OriginalDrivingClock` nominal
852×530/27MHz period, and each callback advances at most one new network
timeline frame. Smoke reports still divide ticks by60 for legacy game-time
units and separately report nominal wall time. Their ±15% wall-clock bounds
are a coarse runtime check, not evidence of exact real-time cadence or USB
latency. The earlier captured ten-second discrepancy is not explained by
renaming those units.

`combined-check6` compiled successfully, then passed58/60 CTest suites in
91.40seconds. All four new offline Finish suites passed. Existing
`original_showcase_handoff_app` failed with "guard fixture did not exit
showcase", and `original_recording_progress_app` failed with "Finished
solver/restamping lost retained source progress". Both failures are retained
and under diagnosis; the five later standalone application suites were not
run. Source stayed unchanged during the attempt. This is a failed candidate,
not a replacement for the last verified check5 binary.

A controlled repeated-shift comparison is now prepared under
`owners/tst-counterfactual`. Earlier B/C tapes first differ at solver1997 in
both steering and shifting, so their difference cannot isolate a second-shift
penalty. The new experiment first replays unchanged C in a private Flycast
instance and requires its accepted inputs and all retained driving/publication
words to match. Only after that control passes may a second run remove the
four additional accepted shift edges1997/1998/2004/2005 while retaining every
steering/pedal input. The intended clean comparison window1997..2076 must be
shortened if either new branch contacts a wall earlier. This is a deliberately
constructed counterfactual, not an expert TST reproduction or a basis for an
invented cooldown. No result is claimed from preparation alone.

The two combined6 failures were confirmed as fixture setup omissions. The
showcase guard retained App's default open menu, and the recording fixture
left its synthetic loading boundary active before manually selecting Finished.
The reviewed fixture-only correction first asserts those states freeze, then
explicitly closes the menu/completes loading and retains the original
progression/stamp assertions. Private runs passed744showcase and1087recording
checks. Patch `dd8eeeeeb41e7cf12740a4e8639bc482ad54329dfa3544e581a0b09a21aa5b17`
is promoted; no production guard was relaxed. Combined7 repeats all60 suites
and the five standalone applications. Managed-build6 independently compiled
the corrected result-screen test ordering, preserving private player, source,
Current and ordinary user files; its assembly is
`cc039cfd4a553b58ad5d7fe683dd465a020de2b0f19be58e9a927beba2ecc6ea`.

A driving-view ordering difference is source-confirmed. Both
PreInit05BE00 and GO05C040 dispatch the main view before parent062DE0 and
063CE0. The latter refreshes ACar's matrix/Euler fields through034840 after
the solver. The view therefore sees the previous ACar pose snapshot, whereas
the current native countdown/GO wrapper samples current drive after its solver.
For ordinary offline modes the source ACar uses its live actor+95C fallback:
1595C0 binds global0C900954 to live player0C9008A4, and062FA0 installs that
pointer. The alternate+960 published-actor pointer is installed only for
numeric mode1. The subsequent whole-scene review changes the implementation
conclusion: source view024100 updates the camera and immediately draws attached
children before physics. Native renders camera and car from the same completed
update. Moving only the native camera to the previous update would misalign it
with the rendered car. No such production change is approved.

Native road-contact completion copies drive XYZ/Euler into the live actor, and
the recovery path restores the corresponding pair. No concrete completed-pose
divergence has been established. Initial history and paired whole-scene
timestamps remain unverified; ordering alone is neither proof of a visible
camera bug nor of extra grip. The private source oracle passed33,500checks;
its software-rendering App fixture failed on an unsupported overlay blend
after omitting normal Unity UI capture. That failed attempt is retained.

### Combined check7 and controlled repeated-shift result

`combined-check7` passed all60 CTest suites in76.09seconds and all five broader
application suites with source unchanged. Its DLL is
`5047b411bb5a54b9a28bbdfc513a860cf80853afa30001421c5b56951399626a`.
The prior combined6 failures and the fixture-only corrections remain recorded
above. The complete diff, untracked source and ignored vendor inputs are
archived beside the new candidate; Current remains unchanged.

The fresh unchanged-C Flycast control reproduced the retained drive and actor
files byte-for-byte. The counterfactual then removed only accepted shift
edges1997/1998/2004/2005. Both branches match through1996 and remain contact-free
through2076. At2076 the repeated-shift branch is approximately2.11degrees
farther rotated and0.0422km/h faster. Later contacts begin at2095 versus2093;
that later divergence is not a clean-corner penalty measurement.

Independent check7 App continuations reproduce both branches exactly in36/36
named fields and84/84 published words across3,941rows each. The full320-word
drive comparison retains the existing+0x118 difference from the initial
frame60;319words agree. First GO is482 in both. Only the declared initial seed
and accepted input tape are supplied; no later state, RNG, gear or progress
resynchronization occurs. The paired result receipt is
`owners/tst-counterfactual/native-check7/paired-results.json`, SHA-256
`03f543cc025d18b5dee24a48c8a9bb793b725f2610f9ed0a98ca3f71be29f170`.

The native startup float receipt measures entry MXCSR0x1F80, explicit requested
FTZ/DAZ0x9FC0, the original scope0x9FC0 and restoration to that requested state
before App startup. It does not measure end-of-run or Flycast-thread MXCSR.
Both original captures flushed complete data before a renderer shutdown
assertion; data qualification and clean process exit remain separate, with
clean exit false. These limits preclude a blanket1:1 or expert double-TST claim
and do not justify adding a shift cooldown.

`runtime-check4` used check7 and managed-build6. Solo passed18,507checks;
both peers passed first-race verification, the source Finish hold, held-input
release guard and uninterrupted600-tick windows. Host and join windows took
10.0068033 and10.0290543seconds, with zero lifetime stall observations in both
final reports. The complete attempt still failed: both reached "Points did
not advance to Continue" (14,801host /14,791join checks). Native page and
managed input-arm state at failure were not captured, so this does not yet
distinguish a missing edge from delayed rearming after page loading.

Both traces show roughly191 managed frames after the points capture, followed
by a4.94-second main-thread gap and failure in the next observation. That is
consistent with the original points skip taking about95UI ticks before
synchronous Continue resources load, followed by the intentional new-page
release guard. It remains a hypothesis pending a bounded input/page trace.
Existing predicates, timeout bounds and raw input pulses are retained for that
diagnostic run. Both owned players shut down normally; private binaries were
restored and Current/user files were unchanged. Earlier runtime2 stalls remain
unresolved rather than erased by these later clean windows.

The revised private Forced Quit package passes22,869source cases /
3,607,384checks /697,227original instructions,2,553actual App checks and467
exported-bridge checks. Countdown View, existing positive-timer automatic
braking, same-update goal clearing and chord ordering are now explicit cases.
Unity focus/modal/pause transitions discard interrupted button history without
advancing the original counter owner or clearing its race marker; this is
declared host-input neutralization. The first new PreInit test inverted an
optional-view branch; its failed run is retained, and the correction asserts
the actual source dispatch sequence. Final independent review and real Editor
checks remain pending. Package results SHA-256:
`b2a3563b2a7dc825337a3f174f070bc9eb0319abb448de19105e75e47c98d717`.

Forced Quit revision3 then passed two independent reviews, including a rehash
of 2,048 unique input/support files. All 16 package files were promoted with
checked before/after hashes. The real Unity Editor run `managed-check10`
passed **296,150 checks**, including **198 new cabinet/Pause/View ownership
checks**, without polling physical devices or launching a game. Source/config,
Current and ordinary user files were unchanged during validation. Combined
native verification is now rerunning with three new suites, for 63 total.

The Points-to-Continue diagnostic is separately reviewed and promoted. It
retains the exact raw Enter pulse and five-second compound predicate. Two
additive read-only native selectors expose the points counter and pending
input bits; their fixture verifies byte invariance and retained queued edges
across repeated reads, not a new zero-delta rendering test. A bounded managed
trace records native page/animation state and the release guard around the
existing input; output is flushed after shutdown. It makes no result-timing,
physics, or input-acceptance change.

### Combined check8 and private player build7

The full candidate passed **63/63 CTest suites in 90.75 seconds** and all five
broader application suites: 13,265 feedback checks, 362,930 ending checks,
164,422 exported-input/bridge checks, 31,533 online-result checks, and the six
241-frame showcase rev curves with their countdown transition. The source
remained unchanged during validation. The DLL SHA-256 is
`b60c586e0e95d80216ee1bbbe4dbd909dcde42ffa41794e11b81296f0d4bae81`.
Its complete diff, untracked source and vendor inputs are archived alongside
the source manifest. The earlier paired TST captures remain specifically
check7 evidence; they are not relabeled as a fresh check8 capture.

Actual Unity private-player `managed-build7` also passed. Assembly SHA-256:
`527efbbf336eabc1e5121e858be1554eb463a5790425835b2a60a168a1c23842`.
The retained private player was restored byte-for-byte; source/configuration,
Current and ordinary user files were unchanged. Runtime-check5 now pairs
these exact binaries on muted, unshown desktops with the original assertions
and read-only result trace. No runtime outcome is inferred from build success.

`runtime-check5` subsequently **passed completely**: 18,507 solo checks,
23,873 host checks and 23,799 join checks. Both peers reached natural results,
accepted Continue, retained their connected lobby and unchanged saves/music,
completed the second countdown and drove 300 further local/remote ticks.
Both final reports record zero lifetime stall observations and completed
shutdown. Private binaries were restored and Current/user files were unchanged.
This remains local TCP, not live EOS across two Internet connections.

Each 391-sample trace has zero dropped observations. The raw Enter press is
retained as a native pending edge across a zero-tick host frame and neutral
writes, then consumed into points phase2/frame241. Native subsequently reaches
Continue; the next managed Update observes the new page and disarms, and the
following neutral Update rearms. The unchanged five-second compound predicate
passes after 3.943242 seconds on the host and 3.952205 on the joiner.

A real single-frame transition pause of 2.353292/2.361130 seconds remains.
Runtime4's roughly 4.94-second pause plausibly exhausted the same deadline
before rearming, but its absent page/input trace prevents proving that cause
or claiming it was fixed. No production result-input/state-machine defect is
demonstrated by runtime5. The independent conclusion is retained at
`owners/online-points-transition-diagnostic/RUNTIME5-INDEPENDENT-CONCLUSION.json`,
SHA-256 `8d744815ddfac4786de61afad2a276b031f6aefb58c2d258ac15fa6c5ba8bf4e`.

The remaining route-search discrepancy is now a bounded four-history audit:
cold TA; AI then same-course TA; AI then a different-course TA; and the
alternate-path profile26 followed by TA. The existing full-rival fixture
explicitly reinstalls global0C901728 as a caller boundary before each reset,
so it cannot certify this retained ownership. The new private probe must
observe the actual initializer and PATH resource/cache lifetime without
installing a captured index or replacing the route before every reset.
No maintained correction is approved from the cold-TA observation alone.

### Final retained-route ownership result

The four-history probe subsequently completed: **165 checks**, 11 complete
original initializations and three PATH cleanup calls, executing **35,392
original SH4 instructions**. The cases cover cold TA, AI to same-course TA,
AI to a different-course TA, and the alternate-path profile followed by TA.
Resource addresses, neutral poses and the cold null global are declared
fixture inputs. The device-reset leaf is hooked; allocator releases are
observed, not implemented as a full heap replay. There are zero solver steps
and no reads of freed backing. These are source-owner tests, not four live
Flycast race captures.

The independent resource audit establishes that ordinary race End destroys
the course and its PATH buffers, including on same-course restarts. Myogi,
Akina and Tsuchisaka destructor chains reach the same cleanup. The shared
route pointer survives that cleanup and the subsequent TA initialization;
the retained pointer does not establish that its old data is still valid.
Later normal active-AI initialization writes its new route and resets the
nearest-path index before pacing. No new human-motion or active-AI speed
discrepancy was demonstrated by this bounded probe.

The first unsupported numerical boundary is reuse of the freed addresses and
the exact bytes read by the next TA search. Retaining a native copy of the
previous route, substituting the next course or forcing a captured index would
invent those semantics. No production correction was made. Closing this gap
requires normal transition captures of the original allocations, addresses
and first-search backing bytes; the existing cold-TA measurement remains
319/320 driving words and all measured motion/publication fields exact.

The result is sealed at `owners/path118-lifetime-probe/results.json`, SHA-256
`2bb06d020ae0432901e9f54ce9bcad432612bb427240f6ec90a6dba097328ad0`.
The independent ownership review SHA-256 is
`72e5ccf3efe78ffcc0338e92342427785b90dbe499817d7dd139790cc18bef4f`;
the resource-lifetime manifest SHA-256 is
`4b42ec868213f084b4ead7023cd2f511147926786912878e0bab2c9a3ce5c08f`.
No further build or runtime test is needed for this documentation-only
closeout. Candidate check8, managed build7 and passing runtime5 remain the
latest verified private artifacts. The unresolved acceptance gaps above
prevent a universal 1:1 certification.

### Continued route-state investigation: original menu observations

A further instruction-liveness review strengthens the bounded consumer result:
the normal nearest-path loop stores its selected index, then overwrites its
route-dependent temporary registers before the following player-force math.
This is positive data-flow evidence, not merely a missing-literal search.
Active AI still has an explicit indirect route through rival pacing and later
body contact; ordinary negative-control TA disables that identified consumer.
This does not prove whole-program noninterference or behavior under arbitrary
invalid-memory/floating-point exceptions. The sealed source review is
`owners/path118-live-transition-consumers/FINDINGS.txt`, SHA-256
`25d29991f0725e090e6367771de71a9e8ca302b03f0c2633db21755cde0737f6`.

Three new unchanged-executable Flycast observations then started from the same
declared original card-menu state, using only official virtual input and
read-only guest sampling. Runs03/04 each contain 1,846 samples with all 38
shared fields equal. Their only action is the same six-frame mode0 accelerator
pulse. They reached AVisitSelect; the first recorder missed its nested
iSelMain02 controller, which the second captured. Neither reached AI, and
neither supports a PATH-lifetime or handling conclusion. Sequence offsets
must be interpreted by class: outer ASeqNode +36 is a list head and +68 is a
selected/current-or-pending child; the inspected iSelMain controller has a
separately verified current-child role at +36.

Run05 adds one source-supported course confirmation, gated by the exact live
iSelCrs tables, dispatch state2, phase1, observed selection0/mapping0 and
released ADC/latch observations. The source accepts it at sample271
(ADC255/latch1/phase2); input is neutral from272. At414 the next iSelRival
child is selected but remains state0/uninitialized. The run stops without
sending rival input. The 269-row common prefix with04 is exact. This is
progress toward an ordinary race transition, not a completed driving test.
Independent qualifications are retained as
`OBSERVATIONS-03-04-INDEPENDENT-QUALIFICATION.json` (SHA-256
`5ae64fd36505878c1e5c4d16b7d6fc67fc219e0fe0d15fd11146846d115c356f`)
and `OBSERVATION-05-INDEPENDENT-QUALIFICATION.json` (SHA-256
`cd71c52339b0c90b18d9b363299d0696689ae931325f15f7e0dd977649bb0c5f`)
under `owners/path118-live-transition`.

All three captures flush before their explicit exit request; Flycast then
hits its renderer shutdown assertion and exits0x80000003. Clean exit remains
false. Run03 also logged an attempted upload of an earlier shared-Temp crash
dump at startup. Subsequent private launchers set their own TEMP/TMP directory
and explicitly disable UploadCrashLogs; runs04/05 record private temporary
files and no upload log line. This is a prospective isolation correction, not
a claim that the earlier launch had no external side effect. No maintained
game change, Current installation or release occurred in this follow-up.

Runs06–09 continue that declared setup boundary. Run06 confirms the original
rival selection only after the exact live selector, phase, default rival,
profile progress and released input guards agree. The accelerator is accepted
at sample441 and is neutral from442. Run07 then lets the source's automatic
iSelEnd complete without an additional input. The next selected owner at576
is AiRivalScene, before its initialization has completed.

Run08 records a further120 neutral vblank intervals. Run09 extends this by1800
intervals, retaining all696 prior samples exactly across their68 shared
columns. The live controller first becomes visible at619, but its resource
fields continue loading. Its phase changes from0 to1 only at726; phase1 then
persists through2496 with no further input. These captures reach the original
rival dialogue, not an AI race or an AI-to-TA transition. Source0234A2–0234AE
sets the outer dispatch flag before calling Init: flag2 is therefore **not**
evidence that loading has finished. The controller pointer's appearance also
does not prove that its constructor/initializer has returned. Preliminary
event-only interpretations were corrected against the CSV and raw snapshots.

The qualified runs retain the renderer shutdown failure and private temporary
file isolation described above. No handling change follows from these menu
observations. Their purpose is to establish normal transition history without
writing guest state, supplying captured route indices or guessing allocator
contents. Exact allocation/search timing is still outside the vblank observer.

Run10 adds one source-verified optional Start skip after the unchanged2496-row
prefix. The command begins at2498; the observed button is active at2499–2500.
At2500 the original publishes Start edge128, retains skip1 and advances the
dialogue tophase3 in the same update. All controls are neutral from2501.
The earlier unlaunched v8 proposal checked phase before acknowledgment and
would have misclassified this successful skip; reviewed v9 fixes that observer
ordering and binds acknowledgment to the same controller. No game code was
changed by this correction.

Normal loading then initializes the AI race. The validated owner appears
at2885 with source counter60; counter61 appears at3016 and advances once per
observed vblank through6498 at9453. Those6438 counter increments include
pre-race/showcase and finish behavior, not6438 moving-corner test frames.
The human driver remains neutral. The original timer reaches zero at8836,
publishes TIMEUP at8846, and leaves the race at9453. AVisit selects Close4
then Points5 at9454; at9455 the Points node has dispatch2. Its presentation is
embedded at+0x50, not an intro-style controller pointer; a zero first word is
not a null-controller failure, and dispatch2 alone still does not prove Init
returned. The capture stops without acting on this result owner.

The shared route pointer remains0x0DD37160 after the race wrapper clears its
owner. This is an actual normal-input lifecycle observation, not a comparison
with a newly run native trajectory or a completed AI-to-TA route. Legacy
`child` columns read ARace+36, the list head; selected/current attribution uses
the source-established+68 and retained raw snapshots. All2496×70 shared prefix
fields match run09. The independent qualification is
`OBSERVATION-10-INDEPENDENT-QUALIFICATION.json`, SHA-256
`25069b9667cf486dbbd2bf576fbcab8221e3be04bb99d0f65b6198d8bb576aed`.
The retained abnormal renderer shutdown is separate from the flushed data.

The field-level cleanup witness also finds the old PATH object at0x0DD3A060
within the retained backing window. By9453 its vtable has changed from
0x0C3823A4 to its base0x0C380964 and buffer slots+8/+0x10 have been cleared.
The first12000 raw bytes at the captured route addresses match across the
sampled boundaries; neighboring objects and preceding bytes change. This is
evidence of normal cleanup and retained payload bytes at those observations,
not a newly established allocation size, uninterrupted immutability or the
contents of a later TA search. The witness is
`observation10-cleanup-result-witness.json`, SHA-256
`412ec277e680bf13837c74bb3734b079df83af60a34dd177001564522a3589b3`.

Run11 preserves all9455×70 prefix observations from10 and extends the result
boundary by6000 neutral vblank intervals. The original selects Points5,
child6/table0x0C388384, Eject16 and GameOver17. Child6's captured Continue
choice becomes No and its parent flag clears without another confirmation.
The final source-bound tree is AGame → AAdvertise → ACaution, after rating
and SEGA-logo states. This is the attract sequence, not a demonstrated title
input or mode-select opportunity. No later TA initialization was performed.

This extension demonstrates why the stale shared pointer cannot stand for a
retained valid route: by the9944 snapshot, all3000 words in each declared
12000-byte old-route window differ from9455, and later snapshots change again.
The shared pointer, source counter and nearest index remain0x0DD37160/6498/8.
These are sampled overwrites during normal post-race flow; exact allocator
reuse and future search-read timing remain unobserved. No old native route
vector was retained or fabricated to force an internal-word match.

Run11's independent qualification SHA-256 is
`6bb52873c86ae754366237f61777d51bc1858f220e550437f30d4634b56cfe87`;
`observation11-session-backing-witness.json` is
`2ebddde0962cc8954c7cd5c549ffb3624063f88b493c7dc3cb8e2fc055070f1d`.
Its explicit renderer-exit failure is retained. All test processes in this
batch have ended; no production change follows from this lifecycle result.

### Remaining priorities and expert reproduction

A separate review found no clearly omitted core numerical driving subsystem
in the documented comparisons. The highest-value remaining question is the
reported second complete TST, followed by physical input/feedback and natural
integration coverage beyond the stock AE86/Myogi/dry/day/manual case. Actual
Flycast jump/landing encounters and two-network online driving also remain
separate gaps. The unresolved PATH word is disclosed, but it is not established
as the cause of easier cornering.

A bounded primary-source search did not produce an accepted-input recording
for the expert complaint. Indexed text from the author's
[advanced technique guide](https://initiald-arcade.com/advanced.php) gives a
qualitative Akina-first-corner sequence, but no reproducible car/tune, gear,
steering amplitude or frame timing. Direct access returned403. The supplied
[technique video](https://www.youtube.com/watch?v=rFziYAZBan8) could not be
retrieved; no frames, transcript or timing were examined. Several erasers
within one TST must not be mislabeled as two complete TSTs. The retrieval
record is `owners/tst-primary-repro-search/FINDINGS.txt`, SHA-256
`e3e11affefbca6b289f6772362c1e892f5513d711fe7279fe311a88d17b530e8`.

The next broader natural driving case should use a different drivetrain and
a tighter original course, with the same inputs applied to both versions.
Such a case would expand integration coverage; it would not substitute for
the still-pending expert reproduction or justify a new shifting cooldown.

Static attribution for that next case is now sealed in
`owners/r32-akina-wet-selector-source`: Nissan manufacturer UI4 maps to ID1,
model0 maps to R32 car7, and transmission selection1 enters the actual manual
shift branch. The original TA stage order is course, direction, weather, time,
with proposed selections3/0/1/0. The object identities, selection fields,
normal steering/confirmation gates and committed profile offsets are recorded
in `selector-map.json` (SHA-256
`7eb8500311409c7c8f2ae2eb307a05d673f3c51525de568e5911da040b3b148e`).
This is source attribution only. The existing saved states already contain an
AE86 card; the R32 must be selected through a fresh normal private card flow.
Intervening tuning/name pages, live direction labeling, the input tape and the
new native comparison remain pending. No R32/Akina/wet parity result is claimed.


### Fresh original card route for the second natural driving case

A new private Flycast process cold-booted from copied, retained cabinet NVRAM,
EEPROM and VMUs. No save state was loaded. This is a traceable fresh process,
not a claim of factory-default cabinet settings. The observer read only the
original memory and used the official virtual input API. Physical inputs were
disabled; audio was muted; the window stayed on an unshown desktop. Private
TEMP/TMP and disabled crash uploads were retained.

`owners/r32-akina-wet-live-capture/runs/cold-01` recorded 3,600 contiguous neutral
vblanks and reached the original active title page. The later `cold-02` repeated
its first 3,600 rows exactly across all 17 original observation columns, then
used three normal, bounded inputs: Start at the title, Start at the ready card
reader, and View to request a new card. The source accepted all three. The
card-reader transition was state6 to1; CardBuy changed phase1 to2 and completed
the original issuance sequence. At vblank4,008, profile flags were66 and the
manufacturer parent was selected but had not yet initialized. The capture
stopped there rather than confirming an unknown page.

Both historical Start presses in the older navigation trace did reach the
original input byte. Its first ignored press was therefore not evidence of a
lost host input. That older trace omitted enough reader fields that the exact
rejecting condition cannot be determined. The new run observes a consecutive
ready-state guard and needs only one accepted press. CardBuy uses View for the
purchase; Start would select the no-card route. These facts qualify the test
setup, not a handling correction.

The original source now supplies typed, current-child traversal through maker,
car, transmission, tuning, name, Saved Data confirmation, Game Mode, and the TA
condition selectors. R32 is Nissan maker1/car7; its MT selection is1. Actual
executing code establishes MT despite reversed labels in an old debug string.
The fresh default package is retained; other tuning choices are not described
as universally cosmetic. Name END uses the original default fullwidth SEGA.
TA selection order is course, direction, weather, time. The one proposed native
seed boundary is a dynamically identified selected TA loading child in state0,
before its Init advances the original random generator; a copied old vblank or
post-initialization seed is not an acceptable substitute.

Cold01 qualification SHA-256:
`3d71636fff6e15c5fa75d6fd626a97f6738789d4c3c89ff5bf56e5365712042b`.
Cold02 qualification SHA-256:
`11aac720c9dc387d6b4d5eb8e13963a3e94247d5091225b4db745e30b4b6ae44`.
Both emulator children asserted in their renderer after the observer flushed
and requested exit. Helper exit0 is not a clean emulator exit. Those failures
remain in the receipts. No R32 numerical comparison follows from these menu
captures, and no maintained game changes, installation or publication occurred.

### Completed cold R32 wet capture and newly exposed owners

Cold03 and cold04 exposed a test-navigation mistake: Game Mode reads absolute wheel position with hysteresis; centering the wheel returns to option0. The normal source selects option1 with axis5376 (ADC149), held through confirmation. This is distinct from the relative selectors elsewhere. Neither failed navigation attempt supplies driving evidence. Cold05 retains the corrected official input sequence, fresh private card route and unchanged stock R32/MT selection.

Cold05 captured12,458 vblanks, with5,941 unique solver rows60 through6000 on Akina downhill/wet/day. All6078 typed race observations include320 drive words and84 published words. The additional137 initial frame60 observations are inert: those words, applied inputs and RNG are identical while queued raw ADC changes during Init. They are retained and do not become artificial native physics updates. Earlier loader frame60 telemetry lacks raw words and is not claimed bit-identical. The seed boundary is selected loading child1/state0 at vblank5845, value2910162535; the next sample is one original LCG step. Artwork11 is source-derived; the actual artwork publication was not observed. The normal renderer assertion after flushed completion remains recorded; this is not a clean emulator exit.

Data qualification SHA-256: `9de210de122553d7e8a9844d93d6c7dc8125097f2ba76fe9c555cdf8f09d8872`. Qualified configuration: `15b37192b1191b216f5a6c5d5440c8360a8b38809e11290621c86e60b407bafe`. The private comparator stays on frozen check8 and replays observed conditioned ADC/accepted shift edges. It applies one starting seed, normal native initialization and later owners. All mismatches remain visible; no drive/RNG/pose/progress/suppression resynchronization is allowed. Native comparison is pending at this entry.

A separately verified source correction now sets the fresh per-car manufacturer fallback from the original35-car catalog. Previously the first Car page reloaded a fresh profile with manufacturer0, overwriting Nissan1 committed by Maker. The original negative control fails the new regression; the candidate passes all35 mappings,307other profile words, existing-save and backup preservation, and unreadable-file recovery checks. This is metadata only; no handling effect is established. The maintained two-file fix was promoted with LF-only normalization; evidence `owners/manufacturer-profile-fix/PROMOTED.json` records hashes. No player/DLL rebuild, installation, personal-save migration or publication follows from this fix. The frozen comparison deliberately retains and reports old maker0.

Two driving-path owner discrepancies are now independently source-bound. Normal wet races still use a private, render-timed rain effect, whereas the recovered shared-RNG rain manager is called only in endings; its normal constructor/child-update/camera state must be reproduced before integration. Separately, original ARace initialization sets throttle suppression and StandBy clears it at counter20: observed raw full throttle remains suppressed through solver80 and is released at81. Native conditioning supports that flag, but the App leaves it zero. The exact proposed fix and full numerical impact remain under review. These findings prevent a1:1 claim even though earlier controlled dry comparisons passed.

The frozen check8 comparison subsequently completed all 5,941 unique rows through solver 6,000 (exit 3: completed with mismatches). Position, orientation, speed, gear, braking, grip, loss and the measured steering memories matched bit for bit throughout this single conditioned-input case. It did not establish full parity. Throttle differed on 20 rows, beginning at solver 61; tachometer RPM differed on 296 rows (maximum difference about 4,065 RPM). Shared RNG differed on all 5,940 active updates, and the random steering-mask scalar differed on 5,923 rows. The initial raw drive mismatch is the previously unresolved route word at +0x118: original 731 versus native 0. All 320 drive words and 84 published words remain included, with no exclusions or later resets. The separately known fresh manufacturer metadata mismatch also remains in the frozen baseline.

The single translation unit and 26 current support artifacts are bound to the frozen source. The reused core library is byte-identical to its independently verified historical baseline; the later manufacturer correction is explicitly excluded. Compiler inputs, system link libraries and all 17,321 runtime data files were bound before execution. This is artifact identity, not independent semantic validation of every asset. Run receipt SHA-256: `14291455c034e76ea124da0c0af10763412d9aa5391717e3dcf99afc37321c4f`. No graphical window or audio was used by this native comparison.
