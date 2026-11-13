# Discord bug follow-up — 4 November 2026

Continued from [the first local batch](bugs-channel-audit-2026-11-04.md). The
earlier fixes remain intact. No Discord posts, reactions, GitHub publication or
changes to `D:\Current` were made.

## Online callback freezes and concurrent queue corruption

Investigated [the VPN-associated stutter report](https://discord.com/channels/1548169613206884355/1566117699837239346), which describes brief freezes alongside network spikes in both solo and online play. The reporter tried multiple VPNs and protocols. The reported VPN connection itself has not been reproduced here.

Two defects were reproduced in the production EOS platform class with fake SDK
and Unity boundaries:

1. `Tick(true)` could run up to 4,096 deferred callbacks in one call, including
   work added by the callbacks themselves. A test burst of 16 deliberately slow
   callbacks occupied about 250.69 ms in one pump before the fix.
2. `Idas3EosActivity.ReleaseAfter` can call `Defer` from a Task continuation on a
   background thread, while the main thread dequeues from the same ordinary
   `Queue<Action>`. A four-producer stress test reproduced a
   `NullReferenceException` in the original dispatcher.

The queue now locks enqueue, dequeue and clear operations. Callbacks execute
outside the lock. Shutdown rejects late background work. Deferred callback
execution shares a 64-callback / 2-ms budget across both pumps in the same Unity
frame, preserving FIFO order and carrying remaining work into later frames.
Shutdown during dispatch cannot start renewal through a released platform.

The budget is checked between callbacks; it cannot interrupt one expensive
callback. The first paired after-fix burst pump executed one callback in 15.22 ms
(the final run measured 8.16 ms). These synthetic sleeps demonstrate yielding,
not a guarantee of 2-ms dispatch time, game FPS improvement, or proof that the
reporter's VPN issue is resolved.

### Validation

- Baseline production code: new burst test failed and concurrent-producer test
  crashed. Original code and failure logs were retained.
- Updated production EOS platform: 54 checks passed, including 2,000 concurrent
  callbacks delivered once in producer order, same-frame repeated pumps, queued
  callback chains, authentication/renewal, bounded capacity and shutdown.
- EOS transport: 261 checks passed.
- EOS activity: 26 checks passed.
- Online session lifecycle: 75 checks passed.
- Current managed game sources compiled against the installed Unity and EOS
  references. No fresh shipping Unity build was produced.
- These tests use fake service boundaries; they do not contact public rooms or
  establish real two-PC connectivity. Helpers ran without visible windows.

Evidence lives in `Verification/bugs-20261104-followup/`:
`platform-before/`, `platform-after/`, `platform-final/`, `transport/`,
`activity/`, `session/`, and `managed/`. Reports contain source hashes and logs.

## Replay TIME EXTENSION artifact: still open

Follow-up: the license issue cleared and this rendering defect was reproduced
and corrected in the [November 10 batch](bugs-channel-audit-2026-11-10.md).
The account below records the original November 4 stopping point.

Reopened [the replay report](https://discord.com/channels/1548169613206884355/1566723556270940190) and inspected its enlarged screenshot. The original TIME EXTENSION mesh uses wrapping UVs from 0 to -1, and the last texture row contains visible lettering. Sampling outside the polygon at partially covered pixels is a hypothesis worth comparing, not an established cause.

Prepared an isolated offscreen GPU comparison using the actual mesh and texture
bytes, the current shader, a candidate centroid-interpolation shader, multiple
scales/offsets, and 1x/4x sampling. The candidate shader exists only in the private
probe project; the game's shader and artwork were not changed.

Unity exited with code 198 before running that test. Its log reports missing
Editor entitlement (`com.unity.editor.headless`) and an unavailable access token.
The user was asked to refresh the existing license in Unity Hub. Resume the
prepared `UiProbe.Run` comparison after licensing is available, and include the
real UI camera settings before accepting a rendering fix.

The Tsuchisaka surface-dependent dust report and the reported VPN connection
remain unverified in the actual game. No visual parity claim is made for them.
