export const COURSES=['Myogi','Usui','Akagi','Akina','Happogahara','Irohazaka','Shomaru','Tsuchisaka','Akina Snow','Hakone','Sadamine','Enna Skyline','Myogi (Special Stage)','Usui (Special Stage)','Momiji Line'];
export const REQUIRED_CLIENT_BUILD='0.3.95-community-replays.32';
export function supportedBuild(build,required=REQUIRED_CLIENT_BUILD){
 return typeof required==='string'&&/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)-[a-z][a-z0-9-]*\.(0|[1-9]\d*)$/.test(required)&&build===required;
}
export function validateRun(x,ruleset){
 if(x&&((x.mode??0)!==0||(x.outcome??0)!==0||(x.opponentBytes??0)!==0))throw new Error('Invalid Time Attack submission: personal battle or incomplete replay.');
 if(!x||typeof x!=='object')throw new Error('Invalid run.');
 x.imported??=0;
 const integer=(v,a,b)=>Number.isInteger(v)&&v>=a&&v<=b;
 if(![0,1].includes(x.imported)||x.ruleset!==ruleset||!integer(x.epoch,1,1000000)||!/^[-a-f0-9]{36}$/.test(x.id||'')||!integer(x.condition,0,COURSES.length*2-1)||!integer(x.weather,0,1)||!integer(x.car,0,34)||!integer(x.ticks6000,60000,10799999))throw new Error('Invalid run or incompatible handling version.');
 if(!Array.isArray(x.nameGlyphs)||x.nameGlyphs.length!==5||!x.nameGlyphs.every(v=>integer(v,0,221)))throw new Error('Invalid driver name.');
 if(!Array.isArray(x.splits)||x.splits.length!==4||!x.splits.every(v=>integer(v,0,x.ticks6000)))throw new Error('Invalid checkpoints.');
 const passed=x.splits.filter(v=>v>0);
 if(!(x.imported===1&&passed.length===0)&&(passed.length<2||passed[passed.length-1]!==x.ticks6000||passed.some((v,i)=>i>0&&v<=passed[i-1])||x.splits.some((v,i)=>v>0&&i>0&&x.splits[i-1]===0)))throw new Error('Incomplete or unordered checkpoints.');
 if(!integer(x.manual,x.imported?-1:0,1)||!integer(x.night,x.imported?-1:0,1)||!integer(x.points,x.imported?-1:0,999999)||typeof x.build!=='string'||x.build.length>64||!/^[a-zA-Z0-9._-]+$/.test(x.build))throw new Error('Invalid race details.');
 if((x.condition===16||x.condition===17)&&(x.weather!==1||(!x.imported&&x.night!==1)))throw new Error('Invalid snow conditions.');
 return x;
}
export function publicRun(r){return {id:r.id,player:r.device_id,epoch:r.epoch,imported:r.imported,replayAvailable:!!r.replay_size,condition:r.condition,weather:r.weather,car:r.car,ticks6000:r.ticks,nameGlyphs:JSON.parse(r.glyphs),splits:JSON.parse(r.splits),manual:r.manual,night:r.night,points:r.points,build:r.build,createdAt:r.created_at};}
export const rankedSql=`WITH personal AS (
 SELECT r.*,ROW_NUMBER() OVER(PARTITION BY r.device_id,r.condition,r.weather,r.car ORDER BY r.ticks,r.created_at,r.id) AS personal_rank
 FROM runs r JOIN devices d ON r.device_id=d.id
 WHERE r.ruleset=? AND r.epoch=? AND (r.imported=0 OR ?=1) AND r.hidden=0 AND d.blocked=0
), ranked AS (
 SELECT *,ROW_NUMBER() OVER(PARTITION BY condition,weather ORDER BY ticks,created_at,id) AS course_rank,
 ROW_NUMBER() OVER(PARTITION BY condition,weather,car ORDER BY ticks,created_at,id) AS model_rank
 FROM personal WHERE personal_rank=1
) SELECT * FROM ranked WHERE course_rank<=10 OR model_rank=1 ORDER BY condition,weather,ticks,created_at,id LIMIT ${COURSES.length*2*2*44}`;
export async function sha(text){return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(text))),v=>v.toString(16).padStart(2,'0')).join('');}
export function token(){const bytes=crypto.getRandomValues(new Uint8Array(32));return Array.from(bytes,v=>v.toString(16).padStart(2,'0')).join('');}
export function sameSecret(a,b){if(typeof a!=='string'||typeof b!=='string'||a.length!==64||b.length!==64)return false;let diff=0;for(let i=0;i<64;i++)diff|=a.charCodeAt(i)^b.charCodeAt(i);return diff===0;}
