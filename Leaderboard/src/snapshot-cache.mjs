import {rankedSql,publicRun} from './core.mjs';

// Shared across all edges through D1, rather than one full-table scan per
// polling player. Never cache authentication or moderation decisions.
export const cachedSnapshotSql=`SELECT s.revision,CAST(e.value AS INTEGER) AS epoch,c.entries
 FROM leaderboard_snapshot_state s JOIN settings e ON e.key='epoch'
 LEFT JOIN leaderboard_snapshots c ON c.ruleset=? AND c.imports=? AND c.revision=s.revision
 AND c.epoch=CAST(e.value AS INTEGER) WHERE s.id=1`;
const metadataSql=`SELECT s.revision,CAST(e.value AS INTEGER) AS epoch
 FROM leaderboard_snapshot_state s JOIN settings e ON e.key='epoch' WHERE s.id=1`;
// Both statements in the build batch observe the same SQLite transaction.
const buildSql=rankedSql.replace('r.epoch=?',"r.epoch=CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)");
const publishSql=`INSERT INTO leaderboard_snapshots(ruleset,imports,epoch,revision,entries)
 SELECT ?,?,?,?,? WHERE EXISTS(SELECT 1 FROM leaderboard_snapshot_state WHERE id=1 AND revision=?)
 ON CONFLICT(ruleset,imports) DO UPDATE SET epoch=excluded.epoch,revision=excluded.revision,entries=excluded.entries`;
const builds=new WeakMap();

async function build(env,imports){
 const [metadata,ranking]=await env.DB.batch([
  env.DB.prepare(metadataSql),env.DB.prepare(buildSql).bind(env.RULESET,imports)
 ]);
 const state=metadata.results[0];
 if(!state)throw new Error('Leaderboard state unavailable.');
 const entries=ranking.results.map(publicRun);
 // A concurrent submission/ban/reset must never publish an older snapshot
 // under the new revision. The in-flight response reflects its read transaction.
 await env.DB.prepare(publishSql).bind(env.RULESET,imports,state.epoch,state.revision,
  JSON.stringify(entries),state.revision).run();
 return {epoch:state.epoch,entries};
}

export async function readSnapshot(env,imports){
 const cached=await env.DB.prepare(cachedSnapshotSql).bind(env.RULESET,imports).first();
 if(!cached)throw new Error('Leaderboard state unavailable.');
 if(cached.entries!==null)return {epoch:cached.epoch,entries:JSON.parse(cached.entries)};
 let pending=builds.get(env.DB);
 if(!pending){pending=new Map();builds.set(env.DB,pending);}
 const key=JSON.stringify([env.RULESET,imports,cached.revision]);
 // Coalesce simultaneous cold requests within an isolate. Other isolates use
 // the same persisted snapshot after publication; failures are never retained.
 if(!pending.has(key)){
  const promise=build(env,imports).finally(()=>pending.delete(key));
  pending.set(key,promise);
 }
 return pending.get(key);
}
