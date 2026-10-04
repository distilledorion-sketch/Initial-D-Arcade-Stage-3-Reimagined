# Online finish and Continue flow

The first car crossing its goal ends the shared race. Both crossings in the
same simulation tick retain the source sub-frame timing comparison, including
an exact draw. One timeout alone does not beat a still-running opponent; double
timeout remains a neutral result. The result is exposed only after both peers'
confirmed history hashes agree through the deciding tick.

After settlement, both cars brake and both race clocks/progress stop. A trailing
car keeps its unfinished source status and has no fabricated finish time.
The result latch is included in checkpoints and the deterministic digest.

The online overlay now presents, in order:

1. YOU WIN / YOU LOSE (or DRAW / TIME UP).
2. Tuning points and battle-level points earned.
3. CONTINUE? with YES and NO. Enter/A confirms the selected choice; Escape/B
   selects No on this page. Input must be released between pages.

The outcome advances after three seconds or a fresh confirmation. The points
page waits for confirmation. One Yes never dismisses the other player's result.
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

- Native result application checks: 89 assertions, including either first
  finisher through real input-packet/hash confirmation, rejecting an unverified
  winner, exact selected-slot/car persistence, unchanged other profile fields,
  duplicate/conflicting results, no disconnected award, and Mode return.
- Timeline checks: first-goal boundaries for either slot, no false trailing
  finish, frozen clocks, braking, exact checkpoint restore, plus 19,200 peer
  frames with 50–250 ms simulated delay, jitter/loss, and collisions.
- Two real Unity players over LAN loopback: natural double timeout, ordered
  result/points/Continue pages, one-Yes waiting, both-Yes connected return, second
  showcase/countdown/race, and No while the opponent is on the points page.
  Evidence: `Verification/online-postrace-lan3`.
- A second two-client run reverses the declining role: guest chooses Yes, host
  chooses No, and both return to Mode with the online overlay closed.
  Evidence: `Verification/online-postrace-waiting-no`.
- Native plugin built with MSVC; production and diagnostic managed assemblies
  compiled against the installed Unity references. The isolated player uses
  private saves. Hidden-window runs check input/state transitions and render
  race-camera captures; they do not establish IMGUI pixel layout or Steam relay
  behavior. First-finish/winning reward checks use native boundary fixtures;
  the LAN result checks use natural timeouts.

This change is local to the Discord bug branch. R35 work is separate. No desktop
installation or GitHub release was made for this request.
