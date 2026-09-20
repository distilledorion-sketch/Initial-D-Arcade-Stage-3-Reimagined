// IDR2: 96-byte appearance header, 160-byte full simulation samples at 60 Hz.
// IDG2 wraps IDR2 with its expanded length and lossless gzip bytes.
export const MAX_REPLAY=18010000, MAX_RAW=18000000;
export async function decodeReplay(bytes){
 if(!(bytes instanceof Uint8Array)||bytes.length<16||bytes.length>MAX_REPLAY)throw new Error('Invalid replay.');
 const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
 if(view.getUint32(0,true)!==0x32474449)return bytes;
 const length=view.getUint32(4,true);if(length<96||length>MAX_RAW)throw new Error('Invalid expanded replay size.');
 const reader=new Blob([bytes.subarray(8)]).stream().pipeThrough(new DecompressionStream('gzip')).getReader();
 const raw=new Uint8Array(length);let at=0;
 try{while(true){const {value,done}=await reader.read();if(done)break;if(at+value.length>length){await reader.cancel();throw new Error('Invalid compressed replay length.');}raw.set(value,at);at+=value.length;}}
 catch{throw new Error('Invalid compressed replay.');}
 if(at!==length||new DataView(raw.buffer).getUint32(0,true)!==0x32524449)throw new Error('Invalid compressed replay payload.');
 return raw;
}
export async function compressReplay(raw){
 const gzip=new Uint8Array(await new Response(new Blob([raw]).stream().pipeThrough(new CompressionStream('gzip'))).arrayBuffer());
 const out=new Uint8Array(8+gzip.length);new DataView(out.buffer).setUint32(0,0x32474449,true);new DataView(out.buffer).setUint32(4,raw.length,true);out.set(gzip,8);return out;
}
export function validateReplay(bytes,run){
 if(!(bytes instanceof Uint8Array)||bytes.length<96||bytes.length>MAX_RAW||run.imported)throw new Error('Invalid replay.');
 const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),count=v.getUint32(8,true);
 if(v.getUint32(0,true)!==0x32524449||v.getUint32(4,true)!==run.ticks6000||v.getUint32(12,true)!==60||count<2||count>108001||v.getUint32(16,true)!==96||v.getUint32(20,true)!==160||v.getUint32(24,true)!==1||v.getUint32(28,true)!==12||v.getUint32(92,true)!==0||bytes.length!==96+count*160)throw new Error('Invalid detailed replay header or finish time.');
 if(v.getUint32(32,true)>1||v.getUint32(36,true)>15||v.getUint32(60,true)>5)throw new Error('Invalid replay appearance.');
 for(let i=2;i<7;i++)if(v.getUint32(32+i*4,true)>221)throw new Error('Invalid replay driver name.');
 let previous=-1,lastElapsed=0;
 for(let i=0;i<count;i++){
  const at=96+i*160,tick=v.getUint32(at,true),gear=v.getUint32(at+24,true),elapsed=v.getUint32(at+96,true);
  // The source finish gate corrects the final HUD clock by up to two ticks.
  const invalidClock=elapsed>run.ticks6000+200||(elapsed<lastElapsed&&(i!==count-1||lastElapsed-elapsed>200));
  if((i===0&&tick>1)||tick<=previous||(i>0&&tick-previous!==1)||tick>108000||gear>6||invalidClock||v.getUint32(at+120,true)>4||v.getUint32(at+124,true)<1||v.getUint32(at+124,true)>4||v.getUint32(at+140,true)>1||v.getUint32(at+148,true)>1)throw new Error('Invalid replay timeline or HUD state.');
  for(const n of [1,2,3,4,5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,36,38])if(!Number.isFinite(v.getFloat32(at+n*4,true)))throw new Error('Invalid replay sample.');
  if(Math.abs(v.getFloat32(at+28,true))>100000||Math.abs(v.getFloat32(at+20,true))>1000)throw new Error('Invalid replay RPM or speed.');
  previous=tick;lastElapsed=elapsed;
 }
 if(previous*100<run.ticks6000||previous*100-run.ticks6000>200||lastElapsed!==run.ticks6000)throw new Error('Invalid replay duration.');
 return count;
}
export function replayCsv(bytes){
 const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),detailed=v.getUint32(0,true)===0x32524449,count=v.getUint32(8,true),stride=detailed?160:28,header=detailed?96:16;
 let i=-1;const encode=new TextEncoder();
 return new ReadableStream({pull(controller){
  if(i<0){controller.enqueue(encode.encode('tick,speed,yaw,pos_x,pos_y,pos_z,gear,finish_ticks6000'+(detailed?',rpm,body_x,body_y,body_z,pitch,roll,steering,suspension_fr,suspension_fl,suspension_rr,suspension_rl,wheel_fr,wheel_fl,wheel_rr,wheel_rl,throttle,brake,elapsed6000,remaining6000':'')+'\n'));i=0;}
  let rows='';const end=Math.min(count,i+256);
  for(;i<end;i++){const p=header+stride*i,row=[v.getUint32(p,true),v.getFloat32(p+20,true),v.getFloat32(p+16,true),v.getFloat32(p+4,true),v.getFloat32(p+8,true),v.getFloat32(p+12,true),v.getUint32(p+24,true),i===count-1?v.getUint32(4,true):0];if(detailed){for(let n=7;n<=23;n++)row.push(v.getFloat32(p+n*4,true));row.push(v.getUint32(p+96,true),v.getInt32(p+100,true));}rows+=row.join(',')+'\n';}
  if(rows)controller.enqueue(encode.encode(rows));if(i===count)controller.close();
 }});
}
