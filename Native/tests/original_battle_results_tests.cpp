#include "original_results.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
namespace {
void require(bool b,const std::string& message){if(!b)throw std::runtime_error(message);}
void bitmap(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels,unsigned w,unsigned h){
    std::ofstream f(path,std::ios::binary);auto u16=[&](unsigned v){for(int i=0;i<2;i++)f.put(char(v>>(8*i)));};auto u32=[&](unsigned v){for(int i=0;i<4;i++)f.put(char(v>>(8*i)));};
    u16(0x4d42);u32(54+w*h*4);u32(0);u32(54);u32(40);u32(w);u32(0u-h);u16(1);u16(32);u32(0);u32(w*h*4);u32(2835);u32(2835);u32(0);u32(0);for(auto p:pixels)u32(p);require(bool(f),"results preview write failed");
}
}
int main(int argc,char**argv)try{
    if(argc<3)throw std::runtime_error("canonical-image game-root [preview-directory]");
    const auto native=OriginalBattleResults::load(argv[2]);RefMemory m(argv[1]);
    constexpr unsigned obj=0x0d000000,bank=0x0d001000,tls=0x0d002000,state=0x0d003000,vtable=0x0d004000,stack=0x0d100000,stop=0x00ff0000;
    m.zeroRegion(obj,0x10000);m.zeroRegion(stack,0x10000);m.zeroRegion(0x0d200000,0x100000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
    m.write32(tls+4,tls+128);m.write32(bank+60,bank+128);m.write32(bank+132,bank+256);
    m.write32(bank,vtable);m.write32(vtable+44,0x00fe0000);m.write32(vtable+60,0x00fe0004);
    m.write32(vtable+76,0x00fe0008);m.write32(vtable+84,0x00fe000c);
    m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
    unsigned heap=0x0d200000;RefCpu c(m);
    // Explicit host boundaries: TLS/allocator, empty bank container setup and
    // unused text-string controls. Every numeric widget constructor executes.
    c.callHooks[0x0c221fc0]=[&](auto& cpu){cpu.r[0]=tls;};
    c.callHooks[0x0c021960]=[&](auto& cpu){cpu.r[0]=heap;heap+=(cpu.r[5]+15)&~15;};
    c.callHooks[0x0c021ee0]=[&](auto& cpu){cpu.r[0]=heap;heap+=(cpu.r[4]+15)&~15;};
    c.callHooks[0x0c0219a0]=[](auto&){};
    c.callHooks[0x0c0ec1a0]=[&](auto& cpu){m.write32(cpu.r[4]+16,bank);cpu.r[0]=cpu.r[4];};
    c.callHooks[0x0c0d7bc0]=[](auto& cpu){cpu.r[0]=cpu.r[4];};
    c.callHooks[0x0c0d7e20]=[](auto&){};c.callHooks[0x0c1d4fc0]=[](auto& cpu){cpu.r[0]=0;};
    c.r[4]=obj;c.r[15]=stack+0xf000;c.pr=stop;
    std::size_t instructions=c.run(0x0c0ee600,stop,100000),cases=0,comparisons=0;
    const auto initialized=m.writes;
    c.r[4]=obj;c.r[15]=stack+0xf000;c.pr=stop;
    instructions+=c.run(0x0c0eeda0,stop,100000);
    const auto timeAttackInitialized=m.writes;
    // PR1 integer-division helpers are declared arithmetic boundaries. Native
    // formatting is checked against all surrounding actual source instructions.
    c.callHooks[0x0c2223e0]=[](auto& cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
    c.callHooks[0x0c2223b8]=[](auto& cpu){cpu.fpul=unsigned(signed32(cpu.r[4])/signed32(cpu.r[5]));};
    std::vector<OriginalHudDraw> captured;
    std::array<float,3> minusPosition{};
    const auto draw=[&](unsigned index,const original::OriginalMatrix& matrix){captured.push_back({OriginalHudDraw::Kind::polygon,index,0xffffffff,matrix});};
    c.callHooks[0x0c145c00]=[&](auto& cpu){original::OriginalMatrix matrix;for(unsigned i=0;i<16;++i)matrix.elements[i]=m.readFloat(cpu.r[6]+4*i);draw(cpu.r[5],matrix);};
    c.callHooks[0x00fe0000]=[&](auto& cpu){auto matrix=original::originalIdentityMatrix();if(cpu.r[5]==18)original::translateOriginalMatrix(matrix,minusPosition);draw(cpu.r[5],matrix);};
    c.callHooks[0x00fe0004]=[&](auto& cpu){auto matrix=original::originalIdentityMatrix();original::translateOriginalMatrix(matrix,{m.readFloat(cpu.r[6]),m.readFloat(cpu.r[6]+4),m.readFloat(cpu.r[6]+8)});draw(cpu.r[5],matrix);};
    c.callHooks[0x00fe0008]=[&](auto& cpu){require(cpu.r[5]==18,"unexpected glyph position query");for(unsigned i=0;i<3;++i)m.writeFloat(cpu.r[2]+i*4,minusPosition[i]);};
    c.callHooks[0x00fe000c]=[&](auto& cpu){require(cpu.r[5]==18,"unexpected glyph position setter");for(unsigned i=0;i<3;++i)minusPosition[i]=m.readFloat(cpu.r[15]+i*4);};
    for(unsigned mode:{0u,1u,2u})for(unsigned deduction=0;deduction<2;++deduction)for(unsigned status=0;status<3;++status)for(unsigned capacity:{2u,3u,4u})for(unsigned sample=0;sample<8;++sample)
    for(unsigned balanceBlink:{0u,1u,20u,30u}){
        if(mode==1&&deduction)continue;
        OriginalBattleResultsState s;s.resultStatus=status;s.sectionCapacity=capacity;s.sectionCount=sample%2?capacity:0;
        s.profileMode=mode;
        s.deduction=deduction!=0;s.circuitLayout=capacity!=4;
        s.balanceHighlighted=balanceBlink!=0;s.balanceVisible=balanceBlink!=20;
        s.totalTicks6000=sample?1024566u+sample*13u:0;s.signedAdvantage=std::array<float,8>{0,-.01f,.01f,123.45f,-453.99f,0.1f,99999.9f,-99999.9f}[sample];
        for(unsigned i=0;i<s.sectionCount;++i)s.sectionTimes6000[i]=s.totalTicks6000*(i+1)/capacity;
        const auto value=std::array<unsigned,8>{0,1,9,10,999,1000,12345678,99999999}[sample];s.points={1000,value,value,value,value};
        m.writes=mode==1?timeAttackInitialized:initialized;captured.clear();m.write32(state,status);m.write32(state+4,capacity);m.write32(state+8,s.totalTicks6000);
        unsigned previous=0;for(unsigned i=0;i<4;++i){const auto duration=i<s.sectionCount?s.sectionTimes6000[i]-previous:0;m.write32(state+12+4*i,duration);if(i<s.sectionCount)previous=s.sectionTimes6000[i];}
        m.writeFloat(state+28,s.signedAdvantage);m.write32(state+32,status>1);for(unsigned i=0;i<5;++i)m.write32(state+36+i*4,s.points[i]);
        m.write32(state+56,deduction);m.write32(state+60,s.circuitLayout);
        m.write32(state+64,s.balanceHighlighted);m.write32(obj+248,balanceBlink?balanceBlink-1:0);
        c.r[4]=obj;c.r[5]=0;c.r[6]=state;c.r[15]=stack+0xf000;c.pr=stop;for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
        instructions+=c.run(mode==1?0x0c0eefc0:0x0c0eec80,stop,500000);
        const auto expected=native.drawList(s);if(captured.size()!=expected.size()){std::cerr<<"case "<<status<<' '<<capacity<<' '<<sample<<"\noriginal: ";for(auto& d:captured)std::cerr<<d.index<<',';std::cerr<<"\nnative: ";for(auto& d:expected)std::cerr<<d.index<<',';std::cerr<<'\n';}require(captured.size()==expected.size(),"draw count mismatch "+std::to_string(captured.size())+" / "+std::to_string(expected.size()));
        for(unsigned i=0;i<expected.size();++i){const auto& a=captured[i];const auto& b=expected[i];++comparisons;require(a.index==b.index,"chunk mismatch draw "+std::to_string(i)+" original "+std::to_string(a.index)+" native "+std::to_string(b.index));
            for(unsigned j=0;j<16;++j){++comparisons;require(std::bit_cast<unsigned>(a.matrix.elements[j])==std::bit_cast<unsigned>(b.matrix.elements[j]),"matrix mismatch draw "+std::to_string(i)+" chunk "+std::to_string(a.index)+" word "+std::to_string(j)+" original "+std::to_string(a.matrix.elements[j])+" native "+std::to_string(b.matrix.elements[j]));}}
        ++cases;
    }
    if(argc>3){const std::filesystem::path directory=argv[3];std::filesystem::create_directories(directory);
        for(unsigned status=0;status<8;++status){OriginalBattleResultsState s;s.resultStatus=status<3?status:status==3?1:0;s.totalTicks6000=1024566;s.sectionCapacity=4;s.sectionCount=status==2?2:4;s.sectionTimes6000={240000,510000,780000,1024566};s.signedAdvantage=status==1?-87.6f:126.4f;s.points={1000,status==0?2000u:0u,status==0?1264u:0u,status==0?4264u:1000u,25436};
            if(status==3){s.profileMode=2;s.deduction=true;s.circuitLayout=true;s.sectionCapacity=s.sectionCount=3;s.sectionTimes6000={340000,680000,1024566,0};s.points={0,0,0,42000,120000};s.signedAdvantage=-100.f;}
            if(status>=4){s.balanceHighlighted=true;s.balanceVisible=status==4;s.points={1000,2000,1264,4264,25436};}
            if(status>=6){s.profileMode=1;s.balanceHighlighted=false;s.balanceVisible=true;s.points={1000,2000,2000,5000,30000};if(status==7){s.circuitLayout=true;s.sectionCapacity=s.sectionCount=3;s.sectionTimes6000={340000,680000,1024566,0};}}
            std::vector<unsigned> pixels(1280*720,0xff263542);native.paint(pixels,1280,720,s);require(std::count_if(pixels.begin(),pixels.end(),[](auto p){return p!=0xff263542;})>100000,"battle results artwork missing");bitmap(directory/("battle-results-"+std::to_string(status)+".bmp"),pixels,1280,720);}}
    OriginalBattleResultsState invalid;invalid.totalTicks6000=10;invalid.sectionCount=1;invalid.sectionTimes6000[0]=11;bool rejected=false;try{native.drawList(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"invalid cumulative clock accepted");
    // Original0EDFE0 asks its eight-entry low-digit widget to draw nine
    // positions before also drawing the high digit. Native storage avoids that
    // out-of-bounds read and emits all nine digits once; no bit-parity claim for
    // that undefined original allocation suffix.
    OriginalBattleResultsState maximum;maximum.points.fill(999999999);const auto maximumDraws=native.drawList(maximum);
    require(std::count_if(maximumDraws.begin(),maximumDraws.end(),[](const auto& d){return d.index==28;})==45,"native nine-digit points truncated");
    std::vector<unsigned> depthCheck(1280*720,0);native.paint(depthCheck,1280,720,{});
    unsigned brightLabels=0;for(unsigned yy=97;yy<123;++yy)for(unsigned xx=339;xx<610;++xx){const auto p=depthCheck[yy*1280+xx];if(((p>>16)&255)>220&&((p>>8)&255)>220&&(p&255)>220)++brightLabels;}
    require(brightLabels>450,"original result labels hidden by farther panel backing");
    std::cout<<"PASS "<<cases<<" actual-byte battle results cases, "<<comparisons<<" bit comparisons, "<<instructions<<" instructions; declared allocation/bank/text/graphics/PR1-division boundaries.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
