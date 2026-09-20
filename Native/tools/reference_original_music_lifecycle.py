"""Bounded RAM-only proofs of original ARM slot poll and exhausted allocation.
Imports only the isolated RAM harness; never boots the driver or maps hardware.
"""
from pathlib import Path
import json,pickle,struct
from reference_original_music_sequence import Reference,WORK
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_CPSR
r=Reference('TYPE');cpu=r.cpu
contexts=pickle.loads((WORK/'music-contexts-TYPE.pickle').read_bytes())
context=contexts[(0,47)]
row=context['rows'][0];slot=(row['address']-0x800000)//128;voice=0xd800+slot*64
def stop(uc,pc,size,user):uc.reg_write(UC_ARM_REG_PC,0x401000)
stopHook=cpu.hook_add(UC_HOOK_CODE,stop,begin=0xb94,end=0xb94)
cases=[]
for looped in (False,True):
 for oneShot in (False,True):
  for released in (False,True):
   for phase in range(4):
    for attenuation in (0,511,512,513,1023):
     for address in (0,99,100,101):
      r.restore(context['physical']);r.cpu.mem_write(voice,bytes([0x88 if oneShot else 0x80]));r.cpu.mem_write(voice+19,bytes([int(released)]))
      r.w(row['address'],0x4000|(0x200 if looped else 0));r.w(row['address']+8,100)
      r.w(0x802810,(phase<<13)|attenuation);r.w(0x802814,address)
      cpu.reg_write(UC_ARM_REG_R6,row['address']);cpu.reg_write(UC_ARM_REG_R7,0x802800)
      cpu.reg_write(UC_ARM_REG_R9,slot);cpu.reg_write(UC_ARM_REG_R10,voice)
      r.run(0xabc)
      actual='stop' if not r.byte(voice)&0x80 else 'release' if not r.u(row['address'])&0x4000 else 'continue'
      if looped:
       if oneShot and not released and address:
        expected='release' if address>=100 else 'continue'
       else:expected='stop' if phase and attenuation>512 else 'continue'
      else:expected='stop' if address==0 and phase else 'continue'
      assert actual==expected,(looped,oneShot,released,phase,attenuation,address,actual,expected)
      cases.append([int(looped),int(oneShot),int(released),phase,attenuation,address,actual])
cpu.hook_del(stopHook)
choke=[]
hat=contexts[(0,54)];hatrow=hat['rows'][0];hatvoice=0xd800+(hatrow['address']-0x800000)//2
newLayer=0x100000+0x5cc
assert r.byte(newLayer+34)==0x81 and r.byte(newLayer+35)==0
for active in (False,True):
 for oldMode in (0,0x80,0x81,0x82):
  for channel in (0,1):
   for bank in (0,1):
    for group in (0,1):
     r.restore(hat['physical']);cpu.mem_write(hatvoice,bytes([0x88 if active else 8,oldMode,group]))
     r.w(hatvoice+4,((0x90|channel)<<24)|(54<<16)|(100<<8)|bank)
     r.w(hatrow['address'],r.u(hatrow['address'])|0x4000)
     r.w(0xc2e0,0x903a6400);cpu.mem_write(0xc2a3,b'\0')
     cpu.reg_write(UC_ARM_REG_R8,newLayer);r.run(0x21f4)
     keyOff=not bool(r.u(hatrow['address'])&0x4000)
     expected=active and oldMode==0x81 and channel==0 and bank==0 and group==0
     assert keyOff==expected,(active,oldMode,channel,bank,group,keyOff)
     assert r.byte(hatvoice)==(0x88 if active else 8), 'choke unexpectedly moved release list'
     choke.append([active,oldMode,channel,bank,group,keyOff])
allocator=[]
for active in ([],[2],[9,2],[35,12,4]):
 for releasing in ([],[7],[41,5],[6,1,3]):
  r.restore(r.baseline)
  for at,indices in ((0xc304,active),(0xc30c,releasing)):
   cpu.mem_write(at,bytes([len(indices),indices[0] if indices else 255,indices[-1] if indices else 255,0]))
   for i,index in enumerate(indices):
    cpu.mem_write(0xd800+64*index+16,bytes([indices[i-1] if i else 255,indices[i+1] if i+1<len(indices) else 255]))
  r.run(0x1e5c)
  success=not bool(cpu.reg_read(UC_ARM_REG_CPSR)&(1<<30))
  actual=cpu.reg_read(UC_ARM_REG_R0) if success else -1
  expected=(releasing or active or [-1])[0]
  assert actual==expected,(active,releasing,actual,expected)
  allocator.append(dict(active=active,releasing=releasing,selected=actual))
out=Path(__file__).resolve().parents[1]/'verification/original-music-sequence/lifecycle.json'
out.write_text(json.dumps(dict(result='passed',pollCases=len(cases),chokeCases=len(choke),allocatorCases=allocator,
    source=dict(poll='0xABC..0xB94',oneShot='0xB04..0xB40',retirement='0xAD4..0xB00/0xB5C..0xB90',exhaustedAllocator='0x1E5C..0x1EA8',releaseQueue='0x23B4',appendQueue='0x24D0'),
    boundary='Original predicates and cleanup ran in bounded isolated RAM; common register readback supplied as plain RAM fixture values. Poll cadence and sound-chip execution are outside this proof.',
    oneShot='When looping, flag8 and voice+13==0: nonzero CA>=LSA issues source2DF4 keyoff and marks+13=1. Ordinary noteoff ignores flag8.',
    choke='Source21F4->22CC: newlayer22&83==81 keyoffs everyexistingvoice(firstu32&8380)==8180 withsamechannel/bank(commandmask0F000007) andgroupbyte2==newlayer23. UsedTYPE/SELECTgroups0. Choke doesnotsetflag20 ormove the release list.',
    retirement='After the first keyoff, or for ordinary looping notes: nonattack phase and attenuation>512 (C206default32 times16) stops/frees the slot. Nonlooping: CA0 plus nonattack.',
    exhaustion='Original normal-music1D9C calls1E5C after empty freequeue: remove release-list head first, then active-music-list head. Both lists append at tail; this means earliest release time, then earliest start time. Source priority/SFX list has separate policy.'),indent=2))
print('Original music lifecycle passed',len(cases),'poll cases,',len(choke),'choke cases and',len(allocator),'exhausted allocation cases')
