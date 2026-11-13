#include "original_smoke_view.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned view=0xc900000,point=view+0x1000,stack=view+0xf000,stop=0xcfffffc;
unsigned checks=0,cases=0,accepted=0,rejected=0;std::uint64_t instructions=0;
void equal(float source,float native,const char* label){++checks;if(std::bit_cast<unsigned>(source)!=std::bit_cast<unsigned>(native)){
    std::cerr<<label<<" case="<<cases<<" source="<<std::hex<<std::bit_cast<unsigned>(source)<<" native="<<std::bit_cast<unsigned>(native)<<std::dec<<'\n';
    throw std::runtime_error("Original smoke view mismatch");}}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical image required");RefMemory m(argv[1]);
    unsigned random=0x1d0b60;const auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned trial=0;trial<512;++trial){
        m.clear();m.zeroRegion(view,0x10000);auto matrix=originalIdentityMatrix();
        if(trial%4){
            for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row)matrix.elements[col*4+row]=float(int(next()%2049)-1024)*.00390625f;
            for(unsigned i=12;i<15;++i)matrix.elements[i]=float(int(next()%8193)-4096)*.125f;
        }
        for(unsigned i=0;i<16;++i)m.writeFloat(view+64+i*4,matrix.elements[i]);
        const auto phase=std::uint16_t(trial%4==0?2367:trial%4==1?12864:trial%4==2?11164:1024+next()%28000);
        const float height=trial%3==0?64.f:trial%3==1?240.f:480.f;
        m.writeFloat(view+128,0);m.writeFloat(view+132,height);m.writeFloat(view+136,0);m.writeFloat(view+140,640);m.write32(view+152,phase);
        RefCpu c(m);c.r[4]=view;c.r[15]=stack;c.pr=stop;
        instructions+=c.run(0xc1d09e0,stop,3000);
        const auto native=originalSmokeView(matrix,phase,height);
        equal(m.readFloat(view+180),native.halfAngleTangent,"original half-angle tangent");
        equal(m.readFloat(view+168),native.viewportHeight,"original viewport height");
        equal(m.readFloat(view+180)*m.readFloat(view+168),native.focalScale(),"smoke sampling scale");
        for(unsigned plane=0;plane<4;++plane)for(unsigned axis=0;axis<3;++axis)
            equal(m.readFloat(view+192+plane*12+axis*4),native.planes[plane][axis],"normalized source side planes");
        for(unsigned sample=0;sample<128;++sample){
            const std::array<float,3> position{float(int(next()%8193)-4096)*.0625f,float(int(next()%8193)-4096)*.0625f,float(int(next()%8193)-4096)*.0625f};
            const float radius=sample%8==0?0.f:float(next()%257)*.03125f;
            for(unsigned i=0;i<3;++i)m.writeFloat(point+i*4,position[i]);
            RefCpu test(m);test.r[4]=view;test.r[5]=point;test.setFloat(4,radius);test.r[15]=stack;test.pr=stop;
            instructions+=test.run(0xc1d0b60,stop,3000);
            const float actual=originalSmokeFrustumDepth(native,position,radius);
            equal(test.getFloat(0),actual,"world sphere visibility/depth");accepted+=actual>0;rejected+=actual<0;++cases;
        }
    }
    if(accepted<100||rejected<100)throw std::runtime_error("Insufficient visibility branches");
    // The race registers ONE smoke view (ARace+76's model at +52).
    // A rear-view camera exists, but it is not passed to the smoke manager.
    // Execute the original call site; testing only the projection helper
    // missed the host's extra, non-arcade mirror submission.
    m.clear();m.zeroRegion(view,0x10000);
    constexpr unsigned race=view+0x2000,owner=view+0x4000,manager=view+0x6000;
    m.write32(owner+0xc0,race);m.write32(owner+0xd4,manager);
    m.write32(race+0x684,0);
    RefCpu binding(m);binding.r[14]=owner;binding.r[15]=stack;binding.pr=stop;
    instructions+=binding.run(0xc0662ae,0xc13e960,100);
    checks+=4;
    if(binding.r[4]!=manager||binding.r[5]!=1||binding.r[6]!=race+76+52||binding.r[7]!=0)
        throw std::runtime_error("Original race smoke view registration differs");
    std::cout<<"PASS "<<cases<<" source smoke visibility cases, "<<checks<<" bitwise checks, "<<instructions
        <<" original instructions; "<<accepted<<" accepted and "<<rejected<<" rejected. Source view setup, rational tangent, plane normalization and scalar world-sphere test execute without hooks.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
