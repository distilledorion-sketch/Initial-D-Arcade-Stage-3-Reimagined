# Initial D project stabilization audit

The project needs a stabilization cycle before more features, tracks, ports, or service migrations. Keep the native gameplay core and the existing tests. Replace fragile coordination incrementally, beginning with release verification and the complete online race cycle. A full rewrite is not supported by the findings below.

The principal problem is that the product has grown faster than its release and integration process. Passing a transport test, a menu fixture, and a save test separately does not prove that two players can finish, collect points, continue, and race again using the published download.

Audited on October 23, 2026 against source commit `01ab25997cdc292fc8c6d4694b86214513ad9b4f`. The running desktop diagnostic reported `0.3.95-community-replays.51`. Scope: Windows source, local release artifacts, selected regression tests, and an isolated packaged-player diagnostic. No live sanctions, player records, services, releases, or ordinary game saves were changed. Existing untracked `Design/` work was preserved.

**What is working**

There is substantial reusable work: original gameplay routines, reference tests, isolated game diagnostics, atomic save replacement and recovery, content manifests, updater checks, and backend validation. The sampled current checks passed except for a localization harness reporting error described below. This audit did not reproduce a new game crash or establish that every historical Discord report remains open.

| Check run for this audit | Result | What it establishes |
| --- | --- | --- |
| Leaderboard backend | 116/116 passed | Local backend regression suite, including time-only retention and disabled replay handling; not a live deployment test. |
| EOS transport | 261 checks passed | Current production transport logic compiled against simulated EOS boundaries; not real NAT traversal. |
| Online session | 67 checks passed | Production lifecycle logic with deterministic dependencies; not a two-PC race. |
| Online record storage | 92 checks passed | Real filesystem recovery and compatibility fixtures with injected progression; not server-authoritative stats. |
| Setup launcher | 3 scenarios passed | CMD and PowerShell default/explicit destinations, including difficult paths and safe refusal to overwrite a sentinel; not a complete download/install. |
| Selected native tests | 8/8 passed after build | Steering, physics, race, host input, original driving session, online timeline, original race rules, and collision reference. |
| Desktop .51 mode flow | 75 checks passed; orderly shutdown | Actual packaged native/Unity finish and timeout logic, finish-music routing, record sequence and controlled visual fixtures with isolated saves. |
| Localization catalog and desktop assembly | 4,744 inner checks passed; harness failed afterward | Catalog/status/placeholder logic passed. Writing the JSON report failed for a player directory outside the repository. Glyphs and layout were not tested. |

The native configuration registers 216 tests; this audit ran eight selected tests, not all 216. Three packaged-player captures were inspected: Time Attack analysis, points, and Continue at 1280 by 720. They render their expected content in these fixtures; this does not establish arcade visual parity, all-resolution correctness, or the live online handoff.

Local logs, binary hashes, fixtures and screenshots are retained in [the audit evidence directory](../Verification/project-audit-20261023/). They are local verification artifacts, not an additional game download.

**Ranked findings**

Priority 1 means it blocks confidence in expanding or marketing the game as stable. Priority 2 means fix during stabilization after the release and player-flow boundaries are secured. A risk or evidence gap is explicitly distinguished from a reproduced defect.

