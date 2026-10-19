-- Separate EOS identities from optional Time Attack installation identities.
-- Additive only: existing scores, ghosts, installation bans and sessions survive.
CREATE TABLE online_players (
 player_id TEXT PRIMARY KEY CHECK(length(player_id)=32 AND player_id NOT GLOB '*[^a-f0-9]*'),
 username TEXT NOT NULL,
 first_seen INTEGER NOT NULL,
 last_seen INTEGER NOT NULL,
 build TEXT NOT NULL,
 blocked INTEGER NOT NULL DEFAULT 0 CHECK(blocked IN (0,1)),
 ban_reason TEXT NOT NULL DEFAULT '',
 banned_at INTEGER,
 CHECK((blocked=0 AND banned_at IS NULL) OR (blocked=1 AND banned_at IS NOT NULL))
);
CREATE INDEX online_players_last_seen ON online_players(last_seen DESC,player_id);
CREATE TABLE online_player_names (
 player_id TEXT NOT NULL REFERENCES online_players(player_id),
 username TEXT NOT NULL,
 name_key TEXT NOT NULL,
 first_seen INTEGER NOT NULL,
 last_seen INTEGER NOT NULL,
 PRIMARY KEY(player_id,username)
);
-- Binary range lookup uses a normalized Unicode uppercase key, not SQLite's
-- ASCII-only NOCASE. Names are deliberately NOT unique across players.
CREATE INDEX online_player_names_search ON online_player_names(name_key,player_id);
CREATE INDEX online_player_names_history ON online_player_names(player_id,last_seen DESC,username);
CREATE INDEX online_player_names_created ON online_player_names(player_id,first_seen);
-- Enforce the rolling limit inside the transaction, including simultaneous
-- requests. Reusing a previous name never consumes another history slot.
CREATE TRIGGER online_player_names_daily_limit BEFORE INSERT ON online_player_names
WHEN NOT EXISTS (SELECT 1 FROM online_player_names WHERE player_id=NEW.player_id AND username=NEW.username)
 AND (SELECT COUNT(*) FROM online_player_names WHERE player_id=NEW.player_id AND first_seen>NEW.first_seen-86400)>=24
BEGIN
 SELECT RAISE(ABORT,'online_name_daily_limit');
END;
