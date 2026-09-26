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
4. Read the SQL migrations before applying them to that database. Migration 0005 intentionally clears prior runs/replays and starts season 2; never apply it to a database you intend to preserve without reviewing/backing up first.
5. Apply the migrations using Wrangler.
6. Generate a private admin key. Set the lowercase hex SHA-256 digest of the key as the Worker secret `ADMIN_KEY_SHA256`; keep the key itself outside the repository.
7. Deploy the Worker with `pnpm exec wrangler deploy` and use `/admin` for moderation.
8. To connect a separate game distribution, change `ServiceUrl` in `Assets/Scripts/Idas3CommunityTimes.cs` and rebuild.

The checked-in configuration has a placeholder database ID. Production credentials, admin keys, installation credentials, database exports and private deployment settings are not included.

## Behavior

- New runs require a matching detailed replay, current season and exactly game build `0.3.95-community-replays.35`. The game still requires a verified original ROM. Older builds, future builds and alternate version strings cannot submit times.
- `REQUIRED_CLIENT_BUILD` is an exact version, not a minimum. The obsolete `MIN_CLIENT_BUILD` variable is ignored. A version mismatch returns permanent HTTP 409 with `code: "client_build_required"` and `requiredBuild`; clients discard that queued run and must complete a new Time Attack in the required build.
- `/health` and `/api/v1/snapshot` expose `requiredBuild`. Changing this upload policy does not reset the season, delete scores, filter historical builds out of rankings, or restrict existing replay downloads. No migration is needed for the version change.
- Historical-time imports and replay-less uploads are rejected.
- Public downloads contain the replay and race metadata, not credentials or moderation fields.
- Moderation supports hiding/restoring runs and blocking/unblocking installations.
- IP addresses are used for rate limiting, not stored with submissions. Worker observability is disabled in the supplied configuration.
- Client-reported telemetry and build versions are not authoritative anti-cheat verification.
- Course IDs 12–14 identify Myogi (Special Stage), Usui (Special Stage) and Momiji Line. Their directions use conditions 24–29; original Myogi and Usui records keep their existing IDs. Migration 0006 expands the condition constraint while preserving runs, replays and replay chunks. Apply it once when upgrading from 0005; do not rerun the season reset.

The service has no Steam-account linking requirement. Personal replay archives are separate from the Time Attack upload queue and are not uploaded automatically.

## Online activity

The public page polls `/api/v1/activity` every 15 seconds while visible. Starting in .21, a game client with Community Times enabled and Steam initialized shares fresh game-scoped Steam surveys using its existing installation token. Reports contain only aggregate online/queuing/racing counts, the search-limit flag, and sample age; no Steam identities, names, room codes or locations are sent. Diagnostics do not publish.

The service retains only the newest survey in `settings.online_activity` (no schema migration). Reports are not summed across observers. The same activity namespace includes older compatible clients. A survey expires after 45 seconds; without a reporting client the page shows a dash rather than claiming zero. A plus sign indicates Steam's search limit. Client surveys are informational and are not authoritative anti-cheat or verified concurrent-user analytics.