| Priority | Finding | Evidence and consequence |
| --- | --- | --- |
| 1 | Release verification is fragmented. Confirmed process gap. | No checked-in `.github/workflows` exists. `Test Native.cmd` runs one CTest selection plus a bridge smoke, whereas the configured native suite contains 216 tests. Release-specific scripts live under dated verification folders. These are useful checks but lack one maintained release gate. |
| 1 | The name “Current” does not identify one artifact. Confirmed artifact mismatch. | `Builds/Current` and desktop `Current` have different `Assembly-CSharp.dll` hashes. Their native DLLs match. The desktop assembly matches the recorded .51 release. This is not proof that the published release is wrong; it means a test against an unspecified “Current” is ambiguous. |
| 1 | Full online race-cycle evidence is insufficient for a stability claim. Coverage gap. | The .51 validation explicitly states that no two-PC online race was performed for that release. This audit's transport/session tests also simulate external dependencies. Real discovery, connection, finish, rewards, Continue, and subsequent races remain a release requirement. |
| 1 | Online identity, leaderboard identity and progression have different trust models. Confirmed design limitation. | EOS uses device authentication. Time Attack generates a separate local installation token. The database has separate `online_players` and `devices` identities. Battle progression is stored locally with a checksum. An EOS ID alone does not reliably identify the matching leaderboard installation, and a local checksum does not make progression authoritative. |
| 1 | Input, screen and session ownership are tightly coupled. Maintenance risk. | `Idas3SceneGame.Update` routes bindings, overlays, native menu inputs, multiplayer, pause, music, diagnostics and scene stepping. A shared catch sends failures to the game's failure path. Native `main.cpp` also coordinates substantial gameplay and presentation. Changes at these boundaries have a large regression surface. File length alone is not the defect. |
| 1 | “Identical Arcade Stage 3 behavior” lacks a complete acceptance baseline. Evidence gap. | The core uses a fixed 60 Hz clock and has original-routine/reference tests. Those do not, by themselves, prove whole-race equivalence with the arcade. The current audit did not measure original-game trajectories, corner speeds, wall losses, scale, or input sampling against the shipped build. |
| 1 | Performance evidence is narrower than the supported audience. Coverage gap. | The October 8 Gunsai/Odawara comparison provides useful full-route evidence on one RX 9070 XT at 2560 by 1080. Its own scope excludes display FPS and pure GPU timing. It cannot certify .51 on lower-end hardware or explain every user's stutter. |
| 2 | User and build documentation contradicts current behavior. Confirmed defect. | README still advertises downloadable leaderboard replays and says new submissions require a replay. BUILDING still says six imported courses; the current catalog has nine. |
| 2 | Localization test cannot report results for an external player path. Reproduced harness defect. | Its `--player` option accepts desktop `Current`, runs the 4,744 checks, then fails at `path.relative_to(ROOT)` while hashing the assembly for the report. This is a test portability failure, not evidence that Japanese or Chinese strings fail. |
| 2 | Course metadata exists in several languages/layers. Maintenance risk. | Native imported-course definitions, the managed catalog, and backend course definitions repeat IDs, names and rules. Existing backend tests check some consistency; a cross-layer contract should cover all IDs, direction labels, epochs, packs and handling profiles. No new mismatch between the sampled catalogs was established. |

Evidence locations for implementation work:

- Release entry points: [Test Native.cmd](../Test%20Native.cmd), [Build Native.cmd](../Build%20Native.cmd), [Idas3Build.cs](../Assets/Editor/Idas3Build.cs). Test and build scripts also use different default native build directories.
- Artifact comparison: [player-hashes.json](../Verification/project-audit-20261023/player-hashes.json); release limitations: [validation-summary.json](../Verification/release-51-20261018/validation-summary.json).
- Identity: [EOS initialization](../Assets/Scripts/Multiplayer/Idas3EosPlatform.cs), [Time Attack client](../Assets/Scripts/Idas3CommunityTimes.cs), [online-player schema](../Leaderboard/migrations/0012_online_players.sql), [installation schema](../Leaderboard/migrations/0001_leaderboard.sql).
- Progression: [record storage](../Assets/Scripts/Multiplayer/Idas3MultiplayerRecords.cs) and [moderation recovery](../Assets/Scripts/Multiplayer/Idas3MultiplayerRecords.Moderation.cs). Atomic replacement, backup recovery and repeat-result protection already exist and should be retained.
- Coordination: [Unity host](../Assets/Scripts/Idas3SceneGame.cs), [online session](../Assets/Scripts/Multiplayer/Idas3MultiplayerSession.cs), [native host](../Native/src/main.cpp).
- Physics and content: [fixed clock](../Native/src/race.h), [imported profiles](../Native/src/imported_course_catalog.h), [managed catalog](../Assets/Scripts/Idas3CourseCatalog.cs), [backend catalog](../Leaderboard/src/core.mjs).
- Localization harness: [test_localization_catalog.py](../Tests/Integrations/test_localization_catalog.py); the passing inner output is in `Verification/project-audit-20261023/localization/checks.log`.

**Physics and competitive scope**

