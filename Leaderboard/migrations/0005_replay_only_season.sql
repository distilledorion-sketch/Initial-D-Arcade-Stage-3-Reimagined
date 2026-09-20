-- User-requested clean start for public replay downloads (0.3.95).
-- D1 applies a migration transactionally and records it so it runs once.
DELETE FROM replay_chunks;
DELETE FROM replays;
DELETE FROM runs;
UPDATE settings SET value=CAST(value AS INTEGER)+1 WHERE key='epoch';
INSERT INTO audit(created_at,action,target,reason)
VALUES (CAST(strftime('%s','now') AS INTEGER),'delete all records','all boards',
        'Owner requested fresh replay-only rankings; minimum build 0.3.95-community-replays.1.');
