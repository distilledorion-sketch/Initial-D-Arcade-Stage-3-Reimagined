# Community leaderboard service

Cloudflare Workers + D1 backend for Time Attack rankings and moderation. The game already points at the public community service; players do not need to deploy this directory.

## Local tests

Use Node.js with `node:sqlite` support (Node 22.13+):

```powershell
node --test test/*.test.mjs
```

The tests use an in-memory database and never submit to the public service.

## Host a separate service

1. Install dependencies with `pnpm install --frozen-lockfile`.
2. Sign into your own Cloudflare account with `pnpm exec wrangler login`.
3. Create your own D1 database and put its ID/name in `wrangler.jsonc`.
   Enable R2, create a private Standard-storage bucket, and set the `REPLAYS`
   bucket name in that configuration. Do not enable public bucket access or
   automatic object expiration: replay visibility is checked by the Worker.
4. Read the SQL migrations before applying them to that database. Migration 0005 intentionally clears prior runs/replays and starts season 2; never apply it to a database you intend to preserve without reviewing/backing up first.
5. Apply the migrations using Wrangler.
6. Generate a private admin key. Set the lowercase hex SHA-256 digest of the key as the Worker secret `ADMIN_KEY_SHA256`; keep the key itself outside the repository.
7. Deploy the Worker with `pnpm exec wrangler deploy` and use `/admin` for moderation.
8. To connect a separate game distribution, change `ServiceUrl` in `Assets/Scripts/Idas3CommunityTimes.cs` and rebuild.

The checked-in configuration has a placeholder database ID. Production credentials, admin keys, installation credentials, database exports and private deployment settings are not included.

## Time-only leaderboard (current policy)

`DISABLE_REPLAYS: "true"` disables replay storage, uploads and all public/admin
replay downloads. Existing times, seasons, installation identities, bans and
online player-directory data are preserved. New finishes still use the exact
required client build, installation authentication, checkpoint/metadata
validation and the existing overall/per-model top-ten retention policy.
Historical imports and personal battle records remain rejected.

Updated clients post small JSON results to `POST /api/v3/runs` and read
`GET /api/v2/snapshot`. They no longer capture, compress or cache replay files
for leaderboard uploads. Old pending times can upload without their recordings.
Personal replay archives and local ghosts are separate and unaffected.

Released clients can continue posting to `/api/v2/runs`: only the envelope's
bounded JSON prefix is consumed, and its recording stream is cancelled without
being decoded or stored. Those clients may still attempt to transmit replay
bytes until updated. Replay telemetry is no longer used for server validation.

Public boards and v2 snapshots report `replayAvailable: false`. The v1 snapshot
keeps its legacy `replayAvailable` validation bit true solely because released
clients otherwise reject every ranking; its `replaysEnabled` policy is false,
and replay URLs return HTTP 410 regardless of that compatibility bit.

For the owner-requested cutover, deploy with `DISABLE_REPLAYS: "true"`, briefly
pause writes with `MAINTENANCE`, save a score/identity metadata inventory, and
apply **only** `0013_disable_leaderboard_replays.sql` to the existing database.
It removes D1 replay bytes, object mappings and replay metadata without deleting
a time. Database triggers reject replay writes from stale in-flight Workers.
Reopen writes immediately, then use authenticated, same-origin
`POST /api/admin/replay-purge` with `confirm: "DELETE ALL LEADERBOARD REPLAYS"`
and a reason to drain R2 in batches. The sweep is restricted to `replays/v1/`
and includes orphaned upload attempts. Scheduled cleanup also sweeps this
namespace to catch late writes. Keep the private R2 binding until cleanup is
verified complete; do not re-enable the old upload mode against these guards.

Verify the complete score/identity metadata inventory is unchanged and all D1
replay tables, replay-size/hash fields, the deletion queue and the R2 replay
prefix are empty. The public site must have no replay controls, old replay URLs
must return 410, and both snapshot versions must still return posted times.

The sections below describe the earlier replay-backed deployment and historical
migrations. Their replay-upload/download behavior is superseded by this policy.

## Behavior

