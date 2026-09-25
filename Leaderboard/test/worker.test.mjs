import {test,beforeEach} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {DatabaseSync} from 'node:sqlite';
import {sha,validateRun,supportedBuild,COURSES,REQUIRED_CLIENT_BUILD} from '../src/core.mjs';
import {decodeReplay,compressReplay} from '../src/replay.mjs';
const source=readFileSync(new URL('../src/worker.mjs',import.meta.url),'utf8').replace("import html from './index.html';","const html='test';").replace("'./core.mjs'",JSON.stringify(new URL('../src/core.mjs',import.meta.url).href)).replace("'./replay.mjs'",JSON.stringify(new URL('../src/replay.mjs',import.meta.url).href));
const worker=(await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'))).default;
let db,env;
const secret='a'.repeat(64),device='b'.repeat(64);
beforeEach(async()=>{db?.close();db=new DatabaseSync(':memory:');db.exec(readFileSync(new URL('../migrations/0001_leaderboard.sql',import.meta.url),'utf8'));db.exec(readFileSync(new URL('../migrations/0002_imported_times.sql',import.meta.url),'utf8'));db.exec(readFileSync(new URL('../migrations/0003_replays.sql',import.meta.url),'utf8'));db.exec(readFileSync(new URL('../migrations/0004_replay_chunks.sql',import.meta.url),'utf8'));db.exec(readFileSync(new URL('../migrations/0005_enna_skyline.sql',import.meta.url),'utf8'));db.exec(readFileSync(new URL('../migrations/0006_special_stage_courses.sql',import.meta.url),'utf8'));const wrap=(sql,args=[])=>({bind(...args){return wrap(sql,args.map(v=>v instanceof ArrayBuffer?new Uint8Array(v):v));},async first(){return db.prepare(sql).get(...args)||null;},async all(){return {results:db.prepare(sql).all(...args)};},async run(){const r=db.prepare(sql).run(...args);return {meta:r};}});env={RULESET:'d3-community-v1',ADMIN_KEY_SHA256:await sha(secret),DB:{prepare:wrap,async batch(queries){db.exec('BEGIN');try{const out=[];for(const q of queries)out.push(await q.run());db.exec('COMMIT');return out;}catch(e){db.exec('ROLLBACK');throw e;}}}};for(const key of ['PUBLIC_LIMIT','WRITE_LIMIT','AUTH_LIMIT'])env[key]={async limit(){return {success:true};}};});
async function call(path,data,headers={}){return worker.fetch(new Request('https://example.test'+path,{method:data?'POST':'GET',headers:{...(data?{'Content-Type':'application/json'}:{}),...headers},body:data?JSON.stringify(data):undefined}),env);}
const run=(extra={})=>({id:crypto.randomUUID(),ruleset:'d3-community-v1',epoch:1,condition:0,weather:0,car:0,ticks6000:1200000,nameGlyphs:[162,163,164,221,221],splits:[300000,600000,900000,1200000],manual:1,night:0,points:999999,build:REQUIRED_CLIENT_BUILD,...extra});
const auth=()=>({Authorization:'Bearer '+device});
test('Enna downhill/uphill dry/wet upload with replays and appear on separate boards',async()=>{
 assert.equal(COURSES[11],'Enna Skyline');
 const html=readFileSync(new URL('../src/index.html',import.meta.url),'utf8');
 assert.ok(html.includes("courses="+JSON.stringify(COURSES).replaceAll('"',"'")));
 await call('/api/v1/register',{token:device});
 for(const condition of [22,23])for(const weather of [0,1]){
  const x=run({condition,weather,night:1});
  assert.equal((await uploadReplay(x)).status,200);
  const board=await(await call(`/api/v1/board?condition=${condition}&weather=${weather}`)).json();
  assert.equal(board.entries.length,1);assert.equal(board.entries[0].id,x.id);
  assert.equal(board.entries[0].replayAvailable,true);
  const download=await call('/api/v1/replay?id='+x.id);assert.equal(download.status,200);
  const bytes=new Uint8Array(await download.arrayBuffer()),n=new DataView(bytes.buffer).getUint32(0,true);
  const meta=JSON.parse(new TextDecoder().decode(bytes.subarray(4,4+n)));
  assert.equal(meta.condition,condition);assert.equal(meta.weather,weather);
 }
 const snapshot=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();
 assert.deepEqual(snapshot.courses,COURSES);assert.equal(snapshot.entries.length,4);
 assert.equal((await call('/api/v1/board?condition=30&weather=0')).status,400);
 assert.equal((await uploadReplay(run({condition:30}))).status,400);
 assert.equal((await uploadReplay(run({condition:22,build:'0.3.95-enna-preview.1'}))).status,409);
 assert.equal((await call('/api/v1/runs',run({condition:22}),auth())).status,426);
 assert.equal((await uploadReplay(run({condition:22,imported:1}))).status,400);
});
test('Special Stage identities preserve six directions and dry/wet boards and replay metadata',async()=>{
 assert.deepEqual(COURSES.slice(12),['Myogi (Special Stage)','Usui (Special Stage)','Momiji Line']);
 await call('/api/v1/register',{token:device});
 for(let condition=24;condition<30;++condition)for(const weather of [0,1]){
  const x=run({condition,weather,night:1});
  assert.equal((await uploadReplay(x)).status,200);
  const board=await(await call(`/api/v1/board?condition=${condition}&weather=${weather}`)).json();
  assert.equal(board.entries.length,1);assert.equal(board.entries[0].id,x.id);
  const download=await call('/api/v1/replay?id='+x.id);assert.equal(download.status,200);
  const bytes=new Uint8Array(await download.arrayBuffer()),n=new DataView(bytes.buffer).getUint32(0,true);
  const meta=JSON.parse(new TextDecoder().decode(bytes.subarray(4,4+n)));
  assert.equal(meta.condition,condition);assert.equal(meta.weather,weather);
 }
 for(const condition of [0,1,2,3,4,5,8,9,12,13])assert.equal((await(await call(`/api/v1/board?condition=${condition}&weather=0`)).json()).entries.length,0);
});
test('personal battle and incomplete recordings cannot upload',async()=>{
 await call('/api/v1/register',{token:device});
 for(const extra of [{mode:1},{mode:2},{outcome:1},{outcome:2},{opponentBytes:100}])assert.equal((await uploadReplay(run(extra))).status,400);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,0);
 assert.equal(db.prepare('SELECT count(*) n FROM replay_chunks').get().n,0);
});
function replayFor(x){
 const count=Math.ceil(x.ticks6000/100),b=new Uint8Array(96+160*count),v=new DataView(b.buffer);
 for(const [at,value] of [[0,0x32524449],[4,x.ticks6000],[8,count],[12,60],[16,96],[20,160],[24,1],[28,12],[32,x.weather],[60,5]])v.setUint32(at,value,true);
 x.nameGlyphs.forEach((n,i)=>v.setUint32(40+i*4,n,true));
 for(let i=0;i<count;i++){const tick=i+1,p=96+i*160;v.setUint32(p,tick,true);v.setFloat32(p+4,tick/2,true);v.setFloat32(p+20,30,true);v.setUint32(p+24,3,true);v.setFloat32(p+28,7123.456,true);v.setUint32(p+96,Math.min(tick*100,x.ticks6000),true);v.setUint32(p+124,4,true);}
 return b;
}
async function uploadReplay(x,b=replayFor(x),headers=auth()){
 const meta=new TextEncoder().encode(JSON.stringify(x)),data=new Uint8Array(4+meta.length+b.length);new DataView(data.buffer).setUint32(0,meta.length,true);data.set(meta,4);data.set(b,4+meta.length);
 return worker.fetch(new Request('https://example.test/api/v2/runs',{method:'POST',headers,body:data}),env);
}
async function submit(x,headers={}){return uploadReplay(x,undefined,headers);}
async function seedOld(x){const player=db.prepare('SELECT id FROM devices').get().id;db.prepare('INSERT INTO runs(id,device_id,ruleset,epoch,condition,weather,car,ticks,glyphs,splits,manual,night,points,build,created_at,imported) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)').run(x.id,player,x.ruleset,x.epoch,x.condition,x.weather,x.car,x.ticks6000,JSON.stringify(x.nameGlyphs),JSON.stringify(x.splits),x.manual,x.night,x.points,x.build,1,x.imported||0);}
async function login(){let r=await call('/api/admin/login',{key:secret},{Origin:'https://example.test'});assert.equal(r.status,200);return {Cookie:r.headers.get('Set-Cookie').split(';')[0],Origin:'https://example.test'};}
test('anonymous registration retries preserve identity and reject arbitrary credentials',async()=>{let a=await(await call('/api/v1/register',{token:device})).json(),b=await(await call('/api/v1/register',{token:device})).json();assert.equal(a.player,b.player);assert.equal((await call('/api/v1/register',{token:'x'})).status,400);assert.equal(db.prepare('SELECT count(*) n FROM devices').get().n,1);});
test('valid finish uploads once and snapshot keeps original tick precision',async()=>{await call('/api/v1/register',{token:device});let x=run();assert.equal((await submit(x,auth())).status,200);assert.equal((await submit(x,auth())).status,200);let s=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();assert.equal(s.entries.length,1);assert.equal(s.entries[0].ticks6000,x.ticks6000);assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,1);assert.equal((await submit(run())).status,401);});
test('impossible checkpoints, unknown rules and malformed fields are rejected',()=>{for(let x of [run({splits:[0,600000,900000,1200000]}),run({splits:[300000,600000,900000,0]}),run({ticks6000:-1}),run({ruleset:'other'}),run({car:35}),run({condition:16}),run({nameGlyphs:[999,1,2,3,4]})])assert.throws(()=>validateRun(x,env.RULESET));});
test('admin requires auth and same-origin mutations; wrong key rejected',async()=>{assert.equal((await call('/api/admin/runs')).status,401);assert.equal((await call('/api/admin/login',{key:secret})).status,403);assert.equal((await call('/api/admin/login',{key:'wrong'},{Origin:'https://example.test'})).status,401);let a=await login();assert.equal((await call('/api/admin/runs',null,a)).status,200);assert.equal((await call('/api/admin/reset',{confirm:'RESET RANKINGS',reason:'test'}, {Cookie:a.Cookie,Origin:'https://evil.test'})).status,403);});
test('hide, restore and installation block affect public rankings and uploads',async()=>{let d=await(await call('/api/v1/register',{token:device})).json(),x=run();await submit(x,auth());let a=await login();const rows=async()=> (await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json()).entries;
 await call('/api/admin/run',{id:x.id,value:1,reason:'Review run'},a);assert.equal((await rows()).length,0);
 await call('/api/admin/run',{id:x.id,value:0,reason:'Run approved'},a);assert.equal((await rows()).length,1);
 await call('/api/admin/player',{id:d.player,value:1,reason:'Invalid runs'},a);assert.equal((await rows()).length,0);assert.equal((await submit(run(),auth())).status,403);
 await call('/api/admin/player',{id:d.player,value:0,reason:'Appeal accepted'},a);assert.equal((await rows()).length,1);assert.equal(db.prepare('SELECT count(*) n FROM audit').get().n,4);
});
test('reset starts a new season without deleting history, duplicate runs cannot re-enter',async()=>{await call('/api/v1/register',{token:device});let x=run();await submit(x,auth());let a=await login();assert.equal((await call('/api/admin/reset',{confirm:'RESET RANKINGS',reason:'New season'},a)).status,200);await submit(x,auth());let s=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();assert.equal(s.entries.length,0);assert.equal(s.epoch,2);assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,1);});
test('personal duplicate runs do not crowd out distinct players; model winners retained',async()=>{await call('/api/v1/register',{token:device});for(let i=0;i<15;i++)await submit(run(),auth());await submit(run({car:5,ticks6000:1500000,splits:[300000,600000,900000,1500000]}),auth());let s=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();assert.equal(s.entries.length,2);});
test('rate limits fail closed',async()=>{env.AUTH_LIMIT={async limit(){return {success:false};}};assert.equal((await call('/api/v1/register',{token:device})).status,429);assert.equal(db.prepare('SELECT count(*) n FROM devices').get().n,0);});
test('session logout invalidates server token and expiration cleanup',async()=>{let a=await login();await call('/api/admin/logout',{},a);assert.equal((await call('/api/admin/runs',null,a)).status,401);db.prepare('INSERT INTO admin_sessions VALUES (?,?)').run('expired',1);await worker.scheduled({},env);assert.equal(db.prepare('SELECT count(*) n FROM admin_sessions').get().n,0);});

