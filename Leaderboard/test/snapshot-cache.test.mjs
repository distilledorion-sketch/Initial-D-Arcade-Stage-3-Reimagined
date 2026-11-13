import {test,beforeEach,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFileSync,readdirSync} from 'node:fs';
import {readSnapshot,cachedSnapshotSql} from '../src/snapshot-cache.mjs';
import {rankedSql,publicRun,REQUIRED_CLIENT_BUILD} from '../src/core.mjs';

const migration=readFileSync(new URL('../migrations/0014_leaderboard_snapshot_cache.sql',import.meta.url),'utf8');
let db,env,queries,beforePublish;
beforeEach(()=>{
 db=new DatabaseSync(':memory:');db.exec('PRAGMA foreign_keys=ON');
 for(const file of readdirSync(new URL('../migrations/',import.meta.url)).filter(n=>/^\d+.*\.sql$/.test(n)).sort())
  db.exec(readFileSync(new URL('../migrations/'+file,import.meta.url),'utf8'));
 // The rollover intentionally starts closed. These cache tests model the
 // published/open season; dedicated Worker tests exercise the closed gate.
 db.exec("UPDATE settings SET value='false' WHERE key='leaderboard_submissions_closed'");
 queries=[];beforePublish=null;
 const wrap=(sql,args=[])=>({bind(...args){return wrap(sql,args);},
  async first(){queries.push(sql);return db.prepare(sql).get(...args)||null;},
  async run(){
   if(sql.startsWith('INSERT INTO leaderboard_snapshots')&&beforePublish)await beforePublish();
   queries.push(sql);const statement=db.prepare(sql);
   return statement.columns().length?{results:statement.all(...args)}:{meta:statement.run(...args)};
  }});
 env={RULESET:'d3-community-v1',DB:{prepare:wrap,async batch(statements){
  db.exec('BEGIN');try{const result=[];for(const statement of statements)result.push(await statement.run());db.exec('COMMIT');return result;}
  catch(error){db.exec('ROLLBACK');throw error;}
 }}};
 db.exec("INSERT INTO devices VALUES('driver','secret',1,0)");
});
afterEach(()=>db.close());
const epoch=()=>Number(db.prepare("SELECT value FROM settings WHERE key='epoch'").get().value);
const revision=()=>db.prepare('SELECT revision FROM leaderboard_snapshot_state').get().revision;
const builds=()=>queries.filter(sql=>sql.startsWith('WITH personal AS')).length;
function seed(id='score',extra={}){
 const x={condition:0,car:0,ticks:1200000,device:'driver',ruleset:env.RULESET,imported:0,epoch:epoch(),...extra};
 db.prepare(`INSERT INTO runs(id,device_id,ruleset,epoch,condition,weather,car,ticks,glyphs,splits,manual,night,points,build,created_at,imported)
 VALUES(?,?,?,?,?,0,?,?,?, ?,1,0,1000,?,1,?)`).run(id,x.device,x.ruleset,x.epoch,x.condition,x.car,x.ticks,'[162,163,221,221,221]',JSON.stringify([300000,600000,900000,x.ticks]),REQUIRED_CLIENT_BUILD,x.imported);
}
const ids=s=>s.entries.map(x=>x.id);

test('1000 polling requests execute one ranking query and then only indexed cache reads',async()=>{
 seed();const expected=db.prepare(rankedSql).all(env.RULESET,epoch(),1).map(publicRun);
 for(let i=0;i<1000;i++)assert.deepEqual((await readSnapshot(env,1)).entries,expected);
 assert.equal(builds(),1);assert.equal(queries.filter(q=>q===cachedSnapshotSql).length,1000);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,1);
 const plan=db.prepare('EXPLAIN QUERY PLAN '+cachedSnapshotSql).all(env.RULESET,1).map(x=>x.detail).join('\n');
 assert.match(plan,/SEARCH c USING PRIMARY KEY/);assert.doesNotMatch(plan,/runs_storage|SCAN c/);
});

test('cold concurrent requests share one rebuild, empty snapshots also cache',async()=>{
 const results=await Promise.all(Array.from({length:50},()=>readSnapshot(env,0)));
 assert.ok(results.every(x=>x.entries.length===0));assert.equal(builds(),1);
 await readSnapshot(env,0);assert.equal(builds(),1);
});

test('new finishes, improved times, hiding, restoring and deleting scores invalidate immediately',async()=>{
 seed();await readSnapshot(env,1);let count=1;
 for(const change of [
  ()=>seed('second',{car:1}),
  ()=>db.exec("UPDATE runs SET ticks=1100000 WHERE id='score'"),
  ()=>db.exec("UPDATE runs SET hidden=1 WHERE id='score'"),
  ()=>db.exec("UPDATE runs SET hidden=0 WHERE id='score'"),
  ()=>db.exec("DELETE FROM runs WHERE id='score'")
 ]){
  change();const expected=db.prepare(rankedSql).all(env.RULESET,epoch(),1).map(publicRun);
  assert.deepEqual((await readSnapshot(env,1)).entries,expected);assert.equal(builds(),++count);
 }
});

