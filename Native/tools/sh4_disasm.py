# -*- coding: utf-8 -*-
"""SH-4 disassembler with the FPU group capstone's SH mode does not decode.

capstone stops at the first 1111xxxx word and prints nothing useful for the
FPUL/FPSCR transfers either, which between them hide every float computation in
the image -- the force-feedback owner, the camera tables and the solver all read
as .word without this.

capstone stops or emits .word for every 1111xxxx opcode, which is exactly the
half of the force-feedback owner that matters -- the physics-to-force mapping is
all single-precision. Only the F group is added here; everything else is still
capstone's.
"""
import sys,struct
from capstone import *
# usage: sh4_disasm.py <image> <start> <end> [-n]
# The image is the canonical SH4 main binary; BASE is where it is mapped.
BASE=0x0C020000
d=open(sys.argv[1],'rb').read()
md=Cs(CS_ARCH_SH,CS_MODE_SH4|CS_MODE_LITTLE_ENDIAN)

def fpu(w):
    n=(w>>8)&0xf; m=(w>>4)&0xf; lo=w&0xf
    FR=lambda i:"fr%d"%i; DR=lambda i:"dr%d"%i
    if lo==0x0: return "fadd     %s,%s"%(FR(m),FR(n))
    if lo==0x1: return "fsub     %s,%s"%(FR(m),FR(n))
    if lo==0x2: return "fmul     %s,%s"%(FR(m),FR(n))
    if lo==0x3: return "fdiv     %s,%s"%(FR(m),FR(n))
    if lo==0x4: return "fcmp/eq  %s,%s"%(FR(m),FR(n))
    if lo==0x5: return "fcmp/gt  %s,%s"%(FR(m),FR(n))
    if lo==0x6: return "fmov     @(r0,r%d),%s"%(m,FR(n))
    if lo==0x7: return "fmov     %s,@(r0,r%d)"%(FR(m),n)
    if lo==0x8: return "fmov     @r%d,%s"%(m,FR(n))
    if lo==0x9: return "fmov     @r%d+,%s"%(m,FR(n))
    if lo==0xa: return "fmov     %s,@r%d"%(FR(m),n)
    if lo==0xb: return "fmov     %s,@-r%d"%(FR(m),n)
    if lo==0xc: return "fmov     %s,%s"%(FR(m),FR(n))
    if lo==0xe: return "fmac     fr0,%s,%s"%(FR(m),FR(n))
    if lo==0xd:
        if w==0xf3fd: return "fschg"
        if w==0xfbfd: return "frchg"
        sub=(w>>4)&0xf
        one={0x0:"fsts     fpul,%s"%FR(n),0x1:"flds     %s,fpul"%FR(n),
             0x2:"float    fpul,%s"%FR(n),0x3:"ftrc     %s,fpul"%FR(n),
             0x4:"fneg     %s"%FR(n),0x5:"fabs     %s"%FR(n),
             0x6:"fsqrt    %s"%FR(n),0x7:"fsrra    %s"%FR(n),
             0x8:"fldi0    %s"%FR(n),0x9:"fldi1    %s"%FR(n),
             0xa:"fcnvsd   fpul,%s"%DR(n&0xe),0xb:"fcnvds   %s,fpul"%DR(n&0xe),
             0xe:"fipr     fv%d,fv%d"%((n&3)*4,(n&0xc))}
        if sub==0xf:
            if (n&1)==0: return "fsca     fpul,%s"%DR(n&0xe)
            if (n&3)==1: return "ftrv     xmtrx,fv%d"%(n&0xc)
        return one.get(sub,".word 0x%04X"%w)
    return ".word 0x%04X"%w

SYS={0x5a:("lds      r%d,fpul","sts      fpul,r%d"),
     0x6a:("lds      r%d,fpscr","sts      fpscr,r%d"),
     0x56:("lds.l    @r%d+,fpul",None),0x52:(None,"sts.l    fpul,@-r%d"),
     0x66:("lds.l    @r%d+,fpscr",None),0x62:(None,"sts.l    fpscr,@-r%d")}
def sysreg(w):
    """capstone's SH mode drops the FPUL/FPSCR transfers, and every ftrc/float
    result crosses through one of them, so the math is unreadable without."""
    n=(w>>8)&0xf; lo=w&0xff; top=w>>12
    if top==4 and lo in SYS and SYS[lo][0]: return SYS[lo][0]%n
    if top==0 and lo in SYS and SYS[lo][1]: return SYS[lo][1]%n
    return None

def lit(pc,disp):
    t=((pc&~3)+4+disp*4)-BASE; return struct.unpack('<I',d[t:t+4])[0]

a=int(sys.argv[2],16); end=int(sys.argv[3],16)
skipnop='-n' in sys.argv
while a<end:
    off=a-BASE; w=struct.unpack('<H',d[off:off+2])[0]
    if (w>>12)==0xf: txt=fpu(w)
    elif sysreg(w): txt=sysreg(w)
    else:
        txt=None
        for i in md.disasm(d[off:off+2],a): txt="%-8s %s"%(i.mnemonic,i.op_str); break
        if txt is None: txt=".word 0x%04X"%w
    note=''
    if (w>>12)==0xd: note='  ; = 0x%08X'%lit(a,w&0xff)
    elif (w>>12)==0x9:
        t=(a+4+(w&0xff)*2)-BASE; note='  ; = 0x%04X'%struct.unpack('<H',d[t:t+2])[0]
    elif (w>>12)==0xc and ((w>>8)&0xf)==7:
        t=((a&~3)+4+(w&0xff)*4)
        v=struct.unpack('<I',d[t-BASE:t-BASE+4])[0]
        note='  ; mova 0x%08X -> 0x%08X (%g)'%(t,v,struct.unpack('<f',struct.pack('<I',v))[0])
    if not(skipnop and txt.strip()=='nop'): print("%08X  %-34s%s"%(a,txt,note))
    a+=2
