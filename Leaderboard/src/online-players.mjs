// EOS Connect IDs, never Auth/Epic account IDs or leaderboard installation IDs.
// Reference: https://dev.epicgames.com/docs/epic-online-services/eos-fundamentals/connect-interface/connect-reference
export const CONNECT_JWKS_URL='https://api.epicgames.dev/auth/v1/oauth/jwks';
const encoder=new TextEncoder();
const decoder=new TextDecoder('utf-8',{fatal:true});
const invalidText=/[\p{Cc}\p{Cf}\p{Cs}\p{Cn}\p{Zl}\p{Zp}<>]/u;
const idPattern=/^[a-f0-9]{32}$/;
const pageSize=50,aliasLimit=50;
const securityHeaders={'Cache-Control':'no-store','Content-Type':'application/json','X-Content-Type-Options':'nosniff'};
function fail(status,error,code){throw new Response(JSON.stringify({error,code}),{status,headers:securityHeaders});}
const invalidToken=()=>fail(401,'Online identity could not be verified. Sign in to online play again.','online_auth_invalid');
const unavailable=()=>fail(503,'Online identity verification is temporarily unavailable. Try again.','online_auth_unavailable');
function configuration(env){
 const names=['EOS_CLIENT_ID','EOS_PRODUCT_ID','EOS_SANDBOX_ID','EOS_DEPLOYMENT_ID'];
 if(names.some(k=>typeof env[k]!=='string'||!env[k]||env[k]!==env[k].trim()||env[k].length>128||/^REPLACE_/i.test(env[k])))
  fail(503,'Online player verification is not configured.','online_not_configured');
 return env;
}
function bytes(value){
 if(typeof value!=='string'||!value||!/^[A-Za-z0-9_-]+$/.test(value)||value.length%4===1)invalidToken();
 try{return Uint8Array.from(atob(value.replaceAll('-','+').replaceAll('_','/')+'='.repeat((4-value.length%4)%4)),c=>c.charCodeAt(0));}
 catch{invalidToken();}
}
function jsonPart(value){
 try{const x=JSON.parse(decoder.decode(bytes(value)));if(!x||typeof x!=='object'||Array.isArray(x))invalidToken();return x;}
 catch{invalidToken();}
}
function playerId(value){
 if(typeof value!=='string'||!idPattern.test(value.toLowerCase())||/^0+$/.test(value))
  fail(400,'Invalid online Player ID.','invalid_player_id');
 return value.toLowerCase();
}
function issuerValid(value){
 if(typeof value!=='string')return false;
 try{const u=new URL(value);return u.origin==='https://api.epicgames.dev'&&!u.username&&!u.password&&!u.hash&&!u.search;}
 catch{return false;}
}
// Factory exists for isolated tests. Production always uses the fixed URL above;
// token jku/x5u URLs, client-provided keys and Worker environment URLs are ignored.
export function createConnectVerifier(fetchKeys=(...args)=>fetch(...args),clock=()=>Date.now()){
 let cache=null,inFlight=null,nextRefresh=0;
 async function refresh(){
  if(inFlight)return inFlight;
  if(clock()<nextRefresh)return false;
  nextRefresh=clock()+30000; // Bound random-kid traffic and key-service failures.
  inFlight=(async()=>{
   const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),5000);
   let stage='fetch',httpStatus=0;
   try{
    // Workers supports manual/follow redirects; reject redirects explicitly
    // while keeping key downloads pinned to the official Connect endpoint.
    const response=await fetchKeys(CONNECT_JWKS_URL,{redirect:'manual',signal:controller.signal,headers:{Accept:'application/json'}});
    stage='response';httpStatus=response.status;
    if(!response.ok)unavailable();
    stage='reader';
    const reader=response.body?.getReader();if(!reader)unavailable();
    let count=0;const parts=[];
    while(true){const {done,value}=await reader.read();if(done)break;count+=value.length;if(count>65536){await reader.cancel();unavailable();}parts.push(value);}
    const all=new Uint8Array(count);let offset=0;for(const part of parts){all.set(part,offset);offset+=part.length;}
    stage='json';const document=JSON.parse(decoder.decode(all));
    if(!Array.isArray(document.keys)||document.keys.length===0||document.keys.length>32)unavailable();
    stage='import';const keys=new Map();
    for(const key of document.keys){
     if(key.kty!=='RSA'||(key.use&&key.use!=='sig')||(key.alg&&key.alg!=='RS256')||
        typeof key.kid!=='string'||!key.kid||key.kid.length>128||keys.has(key.kid)||
        (key.key_ops&&(!Array.isArray(key.key_ops)||!key.key_ops.includes('verify'))))continue;
     const imported=await crypto.subtle.importKey('jwk',key,{name:'RSASSA-PKCS1-v1_5',hash:'SHA-256'},false,['verify']);
     if(imported.algorithm.modulusLength<2048)continue;
     keys.set(key.kid,imported);
    }
    if(!keys.size)unavailable();
    const maxAge=/\bmax-age=(\d+)/i.exec(response.headers.get('Cache-Control')||'');
    const ttl=Math.max(30,Math.min(3600,maxAge?Number(maxAge[1]):600));
    cache={keys,expires:clock()+ttl*1000};return true;
   }catch{
    // Deliberately fixed fields only: no request, JWT, user ID, key ID, remote
    // response body, exception message or credentials enter diagnostics.
    console.warn('EOS Connect key verification unavailable',stage,httpStatus);
    unavailable();
   }finally{clearTimeout(timeout);}
  })();
  try{return await inFlight;}finally{inFlight=null;}
 }
 return async function verify(request,env){
  configuration(env);
  const authorization=request.headers.get('Authorization')||'';
  if(authorization.length>17000)invalidToken();
  const match=/^Bearer ([A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+)$/i.exec(authorization);
  if(!match)invalidToken();
  const pieces=match[1].split('.');
  if(pieces[0].length>2048||pieces[1].length>14000||pieces[2].length>2048)invalidToken();
  const header=jsonPart(pieces[0]),claims=jsonPart(pieces[1]);
  if(header.alg!=='RS256'||typeof header.kid!=='string'||!header.kid||header.kid.length>128||header.crit!==undefined||header.b64!==undefined)invalidToken();
  const timestamp=Math.floor(clock()/1000);
  if(!issuerValid(claims.iss)||claims.aud!==env.EOS_CLIENT_ID||claims.pfpid!==env.EOS_PRODUCT_ID||
     claims.pfsid!==env.EOS_SANDBOX_ID||claims.pfdid!==env.EOS_DEPLOYMENT_ID||
     !Number.isSafeInteger(claims.iat)||claims.iat<0||claims.iat>timestamp||
     !Number.isSafeInteger(claims.exp)||claims.exp<=timestamp||claims.exp<=claims.iat||
     (claims.nbf!==undefined&&(!Number.isSafeInteger(claims.nbf)||claims.nbf>timestamp))||
     typeof claims.sub!=='string'||!idPattern.test(claims.sub.toLowerCase())||/^0+$/.test(claims.sub))invalidToken();
  if(!cache||cache.expires<=clock()){
   if(!await refresh()||!cache||cache.expires<=clock())unavailable();
  }else if(!cache.keys.has(header.kid))await refresh();
  const key=cache.keys.get(header.kid);if(!key)invalidToken();
  let valid=false;
  try{valid=await crypto.subtle.verify('RSASSA-PKCS1-v1_5',key,bytes(pieces[2]),encoder.encode(pieces[0]+'.'+pieces[1]));}catch{}
  if(!valid)invalidToken();
  return claims.sub.toLowerCase();
 };
}
export const verifyConnectIdentity=createConnectVerifier();

