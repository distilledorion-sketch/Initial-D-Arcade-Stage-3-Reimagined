# Post-race Time Attack leaderboard

The post-results owner required `courseRankingQualified` before opening the
leaderboard. Once ten faster times existed, completed runs outside the top ten
went straight to Continue, even when the ranking data was available.

Completed runs now show the available leaderboard regardless of qualification
or record flags. Ranking order, the ten-row limit, current-run highlighting,
personal-record registration and points remain unchanged. Time-up and an empty
record list still proceed to Continue.

Verification is in `Verification/time-attack-leaderboard-20260926/` (ignored):

- `before.log`: the new nonqualifying-finish regression fails on the old gate.
- `after.log`: 624 native checks pass, including outside-top-ten finishes,
  course/direction/weather filtering, imported courses, ranking timing,
  time-up, empty records and the existing analysis-map checks.
- `unity/time-attack-completion-app.log`: 386 application checks pass, including
  controlled native gate finishes, prior personal-record preservation and the
  actual common-results rendering transition for a nonrecord finish.
- `unity/report.json`: 12 Unity checks pass, with a rendered leaderboard and
  controller confirmation through Continue/Exit. The leaderboard screenshot was
  visually inspected. All records and saves were isolated diagnostic fixtures;
  no times were submitted to the public service.
- `unity-build-fixed.log`: staging script build succeeded. The source and
  staged native DLLs match.

Included in release .35. Release verification is recorded in
`Verification/release-35-20260926/`; the ordinary desktop installation is not
part of this deployment.
