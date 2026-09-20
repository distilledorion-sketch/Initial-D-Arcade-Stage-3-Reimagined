#include "original_time_attack_telemetry.h"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3::original;
namespace {
unsigned checks=0;
void check(bool value,const char* what){++checks;if(!value)throw std::runtime_error(what);}
bool near(float a,float b,float epsilon=0.001f){return std::abs(a-b)<epsilon;}
OriginalImpactRecord impact(float magnitude,std::uint32_t index=1630){return {{-130.f,17.f,-285.f},index,magnitude};}
}
int main(){try{
    OriginalDriveState drive;OriginalTailState tail;OriginalContactCompletionState completion;
    OriginalTimeAttackTelemetry collector;
    drive.setf(0x3E4,167.25f);drive.setu(0x3F0,940);drive.setu(0x3F4,4);
    drive.setu(0x3F8,2);drive.setu(0x3FC,3);drive.setu(0x400,1);drive.setf(0x408,0.123f);
    tail.statistics0C91FB0C[1]=std::bit_cast<std::uint32_t>(0.625f);
    tail.statistics0C91FB0C[2]=std::bit_cast<std::uint32_t>(0.025f);
    tail.statistics0C91FB0C[4]=3; // source publication precedes latest collision
    const std::array impacts{impact(0.f),impact(0.01f),impact(0.03f),impact(0.4f)};
    collector.record(impacts);
    completion.positionCursor=completion.frameCursor=2;
    for(unsigned i=0;i<2;++i){completion.impactPositions[i]={std::bit_cast<std::uint32_t>(-130.f),0,std::bit_cast<std::uint32_t>(-285.f)};completion.impactFrames[i]=1630+i;}
    auto result=collector.snapshot(1,drive,tail,completion);
    check(result.valid&&near(result.maxSpeedKph,167.25f),"source max speed");
    check(near(result.acceleratorFraction,0.625f)&&near(result.brakeFraction,0.025f),"source amplitude means");
    check(result.averagedFrames==940&&near(result.maxSteeringDelta,0.123f),"source mean gate and steering statistic");
    check(result.wallCount==4&&result.publishedStatistics0C91FB0C[4]==3,"finish-frame counters without mutating published record");
    check(result.ditchCount==2&&result.ditchWallCount==2&&result.maxGearUsed==1,"source ditch cap and top-gear flag");
    check(result.convertedEventCount==1&&result.wallEvents.size()==4,"strict threshold and oversized nonincrement branch");
    check(near(result.fullMapWallMarkers[0].scale,0.58f),"source impact scale");
    check(result.ditchEvents.size()==2&&result.ditchEvents[1].progressIndex==1631,"original retained ditch positions");
    check(result.sectionWallMarkers[0].size()==1&&result.sectionDitchMarkers[0].size()==2,"source map bounds and progress membership");
    check(result.sectionWallMarkers[1].empty()&&result.sectionWallMarkers[2].empty(),"events do not bleed into other section maps");
    auto position=originalTimeAttackFullMapPosition(1,{0,888,0});
    check(near(position[0],315)&&near(position[1],291),"full map source centre excludes height");
    position=originalTimeAttackFullMapPosition(1,{650,0,650});
    check(near(position[0],443)&&near(position[1],419),"full map exact authored extent");
    position=originalTimeAttackFullMapPosition(3,{982.5f,0,-1178.5f});
    check(near(position[0],315)&&near(position[1],291),"Akina original reversed map orientation");
    position=originalTimeAttackSectionMapPosition(1,0,{-130,11,-285});
    check(near(position[0],0)&&near(position[1],0),"Usui authored section centre");
    position=originalTimeAttackSectionMapPosition(1,0,{-72.5f,11,-285});
    check(near(position[0],0.5f),"Usui authored section width");
    for(unsigned course=0;course<9;++course){
        check(originalTimeAttackImpactThreshold(course)==0.01f,"all source course thresholds");
        for(unsigned direction=0;direction<2;++direction){
            collector.reset();const auto start=originalTimeAttackStartIndex(course,direction);
            const auto progress=collector.recordProgress(course,direction,start,5000,12000);
            check(progress==(direction?5000-start+(course==1?1400:0):start),"source direction-normalized index");
            collector.recordProgress(course,direction,start,5000,20000);
            check(collector.snapshot(course,drive,tail,completion).startIntervalTicks6000==12000,"start interval first exact index only");
        }
    }
    collector.reset();
    check(originalTimeAttackRecordingLength(0,0)==3204&&originalTimeAttackRecordingLength(1,0)==2800,"recording length spans all circuit laps");
    check(originalTimeAttackRecordingLength(2,0)==2900&&originalTimeAttackRecordingLength(2,1)==2895,"source directional recording lengths");
    check(collector.recordProgress(1,1,1600,12000)==2600,"190D00 uses accumulated multi-lap progress");
    collector.recordProgress(1,0,originalTimeAttackStartIndex(1,0)+1,5000,30000);
    check(collector.snapshot(1,drive,tail,completion).startIntervalTicks6000==0,"source skipped start index stays blank");
    std::vector<OriginalImpactRecord> many(130,impact(0.04f));
    collector.record(many,1630);drive.setu(0x3F4,130);
    check(collector.snapshot(1,drive,tail,completion).wallEvents.size()==99,"original wall append cap");
    collector.record(many);
    check(collector.snapshot(1,drive,tail,completion).wallEvents.size()==99,"wall append remains bounded");
    collector.reset();collector.recordDitches(completion,1635);collector.recordDitches(completion,1640);
    result=collector.snapshot(1,drive,tail,completion);
    check(result.ditchEvents.size()==2&&result.ditchEvents[0].progressIndex==1635,"capture ditch cursor exactly once and replace host frame alias");
    tail.statistics0C91FB0C[1]=0x7fc00000u;
    check(std::isnan(collector.snapshot(1,drive,tail,completion).acceleratorFraction),"preserve source zero-count NaN for caller validity handling");
    OriginalRaceTimeRecord times;times.lapCount=3;times.lapTimes={10000,25000,42000,0};
    times.finishTime=62000;times.sectionCount=3;times.sectionTimes={2000,3000,4000,0};
    OriginalTimeAttackAnalysisInput analysis;
    populateOriginalTimeAttackAnalysisTiming(2,times,0,60000,{9000,23000,40000},analysis);
    check(analysis.currentSections6000==std::array<std::uint32_t,4>{10000,15000,17000,20000},"07CC60 current lap arrays, not checkpoint arrays");
    check(analysis.previousSections6000==std::array<std::uint32_t,4>{9000,14000,17000,20000},"07CC60 previous checkpoint deltas");
    populateOriginalTimeAttackAnalysisTiming(1,times,0,50000,{7000,21000,41000},analysis);
    check(analysis.previousSections6000==std::array<std::uint32_t,4>{21000,29000,0,0},"Usui saved split1 override");
    check(analysis.currentSections6000==std::array<std::uint32_t,4>{10000,52000,0,0},"Usui sole current lap split");
    populateOriginalTimeAttackAnalysisTiming(0,times,2,50000,{10000,0,0},analysis);
    check(analysis.finishTicks6000==0&&analysis.currentSections6000[2]==0,"timeout clears final current time");
    check(analysis.previousSections6000[2]==0,"source absent last saved split clears final previous duration");
    times.lapCount=1;
    populateOriginalTimeAttackAnalysisTiming(2,times,2,0,{},analysis);
    check(analysis.currentSections6000==std::array<std::uint32_t,4>{10000,0,0,0},"unfinished laps remain zero");
    check(analysis.previousSections6000==std::array<std::uint32_t,4>{},"no previous personal record");
    collector.reset();
    collector.recordDrivingPoint({-1000,0,0},1,1,0);
    collector.recordDrivingPoint({-100,0,0},2,1,0);
    collector.recordDrivingPoint({100,0,0},3,0,1);
    collector.recordDrivingPoint({1000,0,0},4,0,0);
    auto path=collector.snapshot(1,drive,tail,completion);
    check(path.drivingPath.size()==4,"Driving recorder retains ordered samples");
    auto lines=originalTimeAttackDrivingLines(1,0,path);
    check(lines.size()==3&&lines[0].color==0xff00aaff&&lines[1].color==0xffff0000&&lines[2].color==0xff00ff00,"Accel brake coast line colors");
    for(const auto& line:lines)for(const auto& point:{line.from,line.to})for(float axis:point)check(axis>=193.39f&&axis<=436.61f,"Map lines clipped to source extent");
    check(originalTimeAttackDrivingLines(1,1,path).empty(),"Zoomed maps reject other progress ranges");
    for(unsigned i=0;i<10000;++i)collector.recordDrivingPoint({1000,0,0},4,0,0);
    check(collector.snapshot(1,drive,tail,completion).drivingPath.size()==4,"Idle frames do not grow driving path");
    collector.recordDrivingPoint({NAN,0,0},5,1,0);
    check(collector.snapshot(1,drive,tail,completion).drivingPath.size()==4,"Reject non-finite line samples");
    collector.reset();check(collector.snapshot(1,drive,tail,completion).drivingPath.empty(),"Next race clears previous path");
    bool rejected=false;try{originalTimeAttackFullMapPosition(9,{});}catch(const std::out_of_range&){rejected=true;}
    check(rejected,"invalid course rejected");
    rejected=false;try{originalTimeAttackSectionMapPosition(0,3,{});}catch(const std::out_of_range&){rejected=true;}
    check(rejected,"invalid map rejected");
    std::cout<<"PASS "<<checks<<" original Time Attack telemetry checks\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
