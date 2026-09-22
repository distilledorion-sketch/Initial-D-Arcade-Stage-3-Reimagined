# Special Stage courses

Version .25 adds three distinct layouts alongside the existing Enna Skyline import. Shomaru already exists in the Arcade Stage 3 course roster.

| Course | Stable ID / conditions | Runtime pack | D3 handling donor |
|---|---|---|---|
| Myogi (Special Stage) | 12 / 24–25 | MYOGI_SPECIAL | Shomaru, 12–13 |
| Usui (Special Stage) | 13 / 26–27 | USUI_SPECIAL | Happogahara, 8–9 |
| Momiji Line | 14 / 28–29 | MOMIJI | Akagi, 4–5 |

Myogi uses Shomaru for its narrow road, closely spaced bends and comparable gradients. Usui uses Happogahara for its sustained sequence of medium and tight bends. Momiji uses Akagi for its road width, slope and corner profile. These are adaptations using D3 handling; they do not reproduce the PS2 physics. Each donor retains its direction and dry/wet variants. Original course IDs and Bunta/Legend lists are unchanged.

All three support Time Attack, online battles, local records, community boards and replay playback. Available extracted scenery is night-only. Timer allowances start from the donor's normal Time Attack rules and increase per section where the imported route is longer. PS2 Time Attack itself has no countdown.

## Source and conversion

`Tools/Import-SpecialStageCourses.py` reads a local Special Stage disc extraction. It converts `MYOUGI_NIT.PAC`, `USUI_NIT.PAC` and `MOMIJI_NIT.PAC`, their corresponding course-data files, and original RCL1 collision meshes. It preserves authored road paths, checkpoints, UVs, vertex colors, lighting, fog, direction-specific gates, trees and spectators.

```powershell
python Tools/Import-SpecialStageCourses.py --source <extraction>/CDVD/DATA/COURSE --output RuntimeAssets
```

The checked-in packs are sufficient to build and play. Re-extraction requires the source files. Each manifest records source hashes and export counts. The ENN2 mesh format adds flags for source depth-write masking and confirmed overlapping, oppositely wound surfaces; older ENN1 Enna geometry remains supported. Only exact paired surfaces get face rejection, preserving other two-sided scenery.

Usui's manifest also retains all 45 original night scenery visibility windows. `CRS_INFO_USUI.BIN` stores inclusive forward-path limits in offsets 12/16 of each 28-byte section record. The original loader selects the night table (`00162420`), associates each record with `crsNN` (`00180210`), and tests those limits before drawing (`00180f10`, `00181d30`). Unity applies the same windows using the viewed car's forward source-path segment, including uphill races and replay seeks. This prevents `crs14`'s forest backdrop from crossing the later road at node 943; it remains visible at its intended nodes 780–907. Geometry, textures and collision remain intact.

`Tools/Export-SpecialStageMenu.ps1` combines a scenery-only in-game capture with the extracted road outline to produce each menu bank. `Tools/Export-ImportedStartTitles.ps1` generates the matching race titles.

Menu outlines use positive world X/Z as screen X/Y, matching the original Special Stage `K_CRSSEL.PAC` map atlas. Negating both axes rotates the maps 180 degrees. Downhill labels belong to the forward start marker and uphill labels to the forward goal marker; labels are drawn after projection so their text stays upright.

## Validation

`special_stage_course_tests` loads all six new directions, checks road contact along the full timed routes, verifies start grids/checkpoints/timers, and exercises the chosen handling with dry/wet, FR/4WD and AT/MT variants. The native frontend, multiplayer and record tests cover the enlarged course domain. Leaderboard tests cover new boards, complete replay uploads/downloads and preservation of old database rows.

Isolated Unity diagnostics use `-special-stage-ta-smoke <PACK> <new-output-directory>`, `-idas3-mode-flow-smoke <new-output-directory> -idas3-special-stage-check`, and the existing two-process multiplayer/replay checks. These use separate test saves. Automated driving and loopback tests do not establish human pace balance or internet latency behavior on every machine.
