// The v2 client sends a replay envelope. Read only its JSON prefix and cancel
// the remaining stream: existing players can still post times without storing,
// decompressing or processing their recording. New clients use JSON /v3/runs.
export const replaysDisabled=env=>env.DISABLE_REPLAYS==='true';
export async function legacyRunMetadata(request){
 const reader=request.body?.getReader();
 if(!reader)throw new Error('Invalid replay envelope.');
 const prefix=new Uint8Array(8196);let used=0,needed=4;
 try{
  while(used<needed){
   const {done,value}=await reader.read();
   if(done)throw new Error('Invalid replay envelope.');
   let at=0;
   while(at<value.length&&used<needed){
    const count=Math.min(needed-used,value.length-at);
    prefix.set(value.subarray(at,at+count),used);used+=count;at+=count;
    if(used===4&&needed===4){
     const size=new DataView(prefix.buffer).getUint32(0,true);
     if(size<2||size>8192)throw new Error('Invalid replay envelope.');
     needed=4+size;
    }
   }
  }
  try{return JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(prefix.subarray(4,needed)));}
  catch{throw new SyntaxError('Invalid request JSON.');}
 }finally{try{await reader.cancel();}catch{}}
}

// The private bucket is shared with no other game features. Restrict every
// sweep to the replay namespace, including unclaimed legacy upload attempts.
export async function purgeReplayObjects(env,limit=500){
 if(!replaysDisabled(env))throw new Error('Replay deletion requires disabled replay storage.');
 if(!env.REPLAYS)return {removed:0,bytes:0,more:false};
 const page=await env.REPLAYS.list({prefix:'replays/v1/',limit});
 if(page.objects.some(x=>!x.key.startsWith('replays/v1/')))throw new Error('Invalid replay object prefix.');
 if(page.objects.length)await env.REPLAYS.delete(page.objects.map(x=>x.key));
 return {removed:page.objects.length,bytes:page.objects.reduce((n,x)=>n+x.size,0),more:page.truncated};
}
