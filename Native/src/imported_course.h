#pragma once
#include "course.h"
#include "original_driving_session.h"
#include "original_race_path.h"
#include "original_start_grid.h"
#include "original_time_attack_visit.h"
#include <fstream>

namespace idas3 {
// Imported course data only: the ordinary App remains the race owner.
struct ImportedCourse {
    // Hakone uses Myogi (0/1); Sadamine uses Usui (2/3); Enna uses Akina
    // (6/7), including wet tables. Enna's current scenery pack is night-only.
    // Imported geometry and presentation remain independent of vehicle handling.
    unsigned handlingCondition(bool reverse)const{return (id==11?6u:id==10?2u:0u)+unsigned(reverse);}
    static unsigned courseId(const std::filesystem::path& root){
        std::ifstream f(root/"course.id");unsigned value=9;
        if(f&&(!(f>>value)||(value!=9&&value!=10&&value!=11)))throw std::runtime_error("Invalid imported course identity");
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
    original::OriginalCollisionData collision;
    std::optional<std::array<int,5>> reverseCheckpoints;
    std::optional<original::OriginalCollisionData> reverseCollision;
    // Indices in the direction's own path, including its approach/runout.
    std::array<int,5> routeCheckpoints(bool reverse)const{
        if(!reverse)return checkpoints;
        if(reverseCheckpoints)return *reverseCheckpoints;
        std::array<int,5> result{};
        for(unsigned i=0;i<5;++i)result[i]=int(center.size())-1-checkpoints[4-i];
        return result;
    }
    std::array<int,4> raceTimes(bool reverse)const{
        if(id!=11)return times;
        // Special Stage's time-attack mode has no countdown (001669a0).
        // Experimental D3 race uses Akina's normal-difficulty timer policy;
        // this is an adaptation, not recovered PS2 time-attack allowances.
        const auto condition=handlingCondition(reverse);
        const auto& bonus=original::originalTimeAttackBonusSeconds(condition);
        return {original::originalTimeAttackInitialSeconds(condition,2),bonus[0],bonus[1],bonus[2]};
    }
    static ImportedCourse load(const std::filesystem::path& root){
        ImportedCourse c;c.root=root;c.id=courseId(root);if(c.id==10){c.slug="sadamine";c.name="SADAMINE";}else if(c.id==11){c.slug="enna";c.name="ENNA SKYLINE";}c.source=Course::load(root,c.slug,c.name);
        for(auto p:c.source.points)c.center.push_back({p.x,p.y,p.z});
        // RacePath uses the source edge winding, while Course canonicalizes it.
        for(auto p:c.source.right)c.left.push_back({p.x,p.y,p.z});
        for(auto p:c.source.left)c.right.push_back({p.x,p.y,p.z});
        if(c.id==11){
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
            // These are original RCL1 meshes; retain the original solver's
            // wall-search policy, unlike the generated Stage 8 wall strips.
            c.times=c.raceTimes(false);return c;
        }
        std::ifstream f(root/"race.bin",std::ios::binary);char magic[4]{};f.read(magic,4);
        f.read(reinterpret_cast<char*>(c.checkpoints.data()),20);f.read(reinterpret_cast<char*>(c.times.data()),16);
        if(!f||std::string(magic,4)!="HKD3")throw std::runtime_error("Imported race metadata missing");
        for(unsigned i=0;i<5;i++)if(c.checkpoints[i]<1||c.checkpoints[i]>=int(c.center.size())-1||(i&&c.checkpoints[i]<=c.checkpoints[i-1]))throw std::runtime_error("Invalid imported checkpoint order");
        std::ifstream lights(root/"lamps.bin",std::ios::binary);std::uint32_t count=0;
        lights.read(magic,4);lights.read(reinterpret_cast<char*>(&count),4);
        if(!lights||std::string(magic,4)!="HKL1"||count>1024)throw std::runtime_error("Invalid imported lamp data");
        for(unsigned i=0;i<count;++i){
            std::array<float,3> p{};lights.read(reinterpret_cast<char*>(p.data()),12);
            if(!lights||!std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v);}))throw std::runtime_error("Invalid imported lamp position");
            c.lamps.push_back({p[0],p[1],p[2]});
        }
        if(lights.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing imported lamp data");
        c.collision=original::OriginalCollisionData::load(root/(c.slug+".rcl"));c.collision.importedSweepFromPrevious=true;return c;
    }
    // Feed the existing D3 coaching screen route-specific geometry. The first
    // page shows the entire timed route; four subsequent pages fit each section.
    std::vector<original::OriginalTimeAttackVisit::MapPage> analysisMaps(const original::OriginalTimeAttackTelemetrySnapshot& trace,bool reverse)const{
        using Point=std::array<float,2>;using Line=original::OriginalTimeAttackDrivingLine;
        std::vector<original::OriginalTimeAttackVisit::MapPage> pages;
        const auto route=routeCheckpoints(reverse);
        const auto sourceIndex=[&](int i){return reverse?int(center.size())-1-i:i;};
        for(unsigned page=0;page<5;++page){
            const int routeFirst=page?route[page-1]:route[0],routeLast=page?route[page]:route[4];
            int first=sourceIndex(routeFirst),last=sourceIndex(routeLast);if(first>last)std::swap(first,last);
            float x0=INFINITY,x1=-INFINITY,z0=INFINITY,z1=-INFINITY;
            for(int i=first;i<=last;++i){const auto p=center[i];x0=std::min(x0,p[0]);x1=std::max(x1,p[0]);z0=std::min(z0,-p[2]);z1=std::max(z1,-p[2]);}
            const float scale=224.f/std::max({x1-x0,z1-z0,1.f}),mx=(x0+x1)*.5f,mz=(z0+z1)*.5f;
            const auto project=[&](const std::array<float,3>& p){return Point{315+(p[0]-mx)*scale,315+(-p[2]-mz)*scale};};
            const auto inside=[](Point p){return p[0]>=195&&p[0]<=435&&p[1]>=195&&p[1]<=435;};
            auto& out=pages.emplace_back();
            const auto segment=[&](std::vector<Line>& dest,Point a,Point b,std::uint32_t color){
                // Liang-Barsky clipping keeps traces and section zooms inside
                // the source map frame, including fast corner crossings.
                float lo=0,hi=1;const float dx=b[0]-a[0],dy=b[1]-a[1];
                const auto clip=[&](float p,float q){if(p==0)return q>=0;float r=q/p;if(p<0)lo=std::max(lo,r);else hi=std::min(hi,r);return lo<=hi;};
                if(clip(-dx,a[0]-195)&&clip(dx,435-a[0])&&clip(-dy,a[1]-195)&&clip(dy,435-a[1]))dest.push_back({{a[0]+lo*dx,a[1]+lo*dy},{a[0]+hi*dx,a[1]+hi*dy},color});
            };
            for(int i=first+1;i<=last;++i)segment(out.road,project(center[i-1]),project(center[i]),0xffc4cbd0);
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
        r.sectionIndices[3]=r.goalIndex;return r;
    }
    original::OriginalStartGridPose spawn(bool reverse)const{
        const auto start=routeCheckpoints(reverse)[0];int i=reverse?int(center.size())-1-start:start;auto p=source.points[i],d=source.points[i+(reverse?-1:1)]-p;
        original::OriginalStartGridPose out;out.position={p.x,p.y,p.z};out.angles={0,std::atan2(-d.x,-d.z),0};return out;
    }
    original::OriginalStartGridPose onlineSpawn(bool reverse,unsigned slot)const{
        auto out=spawn(reverse);const auto start=routeCheckpoints(reverse)[0];const int index=reverse?int(center.size())-1-start:start;
        const auto& edge=(slot==unsigned(reverse))?left[index]:right[index];
        const float dx=edge[0]-out.position[0],dz=edge[2]-out.position[2];
        const float width=std::sqrt(dx*dx+dz*dz);
        if(width<2)throw std::runtime_error("Hakone start grid is too narrow");
        const float fraction=std::min(1.5f,width*.45f)/width;
        for(unsigned axis=0;axis<3;++axis)out.position[axis]+=(edge[axis]-out.position[axis])*fraction;
        return out;
    }
    original::ImportedDrivingRoad drivingRoad(bool reverse)const{
        original::ImportedDrivingRoad road;road.path.points=center;if(reverse)std::reverse(road.path.points.begin(),road.path.points.end());
        road.path.inclusiveLastIndex=unsigned(center.size())-1;road.collision=reverse&&reverseCollision?*reverseCollision:collision;return road;
    }
    original::OriginalRacePath racePath(bool reverse)const{return {center,left,right,reverse};}
    void resetRules(original::OriginalRaceRules& owner,bool reverse,original::OriginalRacePoint position)const{
        const auto t=raceTimes(reverse);owner.resetImported(center,left,right,reverse,rules(reverse),t[0],{t[1],t[2],t[3],0,0,0},position);
    }
};
}
