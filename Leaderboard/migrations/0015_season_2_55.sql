-- Public Season 2 uses internal epoch 3. Epoch 2 was already used by an older
-- reset; never reuse it. Apply ONLY this migration to the existing database
-- after exporting/verifying it and closing score submissions in the Worker.
-- Keep this transaction closed to score writes until the .55 release is ready.
-- No score, installation, ban, admin session or online-player row is deleted.

CREATE TABLE IF NOT EXISTS leaderboard_season_cutovers (
 name TEXT PRIMARY KEY,
 previous_epoch INTEGER NOT NULL,
 new_epoch INTEGER NOT NULL CHECK(new_epoch>previous_epoch),
 created_at INTEGER NOT NULL
);
-- Abort, rather than reuse a namespace, if unexpected epoch>=3 data exists.
-- The completion marker makes accidental reapplication preserve new records.
INSERT INTO leaderboard_season_cutovers(name,previous_epoch,new_epoch,created_at)
SELECT 'season_2_55',MAX(
 CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER),
 COALESCE((SELECT MAX(epoch) FROM runs_storage),0)),3,
 CAST(strftime('%s','now') AS INTEGER)
WHERE NOT EXISTS(SELECT 1 FROM leaderboard_season_cutovers WHERE name='season_2_55');

UPDATE settings SET value='3' WHERE key='epoch'
 AND NOT EXISTS(SELECT 1 FROM settings WHERE key='season_2_55_completed');
INSERT INTO settings(key,value) SELECT 'leaderboard_required_build','0.3.95-community-replays.55'
 WHERE NOT EXISTS(SELECT 1 FROM settings WHERE key='season_2_55_completed')
 ON CONFLICT(key) DO UPDATE SET value=excluded.value;
INSERT INTO settings(key,value) SELECT 'leaderboard_submissions_closed','true'
 WHERE NOT EXISTS(SELECT 1 FROM settings WHERE key='season_2_55_completed')
 ON CONFLICT(key) DO UPDATE SET value=excluded.value;

-- Commit-boundary checks cover old Workers that passed request validation
-- before cutover, direct backing-table inserts and legacy upload routes.
CREATE TRIGGER IF NOT EXISTS leaderboard_submission_closed_guard BEFORE INSERT ON runs_storage
WHEN (SELECT value FROM settings WHERE key='leaderboard_submissions_closed')='true'
BEGIN SELECT RAISE(ABORT,'Leaderboard submissions closed'); END;
CREATE TRIGGER IF NOT EXISTS leaderboard_season_guard BEFORE INSERT ON runs_storage
WHEN NEW.epoch<>CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)
BEGIN SELECT RAISE(ABORT,'Leaderboard season closed'); END;
CREATE TRIGGER IF NOT EXISTS leaderboard_build_guard BEFORE INSERT ON runs_storage
WHEN NEW.build<>(SELECT value FROM settings WHERE key='leaderboard_required_build')
BEGIN SELECT RAISE(ABORT,'Leaderboard client build required'); END;

-- Past rows remain readable in administration. Ignore old retention deletes
-- per-row so an already-running older cleanup cannot erase the archive.
CREATE TRIGGER IF NOT EXISTS leaderboard_archive_delete_guard BEFORE DELETE ON runs_storage
WHEN OLD.epoch<>CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)
BEGIN SELECT RAISE(IGNORE); END;
CREATE TRIGGER IF NOT EXISTS leaderboard_archive_update_guard BEFORE UPDATE ON runs_storage
WHEN OLD.epoch<>CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)
 OR NEW.epoch<>OLD.epoch
BEGIN SELECT RAISE(ABORT,'Archived leaderboard season is read-only'); END;

-- SQLite does not specify the ordering of BEFORE triggers. Even if cleanup
-- runs before the archive delete guard, it must not change archived metadata.
DROP TRIGGER IF EXISTS runs_replay_cleanup;
CREATE TRIGGER runs_replay_cleanup BEFORE DELETE ON runs_storage
WHEN OLD.epoch=CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)
BEGIN
 INSERT OR IGNORE INTO replay_object_deletions(object_key,created_at)
 SELECT COALESCE((SELECT object_key FROM replay_objects WHERE run_id=OLD.id),
  'replays/v1/'||OLD.device_id||'/'||OLD.id||'/'||OLD.replay_sha256||'/'||OLD.replay_size||'.idr'),
  CAST(strftime('%s','now') AS INTEGER)
 WHERE OLD.replay_size>0 AND length(OLD.replay_sha256)=64;
 DELETE FROM replays WHERE run_id=OLD.id;
 DELETE FROM replay_chunks WHERE run_id=OLD.id;
 DELETE FROM replay_objects WHERE run_id=OLD.id;
END;

INSERT INTO audit(created_at,action,target,reason)
SELECT CAST(strftime('%s','now') AS INTEGER),'start Season 2','internal epoch 3',
 'Owner requested fresh Season 2 for .55; previous scores preserved as a read-only archive; score writes remain closed until release.'
WHERE NOT EXISTS(SELECT 1 FROM settings WHERE key='season_2_55_completed');
INSERT OR IGNORE INTO settings(key,value) VALUES('season_2_55_completed','3');
