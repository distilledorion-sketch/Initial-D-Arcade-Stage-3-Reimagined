"""Full original ARM A2 stream volume dispatch in isolated ordinary RAM.

Usage: python tools/reference_original_stream_volume.py AICADRV.bin output.txt
Unicorn is loaded from the workspace's existing work/audio_driver_tools.
No AICA devices, DMA, native audio output, game runtime or user profiles.
"""
from pathlib import Path
import sys,struct,hashlib
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'work/audio_driver_tools'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE,UC_PROT_READ,UC_PROT_EXEC
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_PC

image=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(image).hexdigest()=='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67'
cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);cpu.mem_map(0,0x10000);cpu.mem_write(0,image)
cpu.mem_protect(0x1000,0xb000,UC_PROT_READ|UC_PROT_EXEC)
cpu.mem_map(0x400000,0x10000);cpu.mem_map(0x800000,0x2000)
def u(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
def w(p,v):cpu.mem_write(p,struct.pack('<I',v))
def b(p,v):cpu.mem_write(p,bytes([v]))
def stm(uc,p,n,data):
    # ARM7 stores architecturalPC+12 in STM{pc}; Unicorn's newer ARM model
    # storesPC+8. This compatibility hook changes only that return-address rule.
    sp=uc.reg_read(UC_ARM_REG_SP)-4;w(sp,p+12);uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,p+4)
for p in range(0,0xc000,4):
    if image[p:p+4]==bytes.fromhex('00802de9'):cpu.hook_add(UC_HOOK_CODE,stm,begin=p,end=p)
instructions=0
def count(uc,p,n,data):
    global instructions
    instructions+=1
cpu.hook_add(UC_HOOK_CODE,count)
def run(pc,command=0):
    cpu.reg_write(UC_ARM_REG_R0,command);cpu.reg_write(UC_ARM_REG_R12,0xc200)
    w(0x400ff0,0x401000);cpu.reg_write(UC_ARM_REG_SP,0x400ff0)
    cpu.emu_start(pc,0x401000,count=20000)
    assert cpu.reg_read(UC_ARM_REG_PC)==0x401000,hex(cpu.reg_read(UC_ARM_REG_PC))
run(0x16e4)
assert all(cpu.mem_read(0x101+i*0x60,1)==b'\x40' for i in range(8))
rows=[]
for slot in range(8):
    descriptor=0x100+slot*0x60
    for master in (0,32,64,96,127,255):
        for active in (0,1):
            for right in (0,1):
                for volume in range(128):
                    cpu.mem_write(0x800000,bytes([0x5a])*0x100)
                    b(descriptor+1,master);w(descriptor+4,0x700000 if active else 0);w(descriptor+8,0x700080 if active else 0)
                    b(descriptor+0x51,0xcc);b(descriptor+0x55,0xdd)
                    command=0xa2000000|(slot<<20)|(right<<19)|(volume<<8)
                    run(0x4624,command)
                    index=min(127,max(0,volume+master-64));expected=image[0x7eb0+index]^255
                    offset=0x55 if right else 0x51
                    assert cpu.mem_read(descriptor+offset,1)[0]==volume
                    got=cpu.mem_read(0x800000+right*128+0x29,1)[0]
                    assert got==(expected if active else 0x5a),(slot,master,active,right,volume,got,expected)
                    other=cpu.mem_read(0x800000+(1-right)*128+0x29,1)[0]
                    assert other==0x5a
                    rows.append((slot,master,active,right,volume,expected,got))
out=Path(sys.argv[2]);out.parent.mkdir(parents=True,exist_ok=True)
out.write_text('\n'.join(' '.join(map(str,row)) for row in rows)+'\n')
print(f'PASS {len(rows)} full ARM A2 dispatch cases, {instructions} instructions; exact8-stream master64 initialization; inactive slots unchanged; opposite stereo slot unchanged. ARM7 STM-PC compatibility only, no algorithm/dispatch hooks. {out}')
