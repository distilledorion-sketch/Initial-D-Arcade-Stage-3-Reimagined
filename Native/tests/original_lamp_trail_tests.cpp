#include "original_lamp_trail.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0xc900000,car=owner+0x2000,actor=owner+0x4000;
constexpr unsigned viewRef=owner+0x5000,view=owner+0x6000,history=owner+0x7000;
constexpr unsigned model=owner+0x8000,vertices=owner+0x9000,scratch=owner+0x10000,stack=owner+0x1f000,stop=0xcfffffc;
std::uint64_t instructions=0;unsigned checks=0,cases=0;
void require(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
void equal(float a,float b,const char* label){++checks;if(std::bit_cast<unsigned>(a)!=std::bit_cast<unsigned>(b)&&!(std::isnan(a)&&std::isnan(b))){
    std::cerr<<label<<" case="<<cases<<" source="<<a<<" ("<<std::hex<<std::bit_cast<unsigned>(a)<<") host="<<b<<" ("<<std::bit_cast<unsigned>(b)<<")\n";
    throw std::runtime_error("Lamp trail source mismatch");}}
void put(RefMemory& m,unsigned address,const OriginalLampTrailPoint& p){for(unsigned j=0;j<3;++j)m.writeFloat(address+j*4,p[j]);}
void eqPoint(const RefMemory& m,unsigned address,const OriginalLampTrailPoint& p,const char* label){for(unsigned j=0;j<3;++j)equal(m.readFloat(address+j*4),p[j],label);}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required");RefMemory m(argv[1]);
    // Actual dynamic material constructor and owner edits. Only final texture
    // publication is intercepted; its128x32 size bits are tested separately.
    m.zeroRegion(owner,0x20000);
    for(unsigned index=0;index<2;++index){
        RefCpu init(m);init.r[4]=model+index*2048;init.r[5]=0x0100004a;init.r[6]=18;init.r[15]=stack;init.pr=stop;
        instructions+=init.run(0xc1d0260,stop,2000);
        RefCpu bind(m);bind.r[4]=model+index*2048;bind.r[5]=index;bind.r[15]=stack;bind.pr=stop;
        bind.callHooks[0xc1d8a60]=[](auto&){};
        instructions+=bind.run(0xc1d0720,stop,2000);m.write32(scratch+68+index*4,model+index*2048);
    }
    RefCpu material(m);material.r[14]=scratch;material.r[15]=stack;
    instructions+=material.run(0xc1a8e80,0xc1a8efa,1000);
    for(unsigned index=0;index<2;++index){
        const unsigned ich=m.read32(model+index*2048+16);
        require(m.read32(ich)==lampTrailPcw,"Original trail PCW");
        require(m.read32(ich+4)==lampTrailIsp,"Original trail ISP");
        require((m.read32(ich+8)&~63u)==(lampTrailTsp&~63u),"Original trail TSP");
    }
    // Actual owner gate, including unusual non-boolean values and both actor bindings.
    for(unsigned night=0;night<3;++night)for(unsigned brake=0;brake<3;++brake)
    for(unsigned attract=0;attract<3;++attract)for(unsigned blind=0;blind<2;++blind)
    for(unsigned braking=0;braking<2;++braking)for(unsigned attached=0;attached<2;++attached){
        m.clear();m.zeroRegion(owner,0x20000);m.write32(scratch+60,owner);m.write32(scratch+64,car);
        m.write32(owner+32,night);m.write32(owner+28,brake);m.write32(owner+272,attract);
        m.write32(car+0x95c,actor);m.write32(car+0x960,attached?actor+256:0);
        m.write32(actor+80,blind<<16);m.write32(actor+256+80,blind<<16);m.write8(car+80,braking);
        RefCpu c(m);c.r[14]=scratch;c.r[15]=stack;
        instructions+=c.run(0xc181f0c,0xc181fe8,300);
        const auto native=originalLampTrailSwitches(night,brake,attract,blind,braking);
        require(m.read32(scratch+76)==unsigned(native.front),"Source front lamp gate");
        require(m.read32(scratch+80)==unsigned(native.rear),"Source rear lamp gate");++cases;
        RefCpu replay(m);replay.r[14]=scratch;replay.r[15]=stack;
        instructions+=replay.run(0xc1a9aac,0xc1a9b08,300);
        const auto replayNative=originalReplayLampTrailSwitches(night,attract,blind);
        require(m.read32(scratch+76)==unsigned(replayNative.rear),"Source replay rear lamp gate");
        require(!replayNative.front,"Replay front trail disabled");++cases;
    }
    // Cut owner: continuous triggers restart suppression, ordinary frames drain it.
    for(unsigned variant=0;variant<2;++variant)
    for(unsigned initial=0;initial<12;++initial)for(unsigned flag=0;flag<3;++flag){
        m.clear();m.zeroRegion(owner,0x20000);OriginalLampTrailCut native{initial,flag};
        m.write32(owner+276,initial);m.write32(owner+284,flag);m.write32(owner+280,actor);
        for(unsigned tick=0;tick<35;++tick){unsigned changed=tick==0||tick==12||tick==13?1:tick==23?2:0;
            m.write32(actor,changed);RefCpu c(m);c.r[3]=owner+252;c.r[14]=scratch;c.r[15]=stack;
            const unsigned delta=variant?0x27b20:0;
            instructions+=c.run(0xc182206+delta,0xc18222c+delta,100);native.advance(changed);
            require(m.read32(owner+276)==native.frames276&&m.read32(owner+284)==native.blocked284,"Source cut suppression count");++cases;
        }
    }
    unsigned random=0x181780;const auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned variant=0;variant<2;++variant)for(unsigned trial=0;trial<512;++trial){
        const unsigned delta=variant?0x27ba0:0;
        m.clear();m.zeroRegion(owner,0x20000);OriginalLampTrailHistory native;
        for(unsigned i=0;i<9;++i){native.points[i]={float(int(next()%2049)-1024)*.001f,float(int(next()%2049)-1024)*.001f,-.51f};put(m,history+i*12,native.points[i]);}
        m.write32(scratch+480,history);RefCpu shift(m);shift.r[14]=scratch;shift.r[0]=480;shift.r[15]=stack;
        instructions+=shift.run(0xc1817aa+delta,0xc1817c4+delta,300);
        std::array<OriginalLampTrailPoint,4> cameraPoints{};
        for(auto& p:cameraPoints)p={float(int(next()%8193)-4096)*.125f,float(int(next()%4097)-2048)*.0625f,-float(4+next()%4096)*.125f};
        for(unsigned i=0;i<4;++i)put(m,scratch+304+i*12,cameraPoints[i]);
        const float nearClip=trial%2?.01f:1.f;m.write32(owner+24,viewRef);m.write32(viewRef,view);m.writeFloat(view+144,nearClip);
        m.write32(scratch+376,owner);
        RefCpu projection(m);projection.r[14]=scratch;projection.r[15]=stack;projection.r[12]=316;
        projection.r[9]=scratch+304;projection.r[8]=scratch+316;projection.r[10]=scratch+328;projection.r[11]=scratch+340;projection.r[13]=328;projection.setFloat(12,0);
        instructions+=projection.run(0xc1819b8+delta,0xc181a7a+delta,300);
        const auto projected=projectOriginalLampTrail(cameraPoints,nearClip);native.push(projected.points[0]);
        for(unsigned i=0;i<4;++i)eqPoint(m,scratch+304+i*12,projected.points[i],"Source projected point");
        equal(projection.getFloat(13),projected.height1000,"Source projected lamp height");
        for(unsigned i=0;i<9;++i)eqPoint(m,history+i*12,native.points[i],"Source nine-frame history");
        const unsigned opacity=next()%256;
        // Real ribbon expansion (normalization included), followed by the real
        // dynamic strip writer. Only final PVR publication/draw leaves are hooked.
        RefCpu ribbon(m);ribbon.r[14]=scratch;ribbon.r[15]=stack;ribbon.setFloat(13,projected.height1000);
        instructions+=ribbon.run(0xc181a88+delta,0xc181c16+delta,12000);
        const auto strip=native.strip(projected.height1000,opacity);
        for(unsigned i=0;i<18;++i)eqPoint(m,scratch+24+i*12,strip[i].position,"Source ribbon expanded position");
        m.write32(model+16,vertices);
        RefCpu write(m);write.r[4]=owner;write.r[5]=model;write.r[6]=scratch+24;write.r[7]=opacity;write.r[15]=stack;write.pr=stop;
        unsigned draws=0;write.callHooks[0xc1d5400]=[&](auto& q){require(q.r[4]==model+32,"Source strip publication");q.r[0]=model;};
        write.callHooks[0xc1d7120]=[&](auto& q){require(q.r[4]==model,"Source strip draw");++draws;};
        instructions+=write.run(0xc181600+delta,stop,12000);require(draws==1,"One strip draw");
        for(unsigned i=0;i<18;++i){const auto address=vertices+32+i*32;eqPoint(m,address+4,strip[i].position,"Published strip vertex");
            equal(m.readFloat(address+16),strip[i].u,"Source strip U");equal(m.readFloat(address+20),strip[i].v,"Source strip V");
            require(m.read32(address+24)==strip[i].argb,"Source strip alpha");
            require((m.read32(address)&0xe0000000)==strip[i].stripControl,"Source strip control");}
        // Source invalidation collapses all points to the freshly sampled lamp.
        RefCpu collapse(m);collapse.r[14]=scratch;collapse.r[15]=stack;collapse.r[3]=1;collapse.r[7]=8;
        instructions+=collapse.run(0xc181e20+delta,0xc181e3e+delta,200);native.collapse();
        for(unsigned i=0;i<9;++i)eqPoint(m,history+i*12,native.points[i],"Source disabled-lamp collapse");++cases;
    }
    std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<instructions<<" original instructions. Lamp gates, camera-cut delay, nine-frame history, near-plane projection, ribbon expansion/normalization and strip UV/alpha/control match. Only final strip publication/draw leaves hooked. Full visibility/occlusion owner, outer attract schedule and visual integration remain separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
