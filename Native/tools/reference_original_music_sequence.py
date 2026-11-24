"""Decode supplied TYPE/SELECT scores through bounded original ARM routines.

Only RAM is mapped. 0x800000 is anonymous parameter-capture RAM, never a device.
The driver startup/IRQ/runtime are never entered. The voice allocator is reset
between independent parameter proofs; hardware envelopes and release tails are
owned by the native player, and are deliberately not simulated by this tool.
"""
from pathlib import Path
import argparse, hashlib, json, os, struct

WORK=Path(os.environ.get('IDAS3_AUDIO_WORK', Path(__file__).resolve().parents[3]/'work'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_PROT_READ, UC_PROT_EXEC, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R11, UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_PC

DRIVER=Path(os.environ.get('IDAS3_AUDIO_DRIVER', WORK/'AICADRV.bin'))
DRIVER_SHA='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67'
CUE_IDS={'TYPE':0,'SELECT':1,'RESULT':2,'AFTER_L':34,'AFTER_W':35,'AFTER_W2':36,'WIN2':37}
PHYSICAL=((0xd800,0x1000),(0xedc0,64),(0xc300,16),(0x800000,0x2000))

class Reference:
    def __init__(self,name):
        self.name=name;self.driver=DRIVER.read_bytes()
        assert hashlib.sha256(self.driver).hexdigest()==DRIVER_SHA
        self.bank=(WORK/(name+'_sound.bin')).read_bytes()
        self.cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);c=self.cpu
        c.mem_map(0,0x10000);c.mem_write(0,self.driver)
        c.mem_protect(0x1000,0xb000,UC_PROT_READ|UC_PROT_EXEC)
        c.mem_map(0x100000,0x200000);c.mem_write(0x100000,self.bank)
        # Program groups contain mutable authored pan/pitch phase counters.
        c.mem_map(0x400000,0x10000);c.mem_map(0x800000,0x20000)
        c.reg_write(UC_ARM_REG_R12,0xc200)
        def guard(uc,access,address,size,value,user):
            if address<0x1000 and address+size>0x500:raise RuntimeError('code write')
        c.hook_add(UC_HOOK_MEM_WRITE,guard,begin=0,end=0xfff)
        # ARM7TDMI stores PC+12 for STM{pc}; Unicorn's default differs.
        def stm(uc,pc,size,user):
            sp=uc.reg_read(UC_ARM_REG_SP)-4;self.w(sp,pc+12)
            uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,pc+4)
        for pc in range(0x500,0xc000,4):
            if self.driver[pc:pc+4]==bytes.fromhex('00802de9'):
                c.hook_add(UC_HOOK_CODE,stm,begin=pc,end=pc)
        self.tick=0;self.pending=[];self.commands=[];self.loops=[]
        self.allocated=[];self.zeroDelays=0;self.callCount=0;self.contexts={}
        def output(uc,pc,size,user):
            command=uc.reg_read(UC_ARM_REG_R0)
            self.commands.append([self.tick,command]);self.pending.append(command)
            sp=uc.reg_read(UC_ARM_REG_SP);uc.reg_write(UC_ARM_REG_PC,self.u(sp))
            uc.reg_write(UC_ARM_REG_SP,sp+4)
        c.hook_add(UC_HOOK_CODE,output,begin=0x2b80,end=0x2b80)
        def loop(uc,pc,size,user):
            if uc.reg_read(UC_ARM_REG_R6)==0x90:
                # Before the same list jump: every live track byte is compared.
                self.loops.append(dict(tick=self.tick,eventIndex=len(self.commands),
                    tracks=self.read(0xf498,16*48).hex(),
                    channels=self.read(0xc800,0x1000).hex(),
                    bank=self.read(0xeb00,32).hex(),
                    authoredBankState=hashlib.sha256(self.read(0x100000,len(self.bank))).hexdigest(),
                    synchronization=self.read(0xf798,12).hex()))
        c.hook_add(UC_HOOK_CODE,loop,begin=0x74c8,end=0x74c8)
        def allocated(uc,pc,size,user):
            self.allocated.append((uc.reg_read(UC_ARM_REG_R7),uc.reg_read(UC_ARM_REG_R8),uc.reg_read(UC_ARM_REG_R9),uc.reg_read(UC_ARM_REG_R11)))
        c.hook_add(UC_HOOK_CODE,allocated,begin=0x3b28,end=0x3b28)
        def delay(uc,pc,size,user):
            if uc.reg_read(UC_ARM_REG_R5)==0:self.zeroDelays+=1
        c.hook_add(UC_HOOK_CODE,delay,begin=0x73cc,end=0x73cc)
        # The volume table has been selected by source3034 at this point.
        self.volumes={};self.stereoPans={}
        def stereo_pan(uc,pc,size,user):
            self.stereoPans[self.byte(0xc360)]=self.byte(0xc365)
        c.hook_add(UC_HOOK_CODE,stereo_pan,begin=0x4280,end=0x4280)
        def volume(uc,pc,size,user):
            layer=uc.reg_read(UC_ARM_REG_R8);channel=uc.reg_read(UC_ARM_REG_R11)
            command=self.u(0xc2e0);velocity=(command>>8)&255
            table=uc.reg_read(UC_ARM_REG_R7)
            self.volumes[layer]=[self.byte(table+velocity),self.byte(layer+8)>>1,
                self.byte(channel+10),self.byte(channel+16),self.byte(0xc205),
                self.byte(0xeb06),self.byte(channel)]
        c.hook_add(UC_HOOK_CODE,volume,begin=0x4300,end=0x4300)
        self.run(0x16e4);self.w(0xc280,0x100000);self.w(0xc260,0);self.run(0x1188)
        self.baseline=self.snapshot(PHYSICAL)
    def read(self,p,n):return bytes(self.cpu.mem_read(p,n))
    def byte(self,p):return self.read(p,1)[0]
    def u(self,p):return struct.unpack('<I',self.read(p,4))[0]
    def w(self,p,v):self.cpu.mem_write(p,struct.pack('<I',v))
    def snapshot(self,regions):return [(p,self.read(p,n)) for p,n in regions]
    def restore(self,snapshot):
        for p,b in snapshot:self.cpu.mem_write(p,b)
    def run(self,pc,command=None):
        if command is not None:self.cpu.reg_write(UC_ARM_REG_R0,command)
        self.w(0x400ff0,0x401000);self.cpu.reg_write(UC_ARM_REG_SP,0x400ff0)
        self.cpu.emu_start(pc,0x401000,count=100000);self.callCount+=1
        if self.cpu.reg_read(UC_ARM_REG_PC)!=0x401000:raise RuntimeError('bounded routine did not return')
    def dispatch(self,command):
        self.restore(self.baseline);self.allocated=[];self.volumes={};self.stereoPans={}
        self.run(0x4624,command)
        rows=[]
        for address,layer,group,channel in self.allocated:
            assert 0x800000<=address<0x802000 and address%128==0
            sample=struct.unpack_from('<H',self.bank,layer-0x100000+2)[0]
            source_slot=(address-0x800000)//128
            addresses=[address]
            if sample>=256:
                table=struct.unpack_from('<I',self.bank,0x3c)[0]
                flags=struct.unpack_from('<I',self.bank,table+4+16*(sample-256)+8)[0]
                assert flags in (0,0x80),'unhandled sample descriptor flags'
                if flags==0x80:
                    sibling=self.byte(0xd800+source_slot*64+23)
                    assert sibling<64 and sibling!=source_slot,'source stereo allocation missing'
                    assert self.byte(0xd800+sibling*64+23)==0xfe
                    assert self.byte(0xd800+sibling*64+10)==source_slot
                    addresses.append(0x800000+sibling*128)
            for lane,physical in enumerate(addresses):
                voice=0xd800+(physical-0x800000)//2
                row=dict(layer=layer-0x100000,sample=sample,
                    voiceFlags0=self.byte(voice),voiceFlags1=self.byte(voice+1),
                    address=physical,registers=list(struct.unpack('<18I',self.read(physical,72))),volume=self.volumes[layer],sampleChannel=lane)
                if len(addresses)==2:row['stereoPan']=self.stereoPans[source_slot]
                rows.append(row)
        if rows:
            key=((command>>24)&15,(command>>16)&127)
            # Both authored scores have fixed instruments for each channel/key.
            if key in self.contexts:
                assert [r['layer'] for r in rows]==[r['layer'] for r in self.contexts[key]['rows']]
            else:self.contexts[key]=dict(rows=rows,physical=self.snapshot(PHYSICAL))
        return rows
    def drain(self):
        result=[]
        while self.pending:
            command=self.pending.pop(0);result.append((command,self.dispatch(command)))
        return result

