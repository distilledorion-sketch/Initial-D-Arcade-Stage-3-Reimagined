# Time Attack uploads blocked by replay storage

The production leaderboard database reached 9,999,826,944 bytes, just below
D1's fixed 10 GB per-database limit. Replay blobs occupied almost all of that
space. The last stored run before the repair was October 3 at 13:53:19 UTC;
both the website and the game's snapshot endpoint were serving the same old
committed scores. There is no daily approval or publishing job. Failed uploads
remain pending in the game and retry while it is running.

The Worker now stores new compressed replays in a private R2 Standard bucket
and commits only score metadata to D1. It acknowledges a retained run after the
object write and SQL insert succeed. Retries are idempotent, and an interrupted
SQL insert never removes an object that a simultaneous retry might have used.
Object keys distinguish installation, run, expanded replay hash, stored
length and upload attempt. Public downloads still check season, ruleset,
moderation and bans. A unique object mapping and durable deletion queue prevent
delayed cleanup from deleting another upload attempt's committed replay.

The initial cleanup kept only the overall top ten across all cars and removed
model records outside those ten. The user subsequently confirmed that model
boards must also retain their own top tens. The corrected retention rule keeps
the overall top ten **and each car model's top ten** for every
course/direction/weather board. Ranking continues
to use one best time per installation/car. Future valid runs are still
submitted and compared: slower runs receive HTTP 200 with `retained: false`
without storing their replay; an entry is removed only when neither the overall
nor its model board retains it. The
cutoff is checked again in the score-insert transaction for concurrent races.
Displaced scores, duplicate old personal bests, previous seasons and ineligible
hidden/blocked entries are permanently removed, including their remote replay.
There is no leaderboard restore operation for a pruned score. Local game saves
and personal replay files are not touched.

Existing recordings are transferred individually through the authenticated
admin maintenance endpoint. Each transfer verifies the expanded source hash,
uses a checksum-protected object write, and reads back the entire object before
removing the legacy blob. The transaction records an audit receipt without
changing the run row. Legacy storage remains readable during the transfer.

The initial storage deployment was `79befbc4-bdb6-44e5-a566-8c71a9fda63d`. A real
`.41` client run was accepted at October 5, 01:42:50 UTC (October 4, 8:42:50 PM
Central), then verified on the website board and the game's snapshot. Its
downloaded replay passed the same detailed validation as the upload. No
synthetic scores were submitted to production.

This is a server-only repair. Required game build `.41`, season 2, qualifying
scores and game files are unchanged. Pending runs must still meet the exact
current-build policy; this repair does not reauthorize older builds.

Verification: 58 backend tests pass, including both storage formats,
immediate ranking visibility, failed object writes, SQL failures, duplicate
IDs, access rules, migration readback failures, safe retries, future qualifying
and nonqualifying uploads, transactional cutoff changes, per-board separation
and permanently displaced replay downloads. Local
production proof is in `Verification/leaderboard-storage-20261004/`, including
the before-deployment row hashes, replay inventory, individual transfer
receipts and public download checks.

Future deployments must retain the `REPLAYS` binding. The production config
for this deployment is
`Verification/leaderboard-storage-20261004/wrangler.production.jsonc`.
Do not reuse an older release's configuration without adding that binding and
the `RETAIN_TOP_TEN` policy. Migration `0008_replay_retention.sql` adds mappings,
a deletion queue and cleanup triggers. It does not itself delete data.
Initial pruning uses explicit authenticated maintenance requests in bounded
batches. Do not rerun any older migrations or reset the season. Failed object
deletions retry every five minutes without delaying an accepted score.

The initial overall-only verification passed at October 4, 9:07 PM Central. Deployment
`fcc21738-7e9c-4d22-906f-81deb8de513c` has both R2 and ongoing retention enabled.
There were 586 retained scores across 62 nonempty boards, no board above ten,
and 586 matching replay objects totaling 493,506,354 bytes. D1 occupied
1,576,960 bytes, with no legacy replay blobs or pending object deletions.
Every retained old score's row hash matched the pre-repair baseline. The
game's snapshot contained the same 586 IDs. Public downloads of migrated
replays and a new real `.41` submission passed full replay validation.

## Model records recovery

The overall-only cleanup removed car records which were slower than a course's
overall ten. After the user confirmed that each car must retain a separate top
ten, the service was changed to retain the union of overall and model top tens.
The overall board still returns ten; selecting a car returns that car's ten.
The game snapshot preserves its existing overall-ten/model-best format.

Recovery first copied and verified every table in the current database during
a brief retryable write pause, then moved the live service to the verified
copy. Only the detached historical database was restored. Its 11,558 score
rows matched every saved pre-cleanup row hash, and all 12,482 replay chunks were
present. This recovered 3,314 qualifying model records with unchanged score
metadata, verified original replay hashes and complete R2 readbacks. Live
uploads continued against the new primary database during recovery.

An additional restore point immediately before pruning contained seven later
submissions. Four had been superseded; three still qualified and were restored
from their accepted original score rows. Their R2 objects had already been
deleted and were absent from the historical database. Those three rows retain
their original replay digests but have zero stored replay bytes, so the site
shows no replay download. Each exception is recorded in the audit log. New
submissions still require and validate a complete replay.

At October 4, 9:49 PM Central, production contained 3,906 scores and exactly
3,903 replay objects across 62 overall boards and 1,078 nonempty model boards.
Every model had at most ten entries, and comparison against historical plus
current candidates found no missing qualifying score. The compact game
snapshot matched the expected 1,295 entries. Ten public board checks, three
full recovered replay downloads and all three unavailable-replay displays
passed. Original times, drivers and race metadata remained unchanged.
Storage was 3,390,448,892 bytes in R2 plus 5,197,824 bytes in the live D1 database.
The temporary recovery worker was removed. The detached original database was
returned to its compact, pre-recovery state (1,581,056 bytes, zero replay blobs),
so the temporary multi-gigabyte historical copy does not remain allocated.

The source changes pass 64 tests covering model retention outside the overall
top ten, future submissions, replay cleanup, copy failures, retryable
maintenance, recovery integrity and preservation of current moderation.
Private production/recovery evidence is under
`Verification/leaderboard-model-recovery-20261004/`. The canonical production
configuration remains at the path above, now bound to the verified replacement
database. Do not deploy a stale copy of the original database binding.
