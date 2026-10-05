import {MAX_REPLAY,decodeReplay} from './replay.mjs';

const hex=bytes=>Array.from(new Uint8Array(bytes),v=>v.toString(16).padStart(2,'0')).join('');
const digest=bytes=>crypto.subtle.digest('SHA-256',bytes);

export function replayKey(run){
 if(!/^[-a-f0-9]{36}$/.test(run.id)||!/^[-a-f0-9]{36}$/.test(run.device_id)||! /^[a-f0-9]{64}$/.test(run.replay_sha256)||!Number.isInteger(run.replay_size)||run.replay_size<16||run.replay_size>MAX_REPLAY)throw new Error('Replay storage identity is invalid.');
 // A concurrent upload of different data under the same ID cannot overwrite
 // the winning run's replay. The bucket is private; downloads check SQL visibility.
 return `replays/v1/${run.device_id}/${run.id}/${run.replay_sha256}/${run.replay_size}.idr`;
}

export async function readDatabaseReplay(env,run){
 const row=await env.DB.prepare('SELECT data FROM replays WHERE run_id=?').bind(run.id).first();
 let stored;
 if(row)stored=new Uint8Array(row.data);
 else{
  const chunks=(await env.DB.prepare('SELECT part,data FROM replay_chunks WHERE run_id=? ORDER BY part').bind(run.id).all()).results;
  if(!chunks.length)return null;
  const size=chunks.reduce((n,c)=>n+new Uint8Array(c.data).byteLength,0);
  if(size>MAX_REPLAY)throw new Error('Replay storage size is invalid.');
  stored=new Uint8Array(size);let at=0;
  for(let i=0;i<chunks.length;i++){
   if(chunks[i].part!==i)throw new Error('Replay storage chunks are incomplete.');
   const chunk=new Uint8Array(chunks[i].data);stored.set(chunk,at);at+=chunk.length;
  }
 }
 if(stored.length!==run.replay_size||stored.length>MAX_REPLAY)throw new Error('Replay storage size is invalid.');
 return stored;
}

async function readObjectReplay(env,run){
 const mapping=await env.DB.prepare('SELECT object_key FROM replay_objects WHERE run_id=?').bind(run.id).first();
 const object=await env.REPLAYS.get(mapping?.object_key??replayKey(run));
 if(!object)return null;
 if(object.size!==run.replay_size||object.size>MAX_REPLAY)throw new Error('Replay storage object size is invalid.');
 const bytes=new Uint8Array(await object.arrayBuffer());
 if(bytes.length!==object.size)throw new Error('Replay storage object is incomplete.');
 return bytes;
}

export async function readStoredReplay(env,run){
 // Legacy captures remain downloadable throughout the migration.
 if(env.REPLAYS){const stored=await readObjectReplay(env,run);if(stored)return stored;}
 return readDatabaseReplay(env,run);
}

export async function storeObjectReplay(env,run,stored,key=replayKey(run)){
 if(!env.REPLAYS)throw new Error('Replay object storage is not configured.');
 const checksum=await digest(stored);
 const object=await env.REPLAYS.put(key,stored,{sha256:checksum,httpMetadata:{contentType:'application/octet-stream'},customMetadata:{replaySha256:run.replay_sha256}});
 if(!object||object.size!==stored.length)throw new Error('Replay object storage did not confirm the upload.');
 // Do not delete this object if a later SQL write fails: an overlapping retry
 // may already have committed the same run. Retrying reuses the same key.
 return hex(checksum);
}

export async function moveReplayToObjectStorage(env,run,reason){
 if(!env.REPLAYS)throw new Error('Replay object storage is not configured.');
 const stored=await readDatabaseReplay(env,run);
 if(!stored){
  const existing=await readObjectReplay(env,run);
  if(!existing||hex(await digest(await decodeReplay(existing)))!==run.replay_sha256)throw new Error('Replay object storage verification failed.');
  return {ok:true,alreadyMoved:true,bytes:run.replay_size};
 }
 if(hex(await digest(await decodeReplay(stored)))!==run.replay_sha256)throw new Error('Replay database verification failed.');
 const checksum=await storeObjectReplay(env,run,stored);
 const copied=await readObjectReplay(env,run);
 if(!copied||hex(await digest(copied))!==checksum)throw new Error('Replay object storage readback failed.');
 // Only remove the old blob after a complete, byte-for-byte verified copy.
 // Score metadata, replay identity, moderation and season remain unchanged.
 await env.DB.batch([
  env.DB.prepare('DELETE FROM replays WHERE run_id=?').bind(run.id),
  env.DB.prepare('DELETE FROM replay_chunks WHERE run_id=?').bind(run.id),
  env.DB.prepare('INSERT INTO audit(created_at,action,target,reason) VALUES (?,?,?,?)').bind(Math.floor(Date.now()/1000),'move replay storage',run.id,reason),
  env.DB.prepare('INSERT OR IGNORE INTO replay_object_deletions(object_key,created_at) SELECT ?,? WHERE NOT EXISTS(SELECT 1 FROM runs WHERE id=?)').bind(replayKey(run),Math.floor(Date.now()/1000),run.id)
 ]);
 return {ok:true,alreadyMoved:false,bytes:stored.length,sha256:checksum};
}
