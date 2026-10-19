-- Owner-requested replay removal. Scores, seasons, identities and moderation
-- remain intact. Deploy DISABLE_REPLAYS=true before applying this migration.
-- The durable queue plus the Worker's prefix sweep removes R2 recordings.
INSERT OR IGNORE INTO replay_object_deletions(object_key,created_at)
SELECT object_key,CAST(strftime('%s','now') AS INTEGER) FROM replay_objects;
INSERT OR IGNORE INTO replay_object_deletions(object_key,created_at)
SELECT 'replays/v1/'||device_id||'/'||id||'/'||replay_sha256||'/'||replay_size||'.idr',
 CAST(strftime('%s','now') AS INTEGER)
FROM runs WHERE replay_size>0 AND length(replay_sha256)=64;

DELETE FROM replay_objects;
DELETE FROM replay_chunks;
DELETE FROM replays;
UPDATE runs_storage SET replay_size=0,replay_sha256=''
WHERE replay_size<>0 OR replay_sha256<>'';

-- Reject commits from old Worker requests that were already in flight during
-- the cutover, including replay migrations. Their unique R2 keys are swept.
CREATE TRIGGER IF NOT EXISTS replay_disabled_runs_insert BEFORE INSERT ON runs_storage
WHEN NEW.replay_size<>0 OR NEW.replay_sha256<>'' BEGIN
 SELECT RAISE(ABORT,'Leaderboard replay storage disabled');
END;
CREATE TRIGGER IF NOT EXISTS replay_disabled_runs_update BEFORE UPDATE OF replay_size,replay_sha256 ON runs_storage
WHEN NEW.replay_size<>0 OR NEW.replay_sha256<>'' BEGIN
 SELECT RAISE(ABORT,'Leaderboard replay storage disabled');
END;
CREATE TRIGGER IF NOT EXISTS replay_disabled_objects BEFORE INSERT ON replay_objects BEGIN
 SELECT RAISE(ABORT,'Leaderboard replay storage disabled');
END;
CREATE TRIGGER IF NOT EXISTS replay_disabled_chunks BEFORE INSERT ON replay_chunks BEGIN
 SELECT RAISE(ABORT,'Leaderboard replay storage disabled');
END;
CREATE TRIGGER IF NOT EXISTS replay_disabled_blobs BEFORE INSERT ON replays BEGIN
 SELECT RAISE(ABORT,'Leaderboard replay storage disabled');
END;

INSERT INTO audit(created_at,action,target,reason)
SELECT CAST(strftime('%s','now') AS INTEGER),'disable leaderboard replays','all leaderboard replays',
 'Owner requested replay uploads/downloads disabled and all recordings deleted; preserve posted times.'
WHERE NOT EXISTS(SELECT 1 FROM settings WHERE key='leaderboard_replays_disabled');
INSERT OR IGNORE INTO settings(key,value) VALUES('leaderboard_replays_disabled','true');
