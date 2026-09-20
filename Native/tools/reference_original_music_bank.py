"""Execute only bounded original ARM instrument/sample lookup instructions.

No startup, interrupts, audio device, AICA register map, or full game runtime.
"""
import argparse, hashlib, json, struct, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('driver','assets','output','python-tools'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();sys.path.insert(0,str(a.python_tools))
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_PROT_READ,UC_PROT_EXEC,UC_HOOK_CODE
    from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R8,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_PC,UC_ARM_REG_CPSR
    driver=a.driver.read_bytes();sha=hashlib.sha256(driver).hexdigest()
    if sha!='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67':raise ValueError('Driver identity mismatch')
    rows=[];calls=0;programCount=sampleCount=0
    for name in ('TYPE','SELECT','RESULT'):
        b=(a.assets/(name+'.dtpk')).read_bytes();u16=lambda p:struct.unpack_from('<H',b,p)[0];u32=lambda p:struct.unpack_from('<I',b,p)[0]
        cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);cpu.mem_map(0,0x10000);cpu.mem_write(0,driver);cpu.mem_protect(0,0xc000,UC_PROT_READ|UC_PROT_EXEC)
        cpu.mem_map(0x100000,0x100000);cpu.mem_write(0x100000,b);cpu.mem_map(0x400000,0x10000)
        def stm(uc,pc,size,user):
            sp=uc.reg_read(UC_ARM_REG_SP)-4;uc.mem_write(sp,struct.pack('<I',pc+12));uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,pc+4)
        for pc in range(0,0xc000,4):
            if driver[pc:pc+4]==bytes.fromhex('00802de9'):cpu.hook_add(UC_HOOK_CODE,stm,begin=pc,end=pc)
        chosen=0
        def return_stack(uc):
            sp=uc.reg_read(UC_ARM_REG_SP);uc.reg_write(UC_ARM_REG_PC,struct.unpack('<I',uc.mem_read(sp,4))[0]);uc.reg_write(UC_ARM_REG_SP,sp+4)
        def allocate(uc,pc,size,user):
            uc.reg_write(UC_ARM_REG_CPSR,uc.reg_read(UC_ARM_REG_CPSR)|(1<<30));return_stack(uc)
        def selected(uc,pc,size,user):
            nonlocal chosen
            chosen=uc.reg_read(UC_ARM_REG_R8)-0x100000;return_stack(uc)
        for pc in (0x2000,0x21f4):cpu.hook_add(UC_HOOK_CODE,allocate,begin=pc,end=pc)
        cpu.hook_add(UC_HOOK_CODE,selected,begin=0x3af8,end=0x3af8)
        def run(pc,regs):
            nonlocal calls
            cpu.mem_write(0x400ff0,struct.pack('<I',0x401000));cpu.reg_write(UC_ARM_REG_SP,0x400ff0);cpu.reg_write(UC_ARM_REG_R12,0x402000)
            for reg,val in regs:cpu.reg_write(reg,val)
            cpu.emu_start(pc,0x401000,count=512);calls+=1
            if cpu.reg_read(UC_ARM_REG_PC)!=0x401000:raise ValueError('Bounded lookup did not return')
            return [cpu.reg_read(reg) for reg in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
        program=u32(0x24);sub=program+u16(program+2)
        programCount+=u16(sub)+1;sampleCount+=u32(u32(0x3c))+1
        cpu.mem_write(0x403000,struct.pack('<I',0x100000))
        for i in range(u16(sub)+1):
            cpu.mem_write(0x403100,bytes([0,0,0,i]))
            result=run(0x3084,[(UC_ARM_REG_R0,0x403000),(UC_ARM_REG_R1,0x403100)])[0]
            expected=0x100000+sub+u16(sub+2+2*i)
            if result!=expected:raise ValueError('Program lookup disagrees')
            rows.append(f'P {name} {i} {result-0x100000}')
            program_at=result-0x100000;slot_index=0
            cpu.mem_write(0x403204,struct.pack('<I',result))
            for slot in range(4):
                relative=u16(program_at+8+2*slot)
                if not relative:continue
                for key in range(128):
                    for velocity in (0,64,127):
                        chosen=0;cpu.mem_write(0x4020e1,bytes((velocity,key)))
                        run(0x39a0,[(UC_ARM_REG_R0,relative),(UC_ARM_REG_R10,result),(UC_ARM_REG_R11,0x403200)])
                        rows.append(f'L {name} {i} {slot_index} {key} {velocity} {chosen}')
                slot_index+=1
        sample=u32(0x3c)
        for i in range(u32(sample)+1):
            at=sample+4+16*i;location,lsa,lea,_,_=struct.unpack_from('<IHHII',b,at)
            for offset in sorted(set((0,1,min(8,lea-1),min(lsa,lea-1)))):
                result=run(0x4494,[(UC_ARM_REG_R0,0x100000),(UC_ARM_REG_R1,0x100+i),(UC_ARM_REG_R2,offset)])
                encoding=(location>>23)&3;byte_offset=offset*2 if encoding==0 else offset if encoding==1 else offset//2
                address=location+0x100000+byte_offset
                expected=[address&65535,(address>>16)&0x7ff,(lsa-offset if location&0x2000000 and lsa else lsa)&65535,(lea-offset)&65535]
                if result!=expected:raise ValueError(f'Sample offset lookup mismatch {name} {i} {offset}: {result} vs {expected}')
                rows.append(' '.join(map(str,('S',name,i,offset,*result))))
        result=run(0x4494,[(UC_ARM_REG_R0,0x100000),(UC_ARM_REG_R1,1),(UC_ARM_REG_R2,0)])
        if result!=[0x916c,0x200,0,100]:raise ValueError('Built-in sample lookup mismatch')
        rows.append(' '.join(map(str,('B',name,1,*result))))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(rows)+'\n')
    report=dict(driver_sha256=sha,calls=calls,instruction_bound_per_call=512,programs=programCount,samples=sampleCount,
        hooks=['ARM7 STM sp!,{pc} PC+12 convention','2000/21F4 allocation success','3AF8 capture selected layer instead of audio output'],
        scope='Original3084 instrument,39A0 group/key/velocity selection,4494 sample lookup including PCMS-dependent offsets/loop adjustments and built-in sample1; isolated RAM only.')
    a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

if __name__=='__main__':main()
