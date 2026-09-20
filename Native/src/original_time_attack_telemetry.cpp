#include "original_time_attack_telemetry.h"
#include "original_time_attack_telemetry_data.h"
#include "original_matrix.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
using Point=std::array<float,3>;
const telemetry_data::Course& courseData(unsigned course){
    if(course>=telemetry_data::courses.size())throw std::out_of_range("Original HLecture course must be0..8");
    return telemetry_data::courses[course];
}
float f(std::uint32_t word){return std::bit_cast<float>(word);}
Point corner(const std::array<std::uint32_t,12>& corners,unsigned index){
    return {f(corners[index*3]),f(corners[index*3+1]),f(corners[index*3+2])};
}
float width(const std::array<std::uint32_t,12>& corners){
    const auto a=corner(corners,0),b=corner(corners,1);
    const auto x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];
    //1F6BA0 FIPR accumulates in double then rounds once before FSQRT.
    double squared=double(x)*double(x);
    squared+=double(y)*double(y);squared+=double(z)*double(z);squared+=0.0;
    return std::sqrt(static_cast<float>(squared));
}
Point normalized(unsigned course,const Point& flatPosition){
    const auto& data=courseData(course);
    OriginalMatrix matrix;
    for(unsigned i=0;i<16;++i)matrix.elements[i]=f(data.matrix[i]);
    auto value=transformOriginalPoint(matrix,flatPosition);
    const auto denominator=width(data.corners);
    for(auto& v:value)v/=denominator;
    return value;
}
bool inside(const Point& position){
    //18D38C..18D3C2 and18D5A6..18D5E2: strict bounds on both axes.
    constexpr float limit=std::bit_cast<float>(0x3ef33333u);
    return std::abs(position[0])<limit&&std::abs(position[1])<limit;
}
float impactScale(float magnitude,float threshold){
    auto scale=(magnitude-threshold)*f(0x3e4ccccdu);
    scale/=f(0x3d4ccccdu);
    return scale+0.5f;
}
bool inRange(std::uint32_t value,const std::array<std::uint32_t,2>& range){
    const auto signedValue=std::bit_cast<std::int32_t>(value);
    return signedValue>=std::bit_cast<std::int32_t>(range[0])&&signedValue<=std::bit_cast<std::int32_t>(range[1]);
}
}

float originalTimeAttackImpactThreshold(unsigned course){return f(courseData(course).threshold);}
std::uint32_t originalTimeAttackStartIndex(unsigned course,unsigned direction){
    if(direction>1)throw std::out_of_range("Original HLecture direction must be0..1");
    return courseData(course).start[direction];
}
std::uint32_t originalTimeAttackRecordingLength(unsigned course,unsigned direction){
    if(direction>1)throw std::out_of_range("Original HLecture direction must be0..1");
    return courseData(course).recordingLength[direction];
}
std::array<std::uint32_t,2> originalTimeAttackMapProgressRange(unsigned course,unsigned mapIndex){
    if(mapIndex>=3)throw std::out_of_range("Original HLecture section map must be0..2");
    const auto& data=courseData(course);
    return {data.ranges[mapIndex],data.ranges[mapIndex+3]};
}
Point originalTimeAttackFullMapPosition(unsigned course,const Point& worldPosition){
    auto position=normalized(course,{worldPosition[0],worldPosition[2],0.f});
    for(auto& value:position)value*=256.f;
    position[0]+=315.f;position[1]+=291.f;
    return position;
}
Point originalTimeAttackSectionMapPosition(unsigned course,unsigned mapIndex,const Point& worldPosition){
    if(mapIndex>=3)throw std::out_of_range("Original HLecture section map must be0..2");
    const auto& data=courseData(course);
    const auto& corners=data.sections[mapIndex];
    std::array<Point,4> transformed;
    for(unsigned i=0;i<4;++i)transformed[i]=normalized(course,corner(corners,i));
    Point center{};
    for(unsigned axis=0;axis<2;++axis)
        center[axis]=((transformed[0][axis]+transformed[1][axis])+transformed[2][axis]+transformed[3][axis])*0.25f;
    const auto ratio=width(data.corners)/width(corners);
    auto position=normalized(course,{worldPosition[0],worldPosition[2],0.f});
    for(unsigned axis=0;axis<2;++axis)position[axis]=(position[axis]-center[axis])*ratio;
    position[2]=0.f;
    return position;
}

