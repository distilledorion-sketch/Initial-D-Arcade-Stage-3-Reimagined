# Discord bug pass — 4 November 2026

Read the authenticated Initial D Reimagined `bugs` forum. No messages, reactions,
or changes to report status were sent. This is a local, unreleased batch in the
Discord-Bugs worktree; `D:\Current` was not changed.

## Completed local corrections

| Report | Correction | Evidence |
| --- | --- | --- |
| [FD3S model names swapped](https://discord.com/channels/1548169613206884355/1565999381226725456) | Online car ID 22 now displays RX-7 Spirit R; ID 23 displays RX-7 Type R. This follows the original FD3S VI / FD3S I order. | Current managed sources compile; 75 online-session lifecycle checks pass. |
| [Saved Data always says Toyota](https://discord.com/channels/1548169613206884355/1564473344013639731) | The manufacturer comes from the actual saved car ID, instead of stale manufacturer-selection workspace. Existing profiles need no rewrite. | Rendered all 35 cars with stale maker values; all seven manufacturer strips remain distinct and correct in English and Japanese. Nissan preview inspected. |
| [Car names missing beside Time Attack records](https://discord.com/channels/1548169613206884355/1564261220285157447) | TOTAL and MODEL records show the original car-name artwork. The total record retains its actual car, ties keep the existing holder, empty records do not invent a car, and changing the record car refreshes the cached menu. | Record-storage and frontend tests pass, including changes to visible name pixels with an unchanged record time. |
| [Xbox-emulated shifter disconnects wheel input](https://discord.com/channels/1548169613206884355/1563877717823463434) | Explicitly configured Xbox-style shifters contribute gear input alongside the active wheel. Shifting no longer makes automatic device selection abandon the wheel. Normal gamepads stay independent; the shifter does not supply inherited throttle/brake controls. | Synthetic Unity-device discovery, saved bindings, simultaneous steering/shifting, explicit gamepad selection, unplug/reconnect and held-input release checks pass. Physical wheel hardware remains untested. |

The manufacturer, record-label and wheel fixes were unfinished local changes at
the start of this pass. They were reviewed, compiled and exercised here; the
RX-7 online-label correction was added in this pass. The pre-existing diff is
preserved in `Verification/bugs-20261104/before.patch`.

## Validation

- Native Release build: `frontend_tests`, `original_saved_data_tests`,
  `time_attack_records_tests`, `original_hud_tests`, and `Idas3Unity` succeeded.
- CTest: `frontend`, `time_attack_records`, `original_hud`,
  `original_saved_data_en` and `original_saved_data_ja`: 5/5 passed.
- Managed online session harness: 75 checks passed with fake transports. This
  does not establish live EOS connectivity between two PCs.
- Current managed sources compiled against the installed Unity runtime and EOS
  references. The isolated diagnostic passed 30 wheel-rig checks, 181 controller
  menu checks and 59 private-room/direct-gear checks.
- Inspected the rendered Nissan Saved Data page and the course-selection
  preview showing different TOTAL and MODEL record cars beside their times.
- The diagnostic reused an existing private player, ran with `-batchmode
  -nographics -nosound`, disabled ordinary game startup in diagnostic-only source
  copies, and restored that player's previous assembly afterward. No public room
  or personal save was used. This is not a fresh shipping Unity build.
- `git diff --check` passed.

Logs, source hashes and previews: `Verification/bugs-20261104/`. Key reports are
`native-build.log`, `native-tests.log`, `session/report.json`, `input-report.json`,
`input-player.log` and `frontend-preview-checks.log`.

## Still open

See the [follow-up pass](bugs-channel-followup-2026-11-04.md) for reproduced and
corrected online callback defects and the replay graphics-test licensing block.

- [Replay TIME EXTENSION text corruption](https://discord.com/channels/1548169613206884355/1566723556270940190): read both the artifact description and the follow-up that normal gameplay is unaffected. Playback and live gameplay share the native HUD painter and Unity UI shader. No reproducible cause was established; no artwork or sampler change was guessed. Next: reproduce with an affected recording and compare native draw data, HUD layout and captured pixels at the checkpoint frame.
- [Tsuchisaka roadside dust](https://discord.com/channels/1548169613206884355/1566454363650523288): the report specifies dry weather, near road edges, excluding the tunnel. Current host driving effects cover skid smoke and marks, not this original surface-dependent effect. The correction still needs verified surface conditions and source presentation; no generic dust was added.
- VPN-associated stutters: the reporter tried three VPNs and multiple protocols. The code review found main-thread callback dispatch and snapshot processing worth profiling, but no causal reproduction. Next: correlate frame stalls with callback durations and leaderboard snapshot processing under the affected connection; do not claim a networking fix based on correlation alone.
- The Happogahara battle/control-loss report was visible in the forum. The repository already contains the .53 input-lock/stalled-race changes, but this pass does not establish whether that report was fixed for its author.

No Discord report was marked resolved. No GitHub release, desktop update,
physics change, or visual-quality reduction was performed in this pass.
