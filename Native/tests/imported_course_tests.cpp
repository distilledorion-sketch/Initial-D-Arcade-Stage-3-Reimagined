#include "imported_course.h"
#include "original_host_input.h"
#include "original_math.h"
#include <iostream>
using namespace idas3;using namespace idas3::original;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
bool sameDrivingState(const OriginalVehicleState& a,const OriginalVehicleState& b){
    // Compare defined fields, not struct padding. The transmission's ten-word
    // layout is independently fixed by original_transmission.h.
    return a.drive.words==b.drive.words&&
        std::bit_cast<std::array<std::uint32_t,10>>(a.transmission)==std::bit_cast<std::array<std::uint32_t,10>>(b.transmission)&&
        a.loss.speedLoss0CAA9880==b.loss.speedLoss0CAA9880&&a.loss.persistentPenalty0CAA9884==b.loss.persistentPenalty0CAA9884&&
        a.tail.history0CAA98E0==b.tail.history0CAA98E0&&a.tail.throttleHistory0CAA99E0==b.tail.throttleHistory0CAA99E0&&
        a.tail.steeringHistory0CAA9BE0==b.tail.steeringHistory0CAA9BE0;
}
unsigned checkAccelerationResponse(const OriginalVehicleState& captured,const OriginalVehicleParameters& configured,
        const OriginalVehicleInputs& inputs,bool odawara,bool checkUnpowered){
    const OriginalMath math{originalSinF32,originalCosF32,originalFiprDot3};
    auto stockParameters=configured;stockParameters.accelerationScale=1.f;
    auto stock=captured,tuned=captured;
    const auto stockStep=stepOriginalVehicle(stock,inputs,stockParameters,math);
    const auto tunedStep=stepOriginalVehicle(tuned,inputs,configured,math);
    require(stockStep.frameCoefficient==tunedStep.frameCoefficient&&stockStep.motionScalar==tunedStep.motionScalar,
        "Acceleration adjustment changed frame or steering coefficients");
    require(stock.transmission.target14==tuned.transmission.target14&&stock.transmission.gear00==tuned.transmission.gear00,
        "Acceleration adjustment changed RPM target or gear selection");
    unsigned positive=0;
    // +240 is this frame's propulsion response after the separately computed
    // road/braking losses; total speed or lap time need not increase by 27.05%.
    if(!odawara)require(sameDrivingState(stock,tuned),"Another imported course's driving response changed");
    else if(stock.controls.throttle>0&&stock.transmission.gear00&&stock.drive.f(0x240)>.0001f){
        // Compare against the previous 15.5% adjustment from the same state:
        // another 10% means 1.155 * 1.10 = 1.2705, not an additive 25.5%.
        auto previousParameters=configured;previousParameters.accelerationScale=1.155f;
        auto previous=captured;stepOriginalVehicle(previous,inputs,previousParameters,math);
        const float expected=previous.drive.f(0x240)*1.10f;
        require(std::abs(tuned.drive.f(0x240)-expected)<=std::max(.000005f,std::abs(expected)*.0002f),
            "Odawara propulsion response is not another 10% above the previous 15.5% adjustment");
        require(tuned.drive.f(0x238)>stock.drive.f(0x238),"Odawara acceleration did not increase forward speed");
        ++positive;
    }
    if(checkUnpowered){
        // A warmed real state exercises coast/brake deceleration as well as
        // neutral and a race-owned throttle suppression gate.
        for(unsigned mode=0;mode<4;++mode){
            OriginalHostInputState host;
            const auto control=adaptOriginalHostInput(host,{0,mode>=2?1.f:0.f,mode==1?1.f:0.f,false,false},
                inputs.automaticMode,mode!=2,inputs.elapsedFrames0C900E84);
            auto a=captured,b=captured;
            if(mode==3){a.drive.setu(0x1A8,1);b.drive.setu(0x1A8,1);}
            stepOriginalVehicle(a,control,stockParameters,math);stepOriginalVehicle(b,control,configured,math);
            require(sameDrivingState(a,b),"Acceleration adjustment changed coast, brake, neutral or suppressed-throttle behavior");
            if(mode==2)require(a.transmission.gear00==0,"Neutral regression did not exercise disengaged driving");
            else require(a.controls.throttle==0,"Unpowered regression did not suppress throttle");
        }
    }
    return positive;
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("native_root imported_root");
    const auto course=ImportedCourse::load(argv[2]);int surfaces=0,ticks=0;
    require(OriginalVehicleParameters{}.accelerationScale==1.f&&ImportedDrivingRoad{}.accelerationScale==1.f,
        "Original and unspecified roads must preserve their acceleration response");
    for(const auto& definition:importedCourseDefinitions)
        require(definition.accelerationScale==(definition.id==17?1.2705f:1.f),"Only Odawara should receive the 27.05% acceleration adjustment");
    require(course.id==15?!course.lamps.empty():course.lamps.size()==(course.id==17?105:course.id==16?3:course.id==10?16:46),"Imported authored lamps were not loaded");
    if(course.id==15){
        // Independent surveyed points on the road-facing guardrail mesh, not
        // samples of the generated collision/path files. The old ribbon was
        // up to 2.31 m inside one rail and 1.51 m behind the opposite rail.
        struct Rail {int point;float x,z;};
        for(const auto rail:std::array<Rail,5>{{{3570,648.11699f,434.47448f},
                {3580,651.98541f,450.65958f},{3584,655.51729f,457.83108f},
                {3565,639.27911f,423.18845f},{3584,643.95111f,462.64543f}}}){
            auto p=course.center[rail.point];float dx=rail.x-p[0],dz=rail.z-p[2],length=std::hypot(dx,dz);dx/=length;dz/=length;
            for(float offset:{-.10f,.10f}){
                OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
                q.setf(44,p[0]);q.setf(48,p[1]+1);q.setf(52,p[2]);
                q.setf(32,rail.x+dx*offset);q.setf(36,p[1]+1);q.setf(40,rail.z+dz*offset);
                OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
                require(queryOriginalCollisionSwept(course.collision,q,trace,scratch),"Missing contact at surveyed Tsubaki guardrail");
                if(((q.u(28)&0x8000u)!=0)!=(offset>0))throw std::runtime_error("Collision does not align with visible Tsubaki guardrail at "+std::to_string(rail.point)+" offset "+std::to_string(offset));
                if(offset>0)require(q.f(24)>.05f&&q.f(24)<.13f,"Tsubaki guardrail collision depth differs from visible beam");
            }
        }
        // Surveyed end of the inner beam. Contact must stop here, without a
        // diagonal wall extending down the open pavement beyond the rail.
        for(float along:{-.5f,.5f}){
            const float x=646.67273f+along*.383f,z=470.18747f+along*.924f;
            OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
            q.setf(44,x+.924f);q.setf(48,299);q.setf(52,z-.383f);
            q.setf(32,x-.0924f);q.setf(36,299);q.setf(40,z+.0383f);
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            require(queryOriginalCollisionSwept(course.collision,q,trace,scratch),"Missing road at rail endpoint");
            require(((q.u(28)&0x8000u)!=0)==(along<0),"Collision extends past visible rail endpoint or misses its tip");
        }
        // Put the actual AE86 footprint in the pavement which the old wall
        // crossed. Test both headings; endpoint-only lane traces cannot cover
        // the car's four wall probes or the source contact response.
        for(bool reverse:{false,true}){
            auto road=course.drivingRoad(reverse);OriginalDrivingSelection selection;
            selection.physics=makeOriginalFreshTimeAttackSelection(0,course.handlingCondition(reverse),OriginalWeather::Dry);
            selection.collisionVariant=unsigned(reverse);OriginalRacePoint position{649.928f,299.f,451.509f};
            OriginalCollisionQuery q;clearOriginalCollisionQuery(q);for(unsigned k=0;k<3;k++)q.setf(32+4*k,position[k]);
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            require(queryOriginalCollisionSurface(course.collision,q,trace,scratch),"Missing pavement beside surveyed rail");position[1]=q.f(16);
            auto tangent=course.source.points[3581]-course.source.points[3579];if(reverse)tangent=tangent*-1.f;
            OriginalDrivingSession session;session.reset(argv[1],selection,position,{0,std::atan2(-tangent.x,-tangent.z),0},&road);session.enableRaceStart(2);
            OriginalHostInputState input;
            for(int tick=0;tick<60;tick++){
                const auto effects=session.tick(adaptOriginalHostInput(input,{0,0,0,false,false},true,true,tick));
                require(!effects.invalidScalarDiagnostics&&effects.newImpactRecords.empty()&&session.vehicle().drive.u(0x150)==0,"AE86 hits invisible wall before visible Tsubaki rail");
            }
        }
        std::cout<<"PASS 12 surveyed guardrail clearance/contact/endpoint checks and 120 AE86 contact ticks\n";
    }
    // A circuit is checked over its whole lap, in each direction's own path
    // and collision; other courses over the timed route of their one path.
    const auto lap=[&](bool reverse){return course.laps?std::pair{0,int(course.centerFor(reverse).size())-1}:std::pair{course.checkpoints[0],course.checkpoints[4]};};
    const auto directionCollision=[&](bool reverse)->const OriginalCollisionData&{return course.laps&&reverse?*course.reverseCollision:course.collision;};
    const auto driveable=[&](const OriginalCollisionData& collision,OriginalRacePoint p){
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
        q.setf(32,p[0]);q.setf(36,p[1]+1);q.setf(40,p[2]);OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
        return queryOriginalCollisionSurface(collision,q,trace,scratch)&&!(q.u(28)&0x8000u)&&std::abs(q.f(16)-p[1])<3;
    };
    // Query the converted road along its full length with the actual D3 solver.
    int divided=0;
    for(bool reverse:{false,true}){
        if(reverse&&!course.laps)break;
        const auto& centre=course.centerFor(reverse);const auto [first,last]=lap(reverse);
        for(int i=first;i<=last;i++){
            OriginalCollisionQuery q;clearOriginalCollisionQuery(q);auto p=centre[i];
            if(course.laps&&!driveable(directionCollision(reverse),p)){
                // Odawara's centre path runs through the toll-booth island
                // (path 1778-1800); the road passes on both sides of it.
                for(const auto* edge:{&course.leftFor(reverse)[i],&course.rightFor(reverse)[i]}){
                    OriginalRacePoint lane{};for(unsigned k=0;k<3;++k)lane[k]=(p[k]+(*edge)[k])*.5f;
                    if(!driveable(directionCollision(reverse),lane))throw std::runtime_error("Missing road surface at point "+std::to_string(i));
                }
                ++divided;continue;
            }
            q.setf(32,p[0]);q.setf(36,p[1]+1);q.setf(40,p[2]);OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            if(!queryOriginalCollisionSurface(directionCollision(reverse),q,trace,scratch))throw std::runtime_error("Missing road surface at point "+std::to_string(i));
            require(std::abs(q.f(16)-p[1])<3,"Imported surface height mismatch");surfaces++;
        }
    }
    require(divided<=(course.id==17?60:0),"Imported centre path leaves the road");
    // Each Odawara direction's collision holds only its own road through the
    // corner before the line, so the other direction's path must leave it.
    if(course.reverseRoute)for(bool reverse:{false,true}){
        const auto offRoad=[&](bool path){int count=0;for(const auto& p:course.centerFor(path))count+=!driveable(directionCollision(reverse),p);return count;};
        require(offRoad(!reverse)>offRoad(reverse)+10,"Direction collision does not separate the two roads");
    }
    // Locate every driveable triangle without a cached cell, as after a spawn,
    // reset or failed query. Overlapping source cells must not select a seed
    // whose walk stops at a border before reaching the road. Courses with
    // direction-specific collision check both files.
    int coldLookups=0,tickSweeps=0;
    for(bool reverse:{false,true}){
        if(reverse&&!course.reverseCollision)continue;
        const auto& collision=reverse?*course.reverseCollision:course.collision;
        for(const auto& triangle:collision.triangles){
            if(triangle[7]<0)continue;
            float centre[3]{};
            for(int k=0;k<3;++k)for(int axis=0;axis<3;++axis)centre[axis]+=std::bit_cast<float>(collision.vertices.at(triangle[k])[axis])/3;
            OriginalCollisionQuery q;clearOriginalCollisionQuery(q);q.setf(32,centre[0]);q.setf(36,centre[1]+1);q.setf(40,centre[2]);
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            if(!queryOriginalCollisionSurface(collision,q,trace,scratch))throw std::runtime_error("Cold lookup missed collision triangle "+std::to_string(coldLookups));
            require(std::abs(q.f(16)-centre[1])<3,"Cold lookup found the wrong collision level");++coldLookups;
        }
        // Step just past each edge at per-tick distances: D3 ticks at 60 Hz in
        // metres, so 1.5 units is ~324 km/h. From a start on driveable
        // collision, the solver must report a wall or further surface.
        for(int i=lap(reverse).first+1;i<lap(reverse).second-1;++i)for(int side=0;side<2;++side)for(float outside:{.25f,.5f,1.f,1.5f}){
            const auto& edge=side?course.leftFor(reverse):course.rightFor(reverse);
            OriginalRacePoint wall{},center{};
            for(unsigned k=0;k<3;++k){wall[k]=edge[i][k]*.63f+edge[i+1][k]*.37f;center[k]=course.centerFor(reverse)[i][k]*.63f+course.centerFor(reverse)[i+1][k]*.37f;}
            float dx=wall[0]-center[0],dz=wall[2]-center[2],length=std::sqrt(dx*dx+dz*dz);dx/=length;dz/=length;
            OriginalCollisionQuery start;clearOriginalCollisionQuery(start);
            start.setf(32,wall[0]-dx);start.setf(36,wall[1]+1);start.setf(40,wall[2]-dz);
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            if(!queryOriginalCollisionSurface(collision,start,trace,scratch)||(start.u(28)&0x8000))continue;
            OriginalCollisionQuery q=start;
            q.setf(44,wall[0]-dx);q.setf(48,wall[1]+1);q.setf(52,wall[2]-dz);
            q.setf(32,wall[0]+dx*outside);q.setf(36,wall[1]+1);q.setf(40,wall[2]+dz*outside);
            if(!queryOriginalCollisionSwept(collision,q,trace,scratch))throw std::runtime_error("Per-tick edge step lost collision at point "+std::to_string(i)+" side "+std::to_string(side));
            ++tickSweeps;
        }
    }
    std::cout<<"PASS "<<coldLookups<<" cold collision lookups\n";
    std::cout<<"PASS "<<tickSweeps<<" per-tick edge steps\n";
    // Follow a car along three lanes of the whole route with the race-path
    // projection the app uses. An isolated twisted cell is recovered by the
    // 20-segment local search next tick, but losing the car for longer
    // freezes progress, so no checkpoint or goal can trigger.
    int projections=0;
    for(bool reverse:{false,true}){
        const auto path=course.racePath(reverse);const auto rule=course.rules(reverse);const auto route=course.routeCheckpoints(reverse);
        const auto& centre=course.centerFor(reverse);const int period=int(centre.size())-1;
        for(float lane:{-.8f,0.f,.8f}){
            OriginalPathCoordinate coordinate{rule.startIndex,0},previous=coordinate,progress{0,0};int lastFound=route[0];
            for(int index=route[0];index<route[4];++index)for(float fraction:{0.f,.25f,.5f,.75f}){
                // A circuit's route index runs on past the period each lap.
                const int source=reverse?period-(course.laps?index%period:index):course.laps?index%period:index,next=reverse?source-1:source+1;
                OriginalRacePoint p{};
                for(unsigned k=0;k<3;++k){
                    const float c=centre[source][k]+(centre[next][k]-centre[source][k])*fraction;
                    const auto& edge=lane<0?course.leftFor(reverse):course.rightFor(reverse);
                    const float e=edge[source][k]+(edge[next][k]-edge[source][k])*fraction;
                    p[k]=c+(e-c)*std::abs(lane);
                }
                if(path.project(p,coordinate))lastFound=index;
                else if(index-lastFound>10)throw std::runtime_error("Race path lost the car after point "+std::to_string(lastFound)+(reverse?" reverse":" forward"));
                advanceOriginalRaceProgress(progress,previous,coordinate,path.period());previous=coordinate;++projections;
            }
            require(progress.index>=rule.goalIndex-1,"Race path progress did not reach the goal");
        }
    }
    std::cout<<"PASS "<<projections<<" race path projections over both directions\n";
    // Direction-specific collision exists to stop a car leaving the route
    // behind its start, as IDZero's per-direction R64top/R80btm class does.
    if(course.reverseCollision&&!course.laps&&!importedCourseDefinition(course.id).specialStage)for(bool reverse:{false,true}){
        const auto& collision=reverse?*course.reverseCollision:course.collision;
        const int start=reverse?int(course.center.size())-1-course.routeCheckpoints(true)[0]:course.checkpoints[0];
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);auto p=course.center[start];
        q.setf(32,p[0]);q.setf(36,p[1]+1);q.setf(40,p[2]);OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
        require(queryOriginalCollisionSurface(collision,q,trace,scratch),"Start is off imported collision");
        bool blocked=false;
        for(int i=start;!blocked&&(reverse?i+1<int(course.center.size()):i>1);i+=reverse?1:-1){
            const auto& from=course.center[i];const auto& to=course.center[reverse?i+1:i-1];
            q.setf(44,from[0]);q.setf(48,from[1]+1);q.setf(52,from[2]);q.setf(32,to[0]);q.setf(36,to[1]+1);q.setf(40,to[2]);
            blocked=queryOriginalCollisionSwept(collision,q,trace,scratch)&&(q.u(28)&0x8000);
        }
        require(blocked,"Reversing out of the start never met the direction barrier");
        std::cout<<"PASS "<<(reverse?"reverse":"forward")<<" start barrier\n";
    }
    // Sweep past the finite imported shoulders, including the first Sadamine
    // hairpin. Checking only the road centre never exercised these escapes.
    // Gunsai and Odawara keep the original sweep policy, which locates the
    // destination first; these long jumps end off their meshes, so they use
    // the steps above.
    int wallSweeps=0;
    for(int i=course.checkpoints[0]+1;course.id<16&&i<course.checkpoints[4]-1;++i)for(int side=0;side<2;++side)for(float outside:{6.f,12.f}){
        const auto& edge=side?course.left:course.right;
        OriginalRacePoint wall{},center{};
        for(unsigned k=0;k<3;++k){wall[k]=edge[i][k]*.63f+edge[i+1][k]*.37f;center[k]=course.center[i][k]*.63f+course.center[i+1][k]*.37f;}
        float dx=wall[0]-center[0],dz=wall[2]-center[2],length=std::sqrt(dx*dx+dz*dz);dx/=length;dz/=length;
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
        q.setf(44,wall[0]-dx);q.setf(48,wall[1]+1);q.setf(52,wall[2]-dz);
        q.setf(32,wall[0]+dx*outside);q.setf(36,wall[1]+1);q.setf(40,wall[2]+dz*outside);
        OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
        if(!queryOriginalCollisionSwept(course.collision,q,trace,scratch)||!(q.u(28)&0x8000))throw std::runtime_error("Missed imported wall at point "+std::to_string(i)+" side "+std::to_string(side)+" outside "+std::to_string(outside)+" triangle "+std::to_string(q.u(60))+" flags "+std::to_string(q.u(28)));
        require(std::isfinite(q.f(24))&&std::abs(q.f(0)*q.f(0)+q.f(8)*q.f(8)-1)<.001f,"Invalid imported wall contact");++wallSweeps;
    }
    std::cout<<"PASS "<<wallSweeps<<" whole-route outward wall sweeps\n";
    // Following a lane within the drivable ribbon must not hit a neighboring
    // shoulder, including the lower bridge beneath another part of Tsubaki.
    int laneSweeps=0,falseHits=0;
    for(bool reverse:{false,true})for(int i=lap(reverse).first+1;i<lap(reverse).second-2;++i)for(float lane:{-.6f,0.f,.6f}){
        // The centre lane of Odawara's divided road is the island itself.
        if(course.laps&&lane==0&&!(driveable(directionCollision(reverse),course.centerFor(reverse)[i])&&driveable(directionCollision(reverse),course.centerFor(reverse)[i+1])))continue;
        const auto point=[&](int index){auto p=course.centerFor(reverse)[index];const auto& edge=lane<0?course.leftFor(reverse)[index]:course.rightFor(reverse)[index];
            for(unsigned k=0;k<3;++k)p[k]+=(edge[k]-p[k])*std::abs(lane);p[1]+=1;return p;};
        auto a=point(i),b=point(i+1);if(reverse)std::swap(a,b);
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);for(unsigned k=0;k<3;++k){q.setf(44+k*4,a[k]);q.setf(32+k*4,b[k]);}
        OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
        if(!queryOriginalCollisionSwept(directionCollision(reverse),q,trace,scratch)||(q.u(28)&0x8000u)){
            if(falseHits<20)std::cerr<<"False lane contact at "<<i<<" lane "<<lane<<" reverse "<<reverse<<" triangle "<<q.u(60)<<" flags "<<q.u(28)<<'\n';++falseHits;
        }++laneSweeps;
    }
    require(falseHits==0,"Interior lane sweeps hit a false wall");std::cout<<"PASS "<<laneSweeps<<" full-route interior lane sweeps\n";
    for(bool reverse:{false,true})for(float lane:{-.6f,0.f,.6f}){
        if(course.laps&&lane==0)continue;
        OriginalCollisionQuery q;clearOriginalCollisionQuery(q);const auto& centre=course.centerFor(reverse);
        int first=reverse?lap(reverse).second-1:lap(reverse).first+1;
        int last=reverse?lap(reverse).first+1:lap(reverse).second-1,step=reverse?-1:1;
        for(int i=first;i!=last;i+=step){
            for(unsigned k=0;k<3;++k){const auto& edge=lane<0?course.leftFor(reverse):course.rightFor(reverse);
                q.setf(44+k*4,centre[i][k]+(edge[i][k]-centre[i][k])*std::abs(lane)+(k==1?1:0));
                q.setf(32+k*4,centre[i+step][k]+(edge[i+step][k]-centre[i+step][k])*std::abs(lane)+(k==1?1:0));}
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch scratch;
            if(!queryOriginalCollisionSwept(directionCollision(reverse),q,trace,scratch)||(q.u(28)&0x8000u))throw std::runtime_error("Cached lane contact at "+std::to_string(i)+" reverse "+std::to_string(reverse));
        }
    }
    std::cout<<"PASS cached lane traversal in both directions\n";
    for(bool reverse:{false,true}){
        const auto spawn=course.spawn(reverse);auto road=course.drivingRoad(reverse);auto projection=course.racePath(reverse);
        OriginalPathCoordinate coordinate{course.rules(reverse).startIndex,0};require(projection.project(spawn.position,coordinate,true),"Start is outside race path");
        // The coaching screen's whole-route page and four section pages.
        const auto maps=course.analysisMaps({},reverse);require(maps.size()==5,"Imported coaching maps missing");
        for(const auto& page:maps)require(page.road.size()>20,"Imported coaching map has no road");
        OriginalRaceRules rules;course.resetRules(rules,reverse,spawn.position);rules.start();
        // Test all source gates, including fourth split, through existing rules.
        int extensions=0,sections=0,laps=0;
        for(int index=rules.rules().startIndex;index<=rules.rules().startIndex+rules.rules().goalIndex;index++){
            auto e=rules.tick({course.laps?index%rules.pathPeriod():index,0},course.routePoint(reverse,index));extensions+=e.timeExtension;sections+=e.section;laps+=e.lap;
            if(course.laps&&index<rules.rules().goalIndex)require(rules.state().phase==OriginalRacePhase::Running,"Circuit finished before required laps");
        }
        if(course.laps)require(laps==course.laps-1,"Circuit lap boundary count incorrect");
        require(rules.state().phase==OriginalRacePhase::Finished&&extensions==3&&sections==4,"Original rules did not finish imported race/checkpoints");
        for(bool wet:{false,true})for(bool automatic:{false,true}){
            OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(0,course.handlingCondition(reverse),wet?OriginalWeather::Wet:OriginalWeather::Dry);selection.collisionVariant=unsigned(reverse);
            OriginalDrivingSession session;session.reset(argv[1],selection,spawn.position,spawn.angles,&road);session.enableRaceStart(2);OriginalHostInputState input;
            require(session.vehicle().drive.u(0x434)==unsigned(wet),"D3 wet handling flag not applied");
            require(road.accelerationScale==(course.id==17?1.2705f:1.f)&&session.parameters().accelerationScale==road.accelerationScale,
                "Imported road acceleration adjustment did not reach the driving session");
            unsigned accelerationChecks=0;
            float maximumSpeed=0,maximumRpm=0;unsigned maximumGear=0,contacts=0;
            for(int t=0;t<600;t++){
                const auto controls=adaptOriginalHostInput(input,{0,1,0,false,t==180||t==330},automatic,true,t);
                if(t%60==0)accelerationChecks+=checkAccelerationResponse(session.vehicle(),session.parameters(),controls,course.id==17,t==120);
                auto effects=session.tick(controls);ticks++;
                const auto& v=session.vehicle();maximumSpeed=std::max(maximumSpeed,v.drive.f(0x238));maximumRpm=std::max(maximumRpm,v.transmission.tach1c);maximumGear=std::max(maximumGear,v.transmission.gear00);
                require(!effects.invalidScalarDiagnostics&&std::isfinite(v.drive.f(4)),"Original vehicle failed on imported road");
                contacts+=effects.newImpactRecords.size();
            }
            std::cout<<"direction "<<reverse<<" wet "<<wet<<" automatic "<<automatic<<" speed "<<maximumSpeed<<" rpm "<<maximumRpm<<" gear "<<maximumGear<<'\n';
            require(maximumSpeed>4&&maximumRpm>2000&&maximumGear>1,"Imported race did not drive/shift with D3 vehicle");
            if(course.id==17)require(accelerationChecks>0,"Odawara test did not exercise positive acceleration");
            // Hold into the edge intentionally: the existing wall solver must
            // stop lateral escape rather than letting the car fall off the strip.
            for(int t=0;t<180;t++){
                auto e=session.tick(adaptOriginalHostInput(input,{.8f,1,0,false,false},automatic,true,t));ticks++;contacts+=e.newImpactRecords.size();
                const auto& v=session.vehicle().drive;auto p=course.sourceFor(reverse).project({v.f(0),v.f(4),v.f(8)});
                if(std::abs(v.f(4)-p.sample.center.y)>=5)throw std::runtime_error("D3 car height mismatch at boundary: tick="+std::to_string(t)+" position="+std::to_string(v.f(0))+","+std::to_string(v.f(4))+","+std::to_string(v.f(8))+" roadHeight="+std::to_string(p.sample.center.y)+" segment="+std::to_string(p.sample.segmentIndex));
            }
            require(contacts>0,"Imported boundary did not invoke D3 wall contact");
        }
    }
    std::cout<<"PASS "<<surfaces<<" road queries; both directions/four sections/three extensions; "<<ticks<<" D3 solver ticks with automatic and manual gearbox.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