void convertOriginalTimeAttackEvents(unsigned course,OriginalTimeAttackTelemetrySnapshot& snapshot){
    const auto threshold=originalTimeAttackImpactThreshold(course);
    snapshot.fullMapWallMarkers.clear();
    for(auto& markers:snapshot.sectionWallMarkers)markers.clear();
    for(auto& markers:snapshot.sectionDitchMarkers)markers.clear();
    //18D140 admits at most100 source indices; normal157A80 writer provides99.
    for(std::size_t i=0;i<std::min<std::size_t>({snapshot.wallEvents.size(),snapshot.wallCount,100});++i){
        const auto& event=snapshot.wallEvents[i];
        if(!(event.magnitude>threshold))continue;
        const float scale=impactScale(event.magnitude,threshold);
        // The source clamps an oversized scratch slot but does not increment
        // its count. Preserve that unusual branch;190F00 confirms the rule.
        if(scale>2.f)continue;
        snapshot.fullMapWallMarkers.push_back({originalTimeAttackFullMapPosition(course,event.position),scale,event.tick});
        for(unsigned map=0;map<3;++map){
            if(!inRange(event.tick,originalTimeAttackMapProgressRange(course,map)))continue;
            const auto position=originalTimeAttackSectionMapPosition(course,map,event.position);
            if(inside(position))snapshot.sectionWallMarkers[map].push_back({position,scale,event.tick});
        }
    }
    snapshot.convertedEventCount=static_cast<std::uint32_t>(snapshot.fullMapWallMarkers.size());
    for(const auto& event:snapshot.ditchEvents){
        for(unsigned map=0;map<3;++map){
            if(!inRange(event.progressIndex,originalTimeAttackMapProgressRange(course,map)))continue;
            const auto position=originalTimeAttackSectionMapPosition(course,map,event.position);
            if(inside(position))snapshot.sectionDitchMarkers[map].push_back({position,1.f,event.progressIndex});
        }
    }
}

