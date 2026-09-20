#include "original_road_contact.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t driveAddress=0x0C900F00,actorAddress=0x0CFC0000;
constexpr std::uint32_t stack=0x0CFFF000,stop=0x0CFFFFFC;
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
std::uint32_t seed=0xDBC32807;
std::uint32_t randomBits(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
float number(){return float(randomBits()%65537)/65536.0f;}
std::size_t comparisons=0,instructions=0;
void equal(std::uint32_t expected,std::uint32_t actual,const std::string& label){
    ++comparisons;
    if(expected!=actual){std::cerr<<label<<" expected="<<std::hex<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("Original road contact mismatch");}
}
void writeQuery(RefMemory& m,std::uint32_t address,const OriginalCollisionQuery& q){
    for(std::size_t i=0;i<q.words.size();++i)m.write32(address+std::uint32_t(i*4),q.words[i]);
}
void load(RefMemory& m,const OriginalDriveState& d,const OriginalActorState& a,const OriginalRoadContactState& s,bool clear=true){
    if(clear){m.clear();m.zeroRegion(stack-0x10000,0x11000);}
    for(std::size_t i=0;i<d.words.size();++i)m.write32(driveAddress+std::uint32_t(i*4),d.words[i]);
    for(std::size_t i=0;i<a.words.size();++i)m.write32(actorAddress+std::uint32_t(i*4),a.words[i]);
    m.write32(0x0C900954,actorAddress);m.write32(0x0C92DE30,s.tick0C92DE30);
    m.write32(0x0C91FB20,0x0CFB0000);m.write32(0x0C91FB24,0x0CFB1000);m.write32(0x0C91FB28,0x0CFB2000);
    m.writeFloat(0x0C900E5C,s.impact0C900E5C);m.writeFloat(0x0C900E60,s.impact0C900E60);
    for(std::size_t i=0;i<4;++i){
        writeQuery(m,0x0CAA9518+std::uint32_t(i*64),s.surfaces0CAA9518[i]);
        writeQuery(m,0x0CAA9618+std::uint32_t(i*64),s.sweeps0CAA9618[i]);
        for(std::size_t j=0;j<3;++j)m.writeFloat(0x0CAA94C8+std::uint32_t(i*12+j*4),s.normals0CAA94C8[i][j]);
        m.write32(0x0CAA94F8+std::uint32_t(i*4),s.flags0CAA94F8[i]);
        m.writeFloat(0x0CAA9508+std::uint32_t(i*4),s.impacts0CAA9508[i]);
    }
}
void check(const RefMemory& m,const OriginalDriveState& d,const OriginalActorState& a,
        const OriginalRoadContactState& s,std::uint32_t sample,const std::vector<std::uint32_t>& feedback,std::size_t diagnostics){
    const auto address=[&](std::uint32_t addr,std::uint32_t actual){equal(m.read32(addr),actual,"sample "+std::to_string(sample)+" address "+std::to_string(addr));};
    for(std::size_t i=0;i<d.words.size();++i)address(driveAddress+std::uint32_t(i*4),d.words[i]);
    for(std::size_t i=0;i<a.words.size();++i)address(actorAddress+std::uint32_t(i*4),a.words[i]);
    for(std::size_t i=0;i<4;++i){
        for(std::size_t j=0;j<16;++j){
            address(0x0CAA9518+std::uint32_t(i*64+j*4),s.surfaces0CAA9518[i].words[j]);
            address(0x0CAA9618+std::uint32_t(i*64+j*4),s.sweeps0CAA9618[i].words[j]);
        }
        for(std::size_t j=0;j<3;++j)address(0x0CAA94C8+std::uint32_t(i*12+j*4),bits(s.normals0CAA94C8[i][j]));
        address(0x0CAA94F8+std::uint32_t(i*4),s.flags0CAA94F8[i]);
        address(0x0CAA9508+std::uint32_t(i*4),bits(s.impacts0CAA9508[i]));
    }
    address(0x0C900E5C,bits(s.impact0C900E5C));address(0x0C900E60,bits(s.impact0C900E60));
    equal(std::uint32_t(feedback.size()),std::uint32_t(s.feedback142460.size()),"feedback count");
    for(std::size_t i=0;i<feedback.size();++i)equal(feedback[i],s.feedback142460[i],"feedback id");
    equal(std::uint32_t(diagnostics),s.invalidScalarDiagnostics,"invalid scalar diagnostic count");
    address(0x0C91FB20,0x0CFB0000+std::uint32_t(s.impactRecords.size()*12));
    address(0x0C91FB24,0x0CFB1000+std::uint32_t(s.impactRecords.size()*4));
    address(0x0C91FB28,0x0CFB2000+std::uint32_t(s.impactRecords.size()*4));
    for(std::size_t i=0;i<s.impactRecords.size();++i){
        for(std::size_t j=0;j<3;++j)address(0x0CFB0000+std::uint32_t(i*12+j*4),bits(s.impactRecords[i].position[j]));
        address(0x0CFB1000+std::uint32_t(i*4),bits(s.impactRecords[i].magnitude));
        address(0x0CFB2000+std::uint32_t(i*4),s.impactRecords[i].tick);
    }
}
struct QueryOutput {std::array<std::uint32_t,3> normal{};std::uint32_t scalar=0,flags=0;};
void publish(OriginalCollisionQuery& q,const QueryOutput& out){
    for(std::size_t i=0;i<3;++i)q.setu(i*4,out.normal[i]);
    q.setu(24,out.scalar);q.setu(28,out.flags);
}
void publish(RefMemory& m,std::uint32_t address,const QueryOutput& out){
    for(std::size_t i=0;i<3;++i)m.write32(address+std::uint32_t(i*4),out.normal[i]);
    m.write32(address+24,out.scalar);m.write32(address+28,out.flags);
}
void randomize(OriginalDriveState& d,OriginalActorState& a,OriginalRoadContactState& s){
    for(auto& w:d.words)w=bits(number()*4.0f-2.0f);
    for(auto& w:a.words)w=bits(number()*4.0f-2.0f);
    for(std::size_t i=0;i<4;++i){
        for(auto& w:s.surfaces0CAA9518[i].words)w=bits(number()*4-2);
        for(auto& w:s.sweeps0CAA9618[i].words)w=bits(number()*4-2);
        for(auto& f:s.normals0CAA94C8[i])f=number()*4-2;
        s.flags0CAA94F8[i]=randomBits();s.impacts0CAA9508[i]=(randomBits()%2)?number():0.0f;
    }
    s.tick0C92DE30=randomBits();s.impact0C900E5C=number();s.impact0C900E60=number();
    d.setu(316,randomBits()%58);d.setu(328,randomBits()%2);d.setu(336,randomBits()%2);
    d.setu(1012,randomBits()%105);d.setu(1020,randomBits());d.setu(1028,randomBits());
    for(std::size_t i=0;i<4;++i)d.setu(356+i*4,randomBits()%32);
}
OriginalContactPoint controlledTransform(const OriginalActorState& a,const OriginalContactPoint& p){
    // Deliberately identified test boundary; this is not original matrix code.
    return {p[0]+a.f(0),(p[1]+p[0]*0.125f)+a.f(4),p[2]+a.f(8)};
}
std::vector<std::uint8_t> read(const std::filesystem::path& p){
    std::ifstream file(p,std::ios::binary);if(!file)throw std::runtime_error("Original contact test data unavailable");
    return {std::istreambuf_iterator<char>(file),{}};
}
std::uint32_t word(const std::vector<std::uint8_t>& raw,std::size_t offset){
    return std::uint32_t(raw.at(offset))|(std::uint32_t(raw.at(offset+1))<<8)|
        (std::uint32_t(raw.at(offset+2))<<16)|(std::uint32_t(raw.at(offset+3))<<24);
}
}

