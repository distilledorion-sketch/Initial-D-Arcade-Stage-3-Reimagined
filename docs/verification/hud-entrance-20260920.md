# Race HUD entrance — 2026-09-20

Restores the existing arcade HUD art and portrait motion; no new HUD textures or fonts.

## Original-code evidence

- 0C0C8082..80E8 resets owner age +CC and initializes +D4 at 3 with the 1AE520 response-3 filter; +D0 uses 1AE640 with damping .8, frequency .5 and initial value 3.
- 0C0C8AE6..8AFA targets the backing filter at zero after source frame 1, and the label/portrait filter after frame 7. 8B98..8BB6 publishes their results.
- 1AE640 uses constant 4.0 at 1AE688. The host preserves individual float operations and rounding, verified directly against the original SH4 instructions.
- Original battle names and advantage numbers use frame > 40. TA names use the same entrance gate; record/time digits retain their stationary original drawing coordinates.
- TIME uses negative +D0/+D4 offsets; the right panels and portrait use positive offsets in their existing authored matrices.

## Runtime integration

One immutable age table feeds all live HUD panels from originalRaceOwnerFrame. The owner is held through loading/showcase, starts at the countdown, and stops when paused. Replay HUDs retain their settled presentation. Post-race results/announcements keep settled transforms. No race handling, rules, card, save, networking or record data changed.

## Validation

- 480 float-bit comparisons covering both filters at source ages 1..240 against original instructions, plus 1104 complete original battle draw cases (220720 total comparisons).
- Existing original HUD tests: 10640 saved car/package/performance selections, 63872 valid/bounded RPM checks, 28 HUD appearance cases.
- Live compositor: TA, Legend portrait and online battle at 640x480, 1280x720 and 800x1000; every age 0..60, repeated rendering, race restart and source frame40 name gate.
- Existing online HUD pixel comparisons and tachometer/profile regression checks pass.
- Unity integration passed 6673 checks and captures the actual Time Attack entrance, then the Legend loading/showcase/entrance/3-2-1/GO and chase-view sequence, using isolated diagnostic saves.

Reference files supplied by user: Video Project 2.mp4 (portrait), Video Project 2 (2).mp4 (TIME), Video Project 3.mp4 (RECORD). Decoded frames and comparison sheets remain local in this directory.
