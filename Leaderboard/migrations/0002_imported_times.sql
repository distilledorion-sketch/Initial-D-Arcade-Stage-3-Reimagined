ALTER TABLE runs ADD COLUMN imported INTEGER NOT NULL DEFAULT 0 CHECK(imported IN (0,1));
CREATE UNIQUE INDEX runs_import_once ON runs(device_id,condition,weather,car,ticks) WHERE imported=1;
