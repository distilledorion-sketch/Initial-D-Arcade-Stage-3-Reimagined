# Gunsai and Odawara integration

Integrated the user-supplied `OdawaraPreview.zip` into the current `.43` source plus the unpublished paint, replay camera/HUD and sun-glare changes. The sender's older source was merged selectively; its compiled DLL was not used. Source import/export tools are retained under `Tools` but were not executed for this integration.

## Course behavior

- Stable IDs: Gunsai 16 (conditions 32/33), Odawara 17 (34/35). Earlier course IDs, saves and title textures are unchanged.
- Gunsai: outbound/inbound, direction-specific start barriers and collision.
- Odawara: counterclockwise/clockwise, **two laps**, separate center/edge paths and collision for each direction's road near the finish. Four timed sections cover the full race; coaching adapts section times separately from actual lap crossings.
- Both retain the supplied **Myogi handling donor**, including direction and dry/wet selection. This is an adaptation, not a recreation of IDZero vehicle physics or a completed balancing pass.
- Supplied day/night scenery and lamps; wet conditions use the supplied overcast sky/regrading of dry geometry. Authored normal and area-fog support is isolated to the new packs. Existing PS2 fog, track lighting and earlier analysis-map orientations are preserved.
- Course selection, loading/start titles, Discord presence, online choices/compatibility hashes, local records and replay course identities include both courses.
- The leaderboard source adds both courses and direction/weather boards. Migration `0009_idzero_courses.sql` extends the writable view without rebuilding score/replay storage or changing seasons. The migration and compatible Worker were deployed on October 7 before preparing release .44; all eight new direction/weather board requests passed.

## Local preview

`Builds/IdZeroPreview/InitialDUnity.exe` uses product name `Initial D IdZero Preview` and separate saves. Build with `Idas3Build.BuildIdZeroPreview`. Native DLL was rebuilt from the merged source and refreshed after correcting circuit analysis timing.

This preview includes the pending sun-glare, shared paint and replay-camera/HUD options. It is not published to GitHub and does not replace the Desktop or `Builds/Current` installation. No ROM is included. A temporary hardlink to the owner's verified local ROM was used during player checks, then removed.

## Verification

Evidence: `Verification/odawara-gunsai-20261007`.

- Successful Windows native build and Unity standalone build.
- All 527 supplied runtime files match the extracted package; 671 material texture references resolve. DDS payloads cover declared mip chains. Three Gunsai DDS files contain trailing source data that the loader does not need. All seven earlier race-title textures are byte-identical.
- Native tests pass for Gunsai, Odawara, Hakone, Sadamine and Tsubaki: route/surface checks, cold triangle lookup, collision traversal, checkpoints, finish, and 6,240 automatic/manual dry/wet driving ticks per course. Odawara asserts no early finish and one intermediate lap crossing in a two-lap race.
- Each new course passes a standalone Time Attack test across all eight direction/weather/time combinations: controlled gate finishes, natural timeout, points/tuning, persistence, isolated save slots, five analysis maps, ranking, Continue and menu records. These accelerated gate tests are not player lap-time benchmarks.
- Render captures cover both courses and all eight conditions, plus analysis, results and course cards.
- Two simulated online peers per track: both directions/weather choices, 50–250 ms latency, jitter and packet loss; 115,200 compared peer frames per track in the driving run, including wall impacts. Real two-account Steam play and full manual laps remain untested.
- 158 managed presence, course-choice, direction-specific simulation fingerprint and shader checks pass. Editing either direction's collision/path changes the handshake identity.
- All 61 local leaderboard Worker tests pass, including populated migration preservation of old scores/replay bytes/foreign keys and independent new boards/replay downloads.

The first Odawara player check found missing analysis section times after the lap-boundary correction. The final player runs include the timing adapter fix and pass.

## Desktop installation

At the user's request, the complete current player was rebuilt with the normal `Initial D Unity` product/save identity and installed into `C:/Users/Chris/Desktop/Current` on October 7. This includes both courses, the pending paint/replay/sun-glare options and the newer fixes since the previous Desktop installation. Of 18,968 approved game files, 559 changed or missing files (708,314,546 bytes) were installed; replaced files were backed up and all approved installed files were SHA-256 verified.

The Desktop executable passed the real ROM/startup check using isolated diagnostic saves. Existing saves/settings, ROM and custom music were verified unchanged. Evidence and rollback copies are in `Verification/desktop-all-20261007`. This Desktop installation preceded release .44 packaging.

## Release .44 preparation

The live leaderboard deployment preserved all 4,705 existing runs, 4,148 replay mappings, checked replay downloads and season 2. The service continues accepting .43 until .44 downloads are public. Fresh installations use the automatic setup downloader or both the core and additional-course ZIPs; existing installations use a single changed-file patch. Full Repair in .44 verifies and stages both full archives before installation. No original ROM or player data is distributed.