test('fresh IDs from an old season cannot bypass a ranking reset',async()=>{await call('/api/v1/register',{token:device});let a=await login();await call('/api/admin/reset',{confirm:'RESET RANKINGS',reason:'New season'},a);assert.equal((await submit(run(),auth())).status,409);assert.equal((await submit(run({epoch:2}),auth())).status,200);let s=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();assert.equal(s.entries.length,1);assert.equal(s.entries[0].epoch,2);});

test('legacy upload endpoint rejects live and historical times without inserting anything',async()=>{
 await call('/api/v1/register',{token:device});
 for(const x of [run(),run({imported:1,manual:-1,night:-1,points:-1,splits:[0,0,0,0]})]){
  const response=await call('/api/v1/runs',x,auth());assert.equal(response.status,426);assert.match((await response.json()).error,/replay is required/);
 }
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,0);
});
test('historical uploads remain forbidden even when accompanied by a replay',async()=>{
 await call('/api/v1/register',{token:device});const x=run({imported:1});assert.equal((await uploadReplay(x)).status,400);
 assert.equal((await call('/api/v2/runs',run(),auth())).status,400);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,0);
});
test('existing historical entries survive policy change, remain readable and moderatable',async()=>{
 await call('/api/v1/register',{token:device});const x=run({imported:1,manual:-1,night:-1,points:-1,splits:[0,0,0,0]});await seedOld(x);
 let modern=await(await call('/api/v1/snapshot?ruleset=d3-community-v1&imports=1')).json();assert.equal(modern.entries[0].imported,1);assert.equal(modern.entries[0].replayAvailable,false);
 let old=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();assert.equal(old.entries.length,0);
 const a=await login();await call('/api/admin/run',{id:x.id,value:1,reason:'Review legacy record'},a);modern=await(await call('/api/v1/snapshot?ruleset=d3-community-v1&imports=1')).json();assert.equal(modern.entries.length,0);assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,1);
});