export function normalizeOnlineUsername(value){
 if(value===undefined||value===null||value==='')return null;
 if(typeof value!=='string'||value.length>128||invalidText.test(value))
  fail(400,'Invalid online username.','invalid_username');
 const name=value.normalize('NFC').trim();
 if(!name)return null;
 if(!/[^\s\p{Mn}\p{Me}]/u.test(name))fail(400,'Online usernames must include a visible character.','invalid_username');
 if([...name].length>16)fail(400,'Online usernames must be at most 16 characters.','invalid_username');
 return name;
}
const nameKey=value=>value.normalize('NFC').toUpperCase().normalize('NFC');
function objectBody(data,allowed){
 if(!data||typeof data!=='object'||Array.isArray(data)||Object.keys(data).some(k=>!allowed.includes(k)))
  fail(400,'Invalid online player request.','invalid_online_request');
}
export async function checkOnlinePlayer(env,self,data,timestamp=Math.floor(Date.now()/1000)){
 objectBody(data,['username','build','targetPlayerId']);
 const username=normalizeOnlineUsername(data.username);
 if(typeof data.build!=='string'||!/^[A-Za-z0-9][A-Za-z0-9._+\-]{0,95}$/.test(data.build))
  fail(400,'Invalid game build.','invalid_build');
 const target=data.targetPlayerId===undefined||data.targetPlayerId===null||data.targetPlayerId===''?'':playerId(data.targetPlayerId);
 // self is returned only by the signature verifier, never taken from JSON.
 const id=playerId(self);
 if(username){
  // Avoid even opening a write transaction for ordinary repeated peer checks.
  // The SQL predicate also guards a concurrent request that saw the old row.
  const previous=await env.DB.prepare('SELECT username,build,last_seen FROM online_players WHERE player_id=?').bind(id).first();
  if(!previous||previous.username!==username||previous.build!==data.build||previous.last_seen<=timestamp-60){
   try{await env.DB.batch([
    env.DB.prepare(`INSERT INTO online_players(player_id,username,first_seen,last_seen,build) VALUES (?,?,?,?,?)
     ON CONFLICT(player_id) DO UPDATE SET username=excluded.username,last_seen=MAX(online_players.last_seen,excluded.last_seen),build=excluded.build
     WHERE online_players.username<>excluded.username OR online_players.build<>excluded.build OR online_players.last_seen<=excluded.last_seen-60`).bind(id,username,timestamp,timestamp,data.build),
    // D1 batch runs these statements sequentially in one SQLite transaction.
    // changes() refers to the preceding UPSERT, not an unrelated connection.
    env.DB.prepare(`INSERT INTO online_player_names(player_id,username,name_key,first_seen,last_seen)
     SELECT ?,?,?,?,? WHERE changes()>0
     ON CONFLICT(player_id,username) DO UPDATE SET last_seen=MAX(online_player_names.last_seen,excluded.last_seen)`).bind(id,username,nameKey(username),timestamp,timestamp)
   ]);}catch(error){
    if(String(error?.message).includes('online_name_daily_limit'))
     fail(400,'You have used 24 new online names in the last day. Choose a previous name or try again later.','online_name_change_limit');
    throw error;
   }
  }
 }
 // Policy-only requests create no anonymous directory record. Missing rows
 // mean no local ban; only administrators can ban an already registered ID.
 const rows=(await env.DB.prepare('SELECT player_id,blocked FROM online_players WHERE player_id IN (?,?)').bind(id,target||id).all()).results;
 const blocked=rows.some(row=>row.player_id===id&&row.blocked===1);
 const targetBlocked=!!target&&rows.some(row=>row.player_id===target&&row.blocked===1);
 return {version:1,playerId:id,blocked,status:blocked?'blocked':'allowed',targetPlayerId:target,targetBlocked,
  targetStatus:target?(targetBlocked?'blocked':'allowed'):'',checkedAt:timestamp};
}

