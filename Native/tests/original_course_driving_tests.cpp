#include "original_driving_session.h"
#include "original_host_input.h"
#include "original_start_grid.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_course_driving_tests native_root");
    std::size_t totalTicks=0;
    for(std::uint32_t condition=0;condition<18;++condition)for(std::uint32_t slot=0;slot<2;++slot)for(unsigned wet=0;wet<(condition<16?2u:1u);++wet){
        OriginalDrivingSelection selection;selection.physics.conditionCode=condition;
        selection.physics.vehicleIndex=slot==0?0:7;selection.collisionVariant=condition&1u;
        selectOriginalWeather(selection.physics,wet?OriginalWeather::Wet:OriginalWeather::Dry);
        const auto pose=originalStartPose(condition,slot);
        OriginalDrivingSession session;session.reset(argv[1],selection,pose.position,pose.angles);
        OriginalHostInputState host;
        unsigned validAtStart=0,recoveries=0;float maxSpeed=0;
        for(int tick=0;tick<180;++tick){
            const auto result=session.tick(adaptOriginalHostInput(host,{tick>=80&&tick<105?.15f:0.f,tick<10?0.f:.65f,0,false,false},true,true,tick));
            ++totalTicks;recoveries+=result.restoredValidRoadState;
            const auto& drive=session.vehicle().drive;
            for(const auto offset:{0,4,8,12,16,20,0x238})if(!std::isfinite(drive.f(offset)))throw std::runtime_error("Non-finite original course driving state");
            if(result.invalidScalarDiagnostics)throw std::runtime_error("Invalid original contact scalar");
            if(drive.u(0x438)!=(condition>15?1u:0u))throw std::runtime_error("Original snow flag not initialized");
            if(drive.u(0x434)!=wet)throw std::runtime_error("Original wet flag not initialized");
            maxSpeed=std::max(maxSpeed,drive.f(0x238));
            if(tick==0)for(const auto& query:session.roadContact().surfaces0CAA9518)
                validAtStart+=std::int32_t(query.u(56))>=0&&std::int32_t(query.u(60))>=0;
        }
        const auto& d=session.vehicle().drive;
        const float dx=d.f(0)-pose.position[0],dz=d.f(8)-pose.position[2];
        const float distance=std::sqrt(dx*dx+dz*dz);
        std::cout<<"condition="<<condition<<" slot="<<slot<<" wet="<<wet<<" contacts="<<validAtStart<<" displacement="<<distance<<" speed="<<maxSpeed<<" recoveries="<<recoveries<<'\n';
        if(validAtStart!=4)throw std::runtime_error("Authored course start did not find all four road contacts");
        if(maxSpeed<1.f||distance<1.f)throw std::runtime_error("Original course vehicle failed to move from authored start");
    }
    std::cout<<"PASS all18 original course/direction starts, dry/wet for16 ordinary routes plus2 snow routes, both authored grid slots, two vehicle records, "<<totalTicks<<" native driving ticks with real course collision. This is a short integration check, not full-lap validation.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
