"""Actual original ARM A9 sequencing/choke/priority control in private RAM."""
from pathlib import Path
import json,struct,random
from reference_original_music_sequence import Reference,WORK,DRIVER_SHA
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R5,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_CPSR
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'verification/original-oneshot-audio';OUT.mkdir(parents=True,exist_ok=True)
def decoded(bank,number,cue):
    u=lambda p:struct.unpack_from('<I',bank,p)[0];w=lambda p:struct.unpack_from('<H',bank,p)[0]
    table=u(0x2c);group=next(table+w(table+4+4*i) for i in range(u(table)+1) if bank[table+7+4*i]==0xa9 and bank[table+6+4*i]==number-20)
    assert cue<=u(group);cursor=table+u(group+4+4*cue);assert bank[cursor]==0xc0;cursor+=1
    result=[];status=0;tick=0
    while bank[cursor]!=255:
        if bank[cursor]&128:status=bank[cursor];cursor+=1
        assert status==0xdf
        note,velocity,tail=bank[cursor:cursor+3];cursor+=3;assert tail&127==0
        result.append([tick,0x9f000000|(note<<16)|(velocity<<8)])
        if not tail&128:
            delay=bank[cursor];cursor+=1
            if delay&128:delay=((delay&127)<<7)|bank[cursor];cursor+=1
            tick+=delay
    return result
sequences=[]
for number,count in ((21,15),(22,8),(24,6),(25,3)):
 for cue in range(count):
    r=Reference(f'PACK{number}');expected=decoded(r.bank,number,cue)
    r.run(0x7b64,0xa9000000|((number-20)<<16)|(cue<<8))
    for tick in range(1,expected[-1][0]+2):r.tick=tick;r.run(0x716c);r.run(0x7234)
    assert r.commands==expected,(number,cue,r.commands,expected)
    sequences.append(dict(bank=number,cue=cue,events=expected,calls=r.callCount))
print('PASS',len(sequences),'complete original cue sequences',flush=True)
r=Reference('PACK21');r.run(0x7b64,0xa9010200);r.pending=[];row=r.dispatch(0x9f027800)[0]
physical=r.snapshot(((0xd800,0x1000),(0xedc0,64),(0xc300,16),(0x800000,0x2000)))
voice=0xd800+(row['address']-0x800000)//2;chokes=[]
for active in (False,True):
 for mode in (0,0x40,0x80,0xc0,0x81,0x82):
  for channel in (14,15):
   for bank in (0,1):
    for note in (2,3):
     for velocity in (1,120,127):
      r.restore(physical);r.cpu.mem_write(voice,bytes([0xc8 if active else 0x48,mode]))
      r.w(voice+4,((0x90|channel)<<24)|(note<<16)|(velocity<<8)|bank)
      r.w(row['address'],r.u(row['address'])|0x4000);r.w(0xc2e0,0x9f027800)
      r.cpu.mem_write(0xc2a3,b'\0');r.cpu.reg_write(UC_ARM_REG_R8,0x100000+row['layer']);r.run(0x21f4)
      actual=not bool(r.u(row['address'])&0x4000)
      expected=active and mode&0x83==0x80 and channel==15 and bank==0 and note==2
      assert actual==expected,(active,mode,channel,bank,note,velocity,actual)
      assert r.byte(voice)==(0xc8 if active else 0x48),'Hardware choke must not set release-list flag20'
      chokes.append([active,mode,channel,bank,note,velocity,actual])
print('PASS',len(chokes),'original same-cue choke cases',flush=True)
randomizer=random.Random(319);cases=[];binary=bytearray()
for case in range(1024):
    priorities=[randomizer.choice((0,128,178,200,255)) for _ in range(randomizer.randrange(33))]
    incoming=randomizer.choice((0,128,178,200,255,256,383))
    r.restore(r.baseline);r.cpu.mem_write(0xc308,bytes([len(priorities),0 if priorities else 255,len(priorities)-1 if priorities else 255,0]))
    for i,p in enumerate(priorities):
        r.cpu.mem_write(0xd800+64*i+3,bytes([p]));r.cpu.mem_write(0xd800+64*i+17,bytes([i+1 if i+1<len(priorities) else 255]))
    r.cpu.reg_write(UC_ARM_REG_R5,incoming);r.cpu.reg_write(UC_ARM_REG_R7,0xc308);r.run(0x1f8c)
    selected=-1 if r.cpu.reg_read(UC_ARM_REG_CPSR)&(1<<30) else r.cpu.reg_read(UC_ARM_REG_R0)
    candidates=[(p,-i,i) for i,p in enumerate(priorities) if p>=incoming];expected=max(candidates)[2] if candidates else -1
    assert selected==expected,(priorities,incoming,selected,expected)
    binary+=struct.pack('<IIi',len(priorities),incoming,selected)+bytes(priorities)
    cases.append(dict(priorities=priorities,incoming=incoming,selected=selected))
(OUT/'priority-cases.bin').write_bytes(struct.pack('<I',len(cases))+binary)
# Source fullallocator must retain its32-slot limit even with32 free chip slots.
r=Reference('PACK21');r.run(0x7b64,0xa9010500);r.pending=[]
for _ in range(32):r.run(0x4624,0x9f057f00)
before=len(r.allocated);r.run(0x4624,0x9f057f00)
assert before==32 and len(r.allocated)==32 and r.byte(0xc308)==32 and r.byte(0xc314)==32
(OUT/'source-control.json').write_text(json.dumps(dict(driverSha256=DRIVER_SHA,sequences=sequences,chokeCases=len(chokes),priorityCases=len(cases),priorityIncoming='128+layer24 remains wide for comparison; only stored slot priority is truncated to8bits',sfxCap=32,physicalSlots=64,boundary='Bounded source routines execute in anonymous RAM; original chip sample clock and shared music/ICS allocation are not simulated.'),indent=2))
print('PASS',len(cases),'original priority cases and source32-slot limit')
