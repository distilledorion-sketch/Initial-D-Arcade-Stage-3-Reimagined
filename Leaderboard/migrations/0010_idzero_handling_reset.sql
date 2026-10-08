-- Owner-requested reset for Gunsai/Odawara's .45 handling change.
-- Before applying: enable MAINTENANCE, export the database, and inventory the
-- target replay keys. Reopen uploads only with REQUIRED_CLIENT_BUILD set to .45.
-- D1 applies this migration atomically. The marker also makes a manual retry
-- harmless to scores recorded after the first successful reset.

-- A .44 upload may have passed the old Worker's build check before maintenance
-- and still be waiting on R2. Reject that delayed insert at the commit boundary.
-- These two courses first shipped in .44; later builds and other courses are
-- unaffected. Keep the guard when recovering historical leaderboard data.
CREATE TRIGGER IF NOT EXISTS runs_idzero_old_handling_guard
BEFORE INSERT ON runs_storage
WHEN NEW.condition_idzero BETWEEN 32 AND 35
 AND NEW.build='0.3.95-community-replays.44'
BEGIN
 SELECT RAISE(ABORT,'Gunsai and Odawara require post-reset handling');
END;

-- Delete through the view so runs_replay_cleanup removes D1 replay bytes and
-- mappings and queues both explicit and legacy R2 keys for durable cleanup.
-- No weather, model, visibility, ruleset or season filter: clear both courses.
DELETE FROM runs
WHERE condition BETWEEN 32 AND 35
 AND NOT EXISTS (SELECT 1 FROM settings WHERE key='idzero_handling_reset_45');

INSERT INTO audit(created_at,action,target,reason)
SELECT CAST(strftime('%s','now') AS INTEGER),'reset course records','courses 16,17',
 'Owner requested Gunsai and Odawara rankings/replays cleared for .45 handling; other courses and season preserved.'
WHERE NOT EXISTS (SELECT 1 FROM settings WHERE key='idzero_handling_reset_45');

INSERT OR IGNORE INTO settings(key,value)
VALUES ('idzero_handling_reset_45','completed');
