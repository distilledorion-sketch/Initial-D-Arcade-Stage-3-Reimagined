// Ten overall entries per course/direction/weather board. Match the public
// board's existing one-best-time-per-installation-and-car rule before ranking.
export const retainedSql=`WITH personal AS (
 SELECT r.id,r.condition,r.weather,r.ticks,r.created_at,
 ROW_NUMBER() OVER(PARTITION BY r.device_id,r.condition,r.weather,r.car ORDER BY r.ticks,r.created_at,r.id) AS personal_rank
 FROM runs r JOIN devices d ON d.id=r.device_id
 WHERE r.ruleset=? AND r.epoch=CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)
 AND r.hidden=0 AND d.blocked=0
), ranked AS (
 SELECT id,ROW_NUMBER() OVER(PARTITION BY condition,weather ORDER BY ticks,created_at,id) AS rank
 FROM personal WHERE personal_rank=1
) SELECT id FROM ranked WHERE rank<=10`;

export const retentionEnabled=env=>String(env.RETAIN_TOP_TEN)==='true';
export function pruneStatement(env,limit=0){
 if(!Number.isInteger(limit)||limit<0)throw Error('Invalid retention batch size.');
 return env.DB.prepare(`DELETE FROM runs WHERE id IN (SELECT id FROM runs WHERE id NOT IN (${retainedSql})${limit?' LIMIT '+limit:''})`).bind(env.RULESET);
}

export async function qualifiesForBoard(env,x,device,createdAt){
 if(!retentionEnabled(env))return true;
 // Do not persist a replay which already misses the cutoff. Recheck by pruning
 // in the insert transaction: concurrent faster submissions may change it.
 const best=await env.DB.prepare('SELECT ticks FROM runs WHERE device_id=? AND ruleset=? AND epoch=? AND condition=? AND weather=? AND car=? AND hidden=0 ORDER BY ticks,created_at,id LIMIT 1').bind(device,env.RULESET,x.epoch,x.condition,x.weather,x.car).first();
 if(best&&best.ticks<=x.ticks6000)return false;
 const ahead=await env.DB.prepare(`WITH personal AS (SELECT r.*,
 ROW_NUMBER() OVER(PARTITION BY r.device_id,r.car ORDER BY r.ticks,r.created_at,r.id) AS personal_rank
 FROM runs r JOIN devices d ON d.id=r.device_id
 WHERE r.ruleset=? AND r.epoch=? AND r.condition=? AND r.weather=? AND r.hidden=0 AND d.blocked=0
 ) SELECT COUNT(*) AS n FROM personal r WHERE personal_rank=1 AND NOT (r.device_id=? AND r.car=?)
 AND (r.ticks<? OR (r.ticks=? AND (r.created_at<? OR (r.created_at=? AND r.id<?))))`).bind(env.RULESET,x.epoch,x.condition,x.weather,device,x.car,x.ticks6000,x.ticks6000,createdAt,createdAt,x.id).first();
 return ahead.n<10;
}

export async function cleanupReplayObjects(env,limit=50){
 if(!env.REPLAYS)return {removed:0};
 const rows=(await env.DB.prepare('SELECT object_key FROM replay_object_deletions ORDER BY created_at,object_key LIMIT ?').bind(limit).all()).results;
 if(!rows.length)return {removed:0};
 // These are object keys of deleted scores or failed unique upload attempts.
 // New attempts get new keys, so delayed cleanup cannot remove a newer replay.
 await env.REPLAYS.delete(rows.map(x=>x.object_key));
 await env.DB.batch(rows.map(x=>env.DB.prepare('DELETE FROM replay_object_deletions WHERE object_key=?').bind(x.object_key)));
 return {removed:rows.length};
}