test('time and replay are stored together; admin CSV remains authenticated',async()=>{
 await call('/api/v1/register',{token:device});const x=run(),b=replayFor(x);
 assert.equal((await uploadReplay(x,b)).status,200);assert.equal((await(await uploadReplay(x,b)).json()).duplicate,true);
 assert.equal(db.prepare('SELECT count(*) n FROM replay_chunks').get().n,1);
 const board=await(await call('/api/v1/board?condition=0&weather=0')).json();assert.equal(board.entries[0].replayAvailable,true);assert.equal(board.entries[0].replay_sha256,undefined);
 assert.equal((await call('/api/admin/replay?id='+x.id)).status,401);
 const a=await login(),download=await call('/api/admin/replay?id='+x.id,null,a);assert.equal(download.status,200);
 const csv=await download.text();assert.ok(csv.startsWith('tick,speed,yaw,pos_x,pos_y,pos_z,gear,finish_ticks6000,rpm,'));assert.equal(Number(csv.trimEnd().split('\n').at(-1).split(',')[7]),x.ticks6000);
 assert.equal((await uploadReplay({...x,car:1},b)).status,409);
 const changed=b.slice();new DataView(changed.buffer).setFloat32(100,42,true);assert.equal((await uploadReplay(x,changed)).status,409);
 await call('/api/admin/run',{id:x.id,value:1,reason:'Review replay'},a);assert.equal((await call('/api/admin/replay?id='+x.id,null,a)).status,200);
});
test('wrong time, truncation, gaps, NaN, oversized or imported replays leave no rows',async()=>{
 await call('/api/v1/register',{token:device});const x=run();
 for(const mutate of [b=>b.subarray(0,b.length-1),b=>{new DataView(b.buffer).setUint32(4,123,true);return b;},b=>{new DataView(b.buffer).setUint32(256,99,true);return b;},b=>{new DataView(b.buffer).setFloat32(124,NaN,true);return b;},b=>new Uint8Array(18020000)])assert.equal((await uploadReplay(x,mutate(replayFor(x)))).status,400);
 assert.equal((await uploadReplay({...x,imported:1})).status,400);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,0);assert.equal(db.prepare('SELECT count(*) n FROM replay_chunks').get().n,0);
});
test('failed replay insert rolls back the time; retries recover; bans prevent replay writes',async()=>{
 const d=await(await call('/api/v1/register',{token:device})).json(),x=run();
 db.exec("CREATE TRIGGER fail_replay BEFORE INSERT ON replay_chunks BEGIN SELECT RAISE(ABORT,'disk failure'); END;");
 assert.equal((await uploadReplay(x)).status,503);assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,0);
 db.exec('DROP TRIGGER fail_replay');assert.equal((await uploadReplay(x)).status,200);
 const a=await login();await call('/api/admin/player',{id:d.player,value:1,reason:'Test blocked'},a);
 assert.equal((await uploadReplay(run())).status,403);assert.equal(db.prepare('SELECT count(*) n FROM replay_chunks').get().n,1);
});
test('source finish gate can settle the recorded clock backwards by two ticks',async()=>{
 await call('/api/v1/register',{token:device});
 const x=run(),b=replayFor(x),v=new DataView(b.buffer);
 v.setUint32(b.length-320+96,x.ticks6000+100,true);
 assert.equal((await uploadReplay(x,b)).status,200);
 const bad=replayFor(x);new DataView(bad.buffer).setUint32(bad.length-320+96,x.ticks6000+300,true);
 assert.equal((await uploadReplay({...x,id:crypto.randomUUID()},bad)).status,400);
});

