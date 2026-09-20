#pragma once
#include "native_assets.h"
#include "original_ranking_board.h"
#include "original_rival_dialog_scene.h"
#include "original_legend_return.h"
#include "original_time_attack_analysis.h"
#include "original_time_attack_telemetry.h"
#include "time_attack_records.h"
#include "menu_font.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace idas3::original {
// The source start interval zero is unavailable, not a measured perfect start.
std::string originalTimeAttackStartIntervalText(std::uint32_t ticks6000);
struct OriginalTimeAttackCountdownDraw { unsigned chunk;float x,y; };
std::vector<OriginalTimeAttackCountdownDraw> originalTimeAttackCountdownDraws(std::uint32_t ticks);
// Host telemetry, sampled once per running 60 Hz simulation frame. This is
// measured data for the analysis graph, not synthesized source replay data.
struct OriginalTimeAttackDrivingSample {
    float speedKph=0,distanceMetres=0,throttle=0,brake=0;
    std::uint32_t ticks6000=0;
    bool wall=false;
};
class OriginalTimeAttackDrivingTrace {
public:
    void clear(){samples_.clear();}
    void recordFrame(float speedKph,float distanceMetres,float throttle,float brake,
        bool wall,std::uint32_t ticks6000);
    const std::vector<OriginalTimeAttackDrivingSample>& samples()const{return samples_;}
private:
    std::vector<OriginalTimeAttackDrivingSample> samples_;
};

// The A_VISIT_TA tail (07AB60), split at AResultTA so the existing common
// points/tuning owner remains responsible for its scene and profile changes.
// Main calls advance exactly once per original 60 Hz frame, after freezing
// the time / NEW RECORD summary. HLecture precedes common results.
class OriginalTimeAttackVisit {
public:
    enum class Stage : std::uint8_t { Inactive,Lecture,Ranking,Continue,Finished };
    enum class Route : std::uint8_t { None,Points,Retry,Exit };
    struct MapPage {
        std::vector<OriginalTimeAttackDrivingLine> road,driving;
        std::vector<std::array<float,2>> walls,ditches;
    };
    struct Setup {
        std::string customCourseName;
        std::vector<MapPage> customMaps;
        unsigned condition=0,weather=0,car=0; // condition = source course*2+direction
        std::uint32_t resultStatus=0,ticks6000=0,recordFlags=0;
        // Captured BEFORE insertion using source032000's strict comparison.
        // A top-ten run need not beat the course/model/personal best.
        bool courseRankingQualified=false;
        std::array<std::uint32_t,3> oldBestTimes6000{};
        std::array<std::uint8_t,5> nameGlyphs{221,221,221,221,221};
        bool manual=false,night=false;
        bool continuationEnabled=true,freePlay=true,canContinue=true;
        bool suppressLecture=false; // profile flag 0x00020000, 18ACE0/18D880
        // Actual native local records AFTER this run's normal persistence.
        // Legacy records have no name; those rows deliberately remain blank.
        std::vector<TimeAttackEntry> localRecords;
        OriginalTimeAttackDrivingTrace trace;
        // Prepared once by the race owner from source statistics and shared
        // RNG. Rendering never reruns the classifier or advances that seed.
        bool sourceAnalysisAvailable=false;
        OriginalTimeAttackAnalysis analysis{};
        OriginalTimeAttackAnalysisInput analysisInput{};
        OriginalTimeAttackTelemetrySnapshot telemetry{};
    };
    struct Input { bool confirm=false,previous=false,next=false,skip=false,nextMap=false; };
    static bool available(const std::filesystem::path& root);
    void load(const std::filesystem::path& root);
    bool loaded()const{return loaded_;}
    void beginLecture(const Setup&);
    void beginAfterResults(const Setup&);
    void advance(const Input&);
    void paint(std::span<std::uint32_t> target,int width,int height)const;
    Stage stage()const{return stage_;}
    Route route()const{return route_;}
    bool active()const{return stage_!=Stage::Inactive&&stage_!=Stage::Finished;}
    bool finished()const{return stage_==Stage::Finished;}
    std::uint32_t frame()const{return frame_;}
    std::uint32_t phase()const{return phase_;}
    std::uint32_t selectedIndex()const{return selected_;}
    std::uint32_t countdownTicks()const{return timerTicks_;}
    std::uint32_t mapIndex()const{return mapIndex_;}
    std::uint32_t fadeArgb()const;
    std::vector<OriginalLegendReturnEvent> takeEvents(){return std::move(events_);}
    const Setup& setup()const{return setup_;}
    const std::vector<OriginalRankingRecord>& rankingRows()const{return rows_;}
private:
    struct Bank { NativeModel model; NativeTextureBank textures; };
    Bank lecture_,ranking_,common_,select_;
    NativeTextureBank alphabet_;
    std::array<Bank,8> maps_;
    OriginalRivalDialogScene choices_;
    MenuFont font_;
    Setup setup_{};
    std::array<std::vector<OriginalTimeAttackDrivingLine>,4> drivingLines_;
    std::vector<OriginalRankingRecord> rows_;
    std::vector<bool> rowMetadata_;
    std::vector<OriginalLegendReturnEvent> events_;
    Stage stage_=Stage::Inactive;
    Route route_=Route::None;
    std::uint32_t frame_=0,phase_=0,selected_=0,timerTicks_=0;
    std::uint32_t mapIndex_=0;
    std::uint32_t backgroundFrame_=0,displayedBackgroundFrame_=0;
    std::int32_t fade_=0;
    std::uint32_t rankingFadeAlpha_=0,rankingSettle_=0;
    bool loaded_=false;
    void finish(Route);
    void beginContinue();
    void prepareRanking();
    void paintLecture(std::span<std::uint32_t>,int,int)const;
    void paintRanking(std::span<std::uint32_t>,int,int)const;
};
}