- New runs require a matching detailed replay, current season and exactly game build `0.3.95-community-replays.50`. The game still requires a verified original ROM. Older builds, future builds and alternate version strings cannot submit times.
- `REQUIRED_CLIENT_BUILD` is an exact version, not a minimum. The obsolete `MIN_CLIENT_BUILD` variable is ignored. A version mismatch returns permanent HTTP 409 with `code: "client_build_required"` and `requiredBuild`; clients discard that queued run and must complete a new Time Attack in the required build.
- `/health` and `/api/v1/snapshot` expose `requiredBuild`. Changing this upload policy does not reset the season, delete scores, filter historical builds out of rankings, or restrict existing replay downloads. No migration is needed for the version change.
- Historical-time imports and replay-less uploads are rejected.
- Public downloads contain the replay and race metadata, not credentials or moderation fields.
- Moderation supports hiding/restoring runs and blocking/unblocking installations.
- With `RETAIN_TOP_TEN: "true"`, retain the overall top ten **and each car
  model's top ten** for every course, direction and weather. Model records
  remain even when they fall outside the overall ten. Each installation/car
  keeps only its best qualifying time. New valid
  submissions are still evaluated; nonqualifying times receive a successful
  acknowledgement with `retained: false` so clients do not retry forever.
  An entry and its replay are removed only when neither board retains it.
  Personal game saves and local replay files are not affected.
- Top-ten pruning also removes older seasons and ineligible hidden/blocked
  entries. Once pruned, a run cannot be restored by moderation or downloaded.
  Installation identities, bans and audit events are preserved.
- IP addresses are used for rate limiting, not stored with submissions. Worker observability is disabled in the supplied configuration.
- Client-reported telemetry and build versions are not authoritative anti-cheat verification.
- Course IDs 12–14 identify Myogi (Special Stage), Usui (Special Stage) and Momiji Line. Their directions use conditions 24–29; original Myogi and Usui records keep their existing IDs. Migration 0006 expands the condition constraint while preserving runs, replays and replay chunks. Apply it once when upgrading from 0005; do not rerun the season reset.
- Course ID 15 is Tsubaki Line (conditions 30–31). Apply migration 0007 once. It renames the backing table to `runs_storage` and exposes the same `runs` columns through a writable view; existing replay foreign keys follow the backing table. This widens conditions without copying gigabytes of replay data. Future migrations must account for the view and its insert/update/delete triggers. Normal queries, moderation, constraints and cascade behavior are covered by the worker and migration tests.

The service has no Steam-account linking requirement. Personal replay archives are separate from the Time Attack upload queue and are not uploaded automatically.

## Replay storage

New replay files are stored in private R2 object storage before committing their
score metadata to D1. This prevents replay bytes from filling D1's fixed 10 GB
database limit and blocking all new submissions. Completed upload retries
acknowledge the existing run; failed attempts have separate object keys and
are cleaned up without affecting the retained replay. An unsuccessful object
or database write returns a retryable
503; it does not acknowledge an uncommitted score. No game update is required.

Existing D1 replay blobs remain downloadable. The authenticated, same-origin
`POST /api/admin/replay-storage` endpoint moves one specified replay at a time.
It requires `id`, `confirm: "MOVE REPLAY TO OBJECT STORAGE"`, and a reason.
It verifies the original expanded replay hash, writes R2 with a checksum, reads
the complete object back, and compares the stored bytes before removing only
the old replay blob in an audited transaction. It does not change score rows,
seasons, identities, or moderation. The operation is safe to retry after a
partial transfer. Keep a before/after score-metadata inventory and the per-run
transfer receipts when migrating an existing service.

Apply migration `0008_replay_retention.sql` once before deploying this Worker.
It adds per-attempt object mappings and a durable replay-deletion queue without
deleting existing data. A new object key for each upload attempt prevents a
delayed cleanup from deleting another concurrent attempt's committed replay.
Score insertion, object mapping and top-ten pruning share one SQL transaction.
Object deletion is retried every five minutes if R2 is temporarily unavailable.

For an existing installation, authenticated `GET /api/admin/retention` previews
the exact retained IDs. `POST` with `confirm: "PERMANENTLY KEEP ONLY TOP TEN"`
removes up to 200 nonqualifying records per call and queues their replay objects
for permanent removal. Repeat until the preview reports no removals, then set
`RETAIN_TOP_TEN` to `"true"` to enforce the rule on future uploads. This operation
is intentionally irreversible through the leaderboard. `POST
/api/admin/replay-cleanup` drains up to 500 queued object removals per call.

`/health` exposes `replayStorage: "object"` when the binding is connected.
It reports `retention: "top_ten_overall_and_model"` for the retention policy.
The website board endpoint returns ten entries for either All models or the
selected car. The game snapshot keeps its existing compact representation:
the overall top ten plus each model's best time.
Without the binding, legacy database storage remains supported for local tests
and small independent deployments. Do not remove the binding from production
after migrating replays. Both public boards query committed scores immediately;
there is no daily publishing job.

## Recovery and maintenance

`MAINTENANCE: "true"` keeps public reads available while returning retryable
503 responses to writes and pausing scheduled cleanup. Use it for a short
database cutover; remove it before normal service resumes.