def capture(name):
    r=Reference(name);events=[]
    def drain_and_poll():
        for _ in range(4096):
            for command,rows in r.drain():events.append(dict(tick=r.tick,command=command,voices=rows))
            ready=False
            for i in range(16):
                at=0xf498+48*i;flags=r.byte(at)
                if flags&0x80 and not flags&0x21 and struct.unpack('<i',r.read(at+20,4))[0]<65536:ready=True
            if not ready:return
            r.run(0x7234)
        raise RuntimeError('zero-time score loop')
    sequence=struct.unpack_from('<I',r.bank,0x2c)[0]
    assert r.bank[sequence+7]==0xa8
    r.run(0x7704,(r.bank[sequence+7]<<24)|(r.bank[sequence+6]<<16))
    drain_and_poll()
    for tick in range(1,200000):
        r.tick=tick;r.run(0x716c);r.run(0x7234)
        drain_and_poll()
        if len(r.loops)==2:break
    assert len(r.loops)==2
    a,b=r.loops
    checks={key:a[key]==b[key] for key in ('tracks','channels','bank','authoredBankState','synchronization')}
    print(name,'loop states',checks,'ticks',a['tick'],b['tick'],'contexts',len(r.contexts),flush=True)
    if not all(checks.values()):
        for key,value in checks.items():
            if not value:
                aa=bytes.fromhex(a[key]);bb=bytes.fromhex(b[key]);print(key,[(i,x,y) for i,(x,y) in enumerate(zip(aa,bb)) if x!=y][:100],flush=True)
    # Keep complete evidence even if a source-state comparison fails.
    result=dict(name=name,driverSha256=DRIVER_SHA,bankSha256=hashlib.sha256(r.bank).hexdigest(),
        loops=r.loops,checks=checks,zeroDelays=r.zeroDelays,calls=r.callCount,events=events)
    out=WORK/('music-final-'+name+'.json');out.write_text(json.dumps(result))
    import pickle
    (WORK/('music-contexts-'+name+'.pickle')).write_bytes(pickle.dumps(r.contexts))
    return result

