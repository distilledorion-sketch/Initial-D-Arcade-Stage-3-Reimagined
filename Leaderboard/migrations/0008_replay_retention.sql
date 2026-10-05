-- Score retention is enabled by the Worker configuration, not by this migration.
-- Existing scores and blobs are unchanged until that policy is explicitly run.
CREATE TABLE replay_objects (
 run_id TEXT PRIMARY KEY REFERENCES runs_storage(id) ON DELETE CASCADE,
 object_key TEXT NOT NULL UNIQUE
);
CREATE TABLE replay_object_deletions (
 object_key TEXT PRIMARY KEY,
 created_at INTEGER NOT NULL
);
CREATE TRIGGER runs_replay_cleanup BEFORE DELETE ON runs_storage BEGIN
 INSERT OR IGNORE INTO replay_object_deletions(object_key,created_at)
 SELECT COALESCE((SELECT object_key FROM replay_objects WHERE run_id=OLD.id),
  'replays/v1/'||OLD.device_id||'/'||OLD.id||'/'||OLD.replay_sha256||'/'||OLD.replay_size||'.idr'),
  CAST(strftime('%s','now') AS INTEGER)
 WHERE OLD.replay_size>0 AND length(OLD.replay_sha256)=64;
 DELETE FROM replays WHERE run_id=OLD.id;
 DELETE FROM replay_chunks WHERE run_id=OLD.id;
 DELETE FROM replay_objects WHERE run_id=OLD.id;
END;
