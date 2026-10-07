# Community leaderboard service

Cloudflare Workers + D1 backend for Time Attack rankings, replay downloads and moderation. The game already points at the public community service; players do not need to deploy this directory.

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

## Behavior

- New runs require a matching detailed replay, current season and exactly game build `0.3.95-community-replays.44`. The game still requires a verified original ROM. Older builds, future builds and alternate version strings cannot submit times.
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

Never restore historical data over the live database. First preserve and verify
the current database, move live traffic to its verified copy, and restore only
the detached historical source. `src/recovery.mjs` is a recovery helper, not a
public endpoint. It requires a separate `SOURCE` binding and should only be used
by an authenticated, temporary maintenance worker. It applies current bans,
season and both top-ten cutoffs; preserves original score metadata; verifies the
expanded replay hash and complete R2 readback; and commits the score, object
mapping and pruning together. It never modifies the historical source. Remove
temporary recovery access after verifying the restored boards and replays.

## Online activity

The public page polls `/api/v1/activity` every 15 seconds while visible. Starting in .21, a game client with Community Times enabled and Steam initialized shares fresh game-scoped Steam surveys using its existing installation token. Reports contain only aggregate online/queuing/racing counts, the search-limit flag, and sample age; no Steam identities, names, room codes or locations are sent. Diagnostics do not publish.

The service retains only the newest survey in `settings.online_activity` (no schema migration). Reports are not summed across observers. The same activity namespace includes older compatible clients. A survey expires after 45 seconds; without a reporting client the page shows a dash rather than claiming zero. A plus sign indicates Steam's search limit. Client surveys are informational and are not authoritative anti-cheat or verified concurrent-user analytics.
