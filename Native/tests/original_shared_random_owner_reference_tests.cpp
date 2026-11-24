#include "original_driving_session.h"
#include "original_driving_float.h"
#include "original_start_grid.h"
#include "original_course_crows.h"
#include "original_rival_dialog.h"
#include "original_ending_camera.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("Native root and canonical image required");
    const OriginalDrivingFloatScope floatingPoint;
    const std::filesystem::path root=argv[1];RefMemory memory(argv[2]);
    constexpr unsigned stack=0xcfff000,stop=0xcffffff,object=0xcfe0000,chunks=object+4096;
    std::uint64_t checks=0,instructions=0,hooks=0;
    const auto equal=[&](auto a,auto b,const char* what){++checks;if(a!=b)throw std::runtime_error(what);};
    const auto run=[&](RefCpu& c,unsigned begin,unsigned end,unsigned max=10000){
        try{instructions+=c.run(begin,end,max);}catch(const std::exception&){
            std::cerr<<"Original stage 0x"<<std::hex<<begin<<" failed at PC0x"<<c.pc<<std::dec<<": ";throw;}
    };
    const auto divide=[&](RefCpu& c){++hooks;c.fpul=unsigned(signed32(c.r[4])/signed32(c.r[5]));};
    memory.zeroRegion(0xcffd000,0x3000);memory.zeroRegion(object,8192);
    // Independent original selector covers every15bit output, including the
    // previously wrong native rand()%1000 reduction. The original quotient
    // helper is the only hook; the original global RNG is executed in full.
    for(unsigned output=0;output<32768;++output){
        const unsigned after=output<<16;auto nativeSeed=(after-12345u)*0xeeb9eb65u;
        memory.write32(0xc37c778,nativeSeed);RefCpu c(memory);c.r[15]=stack;c.callHooks[0xc2223b8]=divide;
        run(c,0xc0f8f5c,0xc0f8f7c);
        equal(selectOriginalRivalSpectator(nativeSeed),unsigned(memory.read8(0xc31ce45)),"Original spectator choice");
        equal(nativeSeed,memory.read32(0xc37c778),"Spectator consumes exactly one shared draw");
    }
    // Actual normal ARace init calls course construction before player setup.
    // Both expensive children are explicit trace boundaries here; their RNG
    // blocks execute separately below. Neither ordering is imposed by test.
    memory.zeroRegion(0xc31c99c,0x500);std::vector<unsigned> order;
    RefCpu parent(memory);parent.r[4]=object;parent.r[15]=stack;
    parent.callHooks[0xc221fc0]=[&](auto& c){++hooks;c.r[0]=object+4096;};
    parent.callHooks[0xc055d60]=[&](auto&){++hooks;};
    parent.callHooks[0xc142100]=[&](auto&){++hooks;};
    for(auto entry:{0xc062480u,0xc0625e0u})parent.callHooks[entry]=[&,entry](auto& c){++hooks;equal(c.r[4],object,"Same ARace owner for course/player initialization");order.push_back(entry);};
    run(parent,0xc061b00,0xc061c08);
    equal(order,std::vector<unsigned>{0xc062480,0xc0625e0},"Original course before player order");

    for(unsigned initial:{0u,1u,0x12345678u,0xffffffffu}){
        OriginalDrivingSession owner;owner.withSharedRandom([&](auto& seed){seed=initial;});
        equal(owner.ready(),false,"Menu RNG does not create a vehicle");
        memory.write32(0xc37c778,initial);
        owner.withSharedRandom([&](auto& seed){selectOriginalRivalSpectator(seed);});
        RefCpu spectator(memory);spectator.r[15]=stack;spectator.callHooks[0xc2223b8]=divide;run(spectator,0xc0f8f5c,0xc0f8f7c);
        equal(owner.sharedRandomSeed(),memory.read32(0xc37c778),"First prerace spectator draw persists");
        OriginalCourseCrowState crow;owner.withSharedRandom([&](auto& seed){resetOriginalCourseCrows(crow,seed);});
        //042440 reads the authored group count through owner+16.
        memory.write32(object+16,object+1024);memory.write32(object+1024,19);
        memory.write32(object+36,chunks);RefCpu course(memory);course.r[4]=object;course.r[15]=stack;course.pr=stop;course.callHooks[0xc2223b8]=divide;
        run(course,0xc042440,stop);
        equal(owner.sharedRandomSeed(),memory.read32(0xc37c778),"Nineteen course draws precede first player initialization");
        for(unsigned i=0;i<19;++i)equal(crow.animationFrames[i],memory.read32(chunks+4*i),"Crow phase reads shared stream");
        OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(0,0,OriginalWeather::Dry);selection.collisionVariant=0;
        const auto pose=originalStartPose(0,0);
        const auto initializeSourcePlayer=[&]{RefCpu c(memory);c.r[8]=object;c.r[15]=stack;run(c,0xc15ef5c,0xc15ef7a);};
        const auto verifyReset=[&]{initializeSourcePlayer();owner.reset(root,selection,pose.position,pose.angles);
            equal(owner.sharedRandomSeed(),memory.read32(0xc37c778),"Player initializer continues preceding stream");
            equal(owner.vehicle().drive.u(0x21c),memory.read32(object+0x21c),"Original player initialization noise");};
        verifyReset();
        // External owner consumers leave all car words alone but participate
        // in the rollback checkpoint, as this source global does in a capture.
        const auto originalCar=owner.vehicle().drive.words;
        const auto checkpoint=owner.checkpoint();const auto digest=owner.rollbackDigest();
        const auto frozenDigest=owner.rollbackDigest(false);
        OriginalEndingCamera camera;camera.reset({0,2,0},{0,0,1},owner.sharedRandomSeed());
        constexpr unsigned desired=object+512,previous=object+528,velocity=object+544;
        for(unsigned i=0;i<3;++i){memory.writeFloat(previous+4*i,i==2?1.f:0.f);memory.writeFloat(velocity+4*i,0);}
        for(unsigned tick=0;tick<12;++tick){
            const Vec3 target{float(tick)*.5f,.3f,1.f+float(tick)*.25f};
            const float values[]{target.x,target.y,target.z};for(unsigned i=0;i<3;++i)memory.writeFloat(desired+4*i,values[i]);
            RefCpu source(memory);source.r[4]=desired;source.r[5]=previous;source.r[6]=velocity;source.r[2]=previous;source.r[15]=stack;source.pr=stop;
            memory.write32(stack,0x3eb33331);memory.write32(stack+4,0x3eb33331);memory.write32(stack+8,0x3ea8f5c1);
            run(source,0xc09c9c0,stop);
            owner.withSharedRandom([&](auto& seed){camera.advanceShared({0,2,0},target,seed);});
            equal(owner.sharedRandomSeed(),memory.read32(0xc37c778),"Ending camera retains all three original shared draws");
            equal(owner.rollbackDigest(false),frozenDigest,"Ending changes no numerical state except shared RNG mirrors");
            equal(owner.initializationSide().randomSeed0C37C778,owner.sharedRandomSeed(),"Accepted ending tick synchronizes shared RNG mirror");
            const auto targetActual=camera.target(),velocityActual=camera.velocity();
            for(unsigned i=0;i<3;++i){equal(std::bit_cast<unsigned>((&targetActual.x)[i]),memory.read32(previous+4*i),"Ending target original spring");equal(std::bit_cast<unsigned>((&velocityActual.x)[i]),memory.read32(velocity+4*i),"Ending velocity original spring");}
        }
        equal(owner.vehicle().drive.words,originalCar,"Ending shared draws do not modify car fields");
        verifyReset(); // The next race keeps ending consumption.
        bool oldCheckpointRejected=false;try{owner.restore(checkpoint);}catch(const std::invalid_argument&){oldCheckpointRejected=true;}
        equal(oldCheckpointRejected,true,"Reset invalidates previous checkpoint epoch");
        const auto freshCheckpoint=owner.checkpoint();const auto freshDigest=owner.rollbackDigest();
        owner.withSharedRandom([](auto& seed){seed^=0xabcdef12;});owner.restore(freshCheckpoint);
        equal(owner.rollbackDigest(),freshDigest,"Rollback restores canonical RNG and initialization mirror");
        equal(owner.initializationSide().randomSeed0C37C778,owner.sharedRandomSeed(),"Shared seed mirror after rollback");
        equal(digest==frozenDigest,false,"Default digest still includes shared RNG");
        auto beforeFailure=owner.sharedRandomSeed();bool failed=false;
        try{owner.reset(root/"missing-audit-fixture",selection,pose.position,pose.angles);}catch(const std::exception&){failed=true;}
        equal(failed,true,"Missing assets fail reset");equal(owner.sharedRandomSeed(),beforeFailure,"Failed reset preserves shared RNG");
        const unsigned consumed=beforeFailure*0x41c64e6du+12345u;
        try{owner.withSharedRandom([](auto& seed){seed=seed*0x41c64e6du+12345u;throw std::runtime_error("intentional callback failure");});}catch(const std::runtime_error&){}
        equal(owner.sharedRandomSeed(),consumed,"Throwing callback retains consumed draw");equal(owner.initializationSide().randomSeed0C37C778,consumed,"Throwing callback synchronizes reset mirror");
        memory.write32(0xc37c778,consumed);verifyReset();
        OriginalDrivingSession moved(std::move(owner));equal(moved.sharedRandomSeed(),memory.read32(0xc37c778),"Move preserves ready shared owner");
        OriginalDrivingSession assigned;assigned=std::move(moved);equal(assigned.sharedRandomSeed(),memory.read32(0xc37c778),"Move assignment preserves ready owner");
    }
    OriginalDrivingSession fresh;equal(fresh.sharedRandomSeed(),1u,"Unchanged default native boot policy");
    fresh.withSharedRandom({});equal(fresh.sharedRandomSeed(),1u,"Empty pending callback");
    try{fresh.withSharedRandom([](auto& seed){seed=123;throw std::runtime_error("intentional pending failure");});}catch(const std::runtime_error&){}
    equal(fresh.ready(),false,"Throwing menu callback creates no car");equal(fresh.sharedRandomSeed(),123u,"Throwing pending callback consumes state");
    OriginalDrivingSession pendingMoved(std::move(fresh));equal(pendingMoved.sharedRandomSeed(),123u,"Move preserves prevehicle RNG");
    OriginalDrivingSession pendingAssigned;pendingAssigned=std::move(pendingMoved);equal(pendingAssigned.sharedRandomSeed(),123u,"Move assignment preserves prevehicle RNG");
    std::cout<<"PASS shared RNG owner comparisons="<<checks<<" actual_original_instructions="<<instructions-hooks<<" hook_dispatches="<<hooks<<"; 32768 spectator cases, original race-owner order,4 sequential menu/course/player/ending/reset histories. Hooks: source integer quotient and explicit parent child/service boundaries. No platform seed policy changed.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