Migration `0010_idzero_handling_reset.sql` is the owner-requested .45 reset for
Gunsai and Odawara only (course IDs 16/17, conditions 32–35). It removes every
record for those courses across directions, weather, models, visibility and
seasons, including replay metadata and D1 bytes. The existing cleanup trigger
queues each associated R2 key. All other course records, installation identities,
bans and the current season remain unchanged. A completion marker makes an
accidental repeat preserve scores created after the reset. A narrow insert guard
rejects .44 scores for these two courses, including uploads that were already
in flight when maintenance began; later builds remain subject to the Worker's
exact-version policy.

For this cutover, first deploy maintenance and verify it publicly, export D1,
and back up/verify the target replay objects and metadata. Apply only migration
0010 (never rerun the older season reset), then verify the two courses are empty,
other records/replay mappings are unchanged, and their object keys are queued.
Publish the .45 game packages and reopen the Worker with
`REQUIRED_CLIENT_BUILD: "0.3.95-community-replays.45"`. Verify old queued .44
uploads are permanently rejected, allow the durable object cleanup to finish,
and verify target objects are gone while sample unrelated replays still download.
Keep the database export and replay backups outside the published release.

Never restore historical data over the live database. First preserve and verify
the current database, move live traffic to its verified copy, and restore only
the detached historical source. `src/recovery.mjs` is a recovery helper, not a
public endpoint. It requires a separate `SOURCE` binding and should only be used
by an authenticated, temporary maintenance worker. It applies current bans,
season and both top-ten cutoffs; preserves original score metadata; verifies the
expanded replay hash and complete R2 readback; and commits the score, object
mapping and pruning together. It never modifies the historical source. Remove
temporary recovery access after verifying the restored boards and replays.

### Odawara .47 handling reset

Migration `0011_odawara_handling_reset_47.sql` clears only Odawara (course 17,
conditions 34–35), across both directions, weather settings, all car models,
visibility, rulesets and seasons. It preserves Gunsai and every other course,
identities, bans and the current season. Replay mappings and D1 replay data are
removed with the scores; the existing durable queue removes their R2 objects.
A completion marker prevents a repeat from erasing new post-reset times.

Before applying only migration 0011, enable maintenance, export and validate
the database, and download and verify every target replay. Inventory R2 keys
against known current and historical Odawara run IDs to include identifiable
orphaned upload attempts. Never infer an unknown object's course from its
filename or delete unrelated objects. Keep backups outside the release.

Verify the frozen cutover preserves all non-Odawara rows and replay mappings,
then publish .47 and reopen with the existing exact global upload gate set to
`0.3.95-community-replays.47`. The database insert guard also rejects delayed
pre-.47 Odawara uploads that passed the old Worker before maintenance. It
accepts canonical numeric suffixes 47 and higher in the current build family;
changing the build family requires reviewing this guard. Future submissions
remain subject to the Worker's exact-version gate.

After cleanup, verify every target object is absent, old public replay URLs
return 404, and the website overall/model boards and game snapshot contain no
old Odawara rows. All public responses already use `Cache-Control: no-store`;
there is no separate KV leaderboard or ghost store. Other course samples must
still download with unchanged replay hashes. Local in-game boards and local
ghost invalidation are handled separately by the .47 game update.

## Online activity

The public page polls `/api/v2/activity` every 15 seconds while visible. Updated EOS clients with Community Times enabled share a fresh product-scoped activity survey using their existing installation token. Reports contain only aggregate online/queuing/racing counts, the search-limit flag, and sample age; no player IDs, names, room codes or locations are sent. Diagnostics do not publish. EOS presence itself is independent of the Community Times setting, so turning off leaderboard sharing does not remove a connected player from the in-game population survey.

The service retains only the newest EOS survey in `settings.online_activity_eos` (no schema migration). Legacy Steam surveys continue to use `/api/v1/activity` and their separate `online_activity` setting; they cannot overwrite the EOS website count. Reports are not summed across observers. A survey expires after 45 seconds; without a reporting client the page shows a dash rather than claiming zero. A plus sign indicates the EOS search limit of 200 results. Only clients with the activity publisher installed can be counted; earlier EOS clients do not publish presence. Client surveys are informational and are not authoritative anti-cheat or verified concurrent-user analytics.

## Online player directory and bans

Migration `0012_online_players.sql` adds a separate EOS player directory and name
history. It does not reset or remove scores, ghosts, installation identities,
existing bans, or admin sessions. Apply only this new migration when upgrading
an existing service. Do not rerun the older leaderboard-reset migrations.

Configure the four nonsecret Worker variables `EOS_CLIENT_ID`, `EOS_PRODUCT_ID`,
`EOS_SANDBOX_ID`, and `EOS_DEPLOYMENT_ID` to match the approved game client's
EOS configuration. The checked-in values are placeholders and intentionally
reject online checks. The Worker also requires the `ONLINE_AUTH_LIMIT` (600
requests/minute per IP before signature verification) and `ONLINE_LIMIT` (60
requests/minute per verified player) bindings in the example configuration.
The separate IP limit accommodates players sharing a network. No EOS client secret or sanctions-admin
credential is required or accepted by this backend.