std::vector<OriginalTimeAttackDrivingLine> originalTimeAttackDrivingLines(unsigned course,unsigned mapIndex,const OriginalTimeAttackTelemetrySnapshot& snapshot){
    courseData(course);if(mapIndex>3)throw std::out_of_range("Driving map must be0..3");
    std::vector<OriginalTimeAttackDrivingLine> out;
    for(std::size_t i=1;i<snapshot.drivingPath.size();++i){
        const auto& a=snapshot.drivingPath[i-1];const auto& b=snapshot.drivingPath[i];
        if(mapIndex){
            const auto range=originalTimeAttackMapProgressRange(course,mapIndex-1);
            if(std::max(a.progressIndex,b.progressIndex)<range[0]||std::min(a.progressIndex,b.progressIndex)>range[1])continue;
        }
        const auto point=[&](const auto& p){return mapIndex?originalTimeAttackSectionMapPosition(course,mapIndex-1,p):normalized(course,{p[0],p[2],0.f});};
        const auto start=point(a.position),end=point(b.position);
        const float dx=end[0]-start[0],dy=end[1]-start[1];float first=0,last=1;
        const auto clip=[&](float direction,float distance){
            if(direction==0)return distance>=0;
            const float t=distance/direction;
            if(direction<0){if(t>last)return false;first=std::max(first,t);}
            else {if(t<first)return false;last=std::min(last,t);}return true;
        };
        constexpr float edge=.475f;
        if(!clip(-dx,start[0]+edge)||!clip(dx,edge-start[0])||!clip(-dy,start[1]+edge)||!clip(dy,edge-start[1]))continue;
        if(dx*dx+dy*dy<1e-12f)continue;
        out.push_back({{315.f+256.f*(start[0]+first*dx),315.f+256.f*(start[1]+first*dy)},
                       {315.f+256.f*(start[0]+last*dx),315.f+256.f*(start[1]+last*dy)},b.color});
    }
    return out;
}
void OriginalTimeAttackTelemetry::recordDrivingPoint(const Point& position,std::uint32_t progressIndex,float throttle,float brake){
    if(!std::all_of(position.begin(),position.end(),[](float x){return std::isfinite(x);}))return;
    const std::uint32_t color=brake>.05f?0xffff0000u:throttle>.05f?0xff00aaffu:0xff00ff00u;
    if(!drivingPath_.empty()){
        const auto& previous=drivingPath_.back();const float dx=position[0]-previous.position[0],dz=position[2]-previous.position[2];
        if(dx*dx+dz*dz<1.f&&previous.color==color)return;
    }
    // At most 1.5 MB for a run; stationary frames do not consume samples.
    if(drivingPath_.size()<65536)drivingPath_.push_back({position,progressIndex,color});
}
void OriginalTimeAttackTelemetry::reset(){drivingPath_.clear();walls_.clear();ditches_.clear();ditchCursor_=0;startIntervalTicks6000_=0;}
std::uint32_t OriginalTimeAttackTelemetry::recordProgress(unsigned course,unsigned direction,std::uint32_t rawIndex,
    std::uint32_t recordingLength,std::uint32_t elapsedTicks6000){
    if(rawIndex==originalTimeAttackStartIndex(course,direction)&&startIntervalTicks6000_==0)
        startIntervalTicks6000_=elapsedTicks6000;
    if(direction==0)return rawIndex;
    return recordingLength-rawIndex+(course==1?1400u:0u);
}
std::uint32_t OriginalTimeAttackTelemetry::recordProgress(unsigned course,unsigned direction,std::uint32_t rawIndex,
    std::uint32_t elapsedTicks6000){
    return recordProgress(course,direction,rawIndex,originalTimeAttackRecordingLength(course,direction),elapsedTicks6000);
}
void OriginalTimeAttackTelemetry::record(std::span<const OriginalImpactRecord> impacts){
    const auto count=std::min<std::size_t>(impacts.size(),99-walls_.size());
    walls_.insert(walls_.end(),impacts.begin(),impacts.begin()+count);
}
void OriginalTimeAttackTelemetry::record(std::span<const OriginalImpactRecord> impacts,std::uint32_t originalProgressIndex){
    for(auto event:impacts){
        if(walls_.size()>=99)break;
        event.tick=originalProgressIndex;
        walls_.push_back(event);
    }
}
void OriginalTimeAttackTelemetry::recordDitches(const OriginalContactCompletionState& completion,std::uint32_t originalProgressIndex){
    const auto count=std::min<std::uint32_t>({completion.positionCursor,completion.frameCursor,98});
    if(count<ditchCursor_){ditches_.clear();ditchCursor_=0;}
    for(;ditchCursor_<count;++ditchCursor_){
        const auto& bits=completion.impactPositions[ditchCursor_];
        ditches_.push_back({{f(bits[0]),f(bits[1]),f(bits[2])},originalProgressIndex});
    }
}
OriginalTimeAttackTelemetrySnapshot OriginalTimeAttackTelemetry::snapshot(unsigned course,const OriginalDriveState& drive,
    const OriginalTailState& tail,const OriginalContactCompletionState& completion,std::uint32_t startIntervalTicks6000,bool convertMaps) const{
    OriginalTimeAttackTelemetrySnapshot result;
    result.valid=true;
    result.publishedStatistics0C91FB0C=tail.statistics0C91FB0C;
    result.maxSpeedKph=drive.f(0x3E4);
    result.acceleratorFraction=f(tail.statistics0C91FB0C[1]);
    result.brakeFraction=f(tail.statistics0C91FB0C[2]);
    result.maxSteeringDelta=drive.f(0x408);
    result.averagedFrames=drive.u(0x3F0);
    // Read final drive counters without stepping physics: equivalent to the
    // next15ECE0 publication, including events on the finish-trigger frame.
    result.wallCount=drive.u(0x3F4);result.ditchCount=drive.u(0x3F8);
    result.ditchWallCount=std::bit_cast<std::int32_t>(drive.u(0x3FC))>std::bit_cast<std::int32_t>(drive.u(0x3F8))?drive.u(0x3F8):drive.u(0x3FC);
    result.maxGearUsed=drive.u(0x400);
    result.startIntervalTicks6000=startIntervalTicks6000?startIntervalTicks6000:startIntervalTicks6000_;
    result.drivingPath=drivingPath_;
    result.wallEvents=walls_;result.ditchEvents=ditches_;
    // Standalone callers that already supply original92DE30 can snapshot the
    // retained completion arrays directly without duplicating per-frame reads.
    const auto count=std::min<std::uint32_t>({completion.positionCursor,completion.frameCursor,98});
    for(std::size_t i=result.ditchEvents.size();i<count;++i){
        const auto& bits=completion.impactPositions[i];
        result.ditchEvents.push_back({{f(bits[0]),f(bits[1]),f(bits[2])},completion.impactFrames[i]});
    }
    if(convertMaps)convertOriginalTimeAttackEvents(course,result);
    return result;
}

void populateOriginalTimeAttackAnalysisTiming(unsigned course,const OriginalRaceTimeRecord& current,
    std::uint32_t resultStatus,std::uint32_t previousBestTicks6000,
    const std::array<std::uint32_t,3>& previousCumulativeSections6000,OriginalTimeAttackAnalysisInput& output){
    courseData(course);
    const unsigned sections=course==0?3u:(course==1?2u:4u);
    output.resultStatus=resultStatus;
    output.previousBestTicks6000=previousBestTicks6000;
    output.currentSections6000={};output.previousSections6000={};
    if(previousBestTicks6000!=0){
        std::uint32_t previous=0;
        for(unsigned i=0;i<sections-1;++i){
            const auto value=previousCumulativeSections6000[course==1?1:i];
            output.previousSections6000[i]=value-previous;
            previous=value;
        }
        output.previousSections6000[sections-1]=previous?previousBestTicks6000-previous:0;
    }
    std::uint32_t previous=0;
    for(unsigned i=0;i<sections-1;++i){
        if(i>=current.lapCount)continue;
        output.currentSections6000[i]=current.lapTimes[i]-previous;
        previous=current.lapTimes[i];
    }
    output.finishTicks6000=resultStatus>1?0:current.finishTime;
    output.currentSections6000[sections-1]=resultStatus>1?0:current.finishTime-previous;
}
}
