-- Cache public rankings globally in D1. No scores, identities or bans change.
-- A revision changes in the same transaction as each ranking-affecting write,
-- including writes from an older Worker or the administrator's SQL console.
CREATE TABLE IF NOT EXISTS leaderboard_snapshot_state (
 id INTEGER PRIMARY KEY CHECK(id=1), revision INTEGER NOT NULL
);
INSERT OR IGNORE INTO leaderboard_snapshot_state VALUES(1,0);
CREATE TABLE IF NOT EXISTS leaderboard_snapshots (
 ruleset TEXT NOT NULL, imports INTEGER NOT NULL CHECK(imports IN (0,1)),
 epoch INTEGER NOT NULL, revision INTEGER NOT NULL, entries TEXT NOT NULL,
 PRIMARY KEY(ruleset,imports)
) WITHOUT ROWID;

CREATE TRIGGER IF NOT EXISTS snapshot_runs_insert AFTER INSERT ON runs_storage BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_runs_update AFTER UPDATE ON runs_storage BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_runs_delete AFTER DELETE ON runs_storage BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_devices_insert AFTER INSERT ON devices BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_devices_update AFTER UPDATE OF id,blocked ON devices
WHEN OLD.id<>NEW.id OR OLD.blocked<>NEW.blocked BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_devices_delete AFTER DELETE ON devices BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_epoch_insert AFTER INSERT ON settings WHEN NEW.key='epoch' BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_epoch_update AFTER UPDATE ON settings
WHEN (OLD.key='epoch' OR NEW.key='epoch') AND (OLD.key<>NEW.key OR OLD.value<>NEW.value) BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
CREATE TRIGGER IF NOT EXISTS snapshot_epoch_delete AFTER DELETE ON settings WHEN OLD.key='epoch' BEGIN
 UPDATE leaderboard_snapshot_state SET revision=revision+1 WHERE id=1;
END;