export async function searchOnlinePlayers(env,params){
 const raw=params.get('q')||'',pageText=params.get('page')||'0';
 if(raw.length>128||invalidText.test(raw)||[...raw.normalize('NFC').trim()].length>64||!/^\d{1,4}$/.test(pageText)||Number(pageText)>1000)
  fail(400,'Invalid player search.','invalid_player_search');
 const q=raw.normalize('NFC').trim(),page=Number(pageText),args=[];
 let where='';
 if(q){
  if(idPattern.test(q.toLowerCase())){where='WHERE p.player_id=?';args.push(q.toLowerCase());}
  else{
   // Prefix bounds keep the alias index usable and interpret %, _ and quotes
   // literally. Never interpolate search strings or IDs into SQL.
   const key=nameKey(q);where='WHERE p.player_id IN (SELECT player_id FROM online_player_names WHERE name_key>=? AND name_key<?)';
   args.push(key,key+'\u{10ffff}');
  }
 }
 const result=(await env.DB.prepare(`SELECT p.* FROM online_players p ${where} ORDER BY p.last_seen DESC,p.player_id LIMIT ? OFFSET ?`).bind(...args,pageSize+1,page*pageSize).all()).results;
 const selected=result.slice(0,pageSize),players=[];
 if(selected.length){
  const ids=selected.map(row=>row.player_id),placeholders=ids.map(()=>'?').join(',');
  const names=(await env.DB.prepare(`SELECT player_id,username FROM (
   SELECT n.player_id,n.username,ROW_NUMBER() OVER(PARTITION BY n.player_id ORDER BY n.last_seen DESC,n.username) AS rank
   FROM online_player_names n JOIN online_players p ON p.player_id=n.player_id
   WHERE n.player_id IN (${placeholders}) AND n.username<>p.username
  ) WHERE rank<=? ORDER BY player_id,rank`).bind(...ids,aliasLimit+1).all()).results;
  for(const row of selected){
   const aliases=names.filter(n=>n.player_id===row.player_id).map(n=>n.username);
   players.push({playerId:row.player_id,username:row.username,aliases:aliases.slice(0,aliasLimit),aliasesTruncated:aliases.length>aliasLimit,
    firstSeen:row.first_seen,lastSeen:row.last_seen,build:row.build,blocked:row.blocked===1,banReason:row.ban_reason,bannedAt:row.banned_at});
  }
 }
 return {players,page,hasMore:result.length>pageSize};
}
export async function moderateOnlinePlayer(env,data,timestamp=Math.floor(Date.now()/1000)){
 objectBody(data,['playerId','blocked','reason']);
 const id=playerId(data.playerId);
 if(typeof data.blocked!=='boolean'||typeof data.reason!=='string'||invalidText.test(data.reason)||[...data.reason.trim()].length<3||[...data.reason.trim()].length>300)
  fail(400,'Give a reason (3–300 characters) and the new ban status.','invalid_online_moderation');
 const existing=await env.DB.prepare('SELECT player_id FROM online_players WHERE player_id=?').bind(id).first();
 if(!existing)fail(404,'This online Player ID has not registered with the service.','online_player_not_found');
 const reason=data.reason.trim();
 await env.DB.batch([
  env.DB.prepare('UPDATE online_players SET blocked=?,ban_reason=?,banned_at=? WHERE player_id=?').bind(data.blocked?1:0,reason,data.blocked?timestamp:null,id),
  env.DB.prepare('INSERT INTO audit(created_at,action,target,reason) VALUES (?,?,?,?)').bind(timestamp,data.blocked?'ban online player':'unban online player',id,reason)
 ]);
 return {ok:true,playerId:id,blocked:data.blocked};
}
