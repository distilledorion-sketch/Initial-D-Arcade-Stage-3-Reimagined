CREATE TABLE devices (
 id TEXT PRIMARY KEY, token_hash TEXT NOT NULL UNIQUE,
 created_at INTEGER NOT NULL, blocked INTEGER NOT NULL DEFAULT 0 CHECK(blocked IN (0,1))
);
CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT NOT NULL);
INSERT INTO settings VALUES ('epoch','1');
CREATE TABLE runs (
 id TEXT PRIMARY KEY, device_id TEXT NOT NULL REFERENCES devices(id),
 ruleset TEXT NOT NULL, epoch INTEGER NOT NULL, condition INTEGER NOT NULL CHECK(condition BETWEEN 0 AND 21),
 weather INTEGER NOT NULL CHECK(weather IN (0,1)), car INTEGER NOT NULL CHECK(car BETWEEN 0 AND 34),
 ticks INTEGER NOT NULL CHECK(ticks BETWEEN 60000 AND 10799999),
 glyphs TEXT NOT NULL, splits TEXT NOT NULL, manual INTEGER NOT NULL, night INTEGER NOT NULL,
 points INTEGER NOT NULL, build TEXT NOT NULL, created_at INTEGER NOT NULL,
 hidden INTEGER NOT NULL DEFAULT 0 CHECK(hidden IN (0,1)), reason TEXT NOT NULL DEFAULT ''
);
CREATE INDEX runs_board ON runs(ruleset,epoch,condition,weather,hidden,ticks,created_at);
CREATE INDEX runs_device ON runs(device_id,created_at);
CREATE TABLE admin_sessions (token_hash TEXT PRIMARY KEY, expires_at INTEGER NOT NULL);
CREATE TABLE audit (id INTEGER PRIMARY KEY AUTOINCREMENT, created_at INTEGER NOT NULL, action TEXT NOT NULL, target TEXT NOT NULL, reason TEXT NOT NULL);
