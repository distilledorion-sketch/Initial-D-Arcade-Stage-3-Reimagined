"""Bounded, development-only original ARM voice parameter reference.

This does not start the sound driver. ARM7 STM stored-PC semantics are explicit:
the original compiler uses STM sp!,{pc}; NOP; B as its helper-call convention.
Unicorn's default ARM core stores PC+8; this ARM7 code requires PC+12. The only
hook implements that instruction's architectural store. Helpers run real bytes.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('driver','project','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--development-python-libraries',type=Path)
    a=p.parse_args()
    if a.development_python_libraries:sys.path.insert(0,str(a.development_python_libraries.resolve()))
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_PROT_READ,UC_PROT_EXEC,UC_HOOK_CODE
    from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12
    driver=a.driver.read_bytes();driver_sha=hashlib.sha256(driver).hexdigest()
    if driver_sha!='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67':raise ValueError('Driver identity mismatch')
    cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);cpu.mem_map(0,0x10000);cpu.mem_write(0,driver);cpu.mem_protect(0,0xc000,UC_PROT_READ|UC_PROT_EXEC)
    hooks=0
    def stm_pc7(uc,pc,size,user):
        sp=uc.reg_read(UC_ARM_REG_SP)-4;uc.mem_write(sp,struct.pack('<I',pc+12));uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,pc+4)
    for pc in range(0,0xc000,4):
        if driver[pc:pc+4]==b'\x00\x80\x2d\xe9':cpu.hook_add(UC_HOOK_CODE,stm_pc7,begin=pc,end=pc);hooks+=1
    bank_base=0x100000;stack=0x400ff0;stop=0x401000;control=0x402000;glob=0x403000
    cpu.mem_map(bank_base,0x200000);cpu.mem_map(0x400000,0x10000)
    cpu.mem_write(0xeb00,struct.pack('<I',bank_base));cpu.mem_write(stack,struct.pack('<I',stop))
    root=a.project/'data/original_audio/continuous';manifest=json.loads((root/'manifest.json').read_text())
    rows=bytearray();count=0;layers=0;calls=0
    neutral=[127,64,63,64,255,64,64,64,127]
    for bank_index,bank in enumerate(manifest['banks']):
        data=(root/bank['file']).read_bytes()
        if hashlib.sha256(data).hexdigest()!=bank['sha256']:raise ValueError('Bank identity mismatch')
        cpu.mem_write(bank_base,data);u=lambda p:struct.unpack_from('<I',data,p)[0];ics=u(0x34)
        for program in range(u(ics)+1):
            at=ics+u(ics+4+4*program);n=data[at]+1;at+=8
            for layer in range(n):
                layers+=1;first,last=data[at+4:at+6]
                cases=[(value,neutral) for value in range(first,last+1)]
                # Every control at both extrema, with every layer's own range.
                for field in range(9):
                    for edge in (0,127):
                        c=neutral.copy();c[field]=edge if field!=4 else (0 if edge==0 else 15)
                        cases.append((first+(last-first)*(field%3)//2,c))
                for value,c in cases:
                    raw=bytearray(32)
                    for offset,v in zip((8,9,10,11,13,12,14,20),c[:8]):raw[offset]=v
                    cpu.mem_write(control,bytes(raw));cpu.mem_write(glob+5,bytes([c[8]]));expected=[]
                    for routine in (0x6bc4,0x6c30,0x6c94,0x6cc8,0x6d08,0x6d44):
                        cpu.reg_write(UC_ARM_REG_SP,stack);cpu.reg_write(UC_ARM_REG_R9,bank_base+at);cpu.reg_write(UC_ARM_REG_R10,value)
                        cpu.reg_write(UC_ARM_REG_R11,control);cpu.reg_write(UC_ARM_REG_R12,glob)
                        cpu.emu_start(routine,stop,count=1024)
                        if cpu.reg_read(UC_ARM_REG_PC)!=stop:raise ValueError('Voice helper instruction limit')
                        expected.append(cpu.reg_read(UC_ARM_REG_R0));calls+=1
                        if routine==0x6bc4:expected.append(cpu.reg_read(UC_ARM_REG_R1))
                    rows.extend(struct.pack('<III12B7I',bank_index,at,value,*c,0,0,0,*expected));count+=1
                at+=data[at]
        print(bank['file'],count,'cases',flush=True)
    output=b'ICSVREF1'+struct.pack('<I',count)+rows;a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(output)
    report=dict(driver_sha256=driver_sha,reference_sha256=hashlib.sha256(output).hexdigest(),layers=layers,cases=count,original_calls=calls,
        scope='ARM6BC4/6C30/6C94/6CC8/6D08/6D44; isolated ROM/bank/RAM, no device mappings, initialization or interrupts; 1024-instruction bound per call. Only hook: ARM7 STM sp!,{pc} stores PC+12. All helper calls otherwise execute original bytes.',stored_pc_hook_sites=hooks)
    a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
