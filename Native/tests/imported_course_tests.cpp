#include "imported_course.h"
#include "original_host_input.h"
#include <iostream>
using namespace idas3;using namespace idas3::original;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("native_root imported_root");
    const auto course=ImportedCourse::load(argv[2]);int surfaces=0,ticks=0;
    require(course.lamps.size()==(course.id==10?16:46),"Hakone authored lamps were not loaded");
    // Query the converted road along its full length with the actual D3 solver.
    for(unsigned i=course.checkpoints[0];i<=unsigned(course.checkpoints[4]);i++){
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);auto p=course.center[i];
        q.setf(32,p[0]);q.setf(36,p[1]+1);q.setf(40,p[2]);OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
        if(!queryOriginalCollisionSurface(course.collision,q,trace,scratch))throw std::runtime_error("Missing road surface at point "+std::to_string(i));
        require(std::abs(q.f(16)-p[1])<3,"Imported surface height mismatch");surfaces++;
    }
    // Sweep past the finite imported shoulders, including the first Sadamine
    // hairpin. Checking only the road centre never exercised these escapes.
    int wallSweeps=0;
    for(int i=course.checkpoints[0]+1;i<course.checkpoints[4]-1;++i)for(int side=0;side<2;++side)for(float outside:{6.f,12.f}){
        const auto& edge=side?course.left:course.right;
        OriginalRacePoint wall{},center{};
        for(unsigned k=0;k<3;++k){wall[k]=edge[i][k]*.63f+edge[i+1][k]*.37f;center[k]=course.center[i][k]*.63f+course.center[i+1][k]*.37f;}
        float dx=wall[0]-center[0],dz=wall[2]-center[2],length=std::sqrt(dx*dx+dz*dz);dx/=length;dz/=length;
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
        q.setf(44,wall[0]-dx);q.setf(48,wall[1]+1);q.setf(52,wall[2]-dz);
        q.setf(32,wall[0]+dx*outside);q.setf(36,wall[1]+1);q.setf(40,wall[2]+dz*outside);
        OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
        if(!queryOriginalCollisionSwept(course.collision,q,trace,scratch)||!(q.u(28)&0x8000))throw std::runtime_error("Missed imported wall at point "+std::to_string(i)+" side "+std::to_string(side));
        require(std::isfinite(q.f(24))&&std::abs(q.f(0)*q.f(0)+q.f(8)*q.f(8)-1)<.001f,"Invalid imported wall contact");++wallSweeps;
    }
    std::cout<<"PASS "<<wallSweeps<<" whole-route outward wall sweeps\n";
    for(bool reverse:{false,true}){
        const auto spawn=course.spawn(reverse);auto road=course.drivingRoad(reverse);auto projection=course.racePath(reverse);
        OriginalPathCoordinate coordinate{course.rules(reverse).startIndex,0};require(projection.project(spawn.position,coordinate,true),"Start is outside race path");
        OriginalRaceRules rules;course.resetRules(rules,reverse,spawn.position);rules.start();
        // Test all source gates, including fourth split, through existing rules.
        int extensions=0,sections=0;
        for(int index=rules.rules().startIndex;index<=rules.rules().startIndex+rules.rules().goalIndex;index++){
            int source=reverse?int(course.center.size())-1-index:index;
            auto e=rules.tick({index,0},course.center[source]);extensions+=e.timeExtension;sections+=e.section;
        }
        require(rules.state().phase==OriginalRacePhase::Finished&&extensions==3&&sections==4,"Original rules did not finish imported race/checkpoints");
        for(bool wet:{false,true})for(bool automatic:{false,true}){
            OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(0,course.handlingCondition(reverse),wet?OriginalWeather::Wet:OriginalWeather::Dry);selection.collisionVariant=unsigned(reverse);
            OriginalDrivingSession session;session.reset(argv[1],selection,spawn.position,spawn.angles,&road);session.enableRaceStart(2);OriginalHostInputState input;
            require(session.vehicle().drive.u(0x434)==unsigned(wet),"D3 wet handling flag not applied");
            float maximumSpeed=0,maximumRpm=0;unsigned maximumGear=0,contacts=0;
            for(int t=0;t<600;t++){
                auto effects=session.tick(adaptOriginalHostInput(input,{0,1,0,false,t==180||t==330},automatic,true,t));ticks++;
                const auto& v=session.vehicle();maximumSpeed=std::max(maximumSpeed,v.drive.f(0x238));maximumRpm=std::max(maximumRpm,v.transmission.tach1c);maximumGear=std::max(maximumGear,v.transmission.gear00);
                require(!effects.invalidScalarDiagnostics&&std::isfinite(v.drive.f(4)),"Original vehicle failed on imported road");
                contacts+=effects.newImpactRecords.size();
            }
            std::cout<<"direction "<<reverse<<" wet "<<wet<<" automatic "<<automatic<<" speed "<<maximumSpeed<<" rpm "<<maximumRpm<<" gear "<<maximumGear<<'\n';
            require(maximumSpeed>4&&maximumRpm>2000&&maximumGear>1,"Imported race did not drive/shift with D3 vehicle");
            // Hold into the edge intentionally: the existing wall solver must
            // stop lateral escape rather than letting the car fall off the strip.
            for(int t=0;t<180;t++){
                auto e=session.tick(adaptOriginalHostInput(input,{.8f,1,0,false,false},automatic,true,t));ticks++;contacts+=e.newImpactRecords.size();
                const auto& v=session.vehicle().drive;auto p=course.source.project({v.f(0),v.f(4),v.f(8)});
                require(std::abs(v.f(4)-p.sample.center.y)<5,"D3 car fell below imported road at boundary");
            }
            require(contacts>0,"Imported boundary did not invoke D3 wall contact");
        }
    }
    std::cout<<"PASS "<<surfaces<<" road queries; both directions/four sections/three extensions; "<<ticks<<" D3 solver ticks with automatic and manual gearbox.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
