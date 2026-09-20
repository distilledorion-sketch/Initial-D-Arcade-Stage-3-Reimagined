from pathlib import Path
import json,struct,collections
root=Path('outputs/InitialDRemake')
for name in ('TYPE','SELECT'):
 d=json.loads(Path('work/music-final-'+name+'.json').read_text())
 end=d['loops'][1]['tick'];ev=[e for e in d['events'] if e['tick']<end]
 h=collections.Counter(e['command']>>28 for e in ev)
 channels={}
 missing=[]
 for e in ev:
  c=e['command'];ch=(c>>24)&15
  if c>>28==9 and ((c>>8)&127):
   x=channels.setdefault(ch,dict(commands=0,keyons=0,retunes=0,layers=set(),samples=set(),first_tick=e['tick']))
   x['commands']+=1;x['keyons']+=len(e['voices']);x['retunes']+=len(e.get('retunes',[]))
   for v in e['voices']:x['layers'].add(v['layer']);x['samples'].add(v['sample'])
   if not e['voices'] and not e.get('retunes'):missing.append((e['tick'],hex(c)))
 b=(root/'data/original_audio/selection'/f'{name.lower()}.idms').read_bytes();v=struct.unpack_from('<8s6I',b)
 out=collections.Counter();raw=[];rows=[]
 for off in range(32,len(b),112):
  e=struct.unpack_from('<8I18I8B',b,off);out[e[0]]+=1;rows.append(e)
  if e[0]==0:raw.append((e[1],e[2]))
 assert raw==[(e['tick'],e['command']) for e in ev]
 assert out[1]==sum(len(e['voices']) for e in ev)
 assert out[4]==sum(len(e.get('retunes',[])) for e in ev)
 print(name,'raw',len(ev),'nibbles',dict(h),'IDMS kinds',dict(out),'unhandled keyons',missing)
 for ch,x in sorted(channels.items()):
  x['layers']=sorted(x['layers']);x['samples']=sorted(x['samples']);print('channel',ch,x)
 print('controllers',dict(collections.Counter(((e['command']>>24)&15,(e['command']>>16)&127) for e in ev if e['command']>>28==11)))
 print('raw-other',[(e['tick'],hex(e['command'])) for e in ev if e['command']>>28 not in (8,9,11,14)][:20])
