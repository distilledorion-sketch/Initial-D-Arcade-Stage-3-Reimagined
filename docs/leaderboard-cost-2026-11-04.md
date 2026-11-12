# Leaderboard D1 read costs — November 4, 2026

The owner reported a $66 charge attributed to D1 rows read. The amount and
billing-period boundaries were not independently verified. Authenticated
Wrangler metrics identified the production database and expensive queries.

## Before

The initial rolling 24-hour database report showed 4,653,412,576 rows read,
287,888 read queries, 54,334 write queries and 81,850 rows written. Storage was
7,839,744 bytes. The one-day query breakdown showed:

| Query | Executions | Average rows read | Total rows read |
| --- | ---: | ---: | ---: |
| Public full-ranking snapshot | 41,910 | 106,530 | 4,464,688,731 |
| Global retention pruning | 468 | 204,031 | 95,486,699 |
| Individual website board | 3,122 | 387 | 1,208,700 |

Metrics were sampled moments apart and may be adaptive estimates. Full
snapshot rebuilding accounted for approximately 96% of the database reads.
The game requests a snapshot roughly once per minute; formerly each request
reran all ranking window functions even when scores had not changed.

## Deployed change

Migration `0014_leaderboard_snapshot_cache.sql` adds two derived-cache tables
and invalidation triggers. It does not delete or modify scores, player
identities, bans or seasons. The migration was applied separately and recorded
in `d1_migrations`; no historical migrations were rerun.

The Worker caches public ranking entries in D1, shared across geographic
edges, rather than relying on a short-lived per-edge cache. Score writes,
installation bans/unbans and season changes increment a revision in the same
transaction. Only cache entries matching the current revision and season are
usable. Cold builds read rankings and revision in one transaction and publish
only if the revision still matches. Concurrent requests in the same isolate
can share a build only for the same observed revision.

The public response's build/replay policy is still taken from the current
environment. v1/v2 replay-availability compatibility is unchanged. There is
no time-based delay before a committed ban or score change invalidates cached
results. Online moderation checks are separate and remain uncached.

This change deliberately leaves the much smaller retention and individual
board query costs unchanged. No game update or download is required.

## Verification

- All 130 local backend tests pass, including populated migration preservation,
  season reset, bans/unbans, score hiding/restoration/deletion, import/ruleset
  isolation, concurrency around bans/publication, retry after failure, and
  maintenance/rate limits with a warm cache.
- 1,000 sequential local snapshot reads cause one ranking calculation.
- Production settings, database/bucket bindings and required build `.53`
  were checked before deployment; the existing service policy was retained.
- Deployed Worker version: `f380b53e-7b53-47b2-9734-012e8d33bf05`.
- Post-deployment cache lookup reported **3 rows read, 0 rows written**.
  The cached public JSON was 756,565 bytes. All 2,055 published entries
  matched exactly across rollout; health and season were unchanged.
- Both snapshot protocol versions still return their required replay flags.

Private verification files are under `Verification/leaderboard-cost-20261104/`.
They include baseline insights, schema/settings, migration receipts, test and
deployment logs, public snapshots and measured cache-hit metadata. Rollback
can restore the previously recorded Worker version while leaving the additive
cache schema in place; old Workers ignore it.

## Billing limits of this result

This is a measured reduction in one request's database reads, not a measured
monthly bill reduction. Cold rebuilds, actual score changes and remaining
queries still incur reads. Observe the next 24 hours before reporting an
overall percentage or dollar savings. Already accrued usage is unaffected.

At the time of this change, Cloudflare's paid D1 pricing includes 25 billion
rows read per billing month, then charges $0.001 per million extra rows.
Included usage resets on the subscription renewal date, not necessarily the
first day of the calendar month. A future full cycle could fit the included
allowance if traffic and score-changing writes stay near the observed level;
this is an expectation, not a verified bill.

Sources: [D1 pricing](https://developers.cloudflare.com/d1/platform/pricing/)
and [finding expensive queries](https://developers.cloudflare.com/d1/observability/find-expensive-queries/).

## November 10 follow-up

Authenticated production metrics now show 149,318,375 rows read in the last
24 hours, compared with the initial 4,653,412,576: a **96.8% decrease** between
the two sampled daily windows. Read-query count was 277,712 versus 287,888.
These are different traffic windows, not a controlled benchmark.

The cache lookup ran 46,041 times at 3 rows per request. Full snapshot rebuilds
ran 407 times, reading 44,565,052 rows, versus the original 41,910 rebuilds and
4,464,688,731 rows. Retention pruning is now the largest individual query
category at 72,372,006 rows per day. It remains unchanged because current
overall usage is already comfortably inside the paid plan's included reads.

At the observed rate, 30 days would use approximately 4.48 billion reads,
below the 25 billion included allowance. This projects **$0 in additional D1
read charges for a future full billing cycle** if usage stays similar; it
does not remove accrued charges, verify the current invoice, or include
Workers subscriptions and other services.

The deployed cache version `f380b53e-7b53-47b2-9734-012e8d33bf05` still serves
100% of traffic. Health confirms the service is online, required build is
still `.53`, and replay uploads/downloads remain disabled. No additional
production change was made during this follow-up. Evidence is in
`Verification/leaderboard-cost-20261110/`.
