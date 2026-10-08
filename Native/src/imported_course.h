#pragma once
#include "course.h"
#include "imported_course_catalog.h"
#include "original_battle_metrics.h"
#include "original_driving_session.h"
#include "original_race_path.h"
#include "original_start_grid.h"
#include "original_time_attack_visit.h"
#include <fstream>

namespace idas3 {
// Imported course data only: the ordinary App remains the race owner.
struct ImportedCourse {
    // Preserve each donor's direction and dry/wet tuning while retaining the
    // imported road, surface materials, collision meshes and race identity.
    unsigned handlingCondition(bool reverse)const{return importedCourseDefinition(id).handlingCondition+unsigned(reverse);}
    static unsigned courseId(const std::filesystem::path& root){
        std::ifstream f(root/"course.id");unsigned value=9;
        if(f&&(!(f>>value)||!isImportedCourseId(int(value))))throw std::runtime_error("Invalid imported course identity");
        return value;
    }
    unsigned id=9;
    std::string slug="hakone",name="HAKONE";
    std::filesystem::path root;
    Course source;
    std::vector<original::OriginalRacePoint> center,left,right;
    std::vector<Vec3> lamps;
    std::array<int,5> checkpoints{};
    std::array<int,4> times{};
    std::array<std::array<float,4>,2> timerScale{{{1,1,1,1},{1,1,1,1}}};
    original::OriginalCollisionData collision;
    std::optional<std::array<int,5>> reverseCheckpoints;
    std::optional<original::OriginalCollisionData> reverseCollision;
    // A circuit's lap count; 0 on a point-to-point course. Its path repeats
    // the first point at the end, so the lap period is the point count less 1.
    int laps=0;
    // Odawara's directions take different roads through one corner, so the
    // clockwise race has its own source-ordered path, driven backwards.
    struct Route {Course source;std::vector<original::OriginalRacePoint> center,left,right;};
    std::optional<Route> reverseRoute;
    const Course& sourceFor(bool reverse)const{return reverse&&reverseRoute?reverseRoute->source:source;}
    const std::vector<original::OriginalRacePoint>& centerFor(bool reverse)const{return reverse&&reverseRoute?reverseRoute->center:center;}
    const std::vector<original::OriginalRacePoint>& leftFor(bool reverse)const{return reverse&&reverseRoute?reverseRoute->left:left;}
    const std::vector<original::OriginalRacePoint>& rightFor(bool reverse)const{return reverse&&reverseRoute?reverseRoute->right:right;}
    // The presentation course in race direction, under the course's own id.
    Course course(bool reverse)const{
        auto result=Course::load(root,reverse&&reverseRoute?slug+"_reverse":slug,name,reverse);result.id=slug;return result;
    }
    // Source point of a route index, which exceeds the period after a lap.
    const original::OriginalRacePoint& routePoint(bool reverse,int index)const{
        const auto& points=centerFor(reverse);const int period=int(points.size())-1;
        if(laps)index%=period;
        return points[reverse?period-index:index];
    }
    // Indices in the direction's own path, including its approach/runout.
    // A circuit starts on its line and times each quarter of the distance.
    std::array<int,5> routeCheckpoints(bool reverse)const{
        if(laps){
            std::array<int,5> result{};const int period=int(centerFor(reverse).size())-1;
            for(int i=0;i<5;++i)result[i]=i*laps*period/4;
            return result;
        }
        if(!reverse)return checkpoints;
        if(reverseCheckpoints)return *reverseCheckpoints;
        std::array<int,5> result{};
        for(unsigned i=0;i<5;++i)result[i]=int(center.size())-1-checkpoints[4-i];
        return result;
    }
    std::array<int,4> raceTimes(bool reverse)const{
        if(!importedCourseDefinition(id).specialStage)return times;
        // Special Stage's time-attack mode has no countdown (001669a0).
        // D3 normal-difficulty allowances are an adaptation. New longer routes
        // scale each section up against its donor, never reduce its allowance.
        const auto condition=handlingCondition(reverse);
        const auto& bonus=original::originalTimeAttackBonusSeconds(condition);
        std::array<int,4> result{original::originalTimeAttackInitialSeconds(condition,2),bonus[0],bonus[1],bonus[2]};
        for(unsigned i=0;i<4;++i)result[i]=int(std::ceil(result[i]*timerScale[unsigned(reverse)][i]));
        return result;
    }
    static ImportedCourse load(const std::filesystem::path& root){
        ImportedCourse c;c.root=root;c.id=courseId(root);const auto& definition=importedCourseDefinition(c.id);
        c.slug=definition.slug;c.name=definition.name;c.source=Course::load(root,c.slug,c.name);
        const auto fill=[](const Course& source,std::vector<original::OriginalRacePoint>& center,
            std::vector<original::OriginalRacePoint>& left,std::vector<original::OriginalRacePoint>& right){
            for(auto p:source.points)center.push_back({p.x,p.y,p.z});
            // RacePath uses the source edge winding, while Course canonicalizes it.
            for(auto p:source.right)left.push_back({p.x,p.y,p.z});
            for(auto p:source.left)right.push_back({p.x,p.y,p.z});
        };
        fill(c.source,c.center,c.left,c.right);
        if(std::filesystem::exists(root/(c.slug+"_reverse_path.bin"))){
            c.reverseRoute.emplace();c.reverseRoute->source=Course::load(root,c.slug+"_reverse",c.name);
            fill(c.reverseRoute->source,c.reverseRoute->center,c.reverseRoute->left,c.reverseRoute->right);
        }
        if(definition.specialStage){
            std::ifstream f(root/"race-markers.bin",std::ios::binary);char magic[4]{};f.read(magic,4);
            c.reverseCheckpoints.emplace();
            f.read(reinterpret_cast<char*>(c.checkpoints.data()),20);
            f.read(reinterpret_cast<char*>(c.reverseCheckpoints->data()),20);
            if(!f||std::string(magic,4)!="ENR1"||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid Enna race markers");
            for(bool reverse:{false,true}){
                const auto points=c.routeCheckpoints(reverse);
                for(unsigned i=0;i<5;++i)if(points[i]<1||points[i]>=int(c.center.size())-1||(i&&points[i]<=points[i-1]))throw std::runtime_error("Invalid Enna checkpoint order");
            }
            c.collision=original::OriginalCollisionData::load(root/"collision-0.rcl");
            c.reverseCollision=original::OriginalCollisionData::load(root/"collision-1.rcl");
            if(c.id>=12){
                std::ifstream timing(root/"timer-scale.bin",std::ios::binary);timing.read(magic,4);
                timing.read(reinterpret_cast<char*>(c.timerScale.data()),32);
                if(!timing||std::string(magic,4)!="TSF1"||timing.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid imported timer scale");
                for(const auto& direction:c.timerScale)for(float scale:direction)
                    if(!std::isfinite(scale)||scale<1||scale>4)throw std::runtime_error("Invalid imported section allowance");
            }
            // These are original RCL1 meshes; retain the original solver's
            // wall-search policy, unlike the generated Stage 8 wall strips.
            c.times=c.raceTimes(false);return c;
        }
        std::ifstream f(root/"race.bin",std::ios::binary);char magic[4]{};f.read(magic,4);
        if(std::string(magic,4)=="HKC1"){
            // Circuit: lap count and section allowances; gates follow the lap.
            f.read(reinterpret_cast<char*>(&c.laps),4);f.read(reinterpret_cast<char*>(c.times.data()),16);
            if(!f||c.laps<1||c.laps>4)throw std::runtime_error("Invalid imported lap count");
            for(bool reverse:{false,true})if(!c.sourceFor(reverse).closed||c.centerFor(reverse).size()<65)throw std::runtime_error("Imported circuit path is not a closed lap");
            c.checkpoints=c.routeCheckpoints(false);
        }else{
            f.read(reinterpret_cast<char*>(c.checkpoints.data()),20);f.read(reinterpret_cast<char*>(c.times.data()),16);
            if(!f||std::string(magic,4)!="HKD3"||c.reverseRoute)throw std::runtime_error("Imported race metadata missing");
            for(unsigned i=0;i<5;i++)if(c.checkpoints[i]<1||c.checkpoints[i]>=int(c.center.size())-1||(i&&c.checkpoints[i]<=c.checkpoints[i-1]))throw std::runtime_error("Invalid imported checkpoint order");
        }
        std::ifstream lights(root/"lamps.bin",std::ios::binary);std::uint32_t count=0;
        lights.read(magic,4);lights.read(reinterpret_cast<char*>(&count),4);
        if(!lights||std::string(magic,4)!="HKL1"||count>1024)throw std::runtime_error("Invalid imported lamp data");
        for(unsigned i=0;i<count;++i){
            std::array<float,3> p{};lights.read(reinterpret_cast<char*>(p.data()),12);
            if(!lights||!std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v);}))throw std::runtime_error("Invalid imported lamp position");
            c.lamps.push_back({p[0],p[1],p[2]});
        }
        if(lights.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing imported lamp data");
        c.collision=original::OriginalCollisionData::load(root/(c.slug+".rcl"));c.collision.importedSweepFromPrevious=c.id<16;
        // Gunsai's source blocks a different strip per direction (R64top
        // forward, R80btm backward), so its converter writes both files.
        // Odawara's source has a collision mesh per direction.
        if(const auto reverse=root/(c.slug+"-reverse.rcl");std::filesystem::exists(reverse)){
            c.reverseCollision=original::OriginalCollisionData::load(reverse);
            c.reverseCollision->importedSweepFromPrevious=c.collision.importedSweepFromPrevious;
        }
        return c;
    }
    // Feed the existing D3 coaching screen route-specific geometry. The first
    // page shows the entire timed route; four subsequent pages fit each section.
    std::vector<original::OriginalTimeAttackVisit::MapPage> analysisMaps(const original::OriginalTimeAttackTelemetrySnapshot& trace,bool reverse)const{
        using Point=std::array<float,2>;using Line=original::OriginalTimeAttackDrivingLine;
        std::vector<original::OriginalTimeAttackVisit::MapPage> pages;
        const auto route=routeCheckpoints(reverse);
        const int period=int(centerFor(reverse).size())-1;
        // Tsubaki's course card uses world +Z down the page. Keep its full
        // route, section routes and recorded events in that same orientation.
        const float mapZ=id==15?1.f:-1.f,mapX=id>=16?mapZ:1.f;
        for(unsigned page=0;page<5;++page){
            const int routeFirst=page?route[page-1]:route[0],routeLast=page?route[page]:route[4];
            // One lap already shows a circuit's whole route.
            const int first=routeFirst,last=laps?std::min(routeLast,routeFirst+period):routeLast;
            float x0=INFINITY,x1=-INFINITY,z0=INFINITY,z1=-INFINITY;
            // IDZero cards use the HUD minimap orientation. Preserve the
            // established projection of the earlier imported courses.
            for(int i=first;i<=last;++i){const auto p=routePoint(reverse,i);x0=std::min(x0,mapX*p[0]);x1=std::max(x1,mapX*p[0]);z0=std::min(z0,mapZ*p[2]);z1=std::max(z1,mapZ*p[2]);}
            const float scale=224.f/std::max({x1-x0,z1-z0,1.f}),mx=(x0+x1)*.5f,mz=(z0+z1)*.5f;
            const auto project=[&](const std::array<float,3>& p){return Point{315+(mapX*p[0]-mx)*scale,315+(mapZ*p[2]-mz)*scale};};
            const auto inside=[](Point p){return p[0]>=195&&p[0]<=435&&p[1]>=195&&p[1]<=435;};
            auto& out=pages.emplace_back();
            const auto segment=[&](std::vector<Line>& dest,Point a,Point b,std::uint32_t color){
                // Liang-Barsky clipping keeps traces and section zooms inside
                // the source map frame, including fast corner crossings.
                float lo=0,hi=1;const float dx=b[0]-a[0],dy=b[1]-a[1];
                const auto clip=[&](float p,float q){if(p==0)return q>=0;float r=q/p;if(p<0)lo=std::max(lo,r);else hi=std::min(hi,r);return lo<=hi;};
                if(clip(-dx,a[0]-195)&&clip(dx,435-a[0])&&clip(-dy,a[1]-195)&&clip(dy,435-a[1]))dest.push_back({{a[0]+lo*dx,a[1]+lo*dy},{a[0]+hi*dx,a[1]+hi*dy},color});
            };
            for(int i=first+1;i<=last;++i)segment(out.road,project(routePoint(reverse,i-1)),project(routePoint(reverse,i)),0xffc4cbd0);
            const auto inSection=[&](std::uint32_t progress){if(!page)return true;return progress>=unsigned(routeFirst-route[0])&&progress<=unsigned(routeLast-route[0]);};
            for(std::size_t i=1;i<trace.drivingPath.size();++i){const auto& a=trace.drivingPath[i-1];const auto& b=trace.drivingPath[i];if(inSection(a.progressIndex)||inSection(b.progressIndex))segment(out.driving,project(a.position),project(b.position),b.color);}
            for(const auto& event:trace.wallEvents){auto p=project(event.position);if(event.magnitude>original::originalTimeAttackImpactThreshold(3)&&inSection(event.tick)&&inside(p))out.walls.push_back(p);}
            for(const auto& event:trace.ditchEvents){auto p=project(event.position);if(inSection(event.progressIndex)&&inside(p))out.ditches.push_back(p);}
        }
        return pages;
    }
    original::OriginalRaceRuleRow rules(bool reverse)const{
        original::OriginalRaceRuleRow r;r.lapIndices.fill(-1);r.extensionIndices.fill(-1);r.sectionIndices.fill(-1);
        const auto points=routeCheckpoints(reverse);r.startIndex=points[0];r.goalIndex=points[4]-points[0];
        for(int i=0;i<3;i++)r.lapIndices[i]=r.sectionIndices[i]=r.extensionIndices[i]=points[i+1]-points[0];
        if(laps){
            r.lapIndices.fill(-1);
            for(int i=1;i<laps;++i)r.lapIndices[i-1]=i*(int(centerFor(reverse).size())-1);
        }
        r.sectionIndices[3]=r.goalIndex;return r;
    }
    original::OriginalStartGridPose spawn(bool reverse)const{
        const auto& points=sourceFor(reverse).points;
        const auto start=routeCheckpoints(reverse)[0];int i=reverse?int(points.size())-1-start:start;auto p=points[i],d=points[i+(reverse?-1:1)]-p;
        original::OriginalStartGridPose out;out.position={p.x,p.y,p.z};out.angles={0,std::atan2(-d.x,-d.z),0};return out;
    }
    original::OriginalStartGridPose onlineSpawn(bool reverse,unsigned slot)const{
        auto out=spawn(reverse);const auto start=routeCheckpoints(reverse)[0];const int index=reverse?int(centerFor(reverse).size())-1-start:start;
        const auto& edge=(slot==unsigned(reverse))?leftFor(reverse)[index]:rightFor(reverse)[index];
        const float dx=edge[0]-out.position[0],dz=edge[2]-out.position[2];
        const float width=std::sqrt(dx*dx+dz*dz);
        if(width<2)throw std::runtime_error("Hakone start grid is too narrow");
        const float fraction=std::min(1.5f,width*.45f)/width;
        for(unsigned axis=0;axis<3;++axis)out.position[axis]+=(edge[axis]-out.position[axis])*fraction;
        return out;
    }
    original::ImportedDrivingRoad drivingRoad(bool reverse)const{
        original::ImportedDrivingRoad road;road.path.points=centerFor(reverse);if(reverse)std::reverse(road.path.points.begin(),road.path.points.end());
        road.path.inclusiveLastIndex=unsigned(road.path.points.size())-1;road.collision=reverse&&reverseCollision?*reverseCollision:collision;
        road.accelerationScale=importedCourseDefinition(id).accelerationScale;return road;
    }
    original::OriginalRacePath racePath(bool reverse)const{return {centerFor(reverse),leftFor(reverse),rightFor(reverse),reverse};}
    // Battle distances over the timed route; a circuit's lap repeats.
    original::OriginalBattleMetrics battleMetrics(bool reverse)const{
        if(laps)return {centerFor(reverse),reverse};
        return {std::span(center).subspan(checkpoints[0],rules(reverse).goalIndex+1),reverse};
    }
    void resetRules(original::OriginalRaceRules& owner,bool reverse,original::OriginalRacePoint position)const{
        const auto t=raceTimes(reverse);owner.resetImported(centerFor(reverse),leftFor(reverse),rightFor(reverse),reverse,rules(reverse),t[0],{t[1],t[2],t[3],0,0,0},position);
    }
};
}
