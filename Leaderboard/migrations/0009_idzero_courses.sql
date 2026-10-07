-- Add Gunsai/Odawara without copying or deleting existing records/replays.
ALTER TABLE runs_storage ADD COLUMN condition_idzero INTEGER
 CHECK(condition_idzero IS NULL OR condition_idzero BETWEEN 32 AND 35);
DROP TRIGGER runs_insert;
DROP TRIGGER runs_update;
DROP TRIGGER runs_delete;
DROP VIEW runs;

CREATE VIEW runs AS SELECT
 id,device_id,ruleset,epoch,COALESCE(condition_idzero,condition_extended,condition) AS condition,
 weather,car,ticks,glyphs,splits,manual,night,points,build,created_at,
 hidden,reason,imported,replay_size,replay_sha256
FROM runs_storage;

CREATE TRIGGER runs_insert INSTEAD OF INSERT ON runs BEGIN
 INSERT INTO runs_storage(id,device_id,ruleset,epoch,condition,condition_extended,condition_idzero,
  weather,car,ticks,glyphs,splits,manual,night,points,build,created_at,
  hidden,reason,imported,replay_size,replay_sha256)
 VALUES(NEW.id,NEW.device_id,NEW.ruleset,NEW.epoch,
  CASE WHEN NEW.condition>=30 THEN 0 ELSE NEW.condition END,
  CASE WHEN NEW.condition BETWEEN 30 AND 31 THEN NEW.condition ELSE NULL END,
  CASE WHEN NEW.condition>=32 THEN NEW.condition ELSE NULL END,
  NEW.weather,NEW.car,NEW.ticks,NEW.glyphs,NEW.splits,NEW.manual,NEW.night,
  NEW.points,NEW.build,NEW.created_at,COALESCE(NEW.hidden,0),COALESCE(NEW.reason,''),
  COALESCE(NEW.imported,0),COALESCE(NEW.replay_size,0),COALESCE(NEW.replay_sha256,''));
END;

CREATE TRIGGER runs_update INSTEAD OF UPDATE ON runs BEGIN
 UPDATE runs_storage SET id=NEW.id,device_id=NEW.device_id,ruleset=NEW.ruleset,
  epoch=NEW.epoch,condition=CASE WHEN NEW.condition>=30 THEN 0 ELSE NEW.condition END,
  condition_extended=CASE WHEN NEW.condition BETWEEN 30 AND 31 THEN NEW.condition ELSE NULL END,
  condition_idzero=CASE WHEN NEW.condition>=32 THEN NEW.condition ELSE NULL END,
  weather=NEW.weather,car=NEW.car,ticks=NEW.ticks,glyphs=NEW.glyphs,splits=NEW.splits,
  manual=NEW.manual,night=NEW.night,points=NEW.points,build=NEW.build,
  created_at=NEW.created_at,hidden=NEW.hidden,reason=NEW.reason,imported=NEW.imported,
  replay_size=NEW.replay_size,replay_sha256=NEW.replay_sha256
 WHERE id=OLD.id;
END;

CREATE TRIGGER runs_delete INSTEAD OF DELETE ON runs BEGIN
 DELETE FROM runs_storage WHERE id=OLD.id;
END;

DROP INDEX runs_board;
CREATE INDEX runs_board ON runs_storage(ruleset,epoch,
 COALESCE(condition_idzero,condition_extended,condition),weather,hidden,ticks,created_at);
DROP INDEX runs_import_once;
CREATE UNIQUE INDEX runs_import_once ON runs_storage(device_id,
 COALESCE(condition_idzero,condition_extended,condition),weather,car,ticks) WHERE imported=1;