test('public replay download is anonymous, viewer-compatible and excludes private data',async()=>{
 await call('/api/v1/register',{token:device});const x=run({condition:2,night:1}),raw=replayFor(x);await uploadReplay(x,raw);
 const response=await call('/api/v1/replay?id='+x.id);assert.equal(response.status,200);
 assert.equal(response.headers.get('Content-Type'),'application/octet-stream');assert.equal(response.headers.get('Cache-Control'),'no-store');assert.match(response.headers.get('Content-Disposition'),/attachment;.*\.idreplay/);
 const data=new Uint8Array(await response.arrayBuffer()),size=new DataView(data.buffer).getUint32(0,true),metadata=JSON.parse(new TextDecoder().decode(data.subarray(4,4+size)));
 for(const key of ['id','condition','weather','night','car','manual','ticks6000','build','splits','nameGlyphs'])assert.deepEqual(metadata[key],x[key]);
 for(const key of ['player','device_id','token','token_hash','adminKey','reason','blocked','hidden'])assert.equal(metadata[key],undefined);
 assert.deepEqual(await decodeReplay(data.subarray(4+size)),raw);
 const auth=await login(),adminBytes=new Uint8Array(await(await call('/api/admin/replay?id='+x.id+'&format=package',null,auth)).arrayBuffer());
 assert.deepEqual(data,adminBytes);
});