def patches(name):
    import pickle
    result=json.loads((WORK/('music-final-'+name+'.json')).read_text())
    contexts=pickle.loads((WORK/('music-contexts-'+name+'.pickle')).read_bytes())
    # Legato can reach keys that never receive a fresh keyon. Recover their
    # actual source-retuned contexts first so later controller patches cover
    # them too, including tails carried across the authored loop boundary.
    expand=Reference(name)
    for event in result['events']:
        command=event['command'];key=((command>>24)&15,(command>>16)&127)
        pre=expand.snapshot(((0xc200,0x600),(0xc800,0x1000),(0xe800,0x600)))
        rows=expand.dispatch(command)
        if command>>28!=9 or not (command>>8)&127 or rows or key in contexts:continue
        post=expand.snapshot(((0xc200,0x600),(0xc800,0x1000),(0xe800,0x600)))
        original=next(v for k,v in contexts.items() if k[0]==key[0])
        expand.restore(pre);expand.restore(original['physical']);expand.volumes={};expand.run(0x4624,command)
        newRows=[]
        for row in original['rows']:
            newRow=dict(row);newRow['registers']=list(struct.unpack('<18I',expand.read(row['address'],72)))
            newRow['volume']=next(iter(expand.volumes.values()));newRows.append(newRow)
        contexts[key]=dict(rows=newRows,physical=expand.snapshot(PHYSICAL))
        expand.restore(post)
    r=Reference(name);count=0;changed=set();releaseChecks=0;retuneChecks=0;retuneEvents=0
    # Allowed controller destinations, by actual parameter register. Validate
    # every byte, so an unhandled used source field fails instead of disappearing.
    supported={0x18:1,0x1c:2,0x24:4,0x28:256,0x2c:8,0x30:16,0x34:32,0x38:64,0x3c:128}
    for event in result['events']:
        command=event['command'];r.tick=event['tick']
        pre=r.snapshot(((0xc200,0x600),(0xc800,0x1000),(0xe800,0x600)))
        rows=r.dispatch(command)
        assert len(rows)==len(event['voices'])
        for a,b in zip(rows,event['voices']):
            assert a['registers']==b['registers'] and a['volume']==b['volume']
        event['voices']=rows
        if command>>28==9 and (command>>8)&127 and not rows:
            channel=(command>>24)&15
            if name in ('TYPE','SELECT','RESULT'):
                assert channel==(1 if name=='TYPE' else 8), 'unhandled missing authored voice'
            post=r.snapshot(((0xc200,0x600),(0xc800,0x1000),(0xe800,0x600)))
            targets={}
            for (oldChannel,oldNote),context in contexts.items():
                if oldChannel!=channel:continue
                r.restore(pre);r.restore(context['physical']);r.allocated=[];r.volumes={};r.run(0x4624,command)
                assert not r.allocated
                for row in context['rows']:
                    after=list(struct.unpack('<18I',r.read(row['address'],72)))
                    for i,(a,b) in enumerate(zip(row['registers'],after)):
                        if a!=b:assert i*4 in (0x18,0x28)
                    assert (after[10]&255)==(row['registers'][10]&255)
                    voice=0xd800+(row['address']-0x800000)//2
                    assert r.byte(voice+6)==((command>>16)&127)
                    assert not r.byte(voice+32)&1 and r.u(voice+48)==0, 'used mono voice needs source glide'
                    assert len(r.volumes)==1
                    resultRow=dict(layer=row['layer'],registers=after,volume=next(iter(r.volumes.values())),sampleChannel=row.get('sampleChannel',0))
                    identity=(row['layer'],row.get('sampleChannel',0))
                    previous=targets.get(identity)
                    # Only the changed fields may vary across old-note contexts.
                    if previous:
                        assert previous['registers'][6]==after[6] and previous['registers'][10]>>8==after[10]>>8
                        assert previous['volume']==resultRow['volume']
                    else:targets[identity]=resultRow
                    r.restore(pre);r.restore(context['physical'])
                    r.cpu.mem_write(voice,bytes([r.byte(voice)|0x20]));r.w(row['address'],r.u(row['address'])&~0x4000)
                    r.run(0x4624,command)
                    released=list(struct.unpack('<18I',r.read(row['address'],72)))
                    assert after[1:]==released[1:] and not released[0]&0x4000
                    assert r.byte(voice)&0x20 and r.byte(voice+6)==((command>>16)&127)
                    retuneChecks+=1
            assert targets
            event['retunes']=list(targets.values());retuneEvents+=len(targets)
            r.restore(post)
        if command>>28 not in (0xb,0xe):continue
        post=r.snapshot(((0xc200,0x600),(0xc800,0x1000),(0xe800,0x600)))
        event['patches']=[]
        for (channel,note),context in contexts.items():
            if channel!=((command>>24)&15):continue
            for row in context['rows']:
                r.restore(post);r.restore(context['physical']);r.volumes={};r.run(0x4624,command)
                volume=r.volumes.get(0x100000+row['layer'])
                before=row['registers'];after=list(struct.unpack('<18I',r.read(row['address'],72)))
                r.restore(post);r.restore(context['physical'])
                r.run(0x4624,((0x80|channel)<<24)|(note<<16))
                voice=0xd800+(row['address']-0x800000)//2
                if not r.byte(voice)&9:
                    assert r.byte(voice)&0x20 and not r.u(row['address'])&0x4000
                r.run(0x4624,command)
                released=list(struct.unpack('<18I',r.read(row['address'],72)))
                expected=after if command>>28==0xe else before
                assert expected[1:]==released[1:], ('source release callback eligibility',name,hex(command),channel,note,hex(row['layer']))
                # Original285C switches to a complete slot scan at40 allocated
                # voices. Set only its count input in this independent proof.
                r.cpu.mem_write(0xc300,bytes([40]));r.run(0x4624,command)
                fullScan=list(struct.unpack('<18I',r.read(row['address'],72)))
                assert after[1:]==fullScan[1:]
                releaseChecks+=1
                mask=0
                for i,(x,y) in enumerate(zip(before,after)):
                    if x==y:continue
                    offset=i*4
                    assert offset in supported,(name,hex(command),hex(offset),hex(x),hex(y))
                    if offset==0x24:assert (x^y)&~31==0
                    mask|=supported[offset];changed.add(offset)
                # Emit even if equal to template: source must reset any prior
                # controller state on an already playing/releasing instance.
                if command>>28==0xe:mask|=1
                elif (command>>16)&127 in (7,11):
                    if volume is not None:mask|=256
                elif (command>>16)&127==10:
                    # Original50DC changes pan only for layer byte6 bit80.
                    if r.bank[row['layer']+6]&128:mask|=4
                elif (command>>16)&127==1:
                    # At5368 each mapped group controller sets its destinations.
                    # Infer destinations with a second extremal absolute probe.
                    r.restore(post);r.restore(context['physical'])
                    r.run(0x4624,(command&~0xff00)|0x7f00)
                    extreme=list(struct.unpack('<18I',r.read(row['address'],72)))
                    r.restore(post);r.restore(context['physical'])
                    r.run(0x4624,command&~0xff00)
                    zero=list(struct.unpack('<18I',r.read(row['address'],72)))
                    for i,(x,y) in enumerate(zip(extreme,zero)):
                        if x!=y:
                            assert i*4 in supported
                            mask|=supported[i*4]
                else:raise AssertionError('unhandled score controller')
                if mask:
                    event['patches'].append(dict(channel=channel,note=note,layer=row['layer'],registers=after,mask=mask,
                                                 volume=volume if mask&256 else None,sampleChannel=row.get('sampleChannel',0)));count+=1
        r.restore(post)
    result['parameterPatches']=count;result['controllerRegisters']=sorted(changed)
    result['heldAndReleaseControllerComparisons']=releaseChecks
    result['retuneParameterComparisons']=retuneChecks;result['retuneEvents']=retuneEvents
    result['allAuthoredKeyContexts']=len(contexts)
    (WORK/('music-final-'+name+'.json')).write_text(json.dumps(result))
    print(name,'patches',count,'changed registers',[hex(x) for x in sorted(changed)],flush=True)
    return result