Preserve the original-course solver while establishing a measured baseline. `FixedClock` steps at 1/60 second; offline catch-up clamps elapsed time to 100 ms, while the online host splits up to 250 ms into slices. That warrants explicit frame-stall and focus tests. It does not establish that map scale is wrong or that cars universally corner too fast.

Imported maps deliberately use donor handling. Odawara is configured with the Tsuchisaka handling condition and a Myogi speed condition, as requested. That custom combination should have its own named ruleset and records revision. It should not be presented as evidence for or against original Arcade Stage 3 course parity.

Validate representative FR, FF, AWD and MR cars against a controlled original-game reference: identical tuning, course, direction, weather, gear, input sequence and timing basis. Compare distance traveled, acceleration, speed through chosen corners, steering response, wall-contact loss and finish timing. Keep imported-map balancing separate. Changing physics before those comparisons risks moving the target without fixing the cause.

**Installation size and rendering**

The desktop installation measured 4.50 GB in decimal units, including the user's ROM and custom music. `StreamingAssets` accounts for 3.92 GB, approximately 87% of that installation: 1.94 GB of base data, 1.68 GB of imported courses, and 0.30 GB of SoundRoom content. These are installed bytes, not compressed release-download sizes. See [content-size.json](../Verification/project-audit-20261023/content-size.json).

The project uses Unity's built-in pipeline with Gamma color space and Windows Direct3D 11. Treat pipeline migration as separate feature work. First measure CPU time, GPU time, frame-time tails, allocations, memory, load time, scenery visibility and mirror cost in the current renderer. Asset deduplication/compression and rendering optimization need separate budgets; source refactoring alone will not substantially shrink the installed content.

Retain the existing content-manifest and preservation checks in [Stage-GameData.ps1](../Tools/Stage-GameData.ps1). Before removing assets, establish reachability from normal gameplay, every language, optional menus, replay playback and imported packs.

**Stabilization sequence**

The working scope is the existing Windows game. Pause new tracks, cars, Android work, rendering-pipeline changes and networking-provider swaps. Continue crash fixes, save protection, connection reliability, accessibility of existing controls, and measured performance fixes. The freeze is a project decision recorded here; it is not a branch-protection setting.

1. **Make releases reproducible.** Add one checked-in command that builds/tests a specified candidate and produces an evidence manifest containing source commit, Unity/toolchain versions, managed/native/content hashes, test outcomes and manual checks. Run source tests in CI and licensed/asset-dependent packaged checks on the appropriate local runner. Fix the localization report path and stale documentation. Identify desktop and published packages by manifest, not folder name. Exit: a clean candidate can be rebuilt and verified without copying commands from an old chat or dated release script; failures stop release preparation.

2. **Prove the complete online cycle.** Test the exact candidate on two machines using different networks. Cover Quick Match, public/private room code, join/cancel/retry, host disappearance, an unreachable peer, transient service failure, both finish orders, points applied once, Continue Yes/Yes and Yes/No, and immediate rematches. Proposed minimum gate: ten consecutive full cycles for Quick Match and ten for room-code play without stale state, duplicate rewards or an unexplained disconnect. Record build hashes and correlated logs on both sides. Add repeatable regressions for every failure before changing its owner.

3. **Untangle ownership around those tests.** Extract a single screen/input router and explicit race/session transitions from the host. Keep rendering, persistence and service calls behind narrow interfaces. Change one boundary at a time while the same fixtures stay green. Define which owner handles focus loss, controller reconnection, pause, results, reward commit and return to lobby. Exit: a transition has one owner and its invariants are checked; controllers recover across every menu/race transition without restarting the game.

4. **Choose and implement the identity and progression contract.** Use a recoverable verified account for online features that require durable moderation; link leaderboard submissions through a verified server-side relationship rather than usernames. Preserve offline play and provide a migration path for existing saves. Decide which competitive stats the server validates and stores; treat client-reported values accordingly. Exit: online and leaderboard moderation targets the same verified account, rename/reinstall recovery is exercised, and stat resets survive supported recovery paths. No account scheme should be described as making alternate accounts impossible.

5. **Establish physics and performance acceptance.** Use the original-course comparisons above, then benchmark complete routes and transitions on a declared minimum target and the development PC. Proposed 60 FPS target: p99 frame time at or below 16.7 ms on the declared minimum system/settings, with loading transitions reported separately and no accumulating memory across ten race cycles. Publish the actual hardware/settings with the results. Exit: each regression has a reproducible capture and a defined budget; stock physics and custom handling have distinct baselines.

