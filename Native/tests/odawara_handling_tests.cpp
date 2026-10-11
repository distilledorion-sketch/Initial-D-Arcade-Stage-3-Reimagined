#include "imported_course.h"
#include "original_host_input.h"
#include "original_math.h"
#include <iostream>
#include <tuple>
using namespace idas3;
using namespace idas3::original;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
auto angular(const OriginalAngularParameters& p){return std::array{
    p.carRecord0C283F18_08,p.carRecord0C283F18_0C,p.table0C285124,p.global0C900E40,
    p.table0C285AFC,p.global0C900EC0,p.table0C285610,p.global0C8FF380,p.table0C287398,p.table0C285FE8,p.global0C900E54};}
auto steering(const OriginalSteeringMemoryParameters& p){return std::tuple{
    p.table0C2864D4,p.global0C90094C,p.table0C286EAC,p.global0C900E30,p.table0C287884,p.global0C900EF8,
    p.table0C2869C0,p.global0C900E4C,p.table0C287D70,p.global0C900EB0,p.mask0C900EBC,p.mask0CAA9CF0,p.shiftDownPressed};}
auto loss(const OriginalLossParameters& p){return std::tuple{p.condition0C9015CC,p.normalizedBrake0CAA98A0,p.cap0C284F80,p.growth0C284F84};}
auto transmission(const OriginalTransmissionParameters& p){return std::tuple{p.profileIndex,p.maximumGear,p.workingBase,p.lower,p.upper,p.divisor};}
OriginalVehicleState initialize(const OriginalPhysicsData& data,const OriginalPhysicsSelection& s,OriginalVehicleParameters& p){
    OriginalVehicleState v;OriginalInitializationSideState side;
    initializeOriginalVehicle(v,p,side,data.initialization(s,{0,0,0},{0,0,0}));
    // Isolate acceleration from track curvature, hills and wall collisions.
    // These four level-road contact vectors are also used by the original
    // vehicle instruction-reference fixture. Real-road coverage is separate.
    for(auto offset:{0x2D8,0x2E4,0x2F0,0x2FC})v.drive.setf(offset+8,1.f);
    v.drive.setu(0x3F0,1);
    return v;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("odawara_handling_tests native_root odawara_root");
    const std::filesystem::path root=argv[1];const auto course=ImportedCourse::load(argv[2]);
    require(course.id==17,"Expected Odawara fixture");
    const auto dataRoot=root/"data/original_physics";const auto data=OriginalPhysicsData::load(dataRoot/"tables.bin");
    const OriginalMath math{originalSinF32,originalCosF32,originalFiprDot3};
    unsigned parameterCases=0,driveCases=0,distinctHandling=0;std::uint64_t frames=0;
    for(unsigned reverse=0;reverse<2;++reverse){
        const auto road=course.drivingRoad(reverse!=0);const auto spawn=course.spawn(reverse!=0);
        require(course.handlingCondition(reverse)!=reverse&&course.handlingCondition(reverse)==14+reverse,"Odawara lost Tsuchisaka handling");
        require(road.speedCondition==reverse&&road.accelerationScale==1.f,"Odawara must use Myogi speeds without the old acceleration boost");
        const auto tsuchiPath=data.loadPath(dataRoot,14+reverse),myogiPath=data.loadPath(dataRoot,reverse);
        for(unsigned car=0;car<35;++car)for(bool wet:{false,true})for(unsigned upgrade:{0u,75u}){
            auto selection=makeOriginalFreshTimeAttackSelection(car,14+reverse,wet?OriginalWeather::Wet:OriginalWeather::Dry);
            selection.upgradeIndex0C9015F0=upgrade;
            auto myogiSelection=selection;myogiSelection.conditionCode=reverse;
            auto hybrid=data.parameters(selection,tsuchiPath,road.speedCondition);
            auto tsuchi=data.parameters(selection,tsuchiPath),myogi=data.parameters(myogiSelection,myogiPath);
            require(hybrid.frame.table0C28451C==myogi.frame.table0C28451C,"Wrong per-car Myogi speed coefficient");
            require(hybrid.frame.table0C28866C==tsuchi.frame.table0C28866C,"Saved car tuning changed");
            require(angular(hybrid.angular)==angular(tsuchi.angular)&&steering(hybrid.steeringMemory)==steering(tsuchi.steeringMemory)
                &&loss(hybrid.loss)==loss(tsuchi.loss),"Tsuchisaka steering, drift or braking parameters changed");
            distinctHandling+=angular(hybrid.angular)!=angular(myogi.angular);
            require(transmission(hybrid.transmission)==transmission(myogi.transmission)&&hybrid.profile.words==myogi.profile.words,
                "Car-specific gear/RPM profile changed");
            require(hybrid.road.condition0C9015CC==14+reverse&&hybrid.road.path0C901728.data()==tsuchiPath.points.data(),"Speed donor replaced the road owner");
            ++parameterCases;
            // All cars, directions and transmissions; both tuning endpoints.
            // Weather is also exercised in real Odawara driving sessions below.
            if(wet)continue;
            for(bool automatic:{false,true}){
                auto aParams=hybrid,bParams=myogi;
                auto a=initialize(data,selection,aParams),b=initialize(data,myogiSelection,bParams);
                OriginalHostInputState host;float speedAtFiveSeconds=0;
                std::array<float,60> lastSecond{};unsigned stableFrames=0;
                for(unsigned tick=0;tick<36000;++tick){
                    auto input=adaptOriginalHostInput(host,{0,1,0,false,false},automatic,true,int(tick));
                    if(!automatic){
                        auto gear=b.transmission.gear00;
                        if(gear<bParams.transmission.maximumGear&&b.drive.f(0x248)>=bParams.profile.atByteOffset(4+4*gear)*bParams.profile.atByteOffset(0))++gear;
                        input.requestedGear=std::uint8_t(gear);
                    }
                    stepOriginalVehicle(a,input,aParams,math);stepOriginalVehicle(b,input,bParams,math);
                    require(std::isfinite(a.drive.f(0x238))&&a.drive.f(0x238)==b.drive.f(0x238),"Odawara straight-line acceleration differs from Myogi");
                    require(a.transmission.gear00==b.transmission.gear00&&a.transmission.tach1c==b.transmission.tach1c,"Odawara shift or RPM trace differs from Myogi");
                    if(tick==299)speedAtFiveSeconds=a.drive.f(0x238)*3.6f;
                    const auto speed=a.drive.f(0x238);
                    stableFrames=tick>=7200&&std::abs(speed-lastSecond[tick%60])<.001f?stableFrames+1:0;
                    lastSecond[tick%60]=speed;
                    ++frames;
                    if(stableFrames>=120)break;
                }
                require(a.transmission.gear00==aParams.transmission.maximumGear&&a.drive.f(0x238)>20,"Straight-line test never reached top gear/speed");
                if(stableFrames<120)throw std::runtime_error("Top-speed test did not settle: car="+std::to_string(car)+" speed="+std::to_string(a.drive.f(0x238)));
                if(car==0&&upgrade==0)std::cout<<"direction="<<reverse<<" AT="<<automatic<<" five_seconds_kmh="<<speedAtFiveSeconds<<" terminal_kmh="<<a.drive.f(0x238)*3.6f<<'\n';
                ++driveCases;
            }
        }
        // Validate wiring through the shared solo/online session owner,
        // including weather and Tsuchisaka's full cornering-loss penalty.
        for(bool wet:{false,true}){
            OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(0,14+reverse,wet?OriginalWeather::Wet:OriginalWeather::Dry);selection.collisionVariant=reverse;
            OriginalDrivingSession session;session.reset(root,selection,spawn.position,spawn.angles,&road);session.enableRaceStart(2);
            auto expected=selection.physics;expected.conditionCode=reverse;
            require(session.parameters().frame.table0C28451C==data.parameters(expected,myogiPath).frame.table0C28451C,"Session ignored Myogi speed donor");
            OriginalHostInputState host;
            for(int tick=0;tick<120;++tick)session.tick(adaptOriginalHostInput(host,{0,1,0,false,false},true,true,tick));
            auto p=session.parameters();auto originalTsuchi=data.parameters(selection.physics,tsuchiPath);
            // Initialization supplies live multipliers; compare recovered table cells separately above.
            require(p.loss.condition0C9015CC==originalTsuchi.loss.condition0C9015CC,"Session selected Myogi's reduced cornering penalty");
            require(session.vehicle().drive.u(0x434)==unsigned(wet),"Session lost weather selection");
        }
        for(unsigned bad:{18u,reverse^1u}){
            bool rejected=false;try{data.parameters(makeOriginalFreshTimeAttackSelection(0,14+reverse,OriginalWeather::Dry),tsuchiPath,bad);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"Invalid speed donor accepted");
        }
    }
    require(distinctHandling>0,"Handling test did not distinguish Tsuchisaka from Myogi");
    std::cout<<"PASS "<<parameterCases<<" parameter selections; "<<driveCases<<" Myogi-matching acceleration/top-speed traces; "<<frames<<" vehicle frames; Odawara dry/wet session wiring.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