test('blocking and unblocking an installation changes the next snapshot without expiry delay',async()=>{
 seed();assert.deepEqual(ids(await readSnapshot(env,1)),['score']);
 db.exec("UPDATE devices SET blocked=1 WHERE id='driver'");assert.deepEqual(ids(await readSnapshot(env,1)),[]);
 db.exec("UPDATE devices SET blocked=0 WHERE id='driver'");assert.deepEqual(ids(await readSnapshot(env,1)),['score']);
});

test('season reset cannot reuse an old-season snapshot',async()=>{
 seed();const old=await readSnapshot(env,1);
 db.exec("UPDATE settings SET value=CAST(value AS INTEGER)+1 WHERE key='epoch'");
 const current=await readSnapshot(env,1);assert.equal(current.epoch,old.epoch+1);assert.deepEqual(current.entries,[]);
 seed('new-season');assert.deepEqual(ids(await readSnapshot(env,1)),['new-season']);
});

test('cache keys isolate rulesets and historical-import choices',async()=>{
 seed();seed('historical',{imported:1,car:1});seed('other-ruleset',{ruleset:'another-ruleset',car:2});
 assert.deepEqual(ids(await readSnapshot(env,0)),['score']);
 assert.deepEqual(ids(await readSnapshot(env,1)),['historical','score']);
 const other={...env,RULESET:'another-ruleset'};
 assert.deepEqual(ids(await readSnapshot(other,1)),['other-ruleset']);
 assert.equal(builds(),3);assert.equal(db.prepare('SELECT count(*) n FROM leaderboard_snapshots').get().n,3);
});

test('activity, online directory and admin-session writes do not invalidate public times',async()=>{
 seed();await readSnapshot(env,1);const before=revision();
 db.exec(`INSERT INTO settings VALUES('online_activity_eos','{}');
  UPDATE settings SET value='{"online":3}' WHERE key='online_activity_eos';
  INSERT INTO admin_sessions VALUES('session',9999);
  INSERT INTO online_players(player_id,username,first_seen,last_seen,build) VALUES('00000000000000000000000000000001','Driver',1,1,'build');
  UPDATE online_players SET last_seen=2 WHERE player_id='00000000000000000000000000000001';
  UPDATE devices SET blocked=0 WHERE id='driver';`);
 await readSnapshot(env,1);assert.equal(revision(),before);assert.equal(builds(),1);
});

test('a write between ranking and cache publication cannot label stale results as current',async()=>{
 seed();beforePublish=()=>{beforePublish=null;db.exec("UPDATE devices SET blocked=1 WHERE id='driver'");};
 assert.deepEqual(ids(await readSnapshot(env,1)),['score']);
 assert.equal(db.prepare('SELECT count(*) n FROM leaderboard_snapshots').get().n,0);
 assert.deepEqual(ids(await readSnapshot(env,1)),[]);assert.equal(builds(),2);
});

test('a request after a ban does not join a rebuild that started before the ban',async()=>{
 seed();let entered,release;
 const arrived=new Promise(resolve=>entered=resolve),gate=new Promise(resolve=>release=resolve);
 beforePublish=async()=>{beforePublish=null;entered();await gate;};
 const old=readSnapshot(env,1);await arrived;
 db.exec("UPDATE devices SET blocked=1 WHERE id='driver'");
 assert.deepEqual(ids(await readSnapshot(env,1)),[]);release();await old;
 assert.deepEqual(ids(await readSnapshot(env,1)),[]);assert.equal(builds(),2);
});

test('failed rebuilds are retried and never cached as successful empty rankings',async()=>{
 seed();beforePublish=()=>{beforePublish=null;throw new Error('Injected cache write failure');};
 await assert.rejects(readSnapshot(env,1),/Injected cache write failure/);
 assert.deepEqual(ids(await readSnapshot(env,1)),['score']);assert.equal(builds(),2);
});

test('migration is repeatable and preserves populated scores, bans, seasons and cache state',async()=>{
 seed();await readSnapshot(env,1);
 const tables=['runs_storage','devices','settings','audit','leaderboard_snapshot_state','leaderboard_snapshots'];
 const before=tables.map(t=>db.prepare('SELECT * FROM '+t).all());
 db.exec(migration);assert.deepEqual(tables.map(t=>db.prepare('SELECT * FROM '+t).all()),before);
 assert.deepEqual(db.prepare('PRAGMA foreign_key_check').all(),[]);
});

test('rolled back score changes do not invalidate or alter cached rankings',async()=>{
 seed();const initial=await readSnapshot(env,1),before=revision();
 db.exec('BEGIN');db.exec("DELETE FROM runs WHERE id='score'");db.exec('ROLLBACK');
 assert.equal(revision(),before);assert.deepEqual(await readSnapshot(env,1),initial);assert.equal(builds(),1);
});
