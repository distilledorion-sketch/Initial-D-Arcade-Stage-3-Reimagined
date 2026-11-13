-- Release .56 keeps Season 2 / epoch 3 and every existing score.
-- Apply after publishing .56 and deploying its matching Worker configuration.
-- Reapplication cannot downgrade a later required build or reset a season.
UPDATE settings SET value='0.3.95-community-replays.56'
WHERE key='leaderboard_required_build' AND value='0.3.95-community-replays.55';
