"""Bounded original ARM716C/7234 sequence clock reference, with no device mappings."""
import argparse,csv,hashlib,json,struct,sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('driver','bank','output','python-tools'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();sys.path.insert(0,str(a.python_tools.resolve()))
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_PROT_READ,UC_PROT_EXEC,UC_HOOK_CODE
    from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_PC,UC_ARM_REG_R0,UC_ARM_REG_R12
    driver=a.driver.read_bytes();bank=a.bank.read_bytes()
    driver_hash=hashlib.sha256(driver).hexdigest();bank_hash=hashlib.sha256(bank).hexdigest()
    if driver_hash!='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67':raise ValueError('Original driver identity mismatch')
    if bank_hash!='9885bab452cb423afd9a211ab73356c89639a110949c39594268cef016c538ee':raise ValueError('Original skid bank identity mismatch')
    # Actual reload instructions, not a guessed millisecond conversion.
    if driver[0xcdc:0xce4]!=bytes.fromhex('d400a0e394008be5') or driver[0x1644:0x164c]!=bytes.fromhex('d400a0e3940082e5'):raise ValueError('TimerB source changed')
    u=lambda at:struct.unpack_from('<I',bank,at)[0]
    table=u(0x2c);group=table+struct.unpack_from('<H',bank,table+4)[0];rows=[];calls=0
    for track_id in range(6):
        cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);cpu.mem_map(0,0x10000);cpu.mem_write(0,driver);cpu.mem_protect(0,0xc000,UC_PROT_READ|UC_PROT_EXEC)
        cpu.mem_map(0x100000,0x200000);cpu.mem_write(0x100000,bank);cpu.mem_map(0x400000,0x10000)
        def stm_pc(uc,pc,size,user):
            sp=uc.reg_read(UC_ARM_REG_SP)-4;uc.mem_write(sp,struct.pack('<I',pc+12));uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,pc+4)
        for pc in range(0,0xc000,4):
            if driver[pc:pc+4]==bytes.fromhex('00802de9'):cpu.hook_add(UC_HOOK_CODE,stm_pc,begin=pc,end=pc)
        tick=0
        def output(uc,pc,size,user):
            rows.append((track_id,tick,uc.reg_read(UC_ARM_REG_R0)))
            sp=uc.reg_read(UC_ARM_REG_SP);uc.reg_write(UC_ARM_REG_PC,struct.unpack('<I',uc.mem_read(sp,4))[0]);uc.reg_write(UC_ARM_REG_SP,sp+4)
        cpu.hook_add(UC_HOOK_CODE,output,begin=0x2b80,end=0x2b80)
        def run(pc):
            nonlocal calls
            cpu.mem_write(0x400ff0,struct.pack('<I',0x401000));cpu.reg_write(UC_ARM_REG_SP,0x400ff0)
            cpu.emu_start(pc,0x401000,count=4096);calls+=1
            if cpu.reg_read(UC_ARM_REG_PC)!=0x401000:raise ValueError('Bounded sequence call did not return')
        track=0xf498;pointer=0x100000+table+u(group+4+4*track_id)
        cpu.mem_write(track,bytes(16*0x30));cpu.mem_write(0xf798,bytes(16))
        cpu.mem_write(track,bytes([0x80,bank[pointer-0x100000],0,0]));cpu.mem_write(track+4,struct.pack('<I',0x10000))
        cpu.mem_write(track+0x18,struct.pack('<I',pointer+1));cpu.mem_write(track+0x10,struct.pack('<I',0xff000000));cpu.reg_write(UC_ARM_REG_R12,0x402000)
        run(0x7234)
        while cpu.mem_read(track,1)[0]:
            tick+=1
            if tick>12000:raise ValueError('Sequence did not end inside12000 ticks')
            run(0x716c);run(0x7234)
        print('track',track_id,'ended at timer tick',tick,flush=True)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(('track','tick','command'));writer.writerows(rows)
    manifest=dict(driver_sha256=driver_hash,bank_sha256=bank_hash,fixture_sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),
        calls=calls,events=len(rows),instruction_bound_per_call=4096,
        hooks=['ARM7 STM sp!,{pc} storedPC+12','2B80 final SFX command output'],
        timing='TimerB reload0xD4 divider0:44 AICA sample clocks per timer tick. Original716C and7234 execute once per tick; no external MIDI sync, device mapping, original startup or IRQ execution. Host main-loop/IRQ latency is not modeled.')
    a.output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest),flush=True)

if __name__=='__main__':main()
