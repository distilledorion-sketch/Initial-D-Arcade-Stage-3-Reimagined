# Community leaderboard service

Cloudflare Workers + D1 backend for Time Attack rankings, replay downloads and moderation. The game already points at the public community service; players do not need to deploy this directory.

## Local tests

Use Node.js with `node:sqlite` support (Node 22.13+):

```powershell
node --test test/worker.test.mjs
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

- New runs require a matching detailed replay, current season and supported game build.
- Historical-time imports and replay-less uploads are rejected.
- Public downloads contain the replay and race metadata, not credentials or moderation fields.
- Moderation supports hiding/restoring runs and blocking/unblocking installations.
- IP addresses are used for rate limiting, not stored with submissions. Worker observability is disabled in the supplied configuration.
- Client-reported telemetry and build versions are not authoritative anti-cheat verification.

The service has no Steam-account linking requirement. Personal replay archives are separate from the Time Attack upload queue and are not uploaded automatically.