test('public replay access follows hide, block, restore, season and ruleset visibility',async()=>{
 const registration=await(await call('/api/v1/register',{token:device})).json();const x=run();await uploadReplay(x);const auth=await login(),path='/api/v1/replay?id='+x.id;
 for(const kind of ['run','player']){
  const id=kind==='run'?x.id:registration.player;
  await call('/api/admin/'+kind,{id,value:1,reason:'Public access test'},auth);
  assert.equal((await call(path)).status,404);assert.equal((await call('/api/admin/replay?id='+x.id+'&format=package',null,auth)).status,200);
  await call('/api/admin/'+kind,{id,value:0,reason:'Restore public access'},auth);assert.equal((await call(path)).status,200);
 }
 env.RULESET='new-ruleset';assert.equal((await call(path)).status,404);env.RULESET='d3-community-v1';
 await call('/api/admin/reset',{confirm:'RESET RANKINGS',reason:'New season test'},auth);assert.equal((await call(path)).status,404);
 assert.equal((await call('/api/admin/replay?id='+x.id+'&format=package',null,auth)).status,200);
});

test('public replay rejects missing/invalid IDs, older times without replays and rate limits',async()=>{
 for(const id of ['', 'invalid','../../admin'])assert.equal((await call('/api/v1/replay?id='+encodeURIComponent(id))).status,400);
 assert.equal((await call('/api/v1/replay?id='+crypto.randomUUID())).status,404);
 await call('/api/v1/register',{token:device});const old=run();await seedOld(old);assert.equal((await call('/api/v1/replay?id='+old.id)).status,404);
 const x=run();await uploadReplay(x);env.PUBLIC_LIMIT={async limit(){return {success:false};}};
 assert.equal((await call('/api/v1/replay?id='+x.id)).status,429);
});
test('neutral final gear is legitimate and old times explicitly have no replay',async()=>{
 await call('/api/v1/register',{token:device});const x=run(),b=replayFor(x);new DataView(b.buffer).setUint32(b.length-160+24,0,true);
 assert.equal((await uploadReplay(x,b)).status,200);
 const old=run({car:1});await seedOld(old);const rows=(await(await call('/api/v1/board?condition=0&weather=0')).json()).entries;
 assert.equal(rows.find(r=>r.id===old.id).replayAvailable,false);
});
test('viewer download preserves metadata and every pose without installation identity or credentials',async()=>{
 await call('/api/v1/register',{token:device});const x=run({condition:19,weather:1,night:1,car:34}),poses=replayFor(x);await uploadReplay(x,poses);
 const path='/api/admin/replay?id='+x.id+'&format=package';assert.equal((await call(path)).status,401);
 const auth=await login(),response=await call(path,null,auth);assert.equal(response.status,200);assert.match(response.headers.get('Content-Disposition'),/\.idreplay/);
 const data=new Uint8Array(await response.arrayBuffer()),size=new DataView(data.buffer).getUint32(0,true),metadata=JSON.parse(new TextDecoder().decode(data.subarray(4,4+size)));
 for(const key of ['id','condition','weather','night','car','manual','ticks6000','build','splits','nameGlyphs'])assert.deepEqual(metadata[key],x[key],key);
 for(const key of ['player','device_id','token','adminKey'])assert.equal(metadata[key],undefined);
 assert.deepEqual(await decodeReplay(data.subarray(4+size)),poses);
 assert.equal((await call('/api/admin/replay?id='+crypto.randomUUID()+'&format=package',null,auth)).status,404);
});
test('upload build policy accepts only the exact ROM-required release',()=>{
 assert.equal(REQUIRED_CLIENT_BUILD,'0.3.95-community-replays.34');
 assert.equal(supportedBuild(REQUIRED_CLIENT_BUILD),true);
 for(const build of ['0.3.95-community-replays.1','0.3.95-community-replays.28','0.3.95-community-replays.29','0.3.95-community-replays.30','0.3.95-community-replays.31','0.3.95-community-replays.32','0.3.95-community-replays.33','0.3.95-community-replays.35','0.3.95-community-replays.340','0.3.96-community-replays.34','0.4.0','1.0.0','0.3.95','0.3.95-community-replays.034','0.3.95-Community-replays.34','0.3.95-community-replays.34-extra','0.3.95-community-replays.34 ','replay-smoke','',null,33])assert.equal(supportedBuild(build),false,String(build));
 for(const required of ['',null,'invalid','0.3.95-community-replays.*'])assert.equal(supportedBuild(required,required),false);
});
test('nonmatching builds are permanently rejected before replay work and without database writes',async()=>{
 await call('/api/v1/register',{token:device});
 env.MIN_CLIENT_BUILD='0.3.95-community-replays.1'; // A stale variable cannot restore the old minimum policy.
 const before=db.prepare('SELECT total_changes() n').get().n;
 for(const build of ['0.3.90-performance.5','0.3.95-community-replays.1','0.3.95-community-replays.28','0.3.95-community-replays.29','0.3.95-community-replays.30','0.3.95-community-replays.31','0.3.95-community-replays.32','0.3.95-community-replays.33','0.3.95-community-replays.35','0.3.96-community-replays.1','0.4.0','0.3.95-community-replays.034']){
  const response=await uploadReplay(run({build}));assert.equal(response.status,409);
  const error=await response.json();assert.equal(error.code,'client_build_required');assert.equal(error.requiredBuild,REQUIRED_CLIENT_BUILD);assert.equal(error.permanent,true);assert.ok(error.error.includes(REQUIRED_CLIENT_BUILD));
 }
 assert.equal((await uploadReplay(run({build:'0.3.95-community-replays.30'}),new Uint8Array([0]))).status,409);
 assert.equal(db.prepare('SELECT total_changes() n').get().n,before);
 for(const table of ['runs','replays','replay_chunks'])assert.equal(db.prepare(`SELECT count(*) n FROM ${table}`).get().n,0);
 const current=run();assert.equal((await uploadReplay(current)).status,200);assert.equal((await uploadReplay(current)).status,200);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,1);
 env.REQUIRED_CLIENT_BUILD='invalid';assert.equal((await uploadReplay(run())).status,409);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,1);
});

