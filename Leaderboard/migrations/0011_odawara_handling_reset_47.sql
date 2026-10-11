-- Owner-requested Odawara-only reset for the .47 handling change.
-- Before applying: enable MAINTENANCE, export/verify D1, and back up/verify all
-- target replay objects. Apply only this migration, then reopen with exact
-- REQUIRED_CLIENT_BUILD .47. Do not rerun a historical season/course reset.

-- An old request may already be writing R2 when maintenance begins. Reject
-- its score at the commit boundary. The numeric suffix deliberately accepts
-- subsequent releases in this build family; a new family needs a reviewed
-- guard migration as well as the Worker's exact-version policy update.
CREATE TRIGGER IF NOT EXISTS runs_odawara_pre47_guard
BEFORE INSERT ON runs_storage
WHEN NEW.condition_idzero IN (34,35)
 AND NOT (
  substr(NEW.build,1,length('0.3.95-community-replays.'))='0.3.95-community-replays.'
  AND length(substr(NEW.build,length('0.3.95-community-replays.')+1))>0
  AND substr(NEW.build,length('0.3.95-community-replays.')+1) NOT GLOB '*[^0-9]*'
  AND substr(NEW.build,length('0.3.95-community-replays.')+1,1)<>'0'
  AND CAST(substr(NEW.build,length('0.3.95-community-replays.')+1) AS INTEGER)>=47
 )
BEGIN
 SELECT RAISE(ABORT,'Odawara requires post-reset .47 handling');
END;

-- Delete through the writable view so runs_replay_cleanup removes replay
-- mappings and D1 bytes, and durably queues explicit and legacy R2 keys.
-- Include every model, weather, visibility, ruleset and season for Odawara.
DELETE FROM runs
WHERE condition IN (34,35)
 AND NOT EXISTS (SELECT 1 FROM settings WHERE key='odawara_handling_reset_47');

INSERT INTO audit(created_at,action,target,reason)
SELECT CAST(strftime('%s','now') AS INTEGER),'reset course records','course 17',
 'Owner requested all Odawara rankings and online replays cleared for .47 handling; other courses and season preserved.'
WHERE NOT EXISTS (SELECT 1 FROM settings WHERE key='odawara_handling_reset_47');

INSERT OR IGNORE INTO settings(key,value)
VALUES ('odawara_handling_reset_47','completed');
