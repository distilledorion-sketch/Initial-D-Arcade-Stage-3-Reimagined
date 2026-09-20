"""Bounded actual ARM7 control arithmetic; ordinary mapped RAM only."""
from pathlib import Path
import sys,struct,random,hashlib
sys.path.insert(0,str(Path('work/audio_driver_tools').resolve()))
from unicorn import *
from unicorn.arm_const import *
image=Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files/driveA/HOSTFS/binary/AICADRV.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67'
u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0,0x10000);u.mem_write(0,image);u.mem_protect(0x1000,0xb000,UC_PROT_READ|UC_PROT_EXEC)
u.mem_map(0x100000,0x10000);u.mem_map(0x400000,0x10000)
def rd(p):return struct.unpack('<I',u.mem_read(p,4))[0]
def wr(p,x):u.mem_write(p,struct.pack('<I',x&0xffffffff))
def byte(p,x):u.mem_write(p,bytes([x&255]))
def stm(uc,p,n,d):
 sp=uc.reg_read(UC_ARM_REG_SP)-4;wr(sp,p+12);uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,p+4)
for p in range(0,0xc000,4):
 if image[p:p+4]==bytes.fromhex('00802de9'):u.hook_add(UC_HOOK_CODE,stm,begin=p,end=p)
count=0

def instruction(uc,p,n,d):
 global count
 count+=1
u.hook_add(UC_HOOK_CODE,instruction)
commands=[]
def emit(uc,p,n,d):
 commands.append((uc.reg_read(UC_ARM_REG_R0),1 if p==0x2b80 else 2))
 sp=uc.reg_read(UC_ARM_REG_SP);uc.reg_write(UC_ARM_REG_PC,rd(sp));uc.reg_write(UC_ARM_REG_SP,sp+4)