test('configured exact release rejects both sides of its version and is exposed read-only',async()=>{
 env.REQUIRED_CLIENT_BUILD=REQUIRED_CLIENT_BUILD;
 await call('/api/v1/register',{token:device});
 const before=db.prepare('SELECT total_changes() n').get().n;
 const health=await(await call('/health')).json();assert.equal(health.requiredBuild,REQUIRED_CLIENT_BUILD);
 const snapshot=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();assert.equal(snapshot.requiredBuild,REQUIRED_CLIENT_BUILD);assert.equal(snapshot.epoch,1);
 assert.equal(db.prepare('SELECT total_changes() n').get().n,before);
 for(const build of ['0.3.95-community-replays.32','0.3.95-community-replays.33','0.3.95-community-replays.35'])assert.equal((await uploadReplay(run({build}))).status,409);
 assert.equal((await uploadReplay(run())).status,200);
});

test('exact upload policy preserves historical scores, current season and replay downloads',async()=>{
 await call('/api/v1/register',{token:device});
 const historical=run();assert.equal((await uploadReplay(historical)).status,200);
 db.prepare('UPDATE runs SET build=? WHERE id=?').run('0.3.95-community-replays.30',historical.id);
 const rowBefore=db.prepare('SELECT * FROM runs WHERE id=?').get(historical.id);
 const changesBefore=db.prepare('SELECT total_changes() n').get().n;
 const board=await(await call('/api/v1/board?condition=0&weather=0')).json();
 const snapshot=await(await call('/api/v1/snapshot?ruleset=d3-community-v1')).json();
 for(const result of [board,snapshot]){assert.equal(result.epoch,1);assert.equal(result.entries.length,1);assert.equal(result.entries[0].id,historical.id);assert.equal(result.entries[0].build,'0.3.95-community-replays.30');}
 assert.equal((await call('/api/v1/replay?id='+historical.id)).status,200);
 assert.equal((await uploadReplay({...historical,build:'0.3.95-community-replays.30'})).status,409);
 assert.deepEqual(db.prepare('SELECT * FROM runs WHERE id=?').get(historical.id),rowBefore);
 assert.equal(db.prepare('SELECT total_changes() n').get().n,changesBefore);
 assert.equal(db.prepare("SELECT value FROM settings WHERE key='epoch'").get().value,'1');
});

