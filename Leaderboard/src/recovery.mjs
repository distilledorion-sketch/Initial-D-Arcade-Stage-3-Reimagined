import {publicRun} from './core.mjs';
import {decodeReplay} from './replay.mjs';
import {readDatabaseReplay,replayKey,storeObjectReplay} from './replay-storage.mjs';
import {qualifiesForBoard,pruneStatement} from './retention.mjs';

// Used by an authenticated, temporary recovery worker, never a public route.
// SOURCE is a historical database kept separate from the current production DB.
const hash=async bytes=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),v=>v.toString(16).padStart(2,'0')).join('');
const columns=['id','device_id','ruleset','epoch','condition','weather','car','ticks','glyphs','splits','manual','night','points','build','created_at','hidden','reason','imported','replay_size','replay_sha256'];
export async function recoverRun(env,id){
 if(!/^[-a-f0-9]{36}$/.test(id??''))throw Error('Invalid recovery ID.');
 if(!env.SOURCE||!env.REPLAYS||env.RETAIN_TOP_TEN!=='true')throw Error('Recovery configuration missing.');
 const row=await env.SOURCE.prepare('SELECT * FROM runs WHERE id=?').bind(id).first();
 if(!row)throw Error('Source run missing.');
 const existing=await env.DB.prepare('SELECT * FROM runs WHERE id=?').bind(id).first();
 if(existing){
  if(columns.some(k=>existing[k]!==row[k]))throw Error('Existing run differs; refusing to overwrite.');
  return {id,status:'already_present'};
 }
 const device=await env.DB.prepare('SELECT id,blocked FROM devices WHERE id=?').bind(row.device_id).first();
 const epoch=Number((await env.DB.prepare("SELECT value FROM settings WHERE key='epoch'").first()).value);
 if(!device)throw Error('Source installation missing from current database.');
 if(device.blocked||row.hidden||row.epoch!==epoch||row.ruleset!==env.RULESET)return {id,status:'ineligible'};
 if(!row.replay_size)throw Error('Source run has no replay.');
 if(!await qualifiesForBoard(env,publicRun(row),row.device_id,row.created_at))return {id,status:'outside_top_ten'};
 const bytes=await readDatabaseReplay({DB:env.SOURCE},row);
 if(!bytes||await hash(await decodeReplay(bytes))!==row.replay_sha256)throw Error('Source replay verification failed.');
 const key=replayKey(row).replace(/\.idr$/,`-recovery-${crypto.randomUUID()}.idr`);
 try{
  const checksum=await storeObjectReplay(env,row,bytes,key);
  const object=await env.REPLAYS.get(key);
  if(!object||object.size!==bytes.length||await hash(await object.arrayBuffer())!==checksum)throw Error('Recovery replay readback failed.');
  await env.DB.batch([
   env.DB.prepare(`INSERT INTO runs (${columns.join(',')}) VALUES (${columns.map(()=>'?').join(',')})`).bind(...columns.map(k=>row[k])),
   env.DB.prepare('INSERT INTO replay_objects(run_id,object_key) VALUES (?,?)').bind(id,key),
   env.DB.prepare('INSERT INTO audit(created_at,action,target,reason) VALUES (?,?,?,?)').bind(Math.floor(Date.now()/1000),'recover model record',id,'Recover original score and verified replay from pre-cleanup database'),
   pruneStatement(env)
  ]);
 }catch(error){
  const committed=await env.DB.prepare('SELECT object_key FROM replay_objects WHERE run_id=?').bind(id).first();
  if(committed?.object_key!==key)await env.DB.prepare('INSERT OR IGNORE INTO replay_object_deletions VALUES (?,?)').bind(key,Math.floor(Date.now()/1000)).run();
  throw error;
 }
 const retained=!!await env.DB.prepare('SELECT id FROM runs WHERE id=?').bind(id).first();
 return {id,status:retained?'recovered':'outside_top_ten',bytes:bytes.length};
}
