import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const html=readFileSync(new URL('../src/index.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick=()=>new Promise(resolve=>setImmediate(resolve));
const player=(id,username='Deon',overrides={})=>({playerId:id,username,aliases:['Previous name'],firstSeen:1720000000,lastSeen:1730000000,build:'test-build',blocked:false,banReason:'',bannedAt:null,...overrides});
const firstId='1'.repeat(32),secondId='2'.repeat(32);
const response=(data,status=200)=>({ok:status>=200&&status<300,status,json:async()=>data});

class Element {
 constructor(tag='div'){this.tagName=tag;this.children=[];this.attributes={};this.hidden=false;this.disabled=false;this.value='';this.options=[];this.open=false;this._text='';}
 set textContent(value){this._text=String(value);this.children=[];}
 get textContent(){return this._text+this.children.map(child=>typeof child==='string'?child:child.textContent).join('');}
 set innerHTML(value){throw new Error('Untrusted UI content must not use innerHTML');}
 append(...children){this.children.push(...children);if(this.tagName==='select')this.options.push(...children);}
 replaceChildren(...children){this._text='';this.children=[];this.append(...children);}
 add(option){this.options.push(option);if(this.options.length===1)this.value=option.value;}
 setAttribute(name,value){this.attributes[name]=String(value);}
 showModal(){this.open=true;}
 close(){this.open=false;}
 focus(){}
}

function app(pathname='/admin',handler){
 const elements=new Map();
 for(const match of html.matchAll(/<([a-z][a-z0-9]*)\b([^>]*\bid="([^"]+)"[^>]*)>/g)){
  const el=new Element(match[1]);el.hidden=/\bhidden\b/.test(match[2]);elements.set(match[3],el);
 }
 for(const [id,value] of [['course','0'],['direction','0'],['weather','0'],['car','-1']])elements.get(id).value=value;
 elements.get('direction').options=[new Element('option'),new Element('option')];
 const calls=[];
 const context={
  document:{hidden:false,getElementById:id=>elements.get(id),createElement:tag=>new Element(tag),addEventListener(){}},
  location:{pathname,reload(){}},Option:class {constructor(text,value){this.textContent=text;this.value=String(value);}},
  performance:{now:()=>0},AbortController,setTimeout:()=>1,clearTimeout(){},setInterval(){},
  fetch:async(path,options)=>{calls.push({path,options});return handler?handler(path,options):response(path.startsWith('/api/admin/runs')?{runs:[],epoch:1}:path.startsWith('/api/admin/online-players')?{players:[],page:0,hasMore:false}:path.startsWith('/api/v1/board')?{entries:[],generatedAt:1730000000,epoch:1}:{available:false});},
 };
 runInNewContext(script,context);
 return {$:id=>elements.get(id),calls,context};
}

test('public rankings never request or display the private player directory',async()=>{
 const ui=app('/');await tick();
 assert.equal(ui.$('admin').hidden,true);
 assert.equal(ui.$('adminBody').hidden,true);
 assert.equal(ui.$('onlineRows').children.length,0);
 assert.equal(ui.calls.some(call=>call.path.startsWith('/api/admin/')),false);
 ui.$('onlineSearchForm').onsubmit({preventDefault(){}});await tick();
 assert.equal(ui.calls.some(call=>call.path.startsWith('/api/admin/')),false);
});

test('an unauthenticated admin page cannot fetch the directory',async()=>{
 const ui=app('/admin',()=>response({error:'Sign in required.'},401));await tick();
 assert.equal(ui.$('adminBody').hidden,true);
 assert.equal(ui.$('login').hidden,false);
 assert.equal(ui.calls.length,1);
 assert.equal(ui.$('onlineRows').children.length,0);
});

test('same-name accounts require exact selected ID and a reason before a ban',async()=>{
 const records=[player(firstId),player(secondId)];
 const ui=app('/admin',(path)=>response(path.startsWith('/api/admin/runs')?{runs:[],epoch:1}:path.startsWith('/api/admin/online-players')?{players:records,page:0,hasMore:false}:{ok:true}));await tick();
 assert.equal(ui.$('onlineRows').children.length,2);
 ui.$('onlineRows').children[1].children[3].children[0].onclick();
 assert.equal(ui.$('onlineActionId').textContent,secondId);
 assert.equal(ui.$('onlinePlayerDialog').open,true);
 ui.$('onlineConfirmId').value=firstId;ui.$('onlineActionReason').value='Toxic lobby chat';
 await ui.$('onlineActionForm').onsubmit({preventDefault(){}});
 assert.equal(ui.calls.filter(call=>call.path==='/api/admin/online-player').length,0);
 ui.$('onlineConfirmId').value=secondId;ui.$('onlineActionReason').value='   ';
 await ui.$('onlineActionForm').onsubmit({preventDefault(){}});
 assert.equal(ui.calls.filter(call=>call.path==='/api/admin/online-player').length,0);
 ui.$('onlineActionReason').value='Toxic lobby chat';
 await ui.$('onlineActionForm').onsubmit({preventDefault(){}});
 const mutation=ui.calls.find(call=>call.path==='/api/admin/online-player');
 assert.deepEqual(JSON.parse(mutation.options.body),{playerId:secondId,blocked:true,reason:'Toxic lobby chat'});
 assert.equal(ui.$('onlinePlayerDialog').open,false);
});

test('unbanning targets the selected account and keeps untrusted names as literal text',async()=>{
 const hostile='<img src=x onerror=alert(1)>',record=player(firstId,hostile,{blocked:true,banReason:'<script>no</script>'});
 const ui=app('/admin',path=>response(path.startsWith('/api/admin/runs')?{runs:[],epoch:1}:path.startsWith('/api/admin/online-players')?{players:[record],page:0,hasMore:false}:{ok:true}));await tick();
 const row=ui.$('onlineRows').children[0];
 assert.equal(row.children[0].children[0].textContent,hostile);
 assert.match(row.children[2].textContent,/<script>no<\/script>/);
 row.children[3].children[0].onclick();
 assert.equal(ui.$('onlineActionName').textContent,hostile);
 ui.$('onlineConfirmId').value=firstId;ui.$('onlineActionReason').value='Appeal accepted';
 await ui.$('onlineActionForm').onsubmit({preventDefault(){}});
 assert.deepEqual(JSON.parse(ui.calls.find(call=>call.path==='/api/admin/online-player').options.body),{playerId:firstId,blocked:false,reason:'Appeal accepted'});
});

test('an expired session clears directory, selection, and audit data',async()=>{
 let expired=false;
 const ui=app('/admin',path=>expired?response({error:'Session expired'},401):response(path.startsWith('/api/admin/runs')?{runs:[],epoch:1}:{players:[player(firstId)],page:0,hasMore:false}));await tick();
 ui.$('onlineRows').children[0].children[3].children[0].onclick();
 ui.$('auditOutput').textContent='Private event';expired=true;
 await ui.$('onlineRefresh').onclick();
 assert.equal(ui.$('adminBody').hidden,true);
 assert.equal(ui.$('onlineRows').children.length,0);
 assert.equal(ui.$('auditOutput').textContent,'');
 assert.equal(ui.$('onlineActionId').textContent,'');
 assert.equal(ui.$('onlinePlayerDialog').open,false);
});

test('a late lookup response cannot replace newer search results',async()=>{
 let releaseOld;
 const ui=app('/admin',path=>path.includes('q=old')?new Promise(resolve=>{releaseOld=()=>resolve(response({players:[player(firstId,'old')],page:0,hasMore:false}));}):response(path.startsWith('/api/admin/runs')?{runs:[],epoch:1}:{players:path.includes('q=new')?[player(secondId,'new')]:[],page:0,hasMore:false}));await tick();
 ui.$('onlineQuery').value='old';ui.$('onlineSearchForm').onsubmit({preventDefault(){}});await tick();
 ui.$('onlineQuery').value='new';ui.$('onlineSearchForm').onsubmit({preventDefault(){}});await tick();
 releaseOld();await tick();
 assert.equal(ui.$('onlineRows').children.length,1);
 assert.equal(ui.$('onlineRows').children[0].children[0].children[0].textContent,'new');
});

test('pagination keeps the search term and starts a new search on page one',async()=>{
 const ui=app('/admin',path=>response(path.startsWith('/api/admin/runs')?{runs:[],epoch:1}:{players:[],page:Number(new URL(path,'https://test.invalid').searchParams.get('page')),hasMore:true}));await tick();
 ui.$('onlineQuery').value='old name';ui.$('onlineSearchForm').onsubmit({preventDefault(){}});await tick();
 ui.$('onlineNext').onclick();await tick();
 assert.equal(ui.calls.at(-1).path,'/api/admin/online-players?q=old%20name&page=1');
 ui.$('onlineQuery').value='Deon';ui.$('onlineSearchForm').onsubmit({preventDefault(){}});await tick();
 assert.equal(ui.calls.at(-1).path,'/api/admin/online-players?q=Deon&page=0');
});
