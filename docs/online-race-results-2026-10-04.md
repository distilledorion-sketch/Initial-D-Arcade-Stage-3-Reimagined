# Online finish and Continue flow

The first car crossing its goal ends the shared race. Both crossings in the
same simulation tick retain the source sub-frame timing comparison, including
an exact draw. One timeout alone does not beat a still-running opponent; double
timeout remains a neutral result. The result is exposed only after both peers'
confirmed history hashes agree through the deciding tick.

After settlement, both cars brake and both race clocks/progress stop. A trailing
car keeps its unfinished source status and has no fabricated finish time.
The result latch is included in checkpoints and the deterministic digest.

Online results now use the existing arcade renderers, in order:

1. The original on-track FINISH and outcome artwork/audio.
2. The original battle-result artwork over the rotating selected car, including
   its source point-count animation, sounds, times and saved point balance.
   Battle-level earnings use a small additional caption. An exact draw uses a
   neutral DRAW caption because the source result bank has no draw heading.
3. The original CONTINUE / YES / NO artwork and car view. Enter/A confirms the
   selected choice; Escape/B selects No. Input must be released between pages.
   Mouse hover/click targets come from the imported YES/NO label geometry.

The local presentation runs at the source 60 Hz independently of race packets
and display frame rate. FINISH lasts 120 ticks, followed by the outcome for at
least 180 ticks and the end of its audio, then a 15-tick fade. Fresh confirmation
can skip ahead. The common result owner supplies its original reveal/count/hold
and fade, including confirmation to skip the count or hold. Continue uses its
879-tick timer and 41-tick confirmation dwell; expiry selects No. Losing focus
or opening a covering input owner freezes this local presentation and clears
pending input. One Yes never dismisses the other player's result.
Both Yes votes use the existing host commit/client acknowledgement to retire
the race, restore **Mode** selection, and open the connected online menu.
No leaves the room and returns to Mode selection with the online overlay closed.
If the opponent leaves after a result, that result and its awards stay visible;
a player already waiting on Yes returns to Mode automatically.

## Awards

The new online tuning reward policy is 1,000 participation plus 1,000 for a win.
A draw earns participation only; double timeout and unfinished disconnection earn
none. This is a remake policy, not a claim about recovered original VS scoring.
The ordinary 999,999,999 point cap applies. The points screen reports the amount
actually added at the cap.

Only the exact selected garage car's point balance is committed, through the
existing validated profile writer. Its tuning, parts, transmission and story
progress are retained. Race-normalized profile data and peer data are never
written into that save. The active offline profile's cached balance is refreshed
if it is the rewarded car, so returning cannot undo the award. Duplicate results
cannot award again; conflicting results are rejected. Battle history/level
arithmetic and its receipt store retain their existing behavior. A failure to
save battle history is shown without hiding already saved tuning points.

The online protocol is now 11 because an individual ReturnRequest is a vote,
rather than authority to return both players immediately. Both peers need this
version. Lobby cars are refreshed before the next race so their new point
balances pass the saved-car validation.

## Verification

- Native result application checks: 227 assertions, including either first
  finisher through real input-packet/hash confirmation, rejecting an unverified
  winner, exact selected-slot/car persistence, unchanged other profile fields,
  duplicate/conflicting results, no disconnected award, and Mode return.
  The original post-race owners additionally cover frame rates from 30 to 240,
  point count reaching the saved balance, original Continue artwork loading,
  focus blocking, timeout/peer-departure No, Yes confirmation dwell, and leaving
  while waiting without changing the awarded profile. Both mouse labels remain
  hittable in either selected layout. Evidence:
  `Verification/original-online-results-20261004/native-pointer`.
- Timeline checks: first-goal boundaries for either slot, no false trailing
  finish, frozen clocks, braking, exact checkpoint restore, plus 19,200 peer
  frames with 50–250 ms simulated delay, jitter/loss, and collisions.
- Two real Unity players over LAN loopback: natural double timeout, ordered
  result/points/Continue pages, one-Yes waiting, both-Yes connected return, second
  showcase/countdown/race, and No while the opponent is on the points page.
  Evidence: `Verification/original-online-results-20261004/lan1`.
- A second two-client run reverses the declining role: guest chooses Yes, host
  chooses No, and both return to Mode with the online overlay closed.
  Evidence: `Verification/original-online-results-20261004/lan2`.
- Native plugin built with MSVC; production and diagnostic managed assemblies
  compiled against the installed Unity references. The isolated player uses
  private saves. Hidden-window runs check input/state transitions and render
  actual native result, Continue, and race-camera captures; the results no longer
  use IMGUI panels. These checks do not establish Steam relay behavior or
  physical controller hardware behavior. First-finish/winning reward checks use native boundary fixtures;
  the LAN result checks use natural timeouts.

The original-screen correction is included in release .41. R35 work is separate.
This publication does not install the build on the user's Desktop.

## Rematch and interim finish correction (2026-10-05, release .42)

The returned Mode screen initialized and advanced the source countdown stored
at profile offset 1176 after the lobby had read its car. That changed the full
profile checked by `Idas3MultiplayerStartSaved`, producing the misleading
"Saved car changed" error. Mode now initializes before garage refresh, and a
covering managed menu suspends its hidden selection owner. Closing the overlay
resumes the menu. The full 307-word saved-car validation remains unchanged.

Garage refresh also preserves the exact save slot, car and AT/MT choice when
playing a secondary garage car makes it the save's primary menu entry. A
same-model car in a different file must not take its place.

The native development finish/status panels are now excluded from online
races, including the interval between the local finish and verified result.
The existing original FINISH/outcome, points and Continue owners still run.

The regression reproduces the old snapshot mismatch, then verifies the fix
through a 30-second covered Mode wait, menu resumption, awards and save
preservation (356 native application assertions). Pixel checks cover the
pending-result interval and retain the offline fallback panel. Evidence is in
`Verification/online-rematch-20261005`. These source/plugin changes are included in release .42.
This publication does not install a Desktop build.

Two actual Unity clients also passed the connected result/Continue/rematch and
peer-No flow over local TCP (27,739 host checks, 26,815 guest checks). Both used
private duplicate-model saves, selected a secondary car from Save 2, retained
that same file and manual transmission when it became the primary entry, and
started and drove the second race. The runtime test uses natural double
timeouts; awarded balances and the active offline-car cache are covered by
the native application test. This does not verify two-account Steam relay
behavior. Production C# compiled against the installed Unity player references.
