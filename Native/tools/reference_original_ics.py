"""Development-only, bounded ARM scalar reference for ICS interpolation/pitch.

Requires Unicorn in the development interpreter. Only isolated RAM routines run;
no sound device addresses are mapped, and no driver startup/interrupt executes.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('driver','project','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--development-python-libraries',type=Path)
    args=parser.parse_args()
    if args.development_python_libraries:sys.path.insert(0,str(args.development_python_libraries.resolve()))
    from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_PROT_READ,UC_PROT_EXEC
    from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_SP,UC_ARM_REG_PC
    driver=args.driver.read_bytes()
    if hashlib.sha256(driver).hexdigest()!='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67':
        raise ValueError('Original driver identity mismatch')
    cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);cpu.mem_map(0,0x10000);cpu.mem_write(0,driver);cpu.mem_protect(0,0x10000,UC_PROT_READ|UC_PROT_EXEC)
    bank_base=0x100000;stack=0x400ff0;stop=0x401000
    cpu.mem_map(bank_base,0x200000);cpu.mem_map(0x400000,0x2000)
    cpu.mem_write(stack,struct.pack('<I',stop))
    manifest=json.loads((args.project/'data/original_audio/continuous/manifest.json').read_text())
    rows=bytearray();count=0;pitch=[];layers=0
    for bank_index,bank in enumerate(manifest['banks']):
        data=(args.project/'data/original_audio/continuous'/bank['file']).read_bytes()
        if hashlib.sha256(data).hexdigest()!=bank['sha256']:raise ValueError('Bank identity mismatch')
        cpu.mem_write(bank_base,data)
        word=lambda at:struct.unpack_from('<I',data,at)[0]
        ics=word(0x34)
        for program in range(word(ics)+1):
            at=ics+word(ics+4+4*program)
            layer_count=data[at]+1;at+=8
            for layer in range(layer_count):
                layers+=1
                for control in range(6):
                    for value in range(256):
                        cpu.reg_write(UC_ARM_REG_R0,control);cpu.reg_write(UC_ARM_REG_R9,bank_base+at);cpu.reg_write(UC_ARM_REG_R10,value)
                        cpu.reg_write(UC_ARM_REG_SP,stack)
                        cpu.emu_start(0x6fac,stop,count=256)
                        if cpu.reg_read(UC_ARM_REG_PC)!=stop:raise ValueError('ICS interpolation instruction bound exceeded')
                        rows.extend(struct.pack('<IIIII',bank_index,at,control,value,cpu.reg_read(UC_ARM_REG_R0)));count+=1
                at+=data[at]
    for value in range(0x1800):
        cpu.reg_write(UC_ARM_REG_R0,value);cpu.reg_write(UC_ARM_REG_SP,stack);cpu.emu_start(0x6f60,stop,count=256)
        if cpu.reg_read(UC_ARM_REG_PC)!=stop:raise ValueError('ICS pitch instruction bound exceeded')
        pitch.append(cpu.reg_read(UC_ARM_REG_R0))
    output=b'ICSREF01'+struct.pack('<II',count,len(pitch))+rows+struct.pack('<'+'I'*len(pitch),*pitch)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(output)
    report=dict(driver_sha256=hashlib.sha256(driver).hexdigest(),reference_sha256=hashlib.sha256(output).hexdigest(),
        layers=layers,curve_comparisons=count,pitch_comparisons=len(pitch),
        scope='Original ARM6FAC/6F60 with driver ROM and bank data in isolated RAM. No hooks, I/O, device mappings, driver initialization, or interrupts. 256-instruction limit per call.')
    args.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
if __name__=='__main__':main()
