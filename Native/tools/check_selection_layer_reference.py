from pathlib import Path
import sys,json,struct,hashlib,collections,importlib.util
modspec=importlib.util.spec_from_file_location('score_reference','outputs/InitialDRemake/tools/reference_original_music_sequence.py');mod=importlib.util.module_from_spec(modspec);modspec.loader.exec_module(mod)
sys.path.insert(0,'work/audio_driver_tools')
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_PROT_READ,UC_PROT_EXEC,UC_HOOK_CODE
from unicorn.arm_const import *
driver=Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files/driveA/HOSTFS/binary/AICADRV.bin').read_bytes()
assert hashlib.sha256(driver).hexdigest()=='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67'
for name in ('TYPE','SELECT'):
 data=json.loads(Path('work/music-final-'+name+'.json').read_text());bank=Path('outputs/InitialDRemake/data/original_audio/selection/'+name+'.dtpk').read_bytes()
 c=Uc(UC_ARCH_ARM,UC_MODE_ARM);c.mem_map(0,0x10000);c.mem_write(0,driver);c.mem_protect(0,0xc000,UC_PROT_READ|UC_PROT_EXEC);c.mem_map(0x100000,0x100000);c.mem_write(0x100000,bank);c.mem_map(0x400000,0x10000)
 def w(p,v):c.mem_write(p,struct.pack('<I',v))
 def ret():
  sp=c.reg_read(UC_ARM_REG_SP);c.reg_write(UC_ARM_REG_PC,struct.unpack('<I',c.mem_read(sp,4))[0]);c.reg_write(UC_ARM_REG_SP,sp+4)
 def stm(uc,pc,size,user):
  sp=c.reg_read(UC_ARM_REG_SP)-4;w(sp,pc+12);c.reg_write(UC_ARM_REG_SP,sp);c.reg_write(UC_ARM_REG_PC,pc+4)
 for pc in range(0,0xc000,4):
  if driver[pc:pc+4]==bytes.fromhex('00802de9'):c.hook_add(UC_HOOK_CODE,stm,begin=pc,end=pc)
 def allocated(uc,pc,size,user):c.reg_write(UC_ARM_REG_CPSR,c.reg_read(UC_ARM_REG_CPSR)|(1<<30));ret()
 chosen=[]
 def layer(uc,pc,size,user):chosen.append(c.reg_read(UC_ARM_REG_R8)-0x100000);ret()
 for pc in (0x2000,0x21f4):c.hook_add(UC_HOOK_CODE,allocated,begin=pc,end=pc)
 c.hook_add(UC_HOOK_CODE,layer,begin=0x3af8,end=0x3af8)
 source=mod.Reference(name);programs={}
 for event in data['events']:
  source.dispatch(event['command'])
  ch=(event['command']>>24)&15
  for address,layer,group,channel in source.allocated:
   program=source.u(channel+4)
   if ch in programs:assert programs[ch]==program
   programs[ch]=program
  if len(programs)==(10 if name=='TYPE' else 9):break
 print(name,'source channel programs', {k:hex(v-0x100000) for k,v in programs.items()})
 u16=lambda p:struct.unpack_from('<H',bank,p)[0]
 cache={};calls=0;compared=0;counts=collections.Counter();mismatches=[]
 for event in data['events']:
  if event['tick']>=data['loops'][1]['tick']:break
  raw=event['command'];ch=(raw>>24)&15;key=(raw>>16)&127;vel=(raw>>8)&127
  if raw>>28!=9 or not vel:continue
  cachekey=(ch,key,vel)
  if cachekey not in cache:
   program=programs[ch];assert 0x100000<=program<0x100000+len(bank),(name,ch,hex(program))
   selected=[]
   for group in range(4):
    relative=u16(program-0x100000+8+group*2)
    if not relative:continue
    chosen.clear();c.mem_write(0x4020e1,bytes((vel,key)));w(0x403204,program);w(0x400ff0,0x401000)
    for reg,val in ((UC_ARM_REG_SP,0x400ff0),(UC_ARM_REG_R12,0x402000),(UC_ARM_REG_R0,relative),(UC_ARM_REG_R10,program),(UC_ARM_REG_R11,0x403200)):c.reg_write(reg,val)
    c.emu_start(0x39a0,0x401000,count=512);calls+=1
    assert c.reg_read(UC_ARM_REG_PC)==0x401000
    selected+=chosen
   cache[cachekey]=selected
  actual=[r['layer'] for r in event['voices']]+[r['layer'] for r in event.get('retunes',[])]
  if cache[cachekey]!=actual:mismatches.append([event['tick'],hex(raw),cache[cachekey],actual])
  compared+=1;counts[ch]+=len(actual)
 print(name,'actual ARM39A0 group/key/velocity proof',calls,'bounded calls;',compared,'authored commands;',len(cache),'distinct contexts; mismatches',mismatches[:20])
 assert not mismatches


