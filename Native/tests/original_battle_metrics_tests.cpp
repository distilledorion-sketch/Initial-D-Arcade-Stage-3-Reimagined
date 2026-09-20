#include "original_battle_metrics.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t object=0x0cf00000,points=0x0cf10000,distances=0x0cf20000,
    context=0x0cf30000,coordinate=0x0cf31000,race=0x0cf32000,course=0x0cf33000,
    stack=0x0cffe000,stop=0x0f000000;
std::size_t comparisons{},instructions{},allocationHooks{},tlsHooks{};
void equal(std::uint32_t a,std::uint32_t b,const char* text){++comparisons;if(a!=b){std::cerr<<text<<": "<<std::hex<<a<<" != "<<b<<std::dec<<'\n';throw std::runtime_error("Battle metric bit mismatch");}}
void floatEqual(float a,std::uint32_t b,const char* text){equal(std::bit_cast<std::uint32_t>(a),b,text);}
std::vector<OriginalRacePoint> read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);std::uint32_t n{},k{};f.read(reinterpret_cast<char*>(&n),4);f.read(reinterpret_cast<char*>(&k),4);if(!f||k!=3||n<2)throw std::runtime_error("Invalid test path");std::vector<OriginalRacePoint> v(n);f.read(reinterpret_cast<char*>(v.data()),std::streamsize(n)*12);if(!f)throw std::runtime_error("Truncated test path");return v;}
void setup(RefCpu& c){c.r[15]=stack;c.pr=stop;c.callHooks[0x0c221fc0]=[](RefCpu& c){++tlsHooks;c.r[0]=context;};c.callHooks[0x0c021960]=[](RefCpu& c){++allocationHooks;c.r[0]=distances;};}
void seed(RefMemory& m,const std::vector<OriginalRacePoint>& p,bool reverse){m.clear();m.zeroRegion(object,0x40000);m.zeroRegion(0x0cff0000,0x10000);m.write32(context+4,context+16);m.write32(object,0x0c386194);m.write32(object+28,reverse);m.write32(object+12,object+0x100);m.write32(object+0x100,std::uint32_t(p.size()));m.write32(object+0x104,points);for(std::uint32_t i=0;i<p.size();++i)for(std::uint32_t k=0;k<3;++k)m.writeFloat(points+i*12+k*4,p[i][k]);m.write32(race+1036,course);m.write32(course+24,object);m.write32(race+1640,0);}
void metrics(RefMemory& m,const std::filesystem::path& root){
    constexpr std::array<const char*,9> folders{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};std::mt19937 random(0x99360);
    for(std::uint32_t condition=0;condition<18;++condition){const auto p=read(root/"data/courses"/(std::string(folders[condition/2])+"_path.bin"));seed(m,p,(condition&1)!=0);const auto native=OriginalBattleMetrics::load(root,condition);
        RefCpu builder(m);setup(builder);builder.r[4]=object;instructions+=builder.run(0x0c099da0,stop,15000000);equal(m.read32(object+24),distances,"original allocator destination");equal(m.read8(object+33),1,"source length table readiness");
        for(std::uint32_t i=0;i<p.size();++i)floatEqual(native.cumulativeDistances()[i],m.read32(distances+i*4),"full099DA0 cumulative distance");
        const auto period=std::int32_t(p.size()-1);
        for(std::uint32_t j=0;j<400;++j){OriginalPathCoordinate player{std::int32_t(random()%std::uint32_t(period*7))-3*period,float(j%17)/16},rival{std::int32_t(random()%std::uint32_t(period*7))-3*period,float((j+3)%17)/16};
            if(j<8){player.index=(std::int32_t(j)-4)*period;rival.index=player.index-1;}
            m.write32(coordinate,std::uint32_t(player.index));m.writeFloat(coordinate+4,player.fraction);RefCpu c(m);setup(c);c.r[4]=object;c.r[5]=coordinate;instructions+=c.run(0x0c099360,stop,2000);floatEqual(native.distance(player),c.fr[0],"099360 signed lap/reverse interpolation");
            m.write32(race+1460,std::uint32_t(player.index));m.writeFloat(race+1464,player.fraction);m.write32(race+1472,std::uint32_t(rival.index));m.writeFloat(race+1476,rival.fraction);c.r[4]=race;c.pr=stop;instructions+=c.run(0x0c06a420,stop,5000);floatEqual(native.advantage(player,rival),c.fr[0],"full06A420 local signed advantage");
            // Include the enclosing local solver dispatch: its gap comes
            // from this exact accumulated coordinate pair, not current raw
            // nearest-point indices or a native chase-distance estimate.
            unsigned solverCalls=0;
            c.callHooks[0x0c159920]=[&](auto& cpu){++solverCalls;floatEqual(native.advantage(player,rival),cpu.fr[4],"062DE0 source solver gap argument");};
            c.r[4]=race;c.pr=stop;instructions+=c.run(0x0c062de0,stop,5000);
            equal(solverCalls,1,"single local solver dispatch");
        }
    }
}
std::string originalString(RefMemory& m,std::uint32_t address){std::string result;for(unsigned i=0;i<64;++i){auto c=m.read8(address+i);if(!c)return result;result.push_back(char(c));}throw std::runtime_error("Invalid original portrait name");}
void portraits(RefMemory& m){for(std::uint32_t enemy=0;enemy<31;++enemy){equal(originalBattlePortraitBank(enemy,0)==originalString(m,m.read32(0x0c2fbb24+enemy*4)),true,"original Legend face table");equal(originalBattlePortraitBank(enemy,2)==originalString(m,m.read32(0x0c2fbb9c)),true,"original Bunta override");equal(originalBattlePortraitBank(enemy,1).empty(),true,"TA has no portrait");}}
}
int main(int argc,char** argv)try{if(argc!=3)throw std::invalid_argument("Usage: original_battle_metrics_tests canonical_program native_project_root");RefMemory m{argv[1]};metrics(m,argv[2]);portraits(m);std::cout<<"PASS original battle metrics: "<<comparisons<<" bit/string comparisons, "<<instructions<<" actual instructions, "<<allocationHooks<<" allocation and "<<tlsHooks<<" TLS boundaries; zero distance/math/lookup hooks.\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
