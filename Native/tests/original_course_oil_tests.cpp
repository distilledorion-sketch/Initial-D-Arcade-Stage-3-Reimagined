#include "original_course_oil.h"
#include "course_scene_catalog.h"
#include "original_driving_session.h"
#include "original_start_grid.h"
#include "original_battle_profile.h"
#include "original_driving_float.h"
#include "original_math.h"
#include "online_race_simulation.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
unsigned checks=0;
std::uint64_t ownerInstructions=0,ownerHooks=0;
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
unsigned sourceSoloRoadMode(RefMemory& mem,unsigned condition,unsigned enemy,bool wet){
    constexpr unsigned profile=0x0c31c99c,stack=0x0cfff000,stop=0x00ff0000;
    mem.clear();mem.zeroRegion(0x0c800000,0x800000);
    auto p=makeOriginalFreshBattleProfile();p.setu(0,1);p.setu(24,enemy);p.setu(32,wet);
    for(unsigned i=0;i<p.words.size();++i)mem.write32(profile+i*4,p.words[i]);
    mem.write32(stack+4,0x0cd00000);mem.write32(stack+8,0x0cd00010);
    mem.write32(stack+12,0xffffffff); // Original numeric-mode2 solo control.
    mem.write32(stack+16,0x0cd00020);mem.write32(stack+20,0x0cd00030);
    RefCpu cpu(mem);cpu.r[4]=condition;cpu.r[5]=0x0cb00000;
    cpu.r[6]=0x0ce00000;cpu.r[7]=0x0ce10000;cpu.r[15]=stack;cpu.pr=stop;
    // Only the player initializer is replaced: complete159720 still reads
    // the real profile and executes its negative-control reset/publication.
    cpu.callHooks[0x0c1595c0]=[](auto&){++ownerHooks;};
    ownerInstructions+=cpu.run(0x0c159720,stop,10000);
    return mem.read32(0x0c9015e0);
}
unsigned sourceOilLatch(RefMemory& mem,unsigned mode,bool wet){
    constexpr unsigned drive=0x0c900f00;
    mem.clear();mem.zeroRegion(drive,OriginalDriveState::byteSize);
    mem.write32(0x0c9015e0,mode);mem.write32(drive+0x08c,24);mem.write32(drive+0x434,wet);
    RefCpu cpu(mem);ownerInstructions+=cpu.run(0x0c15d7be,0x0c15d7f8);
    return mem.read32(drive+0x140);
}
unsigned nativeOilLatch(const OriginalDrivingSession& session){
    auto drive=session.vehicle().drive;
    // All four wheel surfaces are oil; preparation selects the relevant
    // contact pair itself before applying the source gate.
    for(unsigned offset:{0x164u,0x168u,0x16cu,0x170u})drive.setu(offset,24);
    OriginalPreparedControls controls{0,1,0};
    prepareOriginalDriveState(drive,controls,150,session.parameters().road,
        {originalSinF32,originalCosF32,originalFiprDot3});
    return drive.u(0x140);
}
void ownerPolicies(const std::filesystem::path& root,RefMemory& mem){
    const OriginalDrivingFloatScope floatingPoint;
    for(unsigned condition:{14u,15u})for(bool wet:{false,true})
    for(unsigned previousEnemy:{0u,1u,2u,24u,25u,30u,31u}){
        const auto sourceMode=sourceSoloRoadMode(mem,condition,previousEnemy,wet);
        const auto sourceOil=sourceOilLatch(mem,sourceMode,wet);
        OriginalDrivingSelection selection;
        selection.physics=makeOriginalFreshTimeAttackSelection(0,condition,wet?OriginalWeather::Wet:OriginalWeather::Dry);
        selection.physics.vehicleMode0C9015E0=previousEnemy;selection.collisionVariant=condition&1;
        selection.bodyContactEnabled=false;
        const auto pose=originalStartPose(condition,0);OriginalDrivingSession session;
        session.reset(root,selection,pose.position,pose.angles);
        const auto sourceMask=mem.read32(0x0c283e08+previousEnemy*4);
        auto unchangedProfile=[&](const OriginalDrivingSession& s){
            check(s.selection().physics.vehicleMode0C9015E0==previousEnemy,"Road policy changed profile/tuning selection");
            check(s.parameters().steeringMemory.mask0CAA9CF0==sourceMask,"Road policy changed original steering mask");
            check(s.vehicle().drive.u(0x434)==unsigned(wet),"Road policy changed weather physics");
        };
        check(session.parameters().road.mode0C9015E0==sourceMode,"Solo road gate differs from complete original159720");
        check(nativeOilLatch(session)==sourceOil,"Solo oil latch differs from original15D7BE");
        unchangedProfile(session);
        ImportedDrivingRoad imported{session.path(),session.collision()};
        OriginalDrivingSession adapted;adapted.reset(root,selection,pose.position,pose.angles,&imported);
        check(adapted.parameters().road.mode0C9015E0==0,"Imported road policy changed");
        check(nativeOilLatch(adapted)==sourceOilLatch(mem,0,wet),"Imported oil latch changed");
        unchangedProfile(adapted);
        for(unsigned overrideMode:{0u,31u}){
            selection.roadModeOverride=overrideMode;
            OriginalDrivingSession overridden;overridden.reset(root,selection,pose.position,pose.angles);
            check(overridden.parameters().road.mode0C9015E0==overrideMode,"Explicit road override was not applied");
            check(nativeOilLatch(overridden)==sourceOilLatch(mem,overrideMode,wet),"Explicit road override changed oil formula");
            unchangedProfile(overridden);
        }
        for(unsigned invalidMode:{32u,std::numeric_limits<unsigned>::max()}){
            selection.roadModeOverride=invalidMode;bool rejected=false;
            try{session.reset(root,selection,pose.position,pose.angles);}catch(const std::invalid_argument&){rejected=true;}
            check(rejected,"Invalid road-mode override accepted");
            check(session.parameters().road.mode0C9015E0==sourceMode,"Rejected reset changed the active session");
        }
    }
    // Exercise the real online constructor, so forgetting to set its policy
    // cannot pass merely because the standalone override works.
    for(unsigned condition:{14u,15u})for(bool wet:{false,true}){
        OnlineRaceSetup setup;setup.condition=condition;setup.wet=wet;
        for(unsigned slot=0;slot<2;++slot){setup.profiles[slot]=makeOriginalFreshBattleProfile();setup.profiles[slot].setu(24,24+slot);}
        OnlineRaceSimulation online(root,setup);
        for(unsigned slot=0;slot<2;++slot){const auto& car=online.car(slot);
            check(car.selection().roadModeOverride==0,"Online constructor omitted explicit road policy");
            check(car.parameters().road.mode0C9015E0==0,"Online saved opponent changed oil exclusion");
            check(nativeOilLatch(car)==sourceOilLatch(mem,0,wet),"Online oil behavior changed");
            check(car.selection().physics.vehicleMode0C9015E0==24+slot,"Online oil policy changed initialization selection");
        }
    }
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
    ownerPolicies(root,mem);
    for(unsigned enemy=24;enemy<=30;++enemy){
        auto profile=makeOriginalFreshBattleProfile();selectOriginalRival(profile,enemy);
        check(sourceOilFlag(mem,profile.u(1180))==(enemy>=26&&enemy<=29),"Legend profile oil flag mismatch");
    }
    std::cout<<"PASS "<<checks<<" oil checks: original opcode gates/draws, every Tsuchisaka path index in 8 variants, solo source parity and explicit online/imported policies. Owner regression: "
        <<ownerInstructions<<" original instructions, "<<ownerHooks<<" explicit player-init hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