def export(name):
    result=json.loads((WORK/('music-final-'+name+'.json')).read_text())
    assert all(result['checks'].values())
    assert 'parameterPatches' in result
    start,end=[x['tick'] for x in result['loops']]
    records=[]
    def add(kind,tick,command,channel=0,note=0,layer=0,sample=0,mask=0xffffffff,registers=None,volume=None,sampleChannel=0):
        records.append(struct.pack('<8I18I8B',kind,tick,command,channel,note,layer,sample,mask,
            *(registers or [0]*18),*((volume or [0]*7)+[sampleChannel])))
    for event in result['events']:
        tick=event['tick'];c=event['command'];kind=c>>28;channel=(c>>24)&15;note=(c>>16)&127
        if tick>=end:break
        add(0,tick,c)
        if kind==8 or (kind==9 and ((c>>8)&127)==0):add(2,tick,c,channel,note)
        for row in event['voices']:
            mask=row['voiceFlags0']|(row['voiceFlags1']<<8)
            if 'stereoPan' in row:mask|=(row['stereoPan']<<16)|0x01000000
            add(1,tick,c,channel,note,row['layer'],row['sample'],mask=mask,registers=row['registers'],volume=row['volume'],sampleChannel=row.get('sampleChannel',0))
        for row in event.get('patches',[]):
            add(3,tick,c,row['channel'],row['note'],row['layer'],mask=row['mask'],registers=row['registers'],volume=row.get('volume'),sampleChannel=row.get('sampleChannel',0))
        for row in event.get('retunes',[]):
            add(4,tick,c,channel,note,row['layer'],mask=257,registers=row['registers'],volume=row['volume'],sampleChannel=row.get('sampleChannel',0))
    project=Path(os.environ.get('IDAS3_AUDIO_PROJECT', Path(__file__).resolve().parents[1]))
    directory=project/'data/original_audio'/('selection' if CUE_IDS[name]<3 else 'music');directory.mkdir(parents=True,exist_ok=True)
    version=3 if CUE_IDS[name]<3 else 4
    blob=struct.pack('<8s6I',b'IDMSEQ1\0',version,CUE_IDS[name],44,start,end,len(records))+b''.join(records)
    (directory/(name.lower()+'.idms')).write_bytes(blob)
    verify=project/'verification/original-music-sequence';verify.mkdir(parents=True,exist_ok=True)
    manifest={k:v for k,v in result.items() if k!='events'}
    (verify/(name.lower()+'.commands')).write_bytes(b''.join(struct.pack('<II',e['tick'],e['command']) for e in result['events'] if e['tick']<end))
    (verify/(name.lower()+'.retunes')).write_bytes(b''.join(struct.pack('<7I',e['tick'],e['command'],(e['command']>>24)&15,(e['command']>>16)&127,r['layer'],r['registers'][6]&65535,(r['registers'][10]>>8)&255) for e in result['events'] if e['tick']<end for r in e.get('retunes',[])))
    manifest.update(recordVersion=version,recordCount=len(records),assetSha256=hashlib.sha256(blob).hexdigest(),
        retune='Source2000/2054/2114..2150 retunes existing mono-channel slots immediately via40B0/3FD4 and storesnewkey+6. No keyon. Allusedmono contexts have softwareglideflag32bit0clear andoffset48zero. Newpitch/TL/velocitycontext verified against every authored oldkey.',
        scope='Original bounded ARM RAM-only scheduler and independent per-note/controller parameter calls. No boot, IRQ, hardware, playback emulator or old runtime. Native player owns envelopes, allocation and release tails.',
        stateComparison='All16 track records, all64 channel records, bank state, entire mutable authored bank and scheduler synchronization bytes match at successive loop boundaries. Elapsed diagnostic counter F7A4 excluded; it is not read by the used score path.',
        controllerReleaseEligibility='Pitch29F0 includes releasing voices. Controller285C omits their linked list below40 allocated voices; at40 its full64-slot scan includes them. Both paths independently compared against every used key/controller parameter.',
        clock=dict(samplesPerTick=44,sampleRate=44100,source=['0xCDC','0xCE0','0x1644','0x1648','0x716C']),
        source=dict(score='0x7234..0x7700',voice='0x3890..0x3DC4',pitch='0x4378',volume='0x42D4',pan='0x4424',modulation='0x5368'))
    (verify/(name.lower()+'.json')).write_text(json.dumps(manifest,indent=2))
    print(name,'export',len(records),'records',len(blob),'bytes',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('name')
    parser.add_argument('--cue-id',type=int,help='Index from the verified disc BGM table')
    parser.add_argument('--phase',choices=['capture','patches','export','all'],default='all');a=parser.parse_args()
    if a.cue_id is not None:
        if not 0 <= a.cue_id < 39 or not a.name.isascii() or not a.name.replace('_','').isalnum():
            parser.error('Invalid original cue identity')
        CUE_IDS[a.name]=a.cue_id
    elif a.name not in CUE_IDS:
        parser.error('Select a known cue or give its original disc index')
    if a.phase in ('capture','all'):capture(a.name)
    if a.phase in ('patches','all'):patches(a.name)
    if a.phase in ('export','all'):export(a.name)