test('replay-only season migration deletes times and recordings, preserves identities and bans, rejects stale queues',async()=>{
 const player=await(await call('/api/v1/register',{token:device})).json();
 const previous=run();await uploadReplay(previous);
 const legacy=run({imported:1});await seedOld(legacy);
 db.prepare('INSERT INTO replays(run_id,data) VALUES (?,?)').run(legacy.id,new Uint8Array(44));
 db.prepare('UPDATE devices SET blocked=1 WHERE id=?').run(player.player);
 db.exec('BEGIN');db.exec(readFileSync(new URL('../migrations/0005_replay_only_season.sql',import.meta.url),'utf8'));db.exec('COMMIT');
 for(const table of ['runs','replays','replay_chunks'])assert.equal(db.prepare(`SELECT count(*) n FROM ${table}`).get().n,0);
 assert.equal(db.prepare('SELECT blocked FROM devices WHERE id=?').get(player.player).blocked,1);
 assert.equal(db.prepare("SELECT value FROM settings WHERE key='epoch'").get().value,'2');
 assert.equal(db.prepare("SELECT count(*) n FROM audit WHERE action='delete all records'").get().n,1);
 assert.equal((await call('/api/v1/replay?id='+previous.id)).status,404);
 assert.equal((await uploadReplay(run({epoch:2}))).status,403);
 db.prepare('UPDATE devices SET blocked=0 WHERE id=?').run(player.player);
 assert.equal((await uploadReplay(previous)).status,409);
 assert.equal((await uploadReplay(run({epoch:2,build:'0.3.94-player-replays.4'}))).status,409);
 assert.equal((await uploadReplay(run({epoch:2,imported:1}))).status,400);
 assert.equal((await uploadReplay(run({epoch:2}))).status,200);
 const board=await(await call('/api/v1/snapshot?ruleset=d3-community-v1&imports=1')).json();
 assert.equal(board.epoch,2);assert.equal(board.entries.length,1);assert.equal(board.entries[0].replayAvailable,true);
});

