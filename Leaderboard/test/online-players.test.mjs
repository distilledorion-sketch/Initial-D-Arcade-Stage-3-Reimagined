import {test,beforeEach,after} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {DatabaseSync} from 'node:sqlite';
import {sha} from '../src/core.mjs';
import {CONNECT_JWKS_URL,createConnectVerifier,checkOnlinePlayer,searchOnlinePlayers} from '../src/online-players.mjs';

const config={EOS_CLIENT_ID:'game-client',EOS_PRODUCT_ID:'game-product',EOS_SANDBOX_ID:'game-sandbox',EOS_DEPLOYMENT_ID:'game-live'};
const self='1'.repeat(32),other='2'.repeat(32),unknown='3'.repeat(32),build='0.3.95-community-replays.51';
const keyPair=await crypto.subtle.generateKey({name:'RSASSA-PKCS1-v1_5',modulusLength:2048,publicExponent:new Uint8Array([1,0,1]),hash:'SHA-256'},true,['sign','verify']);
const badPair=await crypto.subtle.generateKey({name:'RSASSA-PKCS1-v1_5',modulusLength:2048,publicExponent:new Uint8Array([1,0,1]),hash:'SHA-256'},true,['sign','verify']);
const jwk={...await crypto.subtle.exportKey('jwk',keyPair.publicKey),kid:'test-key',alg:'RS256',use:'sig'};
const encode=x=>Buffer.from(typeof x==='string'?x:JSON.stringify(x)).toString('base64url');
const seconds=()=>Math.floor(Date.now()/1000);
const claims=(extra={},time=seconds())=>({iss:'https://api.epicgames.dev/auth/v1',aud:config.EOS_CLIENT_ID,
 pfpid:config.EOS_PRODUCT_ID,pfsid:config.EOS_SANDBOX_ID,pfdid:config.EOS_DEPLOYMENT_ID,sub:self,iat:time-60,exp:time+3600,...extra});