6. **Standardize presentation and content contracts.** Generate or validate shared course metadata across native code, managed menus and backend. Run English/Japanese/Chinese custom-menu screenshots at 640x480, 1280x720, 1920x1080 and 2560x1080, including pause, online, music, results and Continue. Check text bounds, glyph coverage, input focus, background/audio restoration and transitions. Exit: all supported combinations are covered or explicitly restricted, and original textured menus retain their separately selected language behavior.

7. **Ship a stabilization candidate.** Exercise fresh installation and update from the supported previous release into isolated directories, verify user-data preservation and recovery from an interrupted update, then repeat the critical player journeys using the actual distributable. Publish precise fixes and known limitations. Resume feature expansion only after this candidate clears the preceding gates.

The first implementation deliverable should be the release-verification command and manifest, followed by online-cycle regressions and targeted ownership fixes. That gives subsequent refactoring a reliable pass/fail boundary. Replacing the whole game now would lose that boundary and reproduce the same integration problems in a new codebase.

**Implementation findings after the baseline audit**

The stabilization branch now has maintained source verification, Windows CI, an isolated native/Unity builder, and packaged-player checks. The earlier candidate and its private test copy matched all 19,066 distributable files. Packaged startup, mode flow, local TCP rematches and socket-loss recovery passed on that candidate. These checks cover controlled local conditions; only one PC is available, so different-network EOS acceptance remains unverified.

Fresh setup had a reproduced transaction defect: damaged compressed data could pass archive metadata validation, fail while extracting, and leave a partial destination that the next attempt refused. The setup change stages both archives beside the destination before publication. Regression checks cover this failure, successful retry, locked publication, trailing separators, and preservation of existing files. This addresses caught failures, not abrupt termination or power loss.

The bundled native updater has a separate reproduced recovery gap. Terminating only an isolated helper after its first replacement left mixed old/new files, a `ready` marker, verified backups, and no terminal result. Running the same plan again was refused. The private save fixture remained unchanged. Backup retention worked; recovery did not. The evidence is retained in `Verification/native-interruption-before-20261023/report.json`, identifying helper SHA-256 `6770b2f538332f5ecd5eadc6151ae50cb92ac84d3581b7ac075d070bc6beb50f`.

This reproduced defect established the recovery requirement: an accessible route that works even when the game cannot start, validates the complete plan and backups before writing, holds the existing session/install locks, refuses a running game, and can itself be interrupted and retried without losing backups. Complete restored hashes and private-file preservation must be verified before permitting cleanup. Process-termination tests and power-loss guarantees must be reported separately.

That route is implemented through the setup download's `Recover Initial D.cmd`. Local tests passed 21 native recovery cases, including restoration after terminating the previously shipped helper and terminating recovery itself; 23 normal installer/rollback/handoff cases; 10 native path cases; 30 managed cache checks; and nine actual recovery-launcher scenarios. The setup package is produced from explicit files and matching test evidence. Running the command from the actual extracted ZIP also restored a missing-executable fixture and preserved its private files.

The recovery revision passed all 32 source stages locally and on the [clean Windows runner](https://github.com/distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined/actions/runs/40051087285). A fresh Unity 6000.6.0f1 candidate passed eleven selected native tests and unchanged-source/content checks. Its 19,066 distributable files total 4,179,231,065 bytes; inventory SHA-256 is `654aff1458d9ffd8f27ef207fdc1d77b1b19e74e2db4fc8f1f4052e8d75fb8a6`. The compiled player passed 4,744 localization and 82 compatibility checks. A separate diagnostic scene used the byte-identical managed assembly and expected embedded helper for five passing Unity updater cases: full update, patch, damaged-retained-file fallback, corrupt full download and corrupt patch download. These exercised actual Unity download/staging/shutdown and native restart with disposable files on a loopback server; they do not establish a full public-release update journey.

Recovery is user-launched; automatic startup recovery and power-loss durability remain unverified. The broader online, identity, physics, performance and presentation gates remain open, and this revision has not been published as a release.
