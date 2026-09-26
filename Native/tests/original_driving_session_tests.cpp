#include "original_driving_session.h"
#include "original_host_input.h"
#include "original_start_grid.h"
#include <cmath>
#include <iostream>

using namespace idas3::original;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void checkFinite(const OriginalDrivingSession& session){
    for(const auto offset:{0,4,8,12,16,20,0x108,0x110,0x220,0x224,0x228,0x238,0x258,0x25C,0x260,0x264})
        require(std::isfinite(session.vehicle().drive.f(offset)),"Non-finite original driving/pose field");
    for(const auto offset:{0,4,8,24,28,32})require(std::isfinite(session.actor().f(offset)),"Non-finite original actor pose");
    require(std::isfinite(session.vehicle().transmission.filtered18)&&std::isfinite(session.vehicle().transmission.tach1c),"Non-finite original engine state");
    require(session.vehicle().drive.f(0x238)>=0.0f,"Negative original speed");
}
float steeringScreenDisplacement(const std::filesystem::path& root,std::uint32_t condition,float steering){
    OriginalDrivingSelection selection;selection.physics.conditionCode=condition;
    selection.collisionVariant=condition&1u;
    const auto pose=originalAkinaStartPose(condition,0);
    OriginalDrivingSession session;session.reset(root,selection,pose.position,pose.angles);
    OriginalHostInputState host;
    // Identical authored-grid starts and acceleration establish the neutral
    // baseline before branching into left, straight and right maneuvers.
    for(std::int32_t tick=0;tick<100;++tick){
        session.tick(adaptOriginalHostInput(host,{0,.8f,0,false,false},true,true,tick));
        checkFinite(session);
    }
    const auto start=session.vehicle().drive;
    // Travel is -forward(originalYaw). In the right-handed chase camera,
    // screen-right is cross(up, eye-target)=(cos(yaw),0,-sin(yaw)).
    // Measure all outcomes in this common pre-maneuver frame, so a rotating
    // follow camera cannot disguise an inverted physical-input mapping.
    const float rightX=std::cos(start.f(0x10)),rightZ=-std::sin(start.f(0x10));
    for(std::int32_t tick=100;tick<120;++tick){
        const auto effects=session.tick(adaptOriginalHostInput(host,{steering,.8f,0,false,false},true,true,tick));
        checkFinite(session);
        require(effects.newImpactRecords.empty()&&!effects.restoredValidRoadState,"Wall/recovery contaminated steering direction check");
    }
    const auto& finish=session.vehicle().drive;
    require(finish.f(0x238)>3.0f,"Steering direction check never reached driving speed");
    return (finish.f(0)-start.f(0))*rightX+(finish.f(8)-start.f(8))*rightZ;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_driving_session_tests native_root");
    const std::filesystem::path root=argv[1];const auto data=OriginalPhysicsData::load(root/"data/original_physics/tables.bin");
    std::size_t ticks=0,wallRecords=0,recoveries=0;
    for(std::uint32_t variant=0;variant<2;++variant){
        OriginalDrivingSelection selection;selection.physics.vehicleIndex=variant?7:0;
        selection.physics.conditionCode=6+variant;selection.collisionVariant=variant;
        const auto path=data.loadPath(root/"data/original_physics",selection.physics.conditionCode);
        auto position=path.points[0];position[1]+=1.0f;
        // Explicit test spawn policy; the session itself does not invent a
        // start grid or derive its supplied original angle from render data.
        const float dx=path.points[1][0]-position[0],dz=path.points[1][2]-position[2];
        const std::array<float,3> angles{0,std::atan2(-dx,-dz),0};
        OriginalDrivingSession session;require(!session.ready(),"Fresh session already ready");
        const auto resetEffects=session.reset(root,selection,position,angles);require(session.ready(),"Reset failed to produce a session");
        require(resetEffects.resetPlatformDigitalInput,"Original reset request was lost");
        float maximumSpeed=0,maximumDistance=0;
        for(std::uint32_t tick=0;tick<720;++tick){
            OriginalVehicleInputs inputs;inputs.calibration={128,32,32};
            const int steer=(tick>=160&&tick<280)?8:(tick>=280&&tick<400)?-8:0;
            const int throttle=tick>=60&&tick<480?171:64;
            const int brake=tick>=480&&tick<600?171:64;
            inputs.analog={std::uint16_t((128+steer)<<8),std::uint16_t(throttle<<8),std::uint16_t(brake<<8)};
            inputs.automaticMode=true;inputs.gearEnabled=true;
            inputs.elapsedFrames0C900E84=-987; // owner counter must prevail
            inputs.pressedByte=tick==240?0x10:tick==360?0x20:0;
            session.setPlatformFrame(10000+tick,tick%120==0?0x80:0);
            const auto effects=session.tick(inputs);++ticks;
            checkFinite(session);
            require(effects.frame0C92DE30==10000+tick&&session.platformFrame()==10001+tick,"Platform frame not preserved");
            require(session.contactCompletion().elapsedFrames0C900E84==tick+1,"Owned elapsed frame did not advance");
            require(session.parameters().road.path0C901728.data()==session.path().points.data(),"Owned original path span detached");
            wallRecords+=effects.newImpactRecords.size();recoveries+=effects.restoredValidRoadState?1:0;
            const auto& d=session.vehicle().drive;maximumSpeed=std::max(maximumSpeed,d.f(0x238));
            const float x=d.f(0)-position[0],z=d.f(8)-position[2];maximumDistance=std::max(maximumDistance,std::sqrt(x*x+z*z));
            if(tick==360){OriginalDrivingSession moved=std::move(session);require(moved.ready()&&!session.ready(),"Session move state");session=std::move(moved);}
            if(tick==400){
                const auto before=session.vehicle().drive.words;auto unsupported=selection;unsupported.physics.conditionCode=18;
                bool rejected=false;try{session.reset(root,unsupported,position,angles);}catch(const std::invalid_argument&){rejected=true;}
                require(rejected&&session.vehicle().drive.words==before,"Failed selection reset changed active state");
            }
        }
        require(maximumSpeed>1.0f&&maximumDistance>1.0f,"Original session did not accelerate and progress");
        std::cout<<"Akina variant"<<variant<<" car"<<selection.physics.vehicleIndex<<": maxSpeed="<<maximumSpeed<<"m/s, maxDisplacement="<<maximumDistance<<" original units\n";
        const auto frameBefore=session.platformFrame();
        session.reset(root,selection,position,angles);
        require(session.contactCompletion().elapsedFrames0C900E84==0&&session.platformFrame()==frameBefore,"Reset frame domains were conflated");
        require(session.vehicle().tail.statistics0C91FB0C[5]==0x0C92DFC4u,"Reset did not restore original impact append identity");
        require(session.contactCompletion().positionCursor==0&&session.contactCompletion().frameCursor==0,"Reset did not restore contact append cursors");
    }
    for(const auto condition:{6u,7u}){
        const float neutral=steeringScreenDisplacement(root,condition,0);
        const float left=steeringScreenDisplacement(root,condition,-.2f)-neutral;
        const float right=steeringScreenDisplacement(root,condition,.2f)-neutral;
        ticks+=360;
        require(left<-.02f,"Physical left input did not move left in the RH camera frame");
        require(right>.02f,"Physical right input did not move right in the RH camera frame");
        std::cout<<"Akina condition"<<condition<<" RH lateral relative to neutral: left="<<left<<", right="<<right<<" original units\n";
    }
    {
        OriginalDrivingSelection selection;selection.physics.conditionCode=6;
        const auto pose=originalStartPose(6,0);
        OriginalDrivingSession session;session.reset(root,selection,pose.position,pose.angles);
        OriginalHostInputState host;
        auto advance=[&](){++ticks;auto effects=session.tick(adaptOriginalHostInput(host,{0,.8f,0,false,false},true,true,0));checkFinite(session);return effects;};
        for(unsigned tick=0;tick<80;++tick)advance();
        require(session.vehicle().drive.f(0x238)>1.0f&&!session.stoppedForRace(),"Auto-brake test did not establish motion");
        const auto flags=session.actor().u(0x50);
        session.setRaceAutomaticBrake(true);
        require(session.actor().u(0x50)==(flags|0x2000u)&&session.raceAutomaticBrakeByte()==1,"Race request changed unrelated actor flags");
        require(session.vehicle().drive.u(0x1A8)==0,"Race setter bypassed original contact-stage request consumption");
        advance();
        require(session.vehicle().drive.u(0x1A8)==1&&session.vehicle().controls.throttle>0,"Original auto-brake first-frame latency changed");
        advance();
        require(session.vehicle().controls.throttle==0&&session.vehicle().controls.brake==0,"Auto-brake was replaced by host pedal input");
        unsigned brakeTicks=2;
        while(!session.stoppedForRace()&&brakeTicks<300){advance();++brakeTicks;}
        require(session.stoppedForRace()&&session.vehicle().drive.u(0x1AC)==1&&session.vehicle().drive.f(0x238)==0,"Original automatic brake failed to publish stopped latch");
        const auto stoppedFlags=session.actor().u(0x50);
        session.setRaceAutomaticBrake(false);
        require(session.actor().u(0x50)==(stoppedFlags&~0x2000u)&&session.raceAutomaticBrakeByte()==0,"Race extension clear changed unrelated flags");
        require(session.stoppedForRace(),"Request clear incorrectly erased original stopped latch");
        advance();require(session.vehicle().drive.u(0x1A8)==0,"Contact stage did not consume cleared race request");
        advance();require(session.vehicle().controls.throttle>0,"Cleared race request still suppressed throttle");
        session.setRaceAutomaticBrake(true);
        session.reset(root,selection,pose.position,pose.angles);
        require(session.raceAutomaticBrakeByte()==0&&(session.actor().u(0x50)&0x2000u)==0&&session.vehicle().drive.u(0x1AC)==0,"Race reset preserved old automatic-brake request or drive latch");
        // Original initializer preserves actor bit14; first contact preparation
        // republishes the cleared drive stopped latch, rather than reset guessing.
        advance();require(!session.stoppedForRace(),"First reset contact did not republish cleared stopped latch");
        std::cout<<"Original race auto-brake: stopped after "<<brakeTicks<<" ticks, exact contact-stage latency, no pedal override, original stopped-latch publication\n";
    }
    {
        // A battle abandoned at the start can leave a published rival there.
        // Restart solo at that pose: retain source state, but disable its pair.
        OriginalDrivingSelection battle;battle.physics.conditionCode=7;battle.collisionVariant=1;
        const auto pose=originalStartPose(7,0);
        OriginalDrivingRivalSetup rival;rival.control=0;rival.position=pose.position;rival.angles=pose.angles;
        rival.position[0]+=.25f;battle.rival=rival;
        OriginalDrivingSession session;session.reset(root,battle,pose.position,pose.angles);
        OriginalVehicleInputs inputs;inputs.calibration={128,32,32};inputs.analog={32768,16384,16384};
        require(session.tick(inputs).bodyCollision.active!=0,"Regression fixture did not produce overlapping battle cars");
        auto solo=battle;solo.rival.reset();solo.physics.progressEnabled0C9015E4=1;
        session.reset(root,solo,pose.position,pose.angles);
        require(session.tick(inputs).bodyCollision.active!=0,"Regression fixture did not preserve abandoned rival contact");
        solo.bodyContactEnabled=false;session.reset(root,solo,pose.position,pose.angles);
        for(unsigned frame=0;frame<30;++frame)
            require(session.tick(inputs).bodyCollision.active==0,"Solo retry collided with a retained invisible rival");
        const OriginalBodyCollisionResult shared{1,.25f,0};
        require(session.tick(inputs,&shared).bodyCollision.active==1,"Solo policy suppressed explicit online pair contact");
        std::cout<<"Solo restart: retained battle contact reproduced, disabled for solo, synchronized contact preserved\n";
    }
    std::cout<<"PASS "<<ticks<<" integrated original input/drivetrain/contact/completion/recovery ticks, finite physical state and progression; "<<wallRecords
        <<" wall records, "<<recoveries<<" recoveries. Real Akina data, two cars/routes; no runtime or GPU launched. This progression check is not the independent opcode comparison.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