for p in [0x2b80,0x2c10]:u.hook_add(UC_HOOK_CODE,emit,begin=p,end=p)
def run(pc,r0=0,r1=0,r2=0):
 u.reg_write(UC_ARM_REG_R0,r0);u.reg_write(UC_ARM_REG_R1,r1);u.reg_write(UC_ARM_REG_R2,r2);u.reg_write(UC_ARM_REG_R12,0xc200)
 wr(0x400ff0,0x401000);u.reg_write(UC_ARM_REG_SP,0x400ff0);u.emu_start(pc,0x401000,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==0x401000,hex(u.reg_read(UC_ARM_REG_PC))
 return u.reg_read(UC_ARM_REG_R0)
rng=random.Random(0x42d4);rows=[]
def seedtrack(t):
 u.mem_write(0xf498,bytes(16*48));u.mem_write(0xf798,bytes(20));byte(0xf498,t[0]);byte(0xf499,t[1]);byte(0xf49a,t[2]);wr(0xf49c,0x10000);wr(0xf4ac,0x40000000)
 for off,x in zip([28,32,36],t[3:]):wr(0xf498+off,x)
def gettrack():
 return list(u.mem_read(0xf498,3))+[rd(0xf498+i) for i in [28,32,36]]
for i in range(4096):
 vals=[rng.randrange(256) for _ in range(4)]+[i&1]
 for off,v in zip([20,22,21,23],vals[:4]):byte(0xeb00+off,v)
 byte(0xc800,vals[4]);got=run(0x34e8,0xeb00,0xc800);rows.append(['G',*vals,got])
for i in range(8192):
 vals=[rng.randrange(256)]+[rng.randrange(128) for _ in range(5)]+[i&1]
 if i<256:vals[0]=i;vals[1:6]=[127]*5
 wr(0xeb00,0x100000);wr(0x100028,0x100);wr(0x100100,0);u.mem_write(0x100104,bytes([vals[0]])*128)
 byte(0xc205,vals[4]);byte(0xeb06,vals[5]);byte(0xc800,vals[6]);byte(0xc80a,vals[2]);byte(0xc810,vals[3]);u.reg_write(UC_ARM_REG_R10,0xeb00);u.reg_write(UC_ARM_REG_R11,0xc800)
 got=run(0x42d4,0,43,vals[1]);rows.append(['V',*vals,got])
for i in range(2048):
 bank=i%8;arg=i%128;t=[rng.choice([0,0x82,0x83,0xa2]),rng.choice([0x40,0x60,0xc0]),bank,rng.randrange(0x7f0001),rng.randrange(0xffffffff),rng.randrange(128)<<16]
 seedtrack(t);run(0x7704,0xa00a0000|(arg<<8)|bank);rows.append(['R',*t,arg,*gettrack()])
 argstop=128 if i&1 else 0;seedtrack(t);run(0x7704,0xa0001200|bank|argstop);rows.append(['S',*t,argstop,*gettrack()])
 # tick full716C: tempo/correction seeded0; preserves pause/queued flags.
 t[1]=0x40;t[4]=(0-rng.randrange(65536))&0xffffffff;seedtrack(t);run(0x716c);rows.append(['T',*t,0,*gettrack()])
 # full7234 runs actualfade event emission and groupcleanup; commands captured.
 t[0]=0x82;t[1]=[0x40,0x20,0x60][i%3];t[3]=[0,0x7f0000,rng.randrange(0x7f0001)][i%3];t[5]=0x120000
 seedtrack(t);commands.clear();run(0x7234)
 merged=[]
 for cmd,dest in commands:
  if merged and merged[-1][0]==cmd:merged[-1][1]|=dest
  else:merged.append([cmd,dest])
 flat=[x for p in merged for x in p];rows.append(['P',*t,0,*gettrack(),len(merged),*flat])
for i in range(128):
 # actual A01C command helper4AF0 through4624: no active voices, onlybank06.
 u.mem_write(0xd800,bytes(0x1000));byte(0xeb06,0);run(0x4624,0xa01c0000|(i<<8));rows.append(['L',i,u.mem_read(0xeb06,1)[0]])
# Actual active-voice matching4C54/4C74. Stop internals are an explicit
# boundary here; selected voices run1C50->2E24 immediate mute/free, below.
releases=[]
def release(uc,p,n,d):
 releases.append((uc.reg_read(UC_ARM_REG_R10)-0xd800)//64)
 sp=uc.reg_read(UC_ARM_REG_SP);uc.reg_write(UC_ARM_REG_PC,rd(sp));uc.reg_write(UC_ARM_REG_SP,sp+4)
u.hook_add(UC_HOOK_CODE,release,begin=0x1c50,end=0x1c50)
for i in range(2048):
 f0=i&255;f1=64 if i&256 else 0;bank=(i>>7)&7;target=bank if i&512 else ((bank+1)&7);low=target|(128 if i&1024 else 0)
 u.mem_write(0xd800,bytes(4096));u.mem_write(0xe800,bytes([255])*512);u.mem_write(0xf498,bytes(768));byte(0xd800,f0);byte(0xd801,f1);byte(0xd804,bank);releases.clear();run(0x4624,0xa0001200|low)
 rows.append(['K',f0,f1,bank,low,len(releases)])
# Actual2E24 executes against scratch RAM, not hardware. It force-mutes TL,
# clears playback registers and resets envelope/filter fields, not release tails.
u.mem_write(0x100800,bytes([0x5a])*0x48);run(0x2e24,0x100800)
stopregs=[rd(0x100800+x) for x in [0,4,8,12,20,24,28,32,36,40,64,68]]
assert stopregs==[0,0,0,0,0x3c1f,0,0,0,0,0x5a5aff20,0x1f1f,0x1f1f]
rows.append(['H',*stopregs])
# Exact complete exit fade, including the integer-volume zero boundary.
t=[0x90,0x40,2,0x7f0000,0,0x7f0000];seedtrack(t);run(0x7704,0xa00a0802)
ticks=0;commands.clear()
while u.mem_read(0xf498,1)[0] and ticks<2000:
 ticks+=1;run(0x716c);run(0x7234)
rows.append(['Z',ticks,len(commands),rd(0xf4b4)])
p=Path('outputs/InitialDRemake/verification/native-music-control/control-reference.txt');p.parent.mkdir(parents=True,exist_ok=True);p.write_text('\n'.join(' '.join(str(x) for x in row) for row in rows)+'\n')
print(f'{len(rows)} cases, {count} actual ARM instructions, queue/selected-voice boundaries, actual2E24 mute stores; fixture {p}')
