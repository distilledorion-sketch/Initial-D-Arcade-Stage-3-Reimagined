# Online disconnect recovery

Players reported random disconnections while queuing and racing after .39.
No affected player's log was supplied, so these fixes address reproduced local
failure paths; they do not establish the cause of every reported disconnect.

## Reproduced race failure

The 64-frame input history was bounded by local confirmation alone. During
one-way packet loss, a peer could confirm the other driver's inputs while the
other driver stopped acknowledging its history. Advancing then overwrote data
still needed for retransmission. The next packet threw
`Input acknowledgement fell outside retransmit history`, ending the race.

`OnlineRaceLink::step` now also stops at the remote acknowledgement boundary.
Packets and reconciliation continue while simulation waits, allowing the same
race to resume after delivery returns. No input window, physics, confirmation
hash, finish authority, or result validation has been relaxed. Both players
still need identical native and managed builds.

## Steam and queue fixes

- A Steam backend disconnect no longer immediately closes an admitted peer
  session. Existing packet delivery and normal peer timeouts remain active.
  Quick Match pauses for backend recovery for up to 60 seconds, then resumes
  its existing queue state. Explicit room departure/kick/ban still revokes it.
- A pending room search is cancelled when a second member arrives. Late search
  callbacks and search deadlines cannot fail the connected race. Cancelled or
  timed-out discovery callbacks are disposed so they do not fill the eight-call
  outstanding-operation limit. Late create/join callbacks retain their cleanup.
- Temporary search/create failures retry with bounded backoff (five retries).
  Search failures while hosting preserve the existing waiting room.
- An existing, admitted lobby member with temporarily missing protocol metadata
  stays admitted while still in that validated lobby. A new member still needs
  valid metadata; an explicit incompatible protocol revokes admission.
- Steam's `LimitExceeded` send result now retains reliable control messages in
  order and retries every 50 ms, up to eight messages per poll. Bounds are ten
  seconds, 128 messages and 256 KiB. Only messages Steam did not accept are
  retried. Heartbeat/ping traffic is unreliable to avoid accumulating stale
  timing messages ahead of race controls. Actual failed sessions are still
  terminal; no automatic session restart risks silently losing accepted
  reliable messages.
- Terminal Steam failures now log the transport reason, reason code when
  available, peer silence and pending control count before cleanup.

Steam's backend connection is distinct from APIs that can keep working without
it: [ISteamUser documentation](https://partner.steamgames.com/doc/api/ISteamUser#BLoggedOn).
The send-buffer-full return is documented under
[SendMessageToConnection](https://partner.steamgames.com/doc/api/ISteamNetworkingSockets#SendMessageToConnection).
The bundled Steamworks.NET comments also document the delivery uncertainty when
restarting a failed reliable session; the fix deliberately does not restart one.

## Verification

Evidence: `Verification/online-stability-20261004`.

- Before the native fix, the new production-wire regression fails with the
  retransmit-history exception (`native-before.log`). Afterward, all three
  three-second outages pass: loss toward host, toward client, and both ways,
  with different peer rates, duplicate packets, and delayed/reordered packets.
  All 3,600 confirmed peer frames and collision-effect digests match the
  uninterrupted baseline exactly.
- Existing timeline/codec checks plus four handling cases at 50/100/200/250 ms
  delay, with and without jitter/loss, pass: 38,400 confirmed peer frames.
  Malformed packets, forged digests, source-rule results and effects remain
  checked (`timeline.log` / `timeline.csv`).
- 48 deterministic checks compile and exercise the production Steam adapter
  and Quick Match with a fake Steam API: callback ordering, backend recovery,
  admission/removal, bounded ordered retries, queue timeouts and real terminal
  failures (`managed-checks.json`). These are not a live Steam network test.
- Two separate actual native DLL instances pass all three outages after both
  have entered a live race, resume normal scene stepping, verify matching
  history and preserve their private saves. No false result is awarded
  (`native-live-races/report.json`). The harness first catches independently
  paced peers up to a common frame using normal scene steps before comparing
  their acknowledgements.
- The complete Windows Unity player rebuild succeeds. Its native DLL hash
  matches the tested DLL:
  `4f1b9a047a443d108f1d0362cff265fc197fb8a90cc7d463a1c0fbe46ff3f34f`.

Re-run the focused checks:

```text
python Tests/Multiplayer/test_steam_stability.py
Native/bin/online_race_timeline_tests.exe Native --recovery
python Tests/Multiplayer/test_network_recovery.py --output <new-private-directory>
```

The rebuilt development player is in `Builds/Current`. No Desktop installation,
GitHub publication, leaderboard policy change or Discord post was made for this
request. A two-account Steam/relay session is still needed to establish behavior
on affected players' connections; actual route failures and prolonged loss
continue to end without awarding an unfinished race.