int main(int argc,char** argv)try{
    if(argc!=5)throw std::invalid_argument("Pass canonical original program image, HOSTFS, primary FSCA header, exported FSCA table");
    RefMemory memory{std::filesystem::path(argv[1])};
    const auto fsca=OriginalFscaTable::load(argv[4]);
    std::ifstream primary(argv[3],std::ios::binary);
    if(!primary)throw std::runtime_error("Primary FSCA header unavailable");
    const std::string sourceText{std::istreambuf_iterator<char>(primary),{}};
    const std::regex pattern("0x([0-9A-Fa-f]{8})");std::vector<std::uint32_t> halfWave;
    for(std::sregex_iterator it(sourceText.begin(),sourceText.end(),pattern),end;it!=end;++it)
        halfWave.push_back(std::stoul((*it)[1].str(),nullptr,16));
    if(halfWave.size()!=32768)throw std::runtime_error("Primary FSCA source word count mismatch");
    for(std::uint32_t sample=0;sample<2048;++sample){
        OriginalDriveState d;OriginalActorState a;OriginalRoadContactState s;randomize(d,a,s);
        OriginalRoadContactParameters p;
        for(auto& value:p.geometry0C2700F4)value=number()*2+0.5f;
        std::array<QueryOutput,4> surfaces{},sweeps{};
        for(std::size_t i=0;i<4;++i){
            for(auto* q:{&surfaces[i],&sweeps[i]}){
                for(auto& n:q->normal)n=bits(number()*2-1);
                q->scalar=bits(number()*12-6);q->flags=randomBits()&0xFFFFu;
            }
            // Exercise exact zero, invalid scalar, and impact-counter limits.
            if(sample%8==0)sweeps[i].scalar=0;
            if(sample%37==0)sweeps[i].scalar=0x7F800000;
            if(sample%41==0)sweeps[i].scalar=0x7FC12345;
            if(sample%13==0)surfaces[i].scalar=bits(4.0f);
        }
        if(sample%7==0)d.setu(316,0);
        if(sample%11==0)d.setu(1012,0xFFFFFFFFu);
        if(sample%17==0){
            const auto& t=p.geometry0C2700F4;
            float depth=1.0f;bool terrain=false;
            for(std::size_t i=0;i<4;++i)terrain|=d.u(356+i*4)==28;
            if(d.u(316)!=0||terrain)depth=std::bit_cast<float>(0x3E0F5C29u);
            const std::array<OriginalContactPoint,4> pts={{{t[0],-depth,t[2]},{-t[0],-depth,t[2]},
                {t[1],-depth,-t[3]},{-t[1],-depth,-t[3]}}};
            for(std::size_t i=0;i<4;++i){const auto out=controlledTransform(a,pts[i]);for(std::size_t j=0;j<3;++j)d.setf(680+i*12+j*4,out[j]);}
        }
        load(memory,d,a,s);memory.write32(0x0C901654,0);
        for(std::size_t i=0;i<11;++i)memory.writeFloat(0x0C2700F4+std::uint32_t(i*4),p.geometry0C2700F4[i]);
        RefCpu cpu(memory);cpu.r[15]=stack;cpu.pr=stop;
        std::vector<std::uint32_t> feedback;std::size_t diagnostics=0,transforms=0,surfaceCalls=0,sweepCalls=0;
        cpu.callHooks[0x0C142460]=[&](RefCpu& c){feedback.push_back(c.r[4]);};
        cpu.callHooks[0x0C055D60]=[&](RefCpu&){++diagnostics;};
        for(const auto function:{0x0C1FCC60u,0x0C1F6AD0u,0x0C1F65C0u})cpu.callHooks[function]=[](RefCpu&){};
        cpu.callHooks[0x0C1F6AC0]=[&](RefCpu& c){for(std::size_t j=0;j<3;++j)equal(a.u(j*4),c.fr[4+j],"translate argument");};
        for(const auto [function,offset]:{std::pair{0x0C1F6780u,28u},{0x0C1F6770u,24u},{0x0C1F6790u,32u}})
            cpu.callHooks[function]=[&,offset](RefCpu& c){equal(a.u(offset),c.fr[4],"rotation argument");};
        cpu.callHooks[0x0C1F6260]=[&](RefCpu& c){
            OriginalContactPoint in{};for(std::size_t j=0;j<3;++j)in[j]=memory.readFloat(c.r[4]+std::uint32_t(j*4));
            const auto out=controlledTransform(a,in);for(std::size_t j=0;j<3;++j)memory.writeFloat(c.r[5]+std::uint32_t(j*4),out[j]);++transforms;
        };
        cpu.callHooks[0x0C022CE0]=[&](RefCpu& c){publish(memory,c.r[4],surfaces.at(surfaceCalls++));};
        cpu.callHooks[0x0C022D20]=[&](RefCpu& c){publish(memory,c.r[4],sweeps.at(sweepCalls++));};
        instructions+=cpu.run(0x0C158200,stop,6000);
        OriginalRoadContactServices services;services.transformPoint=controlledTransform;
        std::size_t ns=0,nw=0;
        services.surface022CE0=[&](OriginalCollisionQuery& q){publish(q,surfaces.at(ns++));};
        services.swept022D20=[&](OriginalCollisionQuery& q){publish(q,sweeps.at(nw++));};
        updateOriginalRoadContact(d,a,s,p,services);
        equal(8,std::uint32_t(transforms),"matrix point count");equal(4,std::uint32_t(surfaceCalls),"surface query count");equal(4,std::uint32_t(sweepCalls),"swept query count");
        check(memory,d,a,s,sample,feedback,diagnostics);
    }
    const auto controlledComparisons=comparisons,controlledInstructions=instructions;
    std::size_t actualSurfaceHits=0,actualWallHits=0;
    for(std::uint32_t variant=0;variant<2;++variant){
        const std::filesystem::path hostfs=argv[2];
        const auto source=hostfs/"binary"/("k_df_colli_"+std::to_string(variant)+".bin.nz");
        const auto collision=OriginalCollisionData::load(source);
        const auto raw=read(source),path=read(hostfs/"binary"/(variant?"PATH_dfo_0.bin":"PATH_dfi_0.bin"));
        memory.clear();memory.zeroRegion(stack-0x10000,0x11000);
        constexpr std::uint32_t dataBase=0x0CB00000;
        for(std::size_t i=0;i<raw.size();i+=4)memory.write32(dataBase+std::uint32_t(i),word(raw,i));
        for(const auto offset:{12u,20u,28u,36u,44u})memory.write32(dataBase+offset,dataBase+word(raw,offset));
        memory.write32(0x0C2EEF6C,dataBase);
        for(std::uint32_t sample=0;sample<120;++sample){
            OriginalDriveState d;OriginalActorState a;OriginalRoadContactState s;randomize(d,a,s);
            const auto index=(sample*6)%772;
            for(std::size_t j=0;j<3;++j){
                float value=std::bit_cast<float>(word(path,index*12+j*4));
                if(j==1)value+=sample%7==0?8.0f:1.0f;
                if(j==0&&sample%5==0)value+=8.0f;
                d.setf(j*4,value);a.setf(j*4,value);
            }
            for(std::size_t i=0;i<4;++i){
                s.surfaces0CAA9518[i]={};s.sweeps0CAA9618[i]={};
                clearOriginalCollisionQuery(s.surfaces0CAA9518[i]);
                clearOriginalCollisionQuery(s.sweeps0CAA9618[i]);
                s.impacts0CAA9508[i]=0;
                for(std::size_t j=0;j<3;++j){
                    s.surfaces0CAA9518[i].setf(32+j*4,a.f(j*4));
                    d.setf(680+i*12+j*4,a.f(j*4));
                }
            }
            OriginalRoadContactParameters p;const auto car=sample%35;
            for(std::size_t i=0;i<11;++i)p.geometry0C2700F4[i]=memory.readFloat(0x0C2700F4+car*44+std::uint32_t(i*4));
            load(memory,d,a,s,false);memory.write32(0x0C901654,car);
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            for(std::size_t i=0;i<100;++i)memory.write32(0x0C99A904+std::uint32_t(i*4),0);
            memory.write32(0x0C99AA94,0);
            for(std::size_t i=0;i<21;++i)memory.write32(0x0C99AA98+std::uint32_t(i*4),0);
            RefCpu cpu(memory);cpu.r[15]=stack;cpu.pr=stop;
            cpu.fscaHalfWave=halfWave;
            std::array<std::uint32_t,16> incomingMatrix{};
            for(std::size_t i=0;i<16;++i)cpu.xf[i]=incomingMatrix[i]=bits(number()*4-2);
            std::vector<std::uint32_t> feedback;std::size_t diagnostics=0;
            cpu.callHooks[0x0C142460]=[&](RefCpu& c){feedback.push_back(c.r[4]);};
            cpu.callHooks[0x0C055D60]=[&](RefCpu&){++diagnostics;};
            memory.write32(0x0C98AD0C,0x00200000u);
            memory.write32(0x0C98AD10,0x0CFD0000u);
            memory.write32(0x0C98AD14,0x0CFD0000u);
            memory.zeroRegion(0x0CFD0000,32*64);
            instructions+=cpu.run(0x0C158200,stop,3000000);
            equal(0x00200000u,memory.read32(0x0C98AD0C),"matrix stack depth restored");
            equal(0x0CFD0000u,memory.read32(0x0C98AD14),"matrix stack pointer restored");
            for(std::size_t i=0;i<16;++i)equal(incomingMatrix[i],cpu.xf[i],"enclosing matrix restored");
            const auto services=bindOriginalRoadContactServices(collision,trace,scratch,fsca);
            updateOriginalRoadContact(d,a,s,p,services);
            check(memory,d,a,s,2048+variant*120+sample,feedback,diagnostics);
            for(std::size_t i=0;i<100;++i)equal(memory.read32(0x0C99A904+std::uint32_t(i*4)),std::uint32_t(trace.indices0C99A904[i]),"actual collision trace");
            equal(memory.read32(0x0C99AA94),trace.count0C99AA94,"actual collision trace count");
            for(std::size_t i=0;i<21;++i)equal(memory.read32(0x0C99AA98+std::uint32_t(i*4)),scratch.words[i],"actual surface coefficients");
            for(std::size_t i=0;i<4;++i){if(s.surfaces0CAA9518[i].u(60)!=0xFFFFFFFFu)++actualSurfaceHits;if(s.impacts0CAA9508[i]>0.0f)++actualWallHits;}
        }
    }
    if(actualSurfaceHits==0||actualWallHits==0)throw std::runtime_error("Actual road-contact cases lack surface or wall coverage");
    std::cout<<"PASS 2048 controlled-boundary complete original158200/1593C0 cases: "<<controlledComparisons<<" raw-word comparisons, "<<controlledInstructions
        <<" instructions. PASS240 complete cases on both original Akina collision datasets with actual surface/swept/math routines: "<<comparisons-controlledComparisons
        <<" comparisons, "<<instructions-controlledInstructions<<" instructions; surfaceHits="<<actualSurfaceHits<<", wallHits="<<actualWallHits
        <<". Only platform-service boundaries controlled in dataset tests; no collision or matrix hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
