#include "original_driving_clock.h"
#include "original_driving_session.h"
#include "original_start_grid.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace idas3;
using namespace idas3::original;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass native asset root");
    // Independently decode the original-game MMIO capture rather than using
    // the implementation constant as the expected test frequency.
    constexpr unsigned load=0x02110353,control=0,fb=0x00800005;
    const auto h=(load&1023)+1,v=((load>>16)&1023)+1;
    const double pixel=27000000.0/((fb>>23)&1?1:2);
    const double period=double(h)*v/pixel/((control>>4)&1?2:1);
    require(OriginalDrivingClock::step==period,"Driving scheduler differs from captured original video period");
    require(std::abs(OriginalDrivingClock::frequency-59.79271857560457)<1e-10,"Original cadence rounded to60Hz");
    const unsigned expected=unsigned(std::floor(30.0/period));
    require(expected==1793,"Independent30s frame count changed");
    std::vector<std::uint64_t> reference;
    std::size_t comparisons=0;
    for(unsigned rate:{20,30,60,75,120,144,240}){
        OriginalDrivingSelection selection;selection.bodyContactEnabled=false;
        selection.physics=makeOriginalFreshTimeAttackSelection(0,6,OriginalWeather::Dry);
        const auto pose=originalStartPose(6,originalSoloStartGridSlot(2));
        OriginalDrivingSession session;session.reset(argv[1],selection,pose.position,pose.angles);
        OriginalDrivingClock clock;unsigned frames=0;
        for(unsigned display=0;display<rate*30;++display){
            clock.advance(1.0/rate,[&]{
                OriginalVehicleInputs input;input.calibration={128,32,32};input.automaticMode=true;input.gearEnabled=true;
                const int steer=frames%600<200?128:frames%600<400?150:106;
                input.analog={std::uint16_t(steer<<8),std::uint16_t((frames%600<450?171:64)<<8),std::uint16_t((frames%600>=450?171:64)<<8)};
                session.tick(input);
                const auto digest=session.rollbackDigest();
                if(rate==20)reference.push_back(digest);
                else{require(frames<reference.size()&&digest==reference[frames],"Display rate changed original driving state");++comparisons;}
                ++frames;
            });
            require(clock.alpha()>=0&&clock.alpha()<=1,"Interpolation escaped its two original frames");
        }
        require(frames==expected,"30 wall seconds must retain original frame count at every display rate");
        require(std::abs(clock.accumulator-(30-expected*period))<1e-10,"Original pacing lost fractional time");
    }
    OriginalDrivingClock paused;paused.advance(period*.75,[]{});paused.reset();
    require(paused.advance(period*.75,[]{})==0,"Pause accumulated stale wall time");
    require(paused.advance(period*.25,[]{})==1,"Resume lost fractional frame");
    OriginalDrivingClock stall;unsigned count=0;stall.advance(10,[&]{++count;});
    require(count==5,"Long offline stall must remain bounded to100ms at original cadence");
    OriginalDrivingClock online;count=0;
    for(double slice:{.1,.1,.05})online.advance(slice,[&]{++count;});
    require(count==unsigned(std::floor(.25/period)),"Online sliced catchup lost original frames");
    std::cout<<"PASS original852x530/27MHz cadence,1793 frames per30wall seconds at7 display rates; "<<comparisons<<" exact full-session digest matches, pause/interpolation/stall/online slicing. Per-frame numerical physics unchanged.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
