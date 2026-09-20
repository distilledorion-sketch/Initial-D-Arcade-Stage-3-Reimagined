"""Bounded original ARM DSP loaders in isolated RAM; no startup/IRQ/audio."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
from import_original_audio_dsp import inspect,DRIVER_SHA
WORK=Path(__file__).resolve().parents[3]/'work'
sys.path.insert(0,str(WORK/'audio_driver_tools'))
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_PROT_READ,UC_PROT_EXEC,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_PC,UC_ARM_REG_SP

class Reference:
    def __init__(self,driver,memory8=True):
        self.cpu=Uc(UC_ARCH_ARM,UC_MODE_ARM);c=self.cpu;c.mem_map(0,0x820000);c.mem_write(0,driver);c.mem_write(0xc000,bytes(0x4000))
        c.mem_protect(0x1000,0xb000,UC_PROT_READ|UC_PROT_EXEC);self.calls=0;self.instructions=0;self.serial=0
        def stm(uc,pc,size,user):
            sp=uc.reg_read(UC_ARM_REG_SP)-4;self.w(sp,pc+12);uc.reg_write(UC_ARM_REG_SP,sp);uc.reg_write(UC_ARM_REG_PC,pc+4)
        for pc in range(0x500,0xc000,4):
            if driver[pc:pc+4]==bytes.fromhex('00802de9'):c.hook_add(UC_HOOK_CODE,stm,begin=pc,end=pc)
        def count(uc,pc,size,user):self.instructions+=1
        c.hook_add(UC_HOOK_CODE,count);c.reg_write(UC_ARM_REG_R12,0xc200)
        c.mem_write(0x51,bytes([2 if memory8 else 0]));c.mem_write(0x802801,bytes([2 if memory8 else 0]))
        self.run(0x16e4);self.run(0x1630);self.run(0x612c)
    def read(self,p,n):return bytes(self.cpu.mem_read(p,n))
    def u(self,p):return struct.unpack('<I',self.read(p,4))[0]
    def byte(self,p):return self.read(p,1)[0]
    def w(self,p,v):self.cpu.mem_write(p,struct.pack('<I',v&0xffffffff))
    def run(self,pc,r0=None,r1=None):
        self.w(0x7dfff0,0x7e0000);self.cpu.reg_write(UC_ARM_REG_SP,0x7dfff0)
        if r0 is not None:self.cpu.reg_write(UC_ARM_REG_R0,r0)
        if r1 is not None:self.cpu.reg_write(UC_ARM_REG_R1,r1)
        self.cpu.emu_start(pc,0x7e0000,count=500000);self.calls+=1
        if self.cpu.reg_read(UC_ARM_REG_PC)!=0x7e0000:raise RuntimeError('DSP routine exceeded bound')
    def load(self,bank):
        self.serial+=1;self.w(0x60,self.serial);self.run(0xbec);self.run(0x153c);self.cpu.mem_write(0x10000,bank);self.run(0x1188)
    def unload(self):self.w(0x60,0xffffffff);self.run(0xbec);self.run(0x153c);self.run(0x1188)
    def select(self,bank,preset):self.run(0x5f78,bank,preset)
    def scene(self):self.cpu.reg_write(UC_ARM_REG_R11,0);self.cpu.reg_write(UC_ARM_REG_R10,0);self.run(0x133c)
    def snapshot(self):return self.read(0x802000,64)+self.read(0x803000,0xc00)+self.read(0x804000,0x5c8)+self.read(0x7f0000,0x10000)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--project',type=Path,required=True);a=p.parse_args()
    driver=(a.hostfs/'binary/AICADRV.bin').read_bytes();assert hashlib.sha256(driver).hexdigest()==DRIVER_SHA
    out=a.project/'verification/original-audio-dsp';out.mkdir(parents=True,exist_ok=True);pack=a.hostfs/'sound/pack';boot=(pack/'PACK20.bin.nz').read_bytes()
    results=[];totalCalls=totalInstructions=0
    for file in sorted(pack.glob('*.bin.nz')):
        bank=file.read_bytes();b=inspect(bank);name=file.name[:-7]
        if not b['presets']:continue
        r=Reference(driver);r.load(boot);assert r.byte(0xc21a)==2;r.load(bank);assert r.byte(0xc21a)==2
        for preset in b['presets']:
            r.select(b['bankId'],preset['index']);assert r.read(0x803000,0xc00)==preset['record'][36:]
            assert [r.u(0x802000+4*i) for i in range(16)]==preset['routes']
            assert r.read(0x7f0000,0x10000)==b'\x00\x60'*32768 and r.read(0x804000,0x5c8)==bytes(0x5c8)
            assert r.u(0x802804)==0x4fe0 and r.u(0xc284)==0x800000 and r.u(0xc290)==0x10000
            assert (r.byte(0xc218),r.byte(0xc219))==(b['bankId'],preset['index'])
            captured=b'IDSR0001'+struct.pack('<6I',2,r.u(0x802804),0x7f0000,32768,b['bankId'],preset['index'])+r.read(0x803000,0xc00)+r.read(0x802000,64)
            (out/(name+'.captured')).write_bytes(captured)
            r.w(0x804000,0x12345);r.w(0x7f0000,0x12345678);state=r.snapshot();r.select(b['bankId'],preset['index']);assert r.snapshot()==state
            r.select(b['bankId'],255);assert r.snapshot()==state;r.select(255,0);assert r.snapshot()==state
            r.select(b['bankId'],127);assert r.read(0x803000,0xc00)==bytes(0xc00) and r.read(0x804000,0x5c8)==bytes(0x5c8)
            assert [r.u(0x802000+4*i) for i in range(16)]==[0x10]*16 and r.u(0x7f0000)==0x12345678
            r.select(b['bankId'],preset['index']);r.w(0x804000,0xabcde);state=r.snapshot();r.unload();assert r.snapshot()==state
            assert r.u(0xc240)==0xffffffff and r.u(0xeb00)==0
            r.select(b['bankId'],preset['index']);assert r.snapshot()==state
            r.select(b['bankId'],127);state=r.snapshot();r.select(b['bankId'],preset['index']);assert r.snapshot()==state
        results.append(dict(name=name,bankId=b['bankId'],declaredRingCode=b['declaredRingCode'],actualRingCode=r.byte(0xc21a),presets=len(b['presets']),calls=r.calls,instructions=r.instructions))
        totalCalls+=r.calls;totalInstructions+=r.instructions
    scenes=[]
    for name in ('PACK20','PACK21','PACK22','PACK23','PACK24','PACK25','PACK0','PACK1','TYPE','SELECT'):
        r=Reference(driver);r.load(boot);r.scene();r.load((pack/(name+'.bin.nz')).read_bytes());r.scene()
        scenes.append(dict(bank=name,cache=[r.byte(0xc218),r.byte(0xc219)],common=r.u(0x802800),ringRegister=r.u(0x802804)))
        totalCalls+=r.calls;totalInstructions+=r.instructions
    # Control predicate proof. Bulk operations are hooked only after their
    # full implementation has been verified on every authored preset above.
    r=Reference(driver);registered=[];at=0x10000
    for name in ('PACK20','TYPE','SELECT','PACK10'):
        b=(pack/(name+'.bin.nz')).read_bytes();r.cpu.mem_write(at,b);registered.append(inspect(b));at+=len(b)
    def ret(uc):
        sp=uc.reg_read(UC_ARM_REG_SP);uc.reg_write(UC_ARM_REG_PC,r.u(sp));uc.reg_write(UC_ARM_REG_SP,sp+4)
    outcome=0
    def loadBoundary(uc,pc,size,user):
        nonlocal outcome
        outcome=2;r.cpu.mem_write(0xc218,bytes([uc.reg_read(UC_ARM_REG_R7)&255,uc.reg_read(UC_ARM_REG_R6)&255]));uc.reg_write(UC_ARM_REG_PC,0x6060)
    def clearBoundary(uc,pc,size,user):
        nonlocal outcome
        outcome=1;r.cpu.mem_write(0xc219,b'\xff');ret(uc)
    r.cpu.hook_add(UC_HOOK_CODE,loadBoundary,begin=0x5fdc,end=0x5fdc);r.cpu.hook_add(UC_HOOK_CODE,clearBoundary,begin=0x612c,end=0x612c)
    controls=[]
    for mask in (0,1,3,7,15):
        # Packed banks advance only across registered entries, exactly as3124.
        at=0x10000
        for i,name in enumerate(('PACK20','TYPE','SELECT','PACK10')):
            r.w(0xc240+4*i,i if mask&(1<<i) else 0xffffffff)
            if mask&(1<<i):b=(pack/(name+'.bin.nz')).read_bytes();r.cpu.mem_write(at,b);at+=len(b)
        for i in range(4,8):r.w(0xc240+4*i,0xffffffff)
        for oldBank in (0,1,2,46,255):
            for oldPreset in (0,1,255):
                for bankId in (1,2,46,255,registered[3]['bankId']):
                    for preset in (0,1,127,255):
                        r.cpu.mem_write(0xc218,bytes([oldBank,oldPreset]));outcome=0;r.select(bankId,preset)
                        controls.append([oldBank,oldPreset,bankId,preset,mask,outcome,r.byte(0xc218),r.byte(0xc219)])
    (out/'control-reference.txt').write_text(' '.join(str(x['bankId']) for x in registered)+'\n'+'\n'.join(' '.join(map(str,x)) for x in controls)+'\n')
    totalCalls+=r.calls;totalInstructions+=r.instructions
    r=Reference(driver);r.load(boot);r.load((pack/'TYPE.bin.nz').read_bytes())
    assert r.read(0xc218,2)==bytes([0,255]) and r.read(0x803000,0xc00)==bytes(0xc00)
    for command in (0xa0001280,0xa01c7f00,0xa0190080,0xa0000080):r.run(0x4624,command)
    assert r.read(0xc218,2)==bytes([1,0])
    program=r.read(0x803000,0xc00);r.load((pack/'PACK4.bin.nz').read_bytes());r.run(0x4624,0xa4701600);r.run(0x4624,0xa4000000)
    assert r.read(0x803000,0xc00)==program and r.read(0xc218,2)==bytes([22,0])
    engineCache=dict(commands=['A0001280','A01C7F00','A0190080(TYPE scene0)','A0000080','A4701600','A4000000'],finalCache=[22,0],programRemains='TYPE',explanation='Registration does not select DSP. A019 selects scene; A470 writes bank18 without invalidating preset19;5F78 cache hit precedes lookup')
    totalCalls+=r.calls;totalInstructions+=r.instructions
    # Full A4 dispatch, including masking, indexed return writes and untouched
    # preset route backups. Mono selection changes do not rewrite this handler.
    r=Reference(driver);routes=[]
    for mono in (0,128):
        r.cpu.mem_write(0xc21f,bytes([mono]))
        for index in range(16):
            for operation in (1,2):
                for argument in range(256):
                    old=0x0b15;r.w(0x802000+4*index,old);r.cpu.mem_write(0xf450,bytes([0xa5])*0x20);r.cpu.mem_write(0xf474,bytes([0x5a])*0x20)
                    command=0xa4000000|(operation<<20)|(index<<16)|(argument<<8);r.run(0x4624,command)
                    routes.append([mono,index,operation,argument,old,r.u(0x802000+4*index)])
                    assert r.read(0xf450,0x20)==bytes([0xa5])*0x20 and r.read(0xf474,0x20)==bytes([0x5a])*0x20
    (out/'return-reference.txt').write_text('\n'.join(' '.join(map(str,row)) for row in routes)+'\n')
    totalCalls+=r.calls;totalInstructions+=r.instructions
    report=dict(result='passed',driverSha256=DRIVER_SHA,banks=results,sceneResults=scenes,controlCases=len(controls),returnRouteCases=len(routes),engineCache=engineCache,calls=totalCalls,instructions=totalInstructions,instructionBound=500000,
        coverage=['All59originalpresets','SourceCOEF/MADRS/MPROtransfer','All16effectroutes','32Kword6000ringfill','firstbankringlatch','samesourceID/presetpreservation','FFno-op/7Fclear','missingbankno-op','unloadregistryremovalwithpersistentDSPstate'],
        boundary='Bounded isolated original routines only. All addresses including800000 are plain private RAM; no device emulation,startup,IRQ,main loop or audio output. Source ARM7STM{pc} PC+12 adaptation is explicit. Memory8MB flag is supplied from independently verified SH4 initialization. Predicate fixtures hookonlybulkload/clearboundary;allactualbulkstoresare separatelyverified.')
    (out/'source-reference.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Original DSP loader passed:',len(results),'banks,',len(controls),'control cases,',len(routes),'route cases,',totalCalls,'bounded calls,',totalInstructions,'original instructions')

if __name__=='__main__':main()
