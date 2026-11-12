#include "original_smoke_depth.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned display=0xc900000,parent=display+0x1000,view=display+0x2000,stack=display+0xf000,stop=0xcfffffc;
std::uint64_t cases=0,checks=0,instructions=0;unsigned nearCount=0,farCount=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void equal(unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<"case "<<cases<<' '<<why<<" source="<<std::hex<<a<<" native="<<b<<std::dec<<'\n';throw std::runtime_error("Original smoke footprint mismatch");}}
void equalFloat(float a,float b,const char* why){equal(std::bit_cast<unsigned>(a),std::bit_cast<unsigned>(b),why);}
void runCase(RefMemory& m,const OriginalMatrix& world,const OriginalMatrix& camera,float halfWidth,float halfLength,unsigned effectIndex){
    m.clear();m.zeroRegion(display,0x10000);m.write32(display+60,parent);m.write32(display+64,view);m.write32(display+68,effectIndex);
    m.writeFloat(display+72,halfWidth);m.writeFloat(display+80,halfLength);
    for(unsigned i=0;i<16;++i){m.writeFloat(parent+240+i*4,world.elements[i]);m.writeFloat(view+64+i*4,camera.elements[i]);}
    RefCpu c(m);c.r[4]=display;c.r[15]=stack;c.pr=stop;c.setFloat(4,-90.25f);c.setFloat(5,-1.125f);
    std::vector<std::array<unsigned,16>> matrixStack;unsigned pushes=0,pops=0;
    // Only global matrix-stack storage is external. Original matrix loading,
    // FTRV transforms, copy, subtraction and cross product all execute.
    c.callHooks[0xc1fcc60]=[&](RefCpu& cpu){matrixStack.push_back(cpu.xf);++pushes;};
    c.callHooks[0xc1f65c0]=[&](RefCpu& cpu){check(cpu.r[4]==1&&!matrixStack.empty(),"Balanced source stack pop");cpu.xf=matrixStack.back();matrixStack.pop_back();++pops;};
    instructions+=c.run(0xc140bc0,stop,100000);
    const auto native=originalSmokeFootprintDepth(world,camera,halfWidth,halfLength,effectIndex,-90.25f,-1.125f);
    equalFloat(c.getFloat(0),native.depth,"Footprint-selected depth");
    constexpr unsigned scratch=stack-272;
    for(unsigned i=0;i<4;++i){
        equal(m.read32(scratch+184+i*4),native.nearDepth[i],"Per-wheel depth choice");
        for(unsigned j=0;j<3;++j)equalFloat(m.readFloat(scratch+48+i*12+j*4),native.cameraCorners[i][j],"Camera-space footprint corner");
        nearCount+=native.nearDepth[i]!=0;farCount+=native.nearDepth[i]==0;
    }
    for(unsigned j=0;j<3;++j)equalFloat(m.readFloat(scratch+108+j*4),native.cameraCenter[j],"Camera-space owner origin");
    check(pushes==9&&pops==9&&matrixStack.empty(),"Complete footprint stack lifecycle");
    ++cases;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required");RefMemory memory(argv[1]);unsigned state=0x1735fbc0;
    const auto random=[&](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    for(unsigned trial=0;trial<1024;++trial){
        auto world=originalIdentityMatrix(),camera=originalIdentityMatrix();
        if(trial){
            const float angle=float(trial%64)*.09817477f;
            world.elements[0]=world.elements[10]=std::cos(angle);world.elements[2]=-std::sin(angle);world.elements[8]=std::sin(angle);
            if(trial%3==0)for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row){
                world.elements[col*4+row]=float(int(random()%2049)-1024)*.00390625f;
                camera.elements[col*4+row]=float(int(random()%2049)-1024)*.00390625f;
            }
            for(unsigned i=12;i<15;++i){world.elements[i]=float(int(random()%16385)-8192)*.0625f;camera.elements[i]=float(int(random()%16385)-8192)*.0625f;}
        }
        const float width=trial%11==0?0.f:float(1+random()%129)*.015625f;
        const float length=trial%13==0?0.f:float(1+random()%257)*.015625f;
        for(unsigned effect=0;effect<4;++effect)runCase(memory,world,camera,width,length,effect);
    }
    check(nearCount>1000&&farCount>1000,"Both depth outcomes exercised");
    std::cout<<"PASS "<<cases<<" original smoke footprint cases, "<<checks<<" bitwise/boundary checks, "<<instructions
        <<" original instructions. Four wheels, owner/view transforms, rotations, sloped/scaled bases, degenerate footprints and both depth choices. Only matrix-stack storage is hooked; original math executes.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
