import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {DatabaseSync} from 'node:sqlite';
import {cleanupReplayObjects} from '../src/retention.mjs';

const migration=readFileSync(new URL('../migrations/0010_idzero_handling_reset.sql',import.meta.url),'utf8');
const priorMigrations=['0001_leaderboard','0002_imported_times','0003_replays','0004_replay_chunks',
 '0005_enna_skyline','0006_special_stage_courses','0007_tsubaki_line','0008_replay_retention','0009_idzero_courses'];

function fixture(){
 const db=new DatabaseSync(':memory:');db.exec('PRAGMA foreign_keys=ON');
 for(const name of priorMigrations)db.exec(readFileSync(new URL('../migrations/'+name+'.sql',import.meta.url),'utf8'));
 db.exec("UPDATE settings SET value='2' WHERE key='epoch'; INSERT INTO settings VALUES('unrelated','preserve'); INSERT INTO devices VALUES('player','token',1,0),('blocked','blocked-token',2,1); INSERT INTO admin_sessions VALUES('session',1234); INSERT INTO audit(created_at,action,target,reason) VALUES(1,'old audit','other','preserve'); INSERT INTO replay_object_deletions VALUES('prior-cleanup',1);");
 const insert=db.prepare('INSERT INTO runs(id,device_id,ruleset,epoch,condition,weather,car,ticks,glyphs,splits,manual,night,points,build,created_at,hidden,reason,imported,replay_size,replay_sha256) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)');
 const objects=new Map([['prior-cleanup',new Uint8Array([1])]]);
 const add=({id,condition,weather=0,car=0,kind=0,hidden=0,epoch=2,build='0.3.95-community-replays.44'})=>{
  const device=hidden?'blocked':'player',hash='a'.repeat(64),bytes=new Uint8Array(44).fill(condition);
  insert.run(id,device,'d3-community-v1',epoch,condition,weather,car,1200000,'[1,2,3,4,5]','[1,2,3,1200000]',1,0,100,build,1,hidden,hidden?'moderated':'',0,bytes.length,hash);
  const key=kind===2?'attempt/'+id:`replays/v1/${device}/${id}/${hash}/${bytes.length}.idr`;
  if(kind===0)db.prepare('INSERT INTO replays VALUES(?,?)').run(id,bytes);
  if(kind===1)db.prepare('INSERT INTO replay_chunks VALUES(?,?,?)').run(id,0,bytes);
  if(kind===2)db.prepare('INSERT INTO replay_objects VALUES(?,?)').run(id,key);
  if(kind>=2)objects.set(key,bytes);
  return key;
 };
 const apply=()=>{db.exec('BEGIN');try{db.exec(migration);db.exec('COMMIT');}catch(error){db.exec('ROLLBACK');throw error;}};
 return {db,add,objects,apply};
}