test('compressed detailed replay preserves every RPM and pose bit through multiple storage chunks',async()=>{
 await call('/api/v1/register',{token:device});const x=run({ticks6000:3500000,splits:[500000,1500000,2500000,3500000]}),raw=replayFor(x),v=new DataView(raw.buffer);
 let seed=1234567;for(let i=0;i<35000;i++)for(const field of [1,2,3,4,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23]){seed=(Math.imul(seed,1664525)+1013904223)>>>0;v.setFloat32(96+i*160+field*4,seed/4294967296*100,true);}
 const compressed=await compressReplay(raw);assert.ok(compressed.length>1000000);assert.deepEqual(await decodeReplay(compressed),raw);
 assert.equal((await uploadReplay(x,compressed)).status,200);assert.ok(db.prepare('SELECT count(*) n FROM replay_chunks').get().n>1);
 const a=await login(),res=await call('/api/admin/replay?id='+x.id+'&format=package',null,a),data=new Uint8Array(await res.arrayBuffer()),size=new DataView(data.buffer).getUint32(0,true);
 assert.deepEqual(await decodeReplay(data.subarray(4+size)),raw);
 const publicResponse=await call('/api/v1/replay?id='+x.id);
 assert.equal(publicResponse.status,200);assert.deepEqual(new Uint8Array(await publicResponse.arrayBuffer()),data);
});
test('missing single simulation frames, invalid RPM and compressed expansion errors are rejected',async()=>{
 await call('/api/v1/register',{token:device});const x=run();
 for(const mutate of [b=>{new DataView(b.buffer).setUint32(256,3,true);return b;},b=>{new DataView(b.buffer).setFloat32(124,Infinity,true);return b;},b=>{new DataView(b.buffer).setUint32(0,0x31524449,true);return b;}])assert.equal((await uploadReplay(x,mutate(replayFor(x)))).status,400);
 const compressed=await compressReplay(replayFor(x));new DataView(compressed.buffer).setUint32(4,96,true);assert.equal((await uploadReplay(x,compressed)).status,400);
 assert.equal(db.prepare('SELECT count(*) n FROM runs').get().n,0);
});

test('activity surveys require an authenticated unblocked installation and valid bounded counts',async()=>{
 const activity={online:12,queuing:3,racing:6,age:0,limited:false};
 assert.deepEqual(await(await call('/api/v1/activity')).json(),{available:false});
 assert.equal((await call('/api/v1/activity',activity)).status,401);
 assert.equal((await call('/api/v1/activity',activity,auth())).status,401);
 await call('/api/v1/register',{token:device});
 for(const extra of [{online:0},{online:52},{online:51},{queuing:-1},{queuing:8,racing:6},{age:31},{age:-1},{age:1.5},{limited:1}])assert.equal((await call('/api/v1/activity',{...activity,...extra},auth())).status,400);
 assert.equal((await call('/api/v1/activity',activity,auth())).status,200);
 const result=await(await call('/api/v1/activity')).json();
 assert.equal(result.available,true);assert.equal(result.online,12);assert.equal(result.queuing,3);assert.equal(result.racing,6);assert.equal(result.limited,false);assert.ok(result.validFor>40&&result.validFor<=45);
 assert.deepEqual(Object.keys(result).sort(),['available','generatedAt','limited','online','queuing','racing','validFor'].sort());
 db.prepare('UPDATE devices SET blocked=1').run();
 assert.equal((await call('/api/v1/activity',activity,auth())).status,403);
});
test('activity surveys are not summed, cannot overwrite newer observations and expire',async()=>{
 await call('/api/v1/register',{token:device});
 const activity={online:4,queuing:1,racing:2,age:0,limited:false};
 await call('/api/v1/activity',activity,auth());await call('/api/v1/activity',activity,auth());
 assert.equal((await(await call('/api/v1/activity')).json()).online,4);
 await call('/api/v1/activity',{...activity,online:20,age:20},auth());
 assert.equal((await(await call('/api/v1/activity')).json()).online,4);
 await call('/api/v1/activity',{...activity,online:51,limited:true},auth());
 assert.equal((await(await call('/api/v1/activity')).json()).limited,true);
 for(const generatedAt of [Math.floor(Date.now()/1000)-45,Math.floor(Date.now()/1000)+100]){
  db.prepare("UPDATE settings SET value=? WHERE key='online_activity'").run(JSON.stringify({...activity,generatedAt}));
  assert.deepEqual(await(await call('/api/v1/activity')).json(),{available:false});
 }
});
