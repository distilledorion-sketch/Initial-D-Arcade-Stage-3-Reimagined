#include "original_race_path.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t object=0x0CF00000,points=0x0CF10000,leftPoints=0x0CF20000,
    rightPoints=0x0CF30000,xyz=0x0CF40000,output=xyz+32,context=xyz+64,
    race=0x0CF50000,course=race+0x1000,car=race+0x2000,timer=race+0x4000,
    stack=0x0CFFE000,stop=0x0F000000;
std::size_t comparisons{},instructions{},tlsCalls{},cardCalls{},divisionCalls{},cases{};
void equal(std::uint32_t a,std::uint32_t b,const char* label){++comparisons;if(a!=b)throw std::runtime_error(std::string(label)+" native0x"+hex(a)+" original0x"+hex(b));}
void point(RefMemory&m,std::uint32_t a,OriginalRacePoint p){for(int k=0;k<3;++k)m.writeFloat(a+k*4,p[k]);}
std::vector<OriginalRacePoint> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);std::uint32_t n{},c{};f.read((char*)&n,4);f.read((char*)&c,4);if(!f||n<21||c!=3)throw std::runtime_error("Invalid test path");std::vector<OriginalRacePoint> out(n);f.read((char*)out.data(),n*12);if(!f)throw std::runtime_error("Truncated test path");return out;}
void setup(RefCpu&c){c.r[15]=stack;c.pr=stop;c.callHooks[0x0C221FC0]=[](RefCpu& c){++tlsCalls;c.r[0]=context;};}
void seed(RefMemory&m,const std::vector<OriginalRacePoint>& p,const std::vector<OriginalRacePoint>& l,const std::vector<OriginalRacePoint>& r,bool reverse){
    m.clear();m.zeroRegion(object,0x60000);m.zeroRegion(0x0CFF0000,0x10000);m.write32(object,0x0C386194);m.write32(object+28,reverse);m.write32(context+4,context+16);
    for(auto [off,v,address]:{std::tuple{12u,&p,points},std::tuple{16u,&l,leftPoints},std::tuple{20u,&r,rightPoints}}){auto stream=object+0x100+off*16;m.write32(object+off,stream);m.write32(stream,std::uint32_t(p.size()));m.write32(stream+4,address);for(std::size_t i=0;i<v->size();++i)point(m,address+std::uint32_t(i)*12,(*v)[i]);}
}
void compare(RefMemory&m,const OriginalRacePath& path,OriginalRacePoint p,OriginalPathCoordinate before,bool full){
    ++cases;point(m,xyz,p);m.write32(output,std::uint32_t(before.index));m.writeFloat(output+4,before.fraction);m.write32(output+8,0x12345678);m.write8(object+32,full);
    RefCpu c(m);setup(c);c.r[4]=object;c.r[5]=xyz;c.r[6]=output;
    try{instructions+=c.run(0x0C096200,stop,12000000);auto native=before;path.project(p,native,full);
        equal(std::uint32_t(native.index),m.read32(output),"path index");equal(std::bit_cast<std::uint32_t>(native.fraction),m.read32(output+4),"path fraction");equal(m.read32(output+8),0x12345678,"coordinate vtable preserved");equal(m.read32(context+20),0,"TLS exception chain restored");
    }catch(const std::exception&e){throw std::runtime_error("Case"+std::to_string(cases)+" previous"+std::to_string(before.index)+" reverse"+std::to_string(path.reverse())+" full"+std::to_string(full)+": "+e.what());}
}
OriginalRacePoint interpolate(OriginalRacePoint a,OriginalRacePoint b,float f){for(int k=0;k<3;++k)a[k]=a[k]+(b[k]-a[k])*f;return a;}
void paths(RefMemory&m,const std::filesystem::path& root){
    constexpr std::array<const char*,8> ids{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu"};std::mt19937 random(0x96200);
    for(std::uint32_t courseIndex=0;courseIndex<8;++courseIndex){const auto prefix=root/"data/courses"/ids[courseIndex];const auto p=read(prefix.string()+"_path.bin"),l=read(prefix.string()+"_path_l.bin"),r=read(prefix.string()+"_path_r.bin");const auto period=std::int32_t(p.size()-1);
        for(bool reverse:{false,true}){seed(m,p,l,r,reverse);const auto path=OriginalRacePath::load(root,courseIndex*2+reverse);
            for(std::uint32_t j=0;j<72;++j){auto index=std::int32_t(random()%std::uint32_t(period));const float fraction=float(j%9)/8;
                auto pos=interpolate(p[index],p[index+1],fraction);if(j%6==0)pos=interpolate(l[index],l[index+1],fraction);if(j%6==1)pos=interpolate(r[index],r[index+1],fraction);if(j%6==2)pos[1]+=250.0f;
                const auto previous=(index+(j%4==0?17:j%4==1?-12:0)+period)%period;
                compare(m,path,pos,{reverse?period-previous-1:previous,.731f},false);
            }
            // Full scan hits early, exact neighboring-cell precedence, missed
            // road preserves the prior record, periodic edge search.
            auto pos=interpolate(p[45],p[46],.3f);
            compare(m,path,pos,{reverse?period-301:300,.123f},true);
            compare(m,path,pos,{reverse?period-301:300,.123f},false);
            compare(m,path,p[0],{reverse?0:period-1,.25f},false);
            compare(m,path,p[period],{reverse?period-1:0,.25f},false);
            compare(m,path,{1e7f,-20,1e7f},{reverse?period-1:0,.413f},false);
            if(!reverse){compare(m,path,pos,{-1,.413f},true);compare(m,path,pos,{-1,.413f},false);}
            if(courseIndex==3&&!reverse)compare(m,path,{1e7f,-20,1e7f},{100,.713f},true);
        }
    }
}
void history(RefMemory&m,const std::filesystem::path& root){
    const auto prefix=root/"data/courses/k_df";const auto p=read(prefix.string()+"_path.bin"),l=read(prefix.string()+"_path_l.bin"),r=read(prefix.string()+"_path_r.bin");
    for(bool reverse:{false,true}){seed(m,p,l,r,reverse);const auto path=OriginalRacePath::load(root,6+reverse);OriginalRacePathHistory h{};const auto initial=reverse?path.period()-61:60;
        h.coordinates.fill({initial,.5f});h.progress={0,.5f};h.publishedCoordinate={initial,.5f};h.positions.fill(p[60]);m.write32(race+1036,course);m.write32(course+24,object);m.write32(race+1048,car);m.write32(race+1180,timer);m.write32(race+1640,2);
        for(int j=0;j<2;++j){m.write32(race+1412+j*12,initial);m.writeFloat(race+1416+j*12,.5f);m.write32(race+1420+j*12,0x13570000+j);point(m,race+1508+j*16,p[60]);m.write32(race+1520+j*16,0x24680000+j);}m.write32(race+1460,0);m.writeFloat(race+1464,.5f);m.write32(race+1468,0x12345678);m.write32(race+1492,0xabcdef00);
        for(std::uint32_t frame=0;frame<120;++frame){const auto index=reverse?60-std::int32_t(frame/4):60+std::int32_t(frame/4);auto pos=interpolate(p[index],p[index+1],float(frame%4)*.25f);if(frame==37)pos={1e7f,-20,1e7f};point(m,car+2472,pos);m.write32(timer+8,frame*100+30000);
            RefCpu c(m);setup(c);c.r[4]=race;c.callHooks[0x0C160160]=[](RefCpu& c){++cardCalls;c.r[0]=0;};c.callHooks[0x0C2223B8]=[](RefCpu& c){++divisionCalls;c.fpul=std::uint32_t(std::bit_cast<std::int32_t>(c.r[4])/std::bit_cast<std::int32_t>(c.r[5]));};
            instructions+=c.run(0x0C0680C0,stop,200000);advanceOriginalRacePathHistory(path,h,pos,frame*100+30000);
            equal(h.activeIndex,m.read32(race+1408),"history active index");for(std::uint32_t j=0;j<2;++j){equal(std::uint32_t(h.coordinates[j].index),m.read32(race+1412+j*12),"history index");equal(std::bit_cast<std::uint32_t>(h.coordinates[j].fraction),m.read32(race+1416+j*12),"history fraction");equal(m.read32(race+1420+j*12),0x13570000+j,"coordinate vtable");for(int k=0;k<3;++k)equal(std::bit_cast<std::uint32_t>(h.positions[j][k]),m.read32(race+1508+j*16+k*4),"history XYZ");equal(h.sampleTimes[j],m.read32(race+1540+j*4),"history time");equal(m.read32(race+1520+j*16),0x24680000+j,"XYZ vtable");}
            equal(std::uint32_t(h.progress.index),m.read32(race+1460),"history progress");equal(std::bit_cast<std::uint32_t>(h.progress.fraction),m.read32(race+1464),"history progress fraction");equal(std::uint32_t(h.publishedCoordinate.index),m.read32(race+1484),"published index");equal(std::bit_cast<std::uint32_t>(h.publishedCoordinate.fraction),m.read32(race+1488),"published fraction");equal(m.read32(race+1468),0x12345678,"progress vtable");equal(m.read32(race+1492),0xabcdef00,"published vtable");
        }
    }
}
void startup(RefMemory&m){
    for(std::uint32_t condition=0;condition<18;++condition){
        m.clear();m.zeroRegion(object,0x60000);m.zeroRegion(0x0CFF0000,0x10000);m.write32(context+4,context+16);
        const auto row=originalRaceRuleRowIndex(condition,2);m.write32(race+1564,row);
        for(std::uint32_t offset:{1412u,1424u,1436u,1448u,1460u,1472u}){m.write32(race+offset,0x12345678);m.write32(race+offset+4,0x87654321);m.write32(race+offset+8,0xabcdef00+offset);}m.write32(race+1408,1);
        RefCpu c(m);setup(c);c.r[4]=race;instructions+=c.run(0x0C0671A0,0x0C06726A,3000);
        for(std::uint32_t offset:{1412u,1424u,1436u,1448u,1460u,1472u}){equal(m.read32(race+offset),offset<1460?std::uint32_t(originalRaceRuleRow(row).startIndex):0,"source initial coordinate/progress");equal(m.read32(race+offset+4),0,"source initial fraction");equal(m.read32(race+offset+8),0xabcdef00+offset,"startup preserves vtable");}equal(m.read32(race+1408),0,"source startup history slot");
    }
}
}
int main(int argc,char**argv)try{if(argc!=3)throw std::invalid_argument("Usage: original_race_path_reference_tests canonical_program project_root");RefMemory m{argv[1]};paths(m,argv[2]);history(m,argv[2]);startup(m);std::cout<<"PASS original race path: "<<cases<<" projections,240 full player-history frames,18 source startup blocks, "<<comparisons<<" exact comparisons, "<<instructions<<" original instructions. "<<tlsCalls<<" TLS hooks, "<<cardCalls<<" disabled-card hooks, "<<divisionCalls<<" bounded integer-division hooks in history only; zero projection/normalizer/math hooks.\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
