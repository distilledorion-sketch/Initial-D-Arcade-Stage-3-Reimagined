-- Detailed 60 Hz captures may exceed D1's per-row limit. Keep one atomic
-- transaction for the run and all losslessly compressed replay chunks.
CREATE TABLE replay_chunks (
  run_id TEXT NOT NULL REFERENCES runs(id) ON DELETE CASCADE,
  part INTEGER NOT NULL CHECK(part >= 0),
  data BLOB NOT NULL,
  PRIMARY KEY(run_id, part)
);