Updated clients call `POST /api/v1/online/player` with an EOS **Connect ID token**
in `Authorization: Bearer <token>` and JSON:

```json
{"username":"Example","build":"0.3.95-community-replays.51","targetPlayerId":"0123456789abcdef0123456789abcdef"}
```

Only `username`, `build`, and optional `targetPlayerId` are accepted. The
requesting identity comes solely from the verified token's `sub` claim.
The token is verified with RS256 against Epic's fixed Connect JWKS endpoint
`https://api.epicgames.dev/auth/v1/oauth/jwks`, using a bounded key cache and
refresh cooldown. Verification requires the expected audience, product,
sandbox, deployment, Epic issuer origin, issue time and expiry; a supplied
not-before time is also checked. Token-supplied key URLs are never followed.
Auth/Epic account tokens and leaderboard installation tokens are not accepted.
See [Epic's Connect reference](https://dev.epicgames.com/docs/epic-online-services/eos-fundamentals/connect-interface/connect-reference).

The response is a versioned policy result:

```json
{"version":1,"playerId":"0123456789abcdef0123456789abcdef","blocked":false,"status":"allowed","targetPlayerId":"","targetBlocked":false,"targetStatus":"","checkedAt":1792000000}
```

`status` and a requested `targetStatus` are either `allowed` or `blocked`.
Without a target, both target ID and target status are empty strings. Clients
must validate the version, returned IDs and explicit statuses; missing fields,
failed authentication, rate limits or service errors
are not permission to connect. The service checks self and target independently
and returns only their policy status, never names, aliases or private reasons.
An unknown ID has no local directory ban; it is not automatically registered
by a target lookup. Epic sanctions remain a separate required client check.

Missing, null, empty or whitespace-only `username` performs a policy-only check
without creating a record or overwriting a saved name. This supports the
first-use username prompt and diagnostic clients. A chosen username registers
the authenticated player and updates their name, last-seen time and reported
build, independently of the Community Times sharing setting. Unchanged name/build
metadata writes occur at most once per 60 seconds; new or changed names/builds
update immediately. Every request still reads current self/peer ban status.
Names are trimmed,
NFC-normalized and limited to 16 Unicode scalars; controls, format characters,
invalid surrogate values, unassigned characters, line separators, angle brackets,
and names consisting only of whitespace/nonspacing/enclosing marks are rejected.
Name history uses an indexed Unicode uppercase search key and preserves display
spelling. Names are player-chosen, nonunique, and are not proof of identity.
Renaming never clears a ban. The database allows at most 24 genuinely new names
per player in a rolling 24-hour period; reusing a previous name or refreshing
current metadata does not consume another slot. This limits growth while
preserving every previous alias. A new name beyond the limit returns HTTP 400
with `code: "online_name_change_limit"` and leaves the directory unchanged.
Policy-only requests remain available to check bans. JWTs, IP addresses and
room chat are not stored.

The existing private admin session provides:

- `GET /api/admin/online-players?q=...&page=0`: exact full Player ID or
  case-insensitive current/previous name prefix; an empty query lists recent
  players. Results contain `players`, `page` and `hasMore`. Each player has
  `playerId`, `username`, `aliases`, `aliasesTruncated`, `firstSeen`, `lastSeen`,
  `build`, `blocked`, `banReason`, and `bannedAt`. Timestamps are Unix seconds;
  `bannedAt` is null when unblocked. Pages contain at most 50 players, and each
  history response at most 50 previous names. Older names remain searchable.
  Duplicate names return all matching identities across pages.
- `POST /api/admin/online-player` with `playerId`, boolean `blocked`, and a
  reason of 3–300 Unicode scalars. A valid admin cookie and matching Origin are
  required. Ban/unban and its audit entry commit atomically. Unknown,
  unregistered Player IDs cannot be banned through this endpoint.

There is no public directory or delete endpoint. All results use `no-store`.
Directory entries appear only after updated clients register; this cannot
reconstruct names or identities from matches played before the directory
existed. The last-seen timestamp records a service check, not verified playtime
or a live-presence guarantee. The build string is informational.

These are this game's own bans enforced by updated clients, not new Epic
sanctions or hardware bans. Older or modified clients may not enforce them,
and a newly created Device ID account is a different identity. Keep the
existing EOS sanctions enforcement. Do not deploy this feature as an
anti-cheat guarantee, or ban an ambiguous name without checking its Player ID
against the report.
