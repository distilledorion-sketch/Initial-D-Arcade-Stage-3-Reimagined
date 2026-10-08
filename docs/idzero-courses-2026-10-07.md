# Gunsai and Odawara integration

Integrated the user-supplied `OdawaraPreview.zip` into the current `.43` source plus the unpublished paint, replay camera/HUD and sun-glare changes. The sender's older source was merged selectively; its compiled DLL was not used. Source import/export tools are retained under `Tools` but were not executed for this integration.

## Course behavior

- Stable IDs: Gunsai 16 (conditions 32/33), Odawara 17 (34/35). Earlier course IDs, saves and title textures are unchanged.
- Gunsai: outbound/inbound, direction-specific start barriers and collision.
- Odawara: counterclockwise/clockwise, **two laps**, separate center/edge paths and collision for each direction's road near the finish. Four timed sections cover the full race; coaching adapts section times separately from actual lap crossings.
- Gunsai now uses **Usui handling** (original conditions 2/3). Odawara uses **Tsuchisaka handling** (original conditions 14/15) with 27.05% stronger powered acceleration. Both retain direction and dry/wet selection. Version .44 shipped both with Myogi handling. This is an adaptation, not a recreation of IDZero vehicle physics or a completed balancing pass.
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

## Unreleased Odawara handling adjustment

Odawara's shared native handling selection changed from Myogi (0/1) to Tsuchisaka (14/15). Subsequent adjustments scale powered acceleration response by 1.2705 (an initial 5%, followed by two further 10% increases), using the transmission's positive-rise divisors for gears 1–6 while retaining the original gear-speed targets, falling response, braking and steering. Neutral and prepared zero-throttle states use the unmodified profile. The factor is carried by the imported road into the shared driving session; other courses default to 1. Solo and online races use this mapping, with independent dry/wet selection. Course identity and the two-lap route are preserved. Multiplayer hashes the native DLL, so old and new handling cannot join the same race.

The rebuilt plugin passed the existing Odawara and Gunsai route/collision/checkpoint tests and 6,240 driving ticks per course across both directions, dry/wet and automatic/manual gears. The same short Odawara acceleration scenarios ran slower than the saved Myogi baseline; this is not a full-lap or top-speed measurement. Four online simulation cases passed 115,200 confirmed peer frames with latency, jitter and packet loss. Unity/Steam transport was not retested.

The plugin was installed into the repository's `Builds/Current` player with a verified backup. At the user's subsequent request, the same tested plugin was installed into `C:/Users/Chris/Desktop/Current`; its SHA-256 matches the tested build, and the previous plugin and build information were backed up. The Desktop build information records this local patch. This adjustment has not been published. Evidence, before/after logs, `desktop-install.json` and both backups are in `Verification/odawara-tsuchisaka-20261007/`.

The subsequent 5% acceleration adjustment passed paired vehicle-state probes across both directions, dry/wet and automatic/manual gears. Positive propulsion increments are approximately 1.05 times the unmodified Tsuchisaka response; coasting, brake-only, neutral and suppressed-throttle states match exactly. Gunsai regression checks, original vehicle reference parity and driving-session tests pass. Four online timeline cases again passed 115,200 confirmed peer frames with latency, jitter and packet loss. These are native solver tests; Unity/Steam transport was not retested.

The acceleration-adjusted DLL was installed into both `Builds/Current` and `C:/Users/Chris/Desktop/Current`. Both installed SHA-256 hashes match `FFB5E769A8588F9FC40832E6876B007BF28D2896873B47558BDA7A1366730DD1`. The previous Tsuchisaka-only plugins and Desktop build information are backed up; the Desktop metadata records the additional local adjustment. This update remains unpublished. Evidence and rollback files are in `Verification/odawara-acceleration-20261007/`.

The next requested increase applies another 10% to the 1.05 setting, for a total factor of 1.155. Paired vehicle-state probes verify propulsion increments are 1.10 times the previous setting across both directions, dry/wet and automatic/manual gears; unpowered behavior checks and Gunsai regressions pass. The online simulation again passed 115,200 confirmed peer frames. The new plugin was installed and SHA-256 verified in both current players as `6A7EEE776F395E01508416EA95BDFEC8BDE7B29034671B19100B1075E3CE3B04`, with the 1.05 plugins and Desktop metadata backed up. Evidence is in `Verification/odawara-acceleration-1155-20261007/`; this local adjustment remains unpublished.

A further requested 10% increase raises the factor from 1.155 to 1.2705. The same paired probes verify 1.10 times the previous propulsion response across all direction/weather/transmission variants; unpowered and Gunsai regression checks pass, as do 115,200 confirmed online simulation frames. Both current players now have the verified DLL SHA-256 `0F7F71E0BCEB4A48ABBE657052672C4A978AF2C8F6C126378B86700C24E6F609`. The previous 1.155 plugins and Desktop metadata are backed up in `Verification/odawara-acceleration-12705-20261007/`, alongside the test logs and installation report. This remains a local, unpublished update.

## Unreleased Gunsai handling adjustment

At the user's request, only Gunsai now uses Stage 3 Usui handling (conditions 2/3) instead of Myogi (0/1), in solo and online races. The two directions retain independent dry/wet selection and the original acceleration scale of 1. Gunsai's authored road, collision, timers and race identity are preserved. Odawara retains Tsuchisaka handling and acceleration scale 1.2705; the other imported courses retain their prior donors.

The rebuilt plugin passed Gunsai route/collision/checkpoint tests and 6,240 driving ticks across both directions, dry/wet and automatic/manual gears. The Odawara regression output matches the approved 1.2705 setup. Four Gunsai online simulation cases verified the selected donor for both cars and passed 115,200 confirmed peer frames with latency, jitter and packet loss; Unity/Steam transport was not retested.

Installed into `Builds/Current` and `C:/Users/Chris/Desktop/Current`, with both DLL hashes verified as `755B554567F383F17A9D3C23892F648684E4F498A25E8E3A5ED0ED31559395FE`. The previous plugins and Desktop build information are backed up, and the new Desktop metadata records the Gunsai change. This update is unpublished. Evidence and rollback files are in `Verification/gunsai-usui-20261007/`.

## Release .45

Release .45 includes the approved Gunsai and Odawara handling adjustments above. Migration `0010_idzero_handling_reset.sql` clears only their community runs (conditions 32–35), including individual-car records and associated replay storage, without advancing the season. It uses the existing replay cleanup triggers, records a one-time reset marker and rejects late .44 inserts on these two tracks. Both the client upload queue and service require .45 for new submissions; local personal records and replays remain intact.

The selective reset tests exercise all target directions/weather/model categories, preservation of unrelated records and settings, D1/R2 replay cleanup, rollback, repeated execution and an upload crossing the maintenance boundary. Seventy-two leaderboard tests and 87 managed release checks pass. The release procedure retains a database export and verified target replay backups, verifies unrelated records while writes are paused, and checks object cleanup after reopening. Release packaging, player checks and production verification are recorded in `Verification/release-45-20261007/`.
