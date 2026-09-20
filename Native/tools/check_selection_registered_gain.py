from pathlib import Path
import sys,struct,json
sys.path.insert(0,'outputs/InitialDRemake/tools')
from reference_original_audio_dsp_bank import Reference
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R10,UC_ARM_REG_R11
base=Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files/driveA/HOSTFS')
for name,level in [('TYPE',109),('SELECT',103)]:
 r=Reference((base/'binary/AICADRV.bin').read_bytes());packed=0x10000
 for slot,n in enumerate(['PACK20','PACK21',name]):
  bank=(base/f'sound/pack/{n}.bin.nz').read_bytes();r.cpu.mem_write(packed,bank);packed+=len(bank)
  r.w(0x60+slot*4,slot);r.run(0xbec);r.run(0x153c);r.run(0x1188)
  if slot<2:r.run(0x4624,0xa0107f00|slot)
 print(name,'pre-score master',r.byte(0xc205),'registry',[r.u(0xc240+i*4) for i in range(8)],'banks',[r.read(0xeb00+32*i,32).hex() for i in range(3)])
 for cmd in [0xa0040002|(level<<8),0xa0001282,0xa01c7f02,0xa0190082,0xa0000082]:
  r.run(0x4624,cmd);print(hex(cmd),'bank2 state',list(r.read(0xeb00+2*32+20,4)))
 # Source143060 repeats4A0 during six60Hzservice calls; its second write
 # occurs beforeeither score's firstnote (TYPE199,SELECT192TimerB ticks).
 r.run(0x4624,0xa0040002|(level<<8))
 # Allocation maps a logical MIDI channel to an implementation channel record;
 # inspect the live source pointers, never use MIDIindex as recordindex.
 contexts=[]
 def volume(uc,pc,size,user):
  layer=uc.reg_read(UC_ARM_REG_R8);channel=uc.reg_read(UC_ARM_REG_R11);record=uc.reg_read(UC_ARM_REG_R10);table=uc.reg_read(UC_ARM_REG_R7);cmd=r.u(0xc2e0)
  contexts.append(dict(command=hex(cmd),layer=layer,channel=hex(channel),bank=hex(record),master=r.byte(0xc205),gains=list(r.read(record+20,4)),channelGain=r.byte(channel+16),channelVolume=r.byte(channel+10),flags=r.byte(channel),tableValue=r.byte(table+((cmd>>8)&127))))
 r.cpu.hook_add(UC_HOOK_CODE,volume,begin=0x4300,end=0x4300)
 events=json.loads(Path('work/music-final-'+name+'.json').read_text())['events'];seen=set()
 for e in events:
  cmd=e['command'];ch=(cmd>>24)&15
  if cmd>>28==10:continue
  if cmd>>28==9 and (cmd>>8)&127 and ch not in seen:
   r.run(0x4624,(cmd&~7)|2);seen.add(ch)
  if len(seen)==(10 if name=='TYPE' else 9):break
 print(name,'live source volume contexts',contexts)
 assert contexts and all(v['master']==127 and v['gains']==[level,127,64,64] and v['channelGain']==level for v in contexts)


