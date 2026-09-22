import html from './index.html';
import {COURSES,validateRun,publicRun,rankedSql,sha,token,sameSecret,MIN_CLIENT_BUILD,supportedBuild} from './core.mjs';
import {MAX_REPLAY,validateReplay,replayCsv,decodeReplay,compressReplay} from './replay.mjs';
const now=()=>Math.floor(Date.now()/1000);
const headers={'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer','X-Frame-Options':'DENY'};
function json(value,status=200,extra={}){return new Response(JSON.stringify(value),{status,headers:{...headers,'Content-Type':'application/json',...extra}});}
async function rawBody(request,maximum=8192){
 if(Number(request.headers.get('Content-Length')||0)>maximum)throw new Error('Request too large.');
 const reader=request.body?.getReader();if(!reader)throw new SyntaxError();let size=0,chunks=[];
 while(true){let {value,done}=await reader.read();if(done)break;size+=value.length;if(size>maximum){await reader.cancel();throw new Error('Request too large.');}chunks.push(value);}
 const data=new Uint8Array(size);let offset=0;for(const part of chunks){data.set(part,offset);offset+=part.length;}return data;
}
async function body(request){return JSON.parse(new TextDecoder().decode(await rawBody(request)));}
async function limit(binding,key){if(!binding||!(await binding.limit({key})).success)throw new Response('Too many requests. Try again later.',{status:429,headers:{...headers,'Retry-After':'60'}});}
async function admin(request,env){const match=/(?:^|;\s*)idas_admin=([a-f0-9]{64})(?:;|$)/.exec(request.headers.get('Cookie')||'');if(!match)throw new Response('Sign in required.',{status:401});const session=await env.DB.prepare('SELECT token_hash FROM admin_sessions WHERE token_hash=? AND expires_at>?').bind(await sha(match[1]),now()).first();if(!session)throw new Response('Session expired.',{status:401});return session;}
function originCheck(request,url){if(request.headers.get('Origin')!==url.origin)throw new Response('Invalid request origin.',{status:403});}
async function downloadReplay(env,run,asPackage){
 const id=run.id;
 const row=await env.DB.prepare('SELECT data FROM replays WHERE run_id=?').bind(id).first();
 let stored;
 if(row)stored=new Uint8Array(row.data);
 else{
  const chunks=(await env.DB.prepare('SELECT part,data FROM replay_chunks WHERE run_id=? ORDER BY part').bind(id).all()).results;
  if(!chunks.length)return json({error:'No replay attached to this run.'},404);
  const size=chunks.reduce((n,c)=>n+new Uint8Array(c.data).byteLength,0);if(size>MAX_REPLAY)throw new Error('Invalid stored replay size.');
  stored=new Uint8Array(size);let at=0;for(let i=0;i<chunks.length;i++){if(chunks[i].part!==i)throw new Error('Invalid stored replay chunks.');const chunk=new Uint8Array(chunks[i].data);stored.set(chunk,at);at+=chunk.length;}
 }
 if(stored.length>MAX_REPLAY)throw new Error('Invalid stored replay size.');
 if(asPackage){
  // Only viewer metadata; never include installation identity or moderation data.
  const {player,...metadata}=publicRun(run);
  const encoded=new TextEncoder().encode(JSON.stringify(metadata));
  const bytes=new Uint8Array(4+encoded.length+stored.length);
  new DataView(bytes.buffer).setUint32(0,encoded.length,true);bytes.set(encoded,4);bytes.set(stored,4+encoded.length);
  return new Response(bytes,{headers:{...headers,'Content-Type':'application/octet-stream','Content-Disposition':`attachment; filename="replay-${id}.idreplay"`}});
 }
 return new Response(replayCsv(await decodeReplay(stored)),{headers:{...headers,'Content-Type':'text/csv; charset=utf-8','Content-Disposition':`attachment; filename="replay-${id}.csv"`}});
}
async function handle(request,env){
 const url=new URL(request.url),path=url.pathname;
 if(request.method==='GET'&&(path==='/'||path==='/admin'))return new Response(html,{headers:{...headers,'Content-Type':'text/html; charset=utf-8','Content-Security-Policy':"default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'"}});
 if(request.method==='GET'&&path==='/health')return json({ok:true,service:'Initial D community times',ruleset:env.RULESET});
 const ip=request.headers.get('CF-Connecting-IP')||'local';
 if(path==='/api/v1/activity'&&request.method==='GET'){
  await limit(env.PUBLIC_LIMIT,'activity-read:'+ip);
  const row=await env.DB.prepare("SELECT value FROM settings WHERE key='online_activity'").first();
  const activity=row?JSON.parse(row.value):null;
  return json(activity&&activity.generatedAt<=now()&&now()-activity.generatedAt<45?{available:true,...activity,validFor:45-(now()-activity.generatedAt)}:{available:false});
 }
 if(path==='/api/v1/activity'&&request.method==='POST'){
  await limit(env.PUBLIC_LIMIT,'activity-write:'+ip);
  const bearer=/^Bearer ([a-f0-9]{64})$/.exec(request.headers.get('Authorization')||'');
  if(!bearer)return json({error:'Installation authentication required.'},401);
  const device=await env.DB.prepare('SELECT id,blocked FROM devices WHERE token_hash=?').bind(await sha(bearer[1])).first();
  if(!device)return json({error:'Unknown installation.'},401);
  if(device.blocked)return json({error:'This installation is blocked.'},403);
  await limit(env.WRITE_LIMIT,'activity:'+device.id);
  const x=await body(request);
  if(!['online','queuing','racing','age'].every(k=>Number.isInteger(x[k]))||x.online<1||x.online>51||x.queuing<0||x.racing<0||x.queuing+x.racing>x.online||x.age<0||x.age>30||typeof x.limited!=='boolean'||(!x.limited&&x.online>50))return json({error:'Invalid activity.'},400);
  // A report is one game-scoped Steam survey, not a new player to add.
  // Keep its original age and reject late surveys so retries cannot extend it.
  const activity={online:x.online,queuing:x.queuing,racing:x.racing,limited:x.limited,generatedAt:now()-x.age};
  await env.DB.prepare("INSERT INTO settings(key,value) VALUES ('online_activity',?) ON CONFLICT(key) DO UPDATE SET value=excluded.value WHERE CAST(json_extract(settings.value,'$.generatedAt') AS INTEGER)<=?").bind(JSON.stringify(activity),activity.generatedAt).run();
  return json({ok:true});
 }
 if(path.startsWith('/api/admin/')){
  if(request.method!=='GET')originCheck(request,url);
  if(path==='/api/admin/login'&&request.method==='POST'){
   await limit(env.AUTH_LIMIT,'admin:'+ip);const data=await body(request);
   if(typeof data.key!=='string'||data.key.length>256||!sameSecret(await sha(data.key),env.ADMIN_KEY_SHA256))return json({error:'Incorrect admin key.'},401);
   const t=token();await env.DB.prepare('INSERT INTO admin_sessions VALUES (?,?)').bind(await sha(t),now()+43200).run();
   return json({ok:true},200,{'Set-Cookie':`idas_admin=${t}; HttpOnly; Secure; SameSite=Strict; Path=/api/admin; Max-Age=43200`});
  }
  const session=await admin(request,env);
  if(path==='/api/admin/replay'&&request.method==='GET'){
   await limit(env.PUBLIC_LIMIT,'replay:'+ip);
   const id=url.searchParams.get('id');if(!/^[-a-f0-9]{36}$/.test(id||''))return json({error:'Invalid run ID.'},400);
   const run=await env.DB.prepare('SELECT * FROM runs WHERE id=?').bind(id).first();
   if(!run)return json({error:'Run not found.'},404);
   return downloadReplay(env,run,url.searchParams.get('format')==='package');
  }
  if(path==='/api/admin/logout'&&request.method==='POST'){await env.DB.prepare('DELETE FROM admin_sessions WHERE token_hash=?').bind(session.token_hash).run();return json({ok:true},200,{'Set-Cookie':'idas_admin=; HttpOnly; Secure; SameSite=Strict; Path=/api/admin; Max-Age=0'});}
  if(path==='/api/admin/runs'&&request.method==='GET'){
   const page=Math.min(1000,Math.max(0,parseInt(url.searchParams.get('page')||'0')||0));
   const result=await env.DB.prepare('SELECT r.*,d.blocked FROM runs r JOIN devices d ON d.id=r.device_id ORDER BY r.created_at DESC,r.id LIMIT 50 OFFSET ?').bind(page*50).all();
   const epoch=Number((await env.DB.prepare("SELECT value FROM settings WHERE key='epoch'").first()).value);
   return json({epoch,runs:result.results.map(r=>({...publicRun(r),hidden:r.hidden,blocked:r.blocked,reason:r.reason})),page});
  }
  if(path==='/api/admin/audit'&&request.method==='GET')return json({events:(await env.DB.prepare('SELECT * FROM audit ORDER BY id DESC LIMIT 100').all()).results});
  if(request.method==='POST'&&['/api/admin/run','/api/admin/player','/api/admin/reset'].includes(path)){
   const x=await body(request);if(typeof x.reason!=='string'||x.reason.trim().length<3||x.reason.length>300)return json({error:'Give a reason (3–300 characters).'},400);
   let query,action,target=x.id||'';
   if(path.endsWith('/reset')){if(x.confirm!=='RESET RANKINGS')return json({error:'Type RESET RANKINGS to confirm.'},400);query=env.DB.prepare("UPDATE settings SET value=CAST(value AS INTEGER)+1 WHERE key='epoch'");action='reset';target='all boards';}
   else{
    if(!/^[a-f0-9-]{36}$/.test(target)||![0,1].includes(x.value))return json({error:'Invalid moderation request.'},400);
    if(path.endsWith('/run')){query=env.DB.prepare('UPDATE runs SET hidden=?,reason=? WHERE id=?').bind(x.value,x.reason,target);action=x.value?'hide run':'restore run';}
    else{query=env.DB.prepare('UPDATE devices SET blocked=? WHERE id=?').bind(x.value,target);action=x.value?'block installation':'unblock installation';}
   }
   await env.DB.batch([query,env.DB.prepare('INSERT INTO audit(created_at,action,target,reason) VALUES (?,?,?,?)').bind(now(),action,target,x.reason.trim())]);
   return json({ok:true});
  }
  return json({error:'Not found.'},404);
 }
 if(path==='/api/v1/register'&&request.method==='POST'){
  await limit(env.AUTH_LIMIT,'register:'+ip);const x=await body(request);
  // The installation supplies a random secret, making retries idempotent.
  if(!/^[a-f0-9]{64}$/.test(x.token||''))return json({error:'Invalid installation token.'},400);
  const hash=await sha(x.token);await env.DB.prepare('INSERT OR IGNORE INTO devices(id,token_hash,created_at) VALUES (?,?,?)').bind(crypto.randomUUID(),hash,now()).run();
  const d=await env.DB.prepare('SELECT id,blocked FROM devices WHERE token_hash=?').bind(hash).first();
  return d.blocked?json({error:'This installation is blocked.'},403):json({player:d.id});
 }
 if((path==='/api/v1/runs'||path==='/api/v2/runs')&&request.method==='POST'){
  await limit(env.PUBLIC_LIMIT,'upload:'+ip);
  if(path==='/api/v1/runs')return json({error:'A replay is required. Update the game to upload new Time Attack runs.'},426);
  const bearer=/^Bearer ([a-f0-9]{64})$/.exec(request.headers.get('Authorization')||'');if(!bearer)return json({error:'Installation authentication required.'},401);
  const d=await env.DB.prepare('SELECT id,blocked FROM devices WHERE token_hash=?').bind(await sha(bearer[1])).first();
  if(!d)return json({error:'Unknown installation.'},401);if(d.blocked)return json({error:'This installation is blocked.'},403);
  await limit(env.WRITE_LIMIT,'device:'+d.id);
  let payload,replay=null,replayHash='';
  if(path==='/api/v2/runs'){
   const data=await rawBody(request,MAX_REPLAY+8196);
   if(data.length<4)throw new Error('Invalid replay envelope.');
   const size=new DataView(data.buffer).getUint32(0,true);
   if(size<2||size>8192||4+size>=data.length)throw new Error('Invalid replay envelope.');
   payload=JSON.parse(new TextDecoder().decode(data.subarray(4,4+size)));replay=data.slice(4+size);
  }
  const x=validateRun(payload,env.RULESET);
  const minimumBuild=env.MIN_CLIENT_BUILD||MIN_CLIENT_BUILD;
  // Permanent rejection lets updated clients discard ineligible old queued
  // races instead of retrying them forever and blocking new finishes.
  if(!supportedBuild(x.build,minimumBuild))return json({error:`This run requires game build ${minimumBuild} or newer. Update and complete a new Time Attack.`,code:'client_build_too_old',minimumBuild},409);
  const rawReplay=await decodeReplay(replay);validateReplay(rawReplay,x);replayHash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',rawReplay)),v=>v.toString(16).padStart(2,'0')).join('');
  if(new DataView(replay.buffer,replay.byteOffset,replay.byteLength).getUint32(0,true)!==0x32474449)replay=await compressReplay(rawReplay);
  const existing=await env.DB.prepare('SELECT * FROM runs WHERE id=?').bind(x.id).first();
  if(existing){
   if(existing.device_id!==d.id)return json({error:'Run ID already used.'},409);
   if(replay&&(existing.replay_sha256!==replayHash||existing.ticks!==x.ticks6000||existing.condition!==x.condition||existing.weather!==x.weather||existing.car!==x.car||existing.glyphs!==JSON.stringify(x.nameGlyphs)||existing.splits!==JSON.stringify(x.splits)||existing.manual!==x.manual||existing.night!==x.night||existing.points!==x.points||existing.build!==x.build||existing.epoch!==x.epoch))return json({error:'Run ID already used with different data.'},409);
   return json({ok:true,duplicate:true,replay:!!existing.replay_size});
  }
  const epoch=Number((await env.DB.prepare("SELECT value FROM settings WHERE key='epoch'").first()).value);
  if(x.epoch!==epoch)return json({error:'This run belongs to an earlier season.'},409);
  const insert=env.DB.prepare('INSERT INTO runs(id,device_id,ruleset,epoch,condition,weather,car,ticks,glyphs,splits,manual,night,points,build,created_at,imported,replay_size,replay_sha256) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)').bind(x.id,d.id,env.RULESET,epoch,x.condition,x.weather,x.car,x.ticks6000,JSON.stringify(x.nameGlyphs),JSON.stringify(x.splits),x.manual,x.night,x.points,x.build,now(),x.imported,replay?.length||0,replayHash);
  const statements=[insert];
  for(let at=0,part=0;at<replay.length;at+=1000000,part++)statements.push(env.DB.prepare('INSERT INTO replay_chunks(run_id,part,data) VALUES (?,?,?)').bind(x.id,part,replay.slice(at,at+1000000).buffer));
  await env.DB.batch(statements);
  return json({ok:true,replay:!!replay});
 }
 if(path==='/api/v1/replay'&&request.method==='GET'){
  await limit(env.PUBLIC_LIMIT,'replay:'+ip);
  const id=url.searchParams.get('id');if(!/^[-a-f0-9]{36}$/.test(id||''))return json({error:'Invalid run ID.'},400);
  // Apply the public board's visibility rules before reading any replay blobs.
  const run=await env.DB.prepare(`SELECT r.* FROM runs r JOIN devices d ON d.id=r.device_id
   WHERE r.id=? AND r.ruleset=? AND r.epoch=CAST((SELECT value FROM settings WHERE key='epoch') AS INTEGER)
   AND r.hidden=0 AND d.blocked=0 AND r.replay_size>0`).bind(id,env.RULESET).first();
  if(!run)return json({error:'Replay not available.'},404);
  return downloadReplay(env,run,true);
 }
 if(path==='/api/v1/board'&&request.method==='GET'){
  await limit(env.PUBLIC_LIMIT,'read:'+ip);
  const condition=Number(url.searchParams.get('condition')),weather=Number(url.searchParams.get('weather')),car=Number(url.searchParams.get('car')??-1);
  if(!Number.isInteger(condition)||condition<0||condition>=COURSES.length*2||![0,1].includes(weather)||!Number.isInteger(car)||car< -1||car>34)return json({error:'Invalid board.'},400);
  const epoch=Number((await env.DB.prepare("SELECT value FROM settings WHERE key='epoch'").first()).value);
  const rows=(await env.DB.prepare(`WITH ranked AS (SELECT r.*,ROW_NUMBER() OVER(PARTITION BY r.device_id,r.car ORDER BY r.ticks,r.created_at,r.id) AS personal_rank FROM runs r JOIN devices d ON r.device_id=d.id WHERE r.ruleset=? AND r.epoch=? AND r.condition=? AND r.weather=? AND r.hidden=0 AND d.blocked=0 AND (?=-1 OR r.car=?)) SELECT * FROM ranked WHERE personal_rank=1 ORDER BY ticks,created_at,id LIMIT 50`).bind(env.RULESET,epoch,condition,weather,car,car).all()).results;
  return json({entries:rows.map(publicRun),epoch,generatedAt:now()});
 }
 if(path==='/api/v1/snapshot'&&request.method==='GET'){
  await limit(env.PUBLIC_LIMIT,'read:'+ip);
  if(url.searchParams.get('ruleset')!==env.RULESET)return json({error:'Update the game to use this leaderboard.'},409);
  const epoch=Number((await env.DB.prepare("SELECT value FROM settings WHERE key='epoch'").first()).value);
  const rows=(await env.DB.prepare(rankedSql).bind(env.RULESET,epoch,url.searchParams.get('imports')==='1'?1:0).all()).results;
  return json({ruleset:env.RULESET,epoch,generatedAt:now(),courses:COURSES,entries:rows.map(publicRun)});
 }
 return json({error:'Not found.'},404);
}
export default {
 async fetch(request,env){try{return await handle(request,env);}catch(e){if(e instanceof Response)return e;return json({error:e instanceof SyntaxError?'Invalid request JSON.':e.message?.startsWith('Invalid')||e.message?.startsWith('Incomplete')||e.message==='Request too large.'?e.message:'Service temporarily unavailable.'},e instanceof SyntaxError||/^(Invalid|Incomplete|Request too large)/.test(e.message||'')?400:503);}},
 async scheduled(event,env){await env.DB.prepare('DELETE FROM admin_sessions WHERE expires_at<?').bind(now()).run();}
};