test('handling reset clears only Gunsai/Odawara across all models, directions, weather and replay stores',async()=>{
 const {db,add,objects,apply}=fixture();
 try{
  const targetKeys=[];
  for(let condition=0;condition<36;++condition)for(let weather=0;weather<2;++weather)for(const car of [0,5,34]){
   const key=add({id:`run-${condition}-${weather}-${car}`,condition,weather,car,kind:(condition+weather+car)%4,hidden:car===34?1:0,epoch:car===5?1:2});
   if(condition>=32)targetKeys.push(key);
  }
  const preserved={};
  for(const table of ['devices','admin_sessions','settings','audit'])preserved[table]=db.prepare('SELECT * FROM '+table).all();
  const scores=db.prepare('SELECT * FROM runs WHERE condition<32 ORDER BY id').all();
  const replayRows=Object.fromEntries(['replays','replay_chunks','replay_objects'].map(table=>[table,db.prepare(`SELECT * FROM ${table} WHERE run_id IN (SELECT id FROM runs WHERE condition<32) ORDER BY run_id`).all()]));
  const survivingObjects=new Map([...objects].filter(([key])=>key!=='prior-cleanup'&&!targetKeys.includes(key)));
  apply();
  assert.deepEqual(db.prepare('SELECT * FROM runs ORDER BY id').all(),scores);
  assert.equal(db.prepare('SELECT count(*) n FROM runs_storage WHERE condition_idzero BETWEEN 32 AND 35').get().n,0);
  for(const [table,rows] of Object.entries(replayRows))assert.deepEqual(db.prepare(`SELECT * FROM ${table} ORDER BY run_id`).all(),rows);
  for(const table of ['devices','admin_sessions'])assert.deepEqual(db.prepare('SELECT * FROM '+table).all(),preserved[table]);
  assert.deepEqual(db.prepare("SELECT * FROM settings WHERE key<>'idzero_handling_reset_45'").all(),preserved.settings);
  assert.deepEqual(db.prepare('SELECT * FROM audit WHERE id=1').all(),preserved.audit);
  assert.equal(db.prepare("SELECT count(*) n FROM audit WHERE action='reset course records'").get().n,1);
  assert.deepEqual(db.prepare('PRAGMA foreign_key_check').all(),[]);
  assert.deepEqual(db.prepare('SELECT object_key FROM replay_object_deletions ORDER BY object_key').all().map(x=>x.object_key),['prior-cleanup',...targetKeys].sort());

  const wrap=(sql,args=[])=>({bind(...values){return wrap(sql,values);},async all(){return {results:db.prepare(sql).all(...args)};},async run(){return db.prepare(sql).run(...args);}});
  const env={DB:{prepare:wrap,async batch(queries){db.exec('BEGIN');try{for(const query of queries)await query.run();db.exec('COMMIT');}catch(error){db.exec('ROLLBACK');throw error;}}},REPLAYS:{async delete(keys){for(const key of keys)objects.delete(key);}}};
  assert.equal((await cleanupReplayObjects(env,500)).removed,targetKeys.length+1);
  assert.deepEqual(objects,survivingObjects);
  assert.equal(db.prepare('SELECT count(*) n FROM replay_object_deletions').get().n,0);

  // A repeated execute must never erase new .45 times or queue their replays.
  add({id:'new-45',condition:32,kind:2,build:'0.3.95-community-replays.45'});
  const after={};for(const table of ['runs','replays','replay_chunks','replay_objects','replay_object_deletions','settings','audit'])after[table]=db.prepare('SELECT * FROM '+table).all();
  apply();
  for(const [table,rows] of Object.entries(after))assert.deepEqual(db.prepare('SELECT * FROM '+table).all(),rows);
 }finally{db.close();}
});

test('reset blocks in-flight .44 commits on the two changed courses without restricting later builds or other boards',()=>{
 const {db,add,apply}=fixture();
 try{
  apply();
  for(const condition of [32,33,34,35]){
   assert.throws(()=>add({id:'stale-'+condition,condition}),/post-reset handling/);
   for(const suffix of [45,46])add({id:`new-${condition}-${suffix}`,condition,build:'0.3.95-community-replays.'+suffix});
  }
  add({id:'old-unrelated',condition:31});
  assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,9);
  assert.deepEqual(db.prepare('PRAGMA foreign_key_check').all(),[]);
 }finally{db.close();}
});

test('failed reset transaction preserves every score, replay and prior cleanup item',()=>{
 const {db,add,apply}=fixture();
 try{
  add({id:'target',condition:34,kind:2});add({id:'other',condition:30});
  const before={};for(const table of ['runs','replays','replay_chunks','replay_objects','replay_object_deletions','settings','audit'])before[table]=db.prepare('SELECT * FROM '+table).all();
  db.exec("CREATE TRIGGER fail_reset BEFORE INSERT ON audit WHEN NEW.action='reset course records' BEGIN SELECT RAISE(ABORT,'test reset failure'); END;");
  assert.throws(apply,/test reset failure/);
  for(const [table,rows] of Object.entries(before))assert.deepEqual(db.prepare('SELECT * FROM '+table).all(),rows);
  assert.equal(db.prepare("SELECT count(*) n FROM sqlite_schema WHERE name='runs_idzero_old_handling_guard'").get().n,0);
 }finally{db.close();}
});
