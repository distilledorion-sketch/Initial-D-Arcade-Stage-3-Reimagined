#include "original_course_oil.h"
#include "course_scene_catalog.h"
#include "original_driving_session.h"
#include "original_start_grid.h"
#include "original_battle_profile.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
unsigned checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
bool sourceOilFlag(RefMemory& mem,std::uint32_t flags){
    constexpr unsigned frame=0xd000000,obj=0xd010000,stop=0xc1a64ca;
    mem.clear();mem.zeroRegion(frame,0x2000);mem.zeroRegion(obj,0x2000);
    mem.write32(0xc31ce18+32,flags);mem.write32(frame+0x130,obj+0x53c);
    RefCpu cpu(mem);cpu.r[14]=frame;cpu.pr=stop;cpu.run(0xc1a64a4,stop);
    return mem.read32(obj+0x568)!=0;
}
std::vector<unsigned> sourceDraw(RefMemory& mem,bool night,bool wet,bool enabled){
    constexpr unsigned obj=0xd000000,stack=0xd010000,vt=0xd020000,hook=0xd030000;
    mem.clear();mem.zeroRegion(obj,0x1000);mem.zeroRegion(stack,0x1000);mem.zeroRegion(vt,0x100);
    mem.write32(obj+4,vt);mem.write32(obj+56,wet);mem.write32(obj+0x568,enabled);
    mem.write16(vt+8,0);mem.write32(vt+12,hook);mem.write32(stack+72,obj+4);
    RefCpu cpu(mem);cpu.r[13]=obj;cpu.r[14]=stack;cpu.r[12]=obj+4;
    const unsigned stop=night?0xc1a77ce:0xc1a6aac;cpu.pr=stop;
    std::vector<unsigned> draws;cpu.callHooks[hook]=[&](RefCpu& c){draws.push_back(c.r[5]);};
    cpu.run(night?0xc1a77aa:0xc1a6a8c,stop);return draws;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Usage: original_course_oil_tests native-root canonical-image");
    const std::filesystem::path root=argv[1];RefMemory mem(argv[2]);
    // The source constructor derives +568 from profile+1180 bit28.
    check(sourceOilFlag(mem,0)&&!sourceOilFlag(mem,0x10000000),"Original constructor oil gate differs");
    check(sourceOilFlag(mem,0xefffffff)&&!sourceOilFlag(mem,0xffffffff),"Unrelated profile flags changed original oil gate");
    for(unsigned course=0;course<9;++course)for(bool wet:{false,true})for(unsigned enemy=0;enemy<32;++enemy)
        check(originalCourseOilEnabled(course,wet,enemy)==(course==7&&!wet&&enemy!=24&&enemy!=25&&enemy!=31),"Oil leaked to another course or excluded race");
    for(bool night:{false,true})for(bool wet:{false,true})for(bool reverse:{false,true}){
        auto scene=OriginalCourseScene::load(root,"k_tu",night,reverse,wet);
        std::array<std::vector<unsigned>,32> sourceDraws;
        for(unsigned enemy:{0u,24u,25u,26u,27u,28u,29u,31u})sourceDraws[enemy]=sourceDraw(mem,night,wet,enemy!=24&&enemy!=25&&enemy!=31);
        for(unsigned index=0;index<3729;++index){
            const auto& base=scene.assemblyForPathIndex(index);
            for(unsigned enemy:{0u,24u,25u,26u,27u,28u,29u,31u}){
                const auto& source=sourceDraws[enemy];
                const auto inserted=originalCourseOilInsertion(base,7,night,wet,enemy);
                check(bool(inserted)==!source.empty(),"Oil admission differs from original primary renderer");
                if(!inserted)continue;
                check(inserted->assembly.instances.size()==1&&inserted->replaceCount==0,"Oil replaced existing scenery");
                const auto& oil=inserted->assembly.instances[0];
                check(oil.chunk==source[0]&&oil.chunk<scene.model.chunks.size(),"Oil chunk differs from original primary renderer");
                check(base.instances.at(inserted->before-1).chunk==(night?96u:95u),"Oil draw order differs from source");
                check(oil.transform==base.instances.at(inserted->before-1).transform,"Oil transform differs from original world transform");
            }
        }
        if(!wet){const auto& oil=scene.model.chunks.at(night?105:104);std::size_t vertices=0,indices=0;
            Vec3 low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};
            for(const auto& batch:oil.batches){vertices+=batch.vertices.size();indices+=batch.indices.size();for(const auto& v:batch.vertices){
                low.x=std::min(low.x,v.position.x);low.y=std::min(low.y,v.position.y);low.z=std::min(low.z,v.position.z);
                high.x=std::max(high.x,v.position.x);high.y=std::max(high.y,v.position.y);high.z=std::max(high.z,v.position.z);
            }}
            check(vertices>0&&indices>0,"Original oil mesh is empty");
            std::cout<<"oil night="<<night<<" reverse="<<reverse<<" vertices="<<vertices<<" bounds="<<low.x<<','<<low.y<<','<<low.z<<" / "<<high.x<<','<<high.y<<','<<high.z<<'\n';
        }
    }
    // Saved Legend history must not suppress oil in a solo/online session.
    for(unsigned condition:{14u,15u})for(bool wet:{false,true})for(unsigned previousEnemy:{24u,25u,31u}){
        OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(0,condition,wet?OriginalWeather::Wet:OriginalWeather::Dry);
        selection.physics.vehicleMode0C9015E0=previousEnemy;selection.collisionVariant=condition&1;
        const auto pose=originalStartPose(condition,0);OriginalDrivingSession session;
        session.reset(root,selection,pose.position,pose.angles);
        check(session.parameters().road.mode0C9015E0==0,"Saved opponent leaked into solo/online oil exclusion");
        check(session.selection().physics.vehicleMode0C9015E0==previousEnemy,"Oil recovery changed tuning selection");
        check(session.vehicle().drive.u(0x434)==unsigned(wet),"Oil recovery changed weather physics");
    }
    for(unsigned enemy=24;enemy<=30;++enemy){
        auto profile=makeOriginalFreshBattleProfile();selectOriginalRival(profile,enemy);
        check(sourceOilFlag(mem,profile.u(1180))==(enemy>=26&&enemy<=29),"Legend profile oil flag mismatch");
    }
    std::cout<<"PASS "<<checks<<" oil checks: original opcode gates/draws, every Tsuchisaka path index in 8 variants, dry/wet and saved-opponent recovery.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
