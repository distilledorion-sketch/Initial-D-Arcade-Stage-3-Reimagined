#pragma once
#include "original_contact_completion.h"
#include "original_road_contact.h"
#include "original_race_rules.h"
#include "original_time_attack_analysis.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace idas3::original {
struct OriginalTimeAttackDitchRecord {
    std::array<float,3> position{};
    std::uint32_t progressIndex=0;
};
struct OriginalTimeAttackMapMarker {
    // Source full-map pixels: (315,291) centre and256-pixel extent. Section
    // markers use centred source coordinates; multiply x/y by256 then add
    // (315,291) to place on their section map. No host track bounds fitting.
    std::array<float,3> position{};
    float scale=1;
    std::uint32_t progressIndex=0;
};
struct OriginalTimeAttackDrivingPoint {
    std::array<float,3> position{};
    std::uint32_t progressIndex=0,color=0xff00ff00;
};
struct OriginalTimeAttackDrivingLine {
    std::array<float,2> from{},to{};
    std::uint32_t color=0;
};
struct OriginalTimeAttackTelemetrySnapshot {
    bool valid=false;
    std::array<std::uint32_t,16> publishedStatistics0C91FB0C{};
    float maxSpeedKph=0,acceleratorFraction=0,brakeFraction=0,maxSteeringDelta=0;
    std::uint32_t averagedFrames=0,wallCount=0,ditchCount=0,ditchWallCount=0,maxGearUsed=0;
    // Source91FB50, populated by recorder190D80; zero means no interval.
    std::uint32_t startIntervalTicks6000=0;
    std::vector<OriginalTimeAttackDrivingPoint> drivingPath;
    std::vector<OriginalImpactRecord> wallEvents;
    std::vector<OriginalTimeAttackDitchRecord> ditchEvents;
    std::vector<OriginalTimeAttackMapMarker> fullMapWallMarkers;
    std::array<std::vector<OriginalTimeAttackMapMarker>,3> sectionWallMarkers,sectionDitchMarkers;
    std::uint32_t convertedEventCount=0;
};

// Original map transforms, clipped to the authored 0.475 map extent.
std::vector<OriginalTimeAttackDrivingLine> originalTimeAttackDrivingLines(unsigned course,unsigned mapIndex,const OriginalTimeAttackTelemetrySnapshot&);

// Collect returned157A80 impacts immediately after each original driving tick.
// The original append buffers hold99 wall records and98 ditch records; raw
// counters remain uncapped. The owner supplies original direction-normalized
// recording progress (190D00), not render time or elapsed platform frames.
class OriginalTimeAttackTelemetry {
public:
    void reset();
    std::uint32_t recordImportedProgress(std::uint32_t progress,std::uint32_t elapsed){if(progress>0&&startIntervalTicks6000_==0)startIntervalTicks6000_=elapsed;return progress;}
    void recordDrivingPoint(const std::array<float,3>& position,std::uint32_t progressIndex,float throttle,float brake);
    //190D00/190D80, caller060070: rawIndex is race+1460 accumulated progress,
    // NOT a projected path coordinate. recordingLength is191B20(row).
    std::uint32_t recordProgress(unsigned course,unsigned direction,std::uint32_t rawIndex,
        std::uint32_t recordingLength,std::uint32_t elapsedTicks6000);
    std::uint32_t recordProgress(unsigned course,unsigned direction,std::uint32_t rawIndex,
        std::uint32_t elapsedTicks6000);
    void record(std::span<const OriginalImpactRecord> impacts);
    void record(std::span<const OriginalImpactRecord> impacts,std::uint32_t originalProgressIndex);
    void recordDitches(const OriginalContactCompletionState&,std::uint32_t originalProgressIndex);
    OriginalTimeAttackTelemetrySnapshot snapshot(unsigned course,const OriginalDriveState&,
        const OriginalTailState&,const OriginalContactCompletionState&,
        std::uint32_t startIntervalTicks6000=0,bool convertMaps=true) const;
private:
    std::vector<OriginalTimeAttackDrivingPoint> drivingPath_;
    std::vector<OriginalImpactRecord> walls_;
    std::vector<OriginalTimeAttackDitchRecord> ditches_;
    std::uint32_t ditchCursor_=0;
    std::uint32_t startIntervalTicks6000_=0;
};

// Source1900A0/190460/190500 and190240. Course is profile+4 (0..8).
std::array<float,3> originalTimeAttackFullMapPosition(unsigned course,const std::array<float,3>& worldPosition);
std::array<float,3> originalTimeAttackSectionMapPosition(unsigned course,unsigned mapIndex,const std::array<float,3>& worldPosition);
//18D140/18D2C0/18D4E0: rebuild markers from actual recorded positions,
// original per-course impact threshold, direction-normalized progress ranges,
// and original map bounds. Does not infer collision events from control inputs.
void convertOriginalTimeAttackEvents(unsigned course,OriginalTimeAttackTelemetrySnapshot&);
float originalTimeAttackImpactThreshold(unsigned course);
std::array<std::uint32_t,2> originalTimeAttackMapProgressRange(unsigned course,unsigned mapIndex);
std::uint32_t originalTimeAttackStartIndex(unsigned course,unsigned direction);
std::uint32_t originalTimeAttackRecordingLength(unsigned course,unsigned direction);
//07CC60 expands cumulative lap and saved checkpoint values to the duration
// arrays consumed by18D0E0. Usui uses saved checkpoint1 for its sole split.
void populateOriginalTimeAttackAnalysisTiming(unsigned course,const OriginalRaceTimeRecord& current,
    std::uint32_t resultStatus,std::uint32_t previousBestTicks6000,
    const std::array<std::uint32_t,3>& previousCumulativeSections6000,
    OriginalTimeAttackAnalysisInput&);
}
