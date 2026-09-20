#include "original_course_lighting.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned object=0x0d000000,ex=object+0x2000,node=ex+0x100,stop=0x0f000000;
std::size_t checks=0,instructions=0,divisionCalls=0;
void eq(unsigned a,unsigned b,const char*field,unsigned row=0,unsigned light=0){++checks;if(a!=b){std::ostringstream s;s<<field<<" row="<<row<<" light="<<light<<" original="<<std::hex<<a<<" native="<<b;throw std::runtime_error(s.str());}}
unsigned bits(float v){return std::bit_cast<unsigned>(v);}
void run(RefCpu&c,unsigned start,unsigned end=stop){c.pr=stop;instructions+=c.run(start,end,100000);}
void setMatrix(RefCpu&c,const OriginalLightMatrix&m){for(unsigned i=0;i<16;++i)c.xf[i]=bits(m[i]);}
void unsignedDivide(RefCpu&c){
    //2223E0 is a compiler unsigned-divide leaf using PR1 double arithmetic.
    // Its divisor0/1 branch returns the dividend. The finite tested calls
    // below all have quotient<=INT32_MAX, so its FTRC equals integer quotient.
    if(c.r[4]!=0xff000000||c.r[5]!=256)throw std::runtime_error("Unexpected compiler divide arguments");
    const auto q=c.r[4]/c.r[5];
    c.fpul=q;++divisionCalls;
}
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("canonical image and FSCA table required");
    RefMemory m(argv[1]);std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");
    std::mt19937 rng(195303);std::uniform_real_distribution<float> unit(-1.f,1.f);
    for(unsigned row=0;row<36;++row){
        m.clear();m.zeroRegion(object,0x100000);m.zeroRegion(0x0c92f000,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
        m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
        m.write32(ex+4,node);m.write32(node,node);m.write32(node+4,node);
        RefCpu c(m);c.r[15]=object+0x80000;c.fscaHalfWave=fsca;setMatrix(c,originalLightIdentityMatrix);
        c.callHooks[0x0c221fc0]=[&](auto&cpu){cpu.r[0]=ex;};
        unsigned allocation=object+0x10000;c.callHooks[0x0c021960]=[&](auto&cpu){cpu.r[0]=allocation;allocation+=0x1000;};
        c.callHooks[0x0c21b460]=[](auto&){};c.callHooks[0x0c1dbe80]=[](auto&){};
        m.write32(0x0c92ea7c,row/4);m.write32(0x0c92ea78,row/2%2);m.write32(0x0c92ea74,row%2);
        c.r[4]=object;run(c,0x0c19acc0);c.r[4]=object;run(c,0x0c19afa0);
        auto native=originalCourseLightingTableRow(row/4,row/2%2,row%2);
        eq(m.read32(object+80),0,"constructor matrix");
        const auto set=m.read32(object+60);eq(m.read32(set+68),native.count,"registration count",row);
        for(unsigned i=0;i<3;++i)eq(m.read32(set+72+4*i),bits(native.ambient[i]),"ambient",row);
        for(unsigned i=0;i<native.count;++i){const auto p=m.read32(set+4+4*i);const auto&l=native.lights[i];
            const unsigned slot=l.kind==OriginalCourseLightKind::Spot?68+4*l.sourceSlot:l.kind==OriginalCourseLightKind::Parallel?32+4*l.sourceSlot:44+4*l.sourceSlot;
            eq(m.read32(object+108+slot),p,"registration order",row,i);
            eq(m.read32(p+20),l.kind==OriginalCourseLightKind::Spot?2:0,"light kind",row,i);eq(m.read8(p+24),l.enabled,"enabled",row,i);
            for(unsigned j=0;j<3;++j){eq(m.read32(p+32+4*j),bits(l.color[j]),"color",row,i);eq(m.read32(p+(l.kind==OriginalCourseLightKind::Spot?68:44)+4*j),bits(l.incomingDirection[j]),"direction",row,i);}
            if(l.kind==OriginalCourseLightKind::Spot){for(unsigned j=0;j<3;++j)eq(m.read32(p+44+4*j),bits(l.position[j]),"position",row,i);
                eq(m.read32(p+60),bits(l.distance0),"distance0",row,i);eq(m.read32(p+64),bits(l.distance1),"distance1",row,i);eq(m.read32(p+84),l.angle0,"angle0",row,i);eq(m.read32(p+88),l.angle1,"angle1",row,i);}
        }
        for(unsigned sample=0;sample<24;++sample){
            auto matrix=originalLightIdentityMatrix;
            if(sample)for(unsigned j=0;j<16;++j)matrix[j]=j%4==3?(j==15?1.f:0.f):unit(rng)*(j>=12?320.f:1.f);
            if(sample>=2){
                // Exercise the complete original1AD580, including actual
                //1FC5A0 matrix load,1F6280 transforms, cross and setters.
                for(unsigned j=0;j<16;++j)m.write32(object+0x50000+4*j,bits(matrix[j]));
                c.callHooks[0x0c1f6610]=[](auto&){};c.callHooks[0x0c1f65c0]=[](auto&){};
                c.r[4]=object+108;c.r[5]=object+0x50000;run(c,0x0c1ad580);
                updateOriginalCourseRelativeDirections(native,matrix);
                for(unsigned i=0;i<native.count;++i)if(native.lights[i].kind==OriginalCourseLightKind::RelativeParallel){const auto p=m.read32(set+4+4*i);for(unsigned j=0;j<3;++j)eq(m.read32(p+44+4*j),bits(native.lights[i].incomingDirection[j]),"relative update",row,i);}
            }
            // Disabled entries exercise source compaction independently of
            // authored order, including the all-disabled final case.
            for(unsigned i=0;i<native.count;++i){const bool enabled=sample<12||((i+sample)%3==0&&sample!=23);native.lights[i].enabled=enabled;m.write8(m.read32(set+4+4*i)+24,enabled);}
            run(c,0x0c1cefe0);c.callHooks[0x0c1cf0a0]=[](auto&){};
            c.r[4]=set;run(c,0x0c0538a0);
            const auto packet=originalCourseLightingPacket(native,matrix);
            // Original high-level→packed GLM stops before PVR PREF.
            m.write32(0x0c9801ec,1);m.write32(0x0c980264,0x09000000);
            c.r[4]=0x0c92f704;run(c,0x0c1d3d00,0x0c1d3e10);
            for(unsigned j=0;j<8;++j)eq(m.read32(c.r[7]+4*j),packet.glm[j],"GLM packet",row,sample);
            // Original1CF0A0 iterates masks, transforms position/direction,
            // and dispatches packing. Stop only list/device submission.
            c.callHooks.erase(0x0c1cf0a0);c.callHooks[0x0c1d0fc0]=[](auto&){};c.callHooks[0x0c1d1000]=[](auto&){};
            c.callHooks[0x0c1d3d00]=[](auto&){};
            c.callHooks[0x0c1d3a60]=[&](auto&cpu){
                const auto index=m.read8(cpu.r[4]);if(index>=packet.count)return; // GLM special selector15 is reset metadata, outside active masks.
                RefCpu d(m);d.fscaHalfWave=fsca;d.r[15]=object+0x70000;d.r[4]=cpu.r[4];d.r[5]=object+0x60000;d.callHooks[0x0c2223e0]=unsignedDivide;
                m.write32(0x0c99a16c,32);run(d,0x0c1d3860);
                for(unsigned j=0;j<8;++j){auto expected=m.read32(object+0x60000+4*(7-j));if(j==1)expected|=index;eq(expected,packet.lights[index][j],"light packet",row,index);}
            };
            setMatrix(c,matrix);run(c,0x0c1cf0a0);
            c.callHooks.erase(0x0c1d3d00);
        }
    }
    // Independently exercise direction quantization near signed boundaries,
    // including source's representable -2048 at an exact direction of -1.
    m.clear();m.zeroRegion(object,0x100000);m.zeroRegion(0x0c99a16c,4);m.write32(0x0c99a16c,32);
    for(unsigned sample=0;sample<4096;++sample){
        OriginalCourseLighting s;s.count=1;s.lights[0].color={.92f,.57f,.127f};
        for(unsigned j=0;j<3;++j)s.lights[0].incomingDirection[j]=sample<2048?float(int(sample)-1024)/512.f:unit(rng)*2.f;
        if(sample==4095)s.lights[0].incomingDirection={-1.f,std::nextafter(-1.f,-2.f),std::nextafter(1.f,2.f)};
        const auto native=originalCourseLightingPacket(s,originalLightIdentityMatrix);
        const unsigned raw=object+0x1000;m.write8(raw+1,2);m.write32(raw+44,0x10000);
        for(unsigned j=0;j<3;++j){m.writeFloat(raw+8+4*j,s.lights[0].color[j]);m.writeFloat(raw+32+4*j,s.lights[0].incomingDirection[j]);}
        RefCpu c(m);c.r[15]=object+0x70000;c.r[4]=raw;c.r[5]=object+0x60000;run(c,0x0c1d3860);
        for(unsigned j=0;j<8;++j)eq(m.read32(object+0x60000+4*(7-j)),native.lights[0][j],"direction boundary",sample);
    }
    bool rejected=false;try{originalCourseLighting(9,false,false);}catch(const std::out_of_range&){rejected=true;}eq(rejected,true,"invalid course");
    std::cout<<"PASS36 course conditions,24 matrix/enable cases each,4096 direction boundary cases; "<<checks<<" comparisons, "<<instructions<<" original instructions; "<<divisionCalls<<" modeled FF000000/256 compiler division leaf calls.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
