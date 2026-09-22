#include "imported_course.h"
#include "original_host_input.h"
#include <iostream>
using namespace idas3;
using namespace idas3::original;
static unsigned checks=0;
void check(bool ok,const std::string& text){++checks;if(!ok)throw std::runtime_error(text);}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("native_root RuntimeAssets");
    for(unsigned id=12;id<=14;++id){
        auto course=ImportedCourse::load(std::filesystem::path(argv[2])/importedCourseDefinition(id).folder);
        check(course.id==id,"Distinct imported identity");
        for(bool reverse:{false,true}){
            const auto markers=course.routeCheckpoints(reverse);auto road=course.drivingRoad(reverse);
            // Query the real D3 surface solver along the entire imported road,
            // not just the start or a fabricated flat strip.
            unsigned surfaces=0;
            for(int i=markers[0];i<=markers[4];++i){
                auto p=road.path.points[i];OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
                q.setf(32,p[0]);q.setf(36,p[1]+1);q.setf(40,p[2]);OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
                check(queryOriginalCollisionSurface(road.collision,q,trace,scratch),course.slug+" missing road at "+std::to_string(i));
                check(std::abs(q.f(16)-p[1])<3,course.slug+" surface height at "+std::to_string(i));++surfaces;
            }
            auto spawn=course.spawn(reverse);auto projection=course.racePath(reverse);OriginalPathCoordinate coordinate{markers[0],0};
            check(projection.project(spawn.position,coordinate,true),"Spawn lies on imported race path");
            for(unsigned slot=0;slot<2;++slot){
                auto pose=course.onlineSpawn(reverse,slot);OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
                q.setf(32,pose.position[0]);q.setf(36,pose.position[1]+1);q.setf(40,pose.position[2]);OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
                check(queryOriginalCollisionSurface(road.collision,q,trace,scratch),"Online grid lies on source collision");
            }
            OriginalRaceRules rules;course.resetRules(rules,reverse,spawn.position);rules.start();unsigned extensions=0,sections=0;
            for(int i=markers[0];i<=markers[4];++i){auto event=rules.tick({i,0},road.path.points[i]);extensions+=event.timeExtension;sections+=event.section;}
            check(rules.state().phase==OriginalRacePhase::Finished&&extensions==3&&sections==4,"All authored checkpoints and finish work in race direction");
            const auto times=course.raceTimes(reverse);const auto donor=course.handlingCondition(reverse);
            check(times[0]>=originalTimeAttackInitialSeconds(donor,2),"Initial countdown is not shortened");
            for(unsigned i=1;i<4;++i)check(times[i]>=originalTimeAttackBonusSeconds(donor)[i-1],"Section allowance is not shortened");
            for(bool wet:{false,true})for(bool automatic:{false,true})for(unsigned car:{0u,19u,27u}){
                OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(car,donor,wet?OriginalWeather::Wet:OriginalWeather::Dry);selection.collisionVariant=unsigned(reverse);
                OriginalDrivingSession session;session.reset(argv[1],selection,spawn.position,spawn.angles,&road);session.enableRaceStart(2);OriginalHostInputState input;
                check(session.selection().physics.conditionCode==donor,"Selected donor condition reaches original solver");
                check(session.vehicle().drive.u(0x434)==unsigned(wet),"Original dry/wet selection reaches original solver");
                check(session.path().points==road.path.points,"Donor never replaces the imported road");
                float maxSpeed=0;unsigned maxGear=0;
                for(unsigned tick=0;tick<480;++tick){
                    auto effects=session.tick(adaptOriginalHostInput(input,{0,1,0,false,tick==180||tick==330},automatic,true,tick));
                    auto& vehicle=session.vehicle();maxSpeed=std::max(maxSpeed,vehicle.drive.f(0x238));maxGear=std::max(maxGear,vehicle.transmission.gear00);
                    check(!effects.invalidScalarDiagnostics&&std::isfinite(vehicle.drive.f(4)),"Finite original vehicle on imported road");
                    auto p=course.source.project({vehicle.drive.f(0),vehicle.drive.f(4),vehicle.drive.f(8)});
                    check(std::abs(vehicle.drive.f(4)-p.sample.center.y)<6,"Vehicle remains on the road");
                }
                check(maxSpeed>4&&maxGear>1,"Donor car accelerates and shifts");
            }
            std::cout<<course.name<<" direction="<<reverse<<" donor="<<donor<<" road queries="<<surfaces<<" timer=";
            for(auto t:times)std::cout<<t<<' ';std::cout<<'\n';
        }
    }
    std::cout<<"PASS "<<checks<<" checks: six routes, dry/wet, AT/MT, FR/4WD cars, all source road surfaces, grids and checkpoints.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