async function token(payload=claims(),header={alg:'RS256',kid:jwk.kid},privateKey=keyPair.privateKey){
 const input=encode(header)+'.'+encode(payload),signature=await crypto.subtle.sign('RSASSA-PKCS1-v1_5',privateKey,new TextEncoder().encode(input));
 return input+'.'+Buffer.from(signature).toString('base64url');
}
const request=jwt=>new Request('https://example.test/api/v1/online/player',{headers:{Authorization:'Bearer '+jwt}});
function keyResponse(){return new Response(JSON.stringify({keys:[jwk]}),{headers:{'Cache-Control':'max-age=600'}});}
const originalFetch=globalThis.fetch;
let keyFetches=0;
globalThis.fetch=async(url,options)=>{assert.equal(url,CONNECT_JWKS_URL);assert.equal(options.redirect,'manual');keyFetches++;return keyResponse();};
after(()=>{globalThis.fetch=originalFetch;db?.close();});
let source=readFileSync(new URL('../src/worker.mjs',import.meta.url),'utf8').replace("import html from './index.html';","const html='test';");
for(const file of ['core','replay','replay-storage','retention','online-players','times-only'])source=source.replace(`'./${file}.mjs'`,JSON.stringify(new URL(`../src/${file}.mjs`,import.meta.url).href));
const worker=(await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'))).default;
let db,env;
const adminKey='a'.repeat(64);
beforeEach(async()=>{
 db?.close();db=new DatabaseSync(':memory:');
 for(const migration of ['0001_leaderboard','0012_online_players'])db.exec(readFileSync(new URL(`../migrations/${migration}.sql`,import.meta.url),'utf8'));
 const wrap=(sql,args=[])=>({bind(...bound){return wrap(sql,bound);},async first(){return db.prepare(sql).get(...args)||null;},async all(){return {results:db.prepare(sql).all(...args)};},async run(){return {meta:db.prepare(sql).run(...args)};}});
 env={...config,RULESET:'d3-community-v1',ADMIN_KEY_SHA256:await sha(adminKey),DB:{prepare:wrap,async batch(queries){
  db.exec('BEGIN');try{const out=[];for(const query of queries)out.push(await query.run());db.exec('COMMIT');return out;}catch(error){db.exec('ROLLBACK');throw error;}
 }}};
 for(const key of ['PUBLIC_LIMIT','AUTH_LIMIT','WRITE_LIMIT','ONLINE_LIMIT','ONLINE_AUTH_LIMIT'])env[key]={async limit(){return {success:true};}};
});
async function call(path,data,headers={}){
 return worker.fetch(new Request('https://example.test'+path,{method:data===undefined?'GET':'POST',headers:{...(data===undefined?{}:{'Content-Type':'application/json'}),...headers},body:data===undefined?undefined:JSON.stringify(data)}),env);
}
async function login(){
 const response=await call('/api/admin/login',{key:adminKey},{Origin:'https://example.test'});assert.equal(response.status,200);
 return {Cookie:response.headers.get('Set-Cookie').split(';')[0],Origin:'https://example.test'};
}
async function post(data,extra={}){return call('/api/v1/online/player',{build,...data},{Authorization:'Bearer '+await token(claims(extra))});}
async function rejectsStatus(action,status){await assert.rejects(action,error=>error instanceof Response&&error.status===status);}

test('Connect signature and scoped claims authenticate only the signed subject',async()=>{
 const verify=createConnectVerifier(async url=>{assert.equal(url,CONNECT_JWKS_URL);return keyResponse();});
 assert.equal(await verify(request(await token()),config),self);
 const response=await post({username:' Deon ',targetPlayerId:unknown});assert.equal(response.status,200);
 const data=await response.json();assert.deepEqual(Object.keys(data).sort(),['version','playerId','blocked','status','targetPlayerId','targetBlocked','targetStatus','checkedAt'].sort());
 assert.equal(data.version,1);assert.equal(data.playerId,self);assert.equal(data.status,'allowed');assert.equal(data.blocked,false);
 assert.equal(data.targetPlayerId,unknown);assert.equal(data.targetStatus,'allowed');assert.equal(data.targetBlocked,false);
 assert.ok(Math.abs(data.checkedAt-seconds())<=2);
 assert.equal(db.prepare('SELECT username FROM online_players').get().username,'Deon');
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM devices').get().n,0);
 assert.equal((await post({username:'Fake',playerId:other})).status,400);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_players').get().n,1);
});

test('forged, malformed, wrong-scope, expired and future tokens never authenticate or write',async()=>{
 const verify=createConnectVerifier(async()=>keyResponse());
 const changedClaims=[{aud:'another-client'},{aud:[config.EOS_CLIENT_ID]},{pfpid:'another-product'},{pfsid:'another-sandbox'},
  {pfdid:'another-deployment'},{iss:'https://api.epicgames.dev.evil.test/auth/v1'},{iss:'https://evil.test@api.epicgames.dev/auth/v1'},
  {iss:'http://api.epicgames.dev/auth/v1'},{iss:'https://api.epicgames.dev:444/auth/v1'},
  {exp:seconds()-1},{exp:seconds()},{iat:seconds()+60},{nbf:seconds()+60},{iat:'123'},{exp:null},{sub:'not-a-puid'},{sub:'0'.repeat(32)}];
 for(const change of changedClaims)await rejectsStatus(()=>token(claims(change)).then(t=>verify(request(t),config)),401);
 for(const header of [{alg:'none',kid:jwk.kid},{alg:'HS256',kid:jwk.kid},{alg:'RS256'},
  {alg:'RS256',kid:jwk.kid,crit:['jku']},{alg:'RS256',kid:jwk.kid,b64:false}])
  await rejectsStatus(()=>token(claims(),header).then(t=>verify(request(t),config)),401);
 const fake=await token(claims(),{alg:'RS256',kid:jwk.kid,jku:'https://attacker.test/keys',jwk:await crypto.subtle.exportKey('jwk',badPair.publicKey)},badPair.privateKey);
 await rejectsStatus(()=>verify(request(fake),config),401);
 const real=await token(),parts=real.split('.');parts[1]=encode(claims({sub:other}));
 await rejectsStatus(()=>verify(request(parts.join('.')),config),401);
 for(const value of ['abc.def.ghi',real+'.extra','x'.repeat(18000),''])await rejectsStatus(()=>verify(request(value),config),401);
 assert.equal((await call('/api/v1/online/player',{username:'Deon',build},{Authorization:'Bearer '+fake})).status,401);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_players').get().n,0);
});

test('missing configuration and JWKS failures fail closed, including expired caches',async()=>{
 let ticks=1700000000000,fetches=0,fail=false;
 const verify=createConnectVerifier(async()=>{fetches++;if(fail)throw new Error('offline');return keyResponse();},()=>ticks);
 const jwt=await token(claims({},ticks/1000));
 for(const key of Object.keys(config)){
  const missing={...config};delete missing[key];await rejectsStatus(()=>verify(request(jwt),missing),503);
 }
 assert.equal(fetches,0);
 assert.equal(await verify(request(jwt),config),self);assert.equal(fetches,1);
 fail=true;ticks+=1000;assert.equal(await verify(request(jwt),config),self);assert.equal(fetches,1);
 ticks+=601000;await rejectsStatus(()=>verify(request(jwt),config),503);assert.equal(fetches,2);
 await rejectsStatus(()=>verify(request(jwt),config),503);assert.equal(fetches,2);
 const malformed=createConnectVerifier(async()=>new Response('{broken'));
 await rejectsStatus(async()=>malformed(request(await token()),config),503);
 const absent=createConnectVerifier(async()=>new Response('{"keys":[]}'));
 await rejectsStatus(async()=>absent(request(await token()),config),503);
 delete env.EOS_DEPLOYMENT_ID;assert.equal((await post({username:'Deon'})).status,503);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_players').get().n,0);
});

test('random key IDs cannot cause unbounded JWKS refreshes and key rotation is supported',async()=>{
 let ticks=1700000000000,fetches=0,rotated=false;
 const second={...await crypto.subtle.exportKey('jwk',badPair.publicKey),kid:'rotated',alg:'RS256'};
 const verify=createConnectVerifier(async()=>{fetches++;return new Response(JSON.stringify({keys:rotated?[jwk,second]:[jwk]}));},()=>ticks);
 const payload=claims({},ticks/1000);
 await verify(request(await token(payload)),config);
 for(let i=0;i<20;i++)await rejectsStatus(()=>token(payload,{alg:'RS256',kid:'random-'+i}).then(t=>verify(request(t),config)),401);
 assert.equal(fetches,1);
 ticks+=30001;rotated=true;
 assert.equal(await verify(request(await token(payload,{alg:'RS256',kid:'rotated'},badPair.privateKey)),config),self);
 assert.equal(fetches,2);
});

test('manual key fetch never follows a redirect to another key source',async()=>{
 const calls=[];
 const verify=createConnectVerifier(async(url,options)=>{
  calls.push(url);assert.equal(options.redirect,'manual');
  return new Response(null,{status:302,headers:{Location:'https://attacker.test/keys'}});
 });
 await rejectsStatus(async()=>verify(request(await token()),config),503);
 assert.deepEqual(calls,[CONNECT_JWKS_URL]);
});

test('policy-only checks create no records and preserve a registered name and ban',async()=>{
 for(const username of [undefined,null,'','   ']){
  const response=await post({username,targetPlayerId:other});assert.equal(response.status,200);assert.equal((await response.json()).targetStatus,'allowed');
 }
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_players').get().n,0);
 await post({username:'Deon'});const admin=await login();
 assert.equal((await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Harassment in lobby'},admin)).status,200);
 const result=await(await post({username:'',targetPlayerId:self})).json();
 assert.equal(result.blocked,true);assert.equal(result.status,'blocked');assert.equal(result.targetBlocked,true);assert.equal(result.targetStatus,'blocked');
 const row=db.prepare('SELECT * FROM online_players').get();assert.equal(row.username,'Deon');assert.equal(row.blocked,1);
 assert.equal(row.ban_reason,'Harassment in lobby');assert.ok(row.banned_at>0);
});

test('renames retain full searchable history and bans; duplicate names identify separate IDs',async()=>{
 await checkOnlinePlayer(env,self,{username:'Deon',build},100);
 await checkOnlinePlayer(env,other,{username:'Deon',build},101);
 const admin=await login();await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Reviewed abuse'},admin);
 assert.equal((await post({username:'Different'})).status,200);
 let result=await(await call('/api/admin/online-players?q=deon',undefined,admin)).json();
 assert.equal(result.players.length,2);assert.equal(result.hasMore,false);
 const banned=result.players.find(p=>p.playerId===self),clean=result.players.find(p=>p.playerId===other);
 assert.equal(banned.username,'Different');assert.deepEqual(banned.aliases,['Deon']);assert.equal(banned.firstSeen,100);assert.equal(banned.blocked,true);
 assert.equal(clean.blocked,false);assert.equal(clean.bannedAt,null);
 const peer=await(await post({targetPlayerId:self},{sub:other})).json();assert.equal(peer.blocked,false);assert.equal(peer.targetBlocked,true);
 assert.equal((await call('/api/admin/online-player',{playerId:self,blocked:false,reason:'Appeal accepted'},admin)).status,200);
 const unbanned=await(await post({targetPlayerId:self},{sub:other})).json();assert.equal(unbanned.targetBlocked,false);
 assert.equal(db.prepare('SELECT banned_at FROM online_players WHERE player_id=?').get(self).banned_at,null);
 assert.deepEqual(db.prepare('SELECT action FROM audit ORDER BY id').all().map(x=>x.action),['ban online player','unban online player']);
});

test('name normalization counts Unicode scalars, preserves NFC and searches Unicode case-insensitively',async()=>{
 assert.equal((await post({username:'  De\u0301on  '})).status,200);
 assert.equal(db.prepare('SELECT username FROM online_players').get().username,'Déon');
 assert.equal((await searchOnlinePlayers(env,new URLSearchParams({q:'DÉON'}))).players.length,1);
 assert.equal((await post({username:'車'.repeat(16)})).status,200);
 assert.equal((await post({username:'😀'.repeat(16)})).status,200);
 for(const username of ['😀'.repeat(17),'a'.repeat(17),'De\u0000on','De\u202eon','<Deon>','De\nOn','\tDeon','\ud800','\u0378','\u0301','\u20dd',' \u0301\u20dd ',12,{},['Deon']])
  assert.equal((await post({username})).status,400,JSON.stringify(username));
 await post({username:'Straße'});
 assert.equal((await searchOnlinePlayers(env,new URLSearchParams({q:'STRASSE'}))).players.length,1);
});

test('directory is private, same-origin moderation is mandatory, and unknown IDs cannot be banned',async()=>{
 await post({username:'Deon'});
 for(const path of ['/api/admin/online-players?q=Deon','/api/admin/online-players'])assert.equal((await call(path)).status,401);
 for(const path of ['/api/v1/online/players?q=Deon','/api/v1/online/player','/api/online-players'])assert.equal((await call(path)).status,404);
 assert.equal((await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Test reason'})).status,403);
 const admin=await login();
 assert.equal((await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Test reason'},{...admin,Origin:'https://evil.test'})).status,403);
 assert.equal((await call('/api/admin/online-player',{playerId:unknown,blocked:true,reason:'Test reason'},admin)).status,404);
 for(const change of [{blocked:1},{reason:'x'},{reason:'x'.repeat(301)},{reason:'Bad\u202etext'},{playerId:"' OR 1=1 --"}])
  assert.equal((await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Valid reason',...change},admin)).status,400);
 assert.equal(db.prepare('SELECT blocked FROM online_players').get().blocked,0);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM audit').get().n,0);
 db.exec('UPDATE admin_sessions SET expires_at=0');assert.equal((await call('/api/admin/online-players',undefined,admin)).status,401);
});

test('search treats wildcard and SQL syntax literally, bounds pages and never duplicates a player',async()=>{
 await checkOnlinePlayer(env,self,{username:'%_Deon',build});await checkOnlinePlayer(env,self,{username:'%_Deon2',build});
 await checkOnlinePlayer(env,other,{username:'Deon',build});
 assert.equal((await searchOnlinePlayers(env,new URLSearchParams({q:'%_'}))).players.length,1);
 assert.equal((await searchOnlinePlayers(env,new URLSearchParams({q:"' OR 1=1 --"}))).players.length,0);
 assert.equal((await searchOnlinePlayers(env,new URLSearchParams({q:self.toUpperCase()}))).players[0].playerId,self);
 for(const params of [{page:'-1'},{page:'1001'},{page:'1.5'},{q:'x'.repeat(65)}])await rejectsStatus(()=>searchOnlinePlayers(env,new URLSearchParams(params)),400);
 for(let i=10;i<61;i++)await checkOnlinePlayer(env,i.toString(16).padStart(32,'0'),{username:'Racer',build},i);
 const page0=await searchOnlinePlayers(env,new URLSearchParams({q:'racer'}));assert.equal(page0.players.length,50);assert.equal(page0.hasMore,true);
 const page1=await searchOnlinePlayers(env,new URLSearchParams({q:'racer',page:'1'}));assert.equal(page1.players.length,1);assert.equal(page1.hasMore,false);
 assert.equal(new Set([...page0.players,...page1.players].map(p=>p.playerId)).size,51);
});

test('database failures roll back both directory writes and moderation audit',async()=>{
 db.exec("CREATE TRIGGER reject_alias BEFORE INSERT ON online_player_names BEGIN SELECT RAISE(ABORT,'disk failure'); END;");
 assert.equal((await post({username:'Deon'})).status,503);assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_players').get().n,0);
 db.exec('DROP TRIGGER reject_alias');await post({username:'Deon'});const admin=await login();
 db.exec("CREATE TRIGGER reject_audit BEFORE INSERT ON audit BEGIN SELECT RAISE(ABORT,'audit failure'); END;");
 assert.equal((await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Reviewed abuse'},admin)).status,503);
 assert.equal(db.prepare('SELECT blocked FROM online_players').get().blocked,0);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM audit').get().n,0);
});

test('history responses are bounded while older aliases remain searchable',async()=>{
 for(let i=0;i<55;i++)await checkOnlinePlayer(env,self,{username:'Alias'+i,build},100+i*86400);
 const result=await searchOnlinePlayers(env,new URLSearchParams({q:'Alias0'}));
 assert.equal(result.players.length,1);assert.equal(result.players[0].aliases.length,50);assert.equal(result.players[0].aliasesTruncated,true);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_player_names').get().n,55);
});

test('missing or rejecting rate limits fail closed before registration',async()=>{
 delete env.ONLINE_LIMIT;assert.equal((await post({username:'Deon'})).status,429);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_players').get().n,0);
 env.ONLINE_LIMIT={async limit(){return {success:false};}};assert.equal((await post({username:'Deon'})).status,429);
 env.ONLINE_LIMIT={async limit(){return {success:true};}};delete env.ONLINE_AUTH_LIMIT;
 assert.equal((await post({username:'Deon'})).status,429);
});

test('unchanged metadata writes at most once per minute while ban reads stay fresh',async()=>{
 db.exec(`CREATE TABLE observed_writes(kind TEXT);
  CREATE TRIGGER observe_player_update AFTER UPDATE ON online_players BEGIN INSERT INTO observed_writes VALUES ('player'); END;
  CREATE TRIGGER observe_alias_update AFTER UPDATE ON online_player_names BEGIN INSERT INTO observed_writes VALUES ('alias'); END;`);
 const data={username:'Deon',build};
 await checkOnlinePlayer(env,self,data,100);
 for(const tick of [100,101,120,145,159])await checkOnlinePlayer(env,self,data,tick);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM observed_writes').get().n,0);
 assert.equal(db.prepare('SELECT last_seen FROM online_players').get().last_seen,100);
 assert.equal(db.prepare('SELECT last_seen FROM online_player_names').get().last_seen,100);
 await checkOnlinePlayer(env,self,data,160);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM observed_writes').get().n,2);
 assert.equal(db.prepare('SELECT last_seen FROM online_player_names').get().last_seen,160);
 db.prepare('UPDATE online_players SET blocked=1,banned_at=161,ban_reason=?').run('Reviewed abuse');
 assert.equal((await checkOnlinePlayer(env,self,data,161)).blocked,true);
 db.prepare('UPDATE online_players SET blocked=0,banned_at=NULL').run();
 assert.equal((await checkOnlinePlayer(env,self,data,162)).blocked,false);
 await checkOnlinePlayer(env,self,{username:'Changed',build},163);
 assert.equal(db.prepare('SELECT username,last_seen FROM online_players').get().last_seen,163);
 await checkOnlinePlayer(env,self,{username:'Changed',build:build+'.1'},164);
 assert.equal(db.prepare('SELECT build,last_seen FROM online_players').get().last_seen,164);
 assert.equal(db.prepare('SELECT last_seen FROM online_player_names WHERE username=?').get('Changed').last_seen,164);
});

test('atomic throttle skips alias writes even if concurrent metadata prechecks see an old row',async()=>{
 const data={username:'Deon',build};await checkOnlinePlayer(env,self,data,100);
 const originalPrepare=env.DB.prepare;
 env.DB.prepare=(sql)=>{
  if(sql==='SELECT username,build,last_seen FROM online_players WHERE player_id=?')return {bind(){return this;},async first(){return {username:'Deon',build,last_seen:100};}};
  return originalPrepare(sql);
 };
 await checkOnlinePlayer(env,self,data,160);
 await checkOnlinePlayer(env,self,data,161);
 assert.equal(db.prepare('SELECT last_seen FROM online_players').get().last_seen,160);
 assert.equal(db.prepare('SELECT last_seen FROM online_player_names').get().last_seen,160);
});

test('24-new-name rolling limit preserves history, allows old names and never blocks policy-only ban checks',async()=>{
 const start=seconds()-100;
 for(let i=0;i<24;i++)await checkOnlinePlayer(env,self,{username:'Alias'+i,build},start+i);
 const before=db.prepare('SELECT * FROM online_players').get();
 const rejected=await post({username:'OneTooMany'});assert.equal(rejected.status,400);
 assert.equal((await rejected.json()).code,'online_name_change_limit');
 assert.deepEqual(db.prepare('SELECT * FROM online_players').get(),before);
 assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_player_names').get().n,24);
 const admin=await login();await call('/api/admin/online-player',{playerId:self,blocked:true,reason:'Reviewed abuse'},admin);
 assert.equal((await(await post({username:null})).json()).blocked,true);
 assert.equal((await(await post({username:'Alias0'})).json()).blocked,true);
 const changed=await checkOnlinePlayer(env,self,{username:'NewDayName',build},start+86400);
 assert.equal(changed.blocked,true);assert.equal(db.prepare('SELECT COUNT(*) AS n FROM online_player_names').get().n,25);
 assert.equal(db.prepare('SELECT first_seen FROM online_player_names WHERE username=?').get('Alias0').first_seen,start);
});

test('online pre-auth uses a distinct shared-network limit and authenticated checks remain per identity',async()=>{
 const calls=[];
 env.PUBLIC_LIMIT={async limit(){throw new Error('Ordinary public limiter must not handle online authentication.');}};
 env.ONLINE_AUTH_LIMIT={async limit(x){calls.push(['ip',x.key]);return {success:true};}};
 env.ONLINE_LIMIT={async limit(x){calls.push(['player',x.key]);return {success:true};}};
 assert.equal((await post({username:'Deon'})).status,200);
 assert.deepEqual(calls,[['ip','online-auth:local'],['player','online-player:'+self]]);
});
