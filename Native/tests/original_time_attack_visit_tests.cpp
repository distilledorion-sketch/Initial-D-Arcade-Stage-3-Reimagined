#include "original_time_attack_visit.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace idas3;
using namespace idas3::original;
namespace {
unsigned checks=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
void picture(const std::filesystem::path& path,const OriginalTimeAttackVisit& visit,int w=640,int h=480){
    std::vector<std::uint32_t> pixels(std::size_t(w)*h,0xff000000);visit.paint(pixels,w,h);
    require(std::count_if(pixels.begin(),pixels.end(),[](auto c){return c!=0xff000000;})>20000,"Scene must contain original artwork");
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(auto c:pixels){const char bytes[]{char(c>>16),char(c>>8),char(c)};out.write(bytes,3);}
    require(bool(out),"Write isolated visual fixture");
}
}
int main(int argc,char** argv){try{
    require(argc==3,"Arguments: native root, isolated output folder");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    OriginalTimeAttackVisit visit;require(OriginalTimeAttackVisit::available(root),"Extracted original visit assets available");visit.load(root);
    require(originalTimeAttackStartIntervalText(0)=="--","Zero start interval remains unavailable dashes");
    require(originalTimeAttackStartIntervalText(4321)=="0.720"&&originalTimeAttackStartIntervalText(5999)=="0.999"&&originalTimeAttackStartIntervalText(6000)=="1.000","Start interval preserves source6000Hz millisecond truncation");
    require(originalTimeAttackCountdownDraws(799).size()==1&&originalTimeAttackCountdownDraws(800).size()==2,"Lecture timer uses original80ticks per displayedunit");
    require(originalTimeAttackCountdownDraws(1279)[0].chunk==6&&originalTimeAttackCountdownDraws(1279)[1].chunk==2,"Original lecture countdown starts at15");
    OriginalTimeAttackVisit::Setup setup;setup.condition=6;setup.car=0;setup.recordFlags=0x78000000;setup.ticks6000=987654;
    setup.courseRankingQualified=true;
    setup.nameGlyphs={67,72,82,73,83};setup.manual=true;
    TimeAttackRecords records;TimeAttackEntry first{6,0,0,987654};first.nameGlyphs=setup.nameGlyphs;first.manual=true;records.record(first);
    records.record({7,0,5,400000});records.record({6,1,4,500000});records.record({6,0,3,1050000});
    setup.localRecords=records.entries();
    for(unsigned frame=0;frame<9600;++frame){
        const float speed=40.f+100.f*(float(frame%2400)/2400.f);
        setup.trace.recordFrame(speed,frame*.25f,frame%2400<2000?1.f:0.f,frame%2400>=2000?.8f:0.f,frame%2400==1800,frame*100);
    }
    const auto count=setup.trace.samples().size();setup.trace.recordFrame(std::numeric_limits<float>::quiet_NaN(),0,0,0,false,1000000);
    setup.trace.recordFrame(10,0,0,0,false,1);require(setup.trace.samples().size()==count,"Reject non-finite and backwards trace frames");
    visit.beginLecture(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Lecture&&visit.fadeArgb()==0xff000000,"Lecture begins original15 fade");
    auto events=visit.takeEvents();require(events.size()==2&&events[0].command==OriginalLegendReturnCommand::SoundSet&&events[0].a==1&&
        events[1].command==OriginalLegendReturnCommand::MusicRequest&&events[1].a==2,"Source lecture sound set1 music2");
    for(unsigned i=0;i<15;++i)visit.advance({true});require(visit.phase()==0,"Confirm does not skip initial fade");
    visit.advance({});require(visit.phase()==1&&visit.fadeArgb()==0,"Lecture enters wait after16 fade steps");
    picture(out/"lecture.ppm",visit);picture(out/"lecture-wide.ppm",visit,1280,720);
    visit.advance({false,false,false,false,true});require(visit.mapIndex()==1&&visit.phase()==1,"View change opens next original course map");
    picture(out/"lecture-section.ppm",visit);
    for(unsigned i=0;i<3;++i)visit.advance({false,false,false,false,true});require(visit.mapIndex()==0,"Course map cycling wraps at recovered bank extent");
    visit.advance({true});require(visit.phase()==2,"Confirm starts source fade out");
    for(unsigned i=0;i<16;++i)visit.advance({});require(visit.phase()==3&&visit.fadeArgb()==0xff000000,"Source fade out16 steps");
    for(unsigned i=0;i<3;++i)visit.advance({});require(visit.phase()==4,"Source three-update double-increment settle");
    visit.advance({});require(visit.finished()&&visit.route()==OriginalTimeAttackVisit::Route::Points,"Lecture routes to separate common points owner");
    const auto frame=visit.frame();visit.advance({true});require(visit.frame()==frame,"Finished owner is inert");
    setup.suppressLecture=true;visit.beginLecture(setup);require(visit.route()==OriginalTimeAttackVisit::Route::Points&&visit.takeEvents().empty(),"Suppression flag bypasses lecture and audio");setup.suppressLecture=false;
    OriginalTimeAttackAnalysisInput analysisInput;analysisInput.course=3;analysisInput.car=0;analysisInput.finishTicks6000=setup.ticks6000;
    analysisInput.acceleratorFraction=.8f;analysisInput.brakeFraction=.03f;analysisInput.maxSteeringDelta=.2f;
    analysisInput.previousBestTicks6000=1000000;
    analysisInput.currentSections6000={260004,250002,245004,232644};
    analysisInput.previousSections6000={261000,249996,249000,240004};
    setup.analysisInput=analysisInput;
    std::uint32_t seed=1234;setup.analysis=analyzeOriginalTimeAttack(analysisInput,seed);setup.sourceAnalysisAvailable=true;
    setup.telemetry.valid=true;setup.telemetry.maxSpeedKph=147;setup.telemetry.acceleratorFraction=.8f;setup.telemetry.brakeFraction=.03f;
    setup.telemetry.wallCount=4;setup.telemetry.convertedEventCount=3;
    setup.telemetry.ditchCount=2;setup.telemetry.startIntervalTicks6000=4321;
    visit.beginLecture(setup);require(visit.countdownTicks()==setup.analysis.countdownTicks,"Use source classifier countdown");
    const auto storedSeed=seed;for(unsigned i=0;i<16;++i)visit.advance({});picture(out/"lecture-advice.ppm",visit);picture(out/"lecture-advice-wide.ppm",visit,1280,720);
    require(seed==storedSeed&&visit.setup().analysis.lineAddresses==setup.analysis.lineAddresses,"Drawing preserves prepared source advice and RNG");
    setup.analysis.kind=2;setup.analysis.countdownTicks=1679;visit.beginLecture(setup);require(visit.countdownTicks()==1679,"Successful analysis kind2 gets recovered longer timer");
    visit.beginAfterResults(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Ranking,"Qualifying finish enters real local ranking");
    require(visit.rankingRows().size()==2,"Rows use actual matching course/direction/weather records only");
    require(visit.rankingRows()[0].word(0)==987654&&visit.rankingRows()[0].car()==0&&visit.rankingRows()[0].bytes[4]==67&&visit.rankingRows()[0].bytes[8]==83,"Stored original name and car encoding retained");
    require(visit.rankingRows()[0].word(12)==1&&visit.rankingRows()[1].bytes[4]==221,"Actual transmission and unknown legacy names retained");
    visit.advance({true});require(visit.stage()==OriginalTimeAttackVisit::Stage::Ranking,"Ranking reveal cannot be skipped immediately");
    for(unsigned i=0;i<100;++i)visit.advance({});picture(out/"ranking.ppm",visit);
    visit.advance({true});require(visit.stage()==OriginalTimeAttackVisit::Stage::Ranking&&visit.phase()==2,"Ranking confirm starts original exit fade");
    for(unsigned i=0;i<32;++i)visit.advance({});require(visit.stage()==OriginalTimeAttackVisit::Stage::Ranking,"Ranking exit holds30 fade ticks plus settled black");
    visit.advance({});require(visit.stage()==OriginalTimeAttackVisit::Stage::Continue&&visit.countdownTicks()==879,"Ranking goes to continue after source fade/dwell, timer879");
    picture(out/"continue.ppm",visit);
    visit.advance({false,false,true});require(visit.selectedIndex()==1,"Continue option changes");
    events=visit.takeEvents();require(events.size()==1&&events[0].command==OriginalLegendReturnCommand::Cue&&events[0].a==2,"Continue selection original cue2");
    visit.advance({true,true});require(visit.selectedIndex()==0&&visit.phase()==1,"Accept selected continue");
    for(unsigned i=0;i<40;++i)visit.advance({});require(visit.active(),"Continue confirmation retains40 complete frames");
    visit.advance({});require(visit.route()==OriginalTimeAttackVisit::Route::Retry,"Accepted continuation returns course selection");
    setup.resultStatus=2;visit.beginAfterResults(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Time-up does not register or show successful ranking");
    for(unsigned i=0;i<879;++i)visit.advance({});require(visit.phase()==1&&visit.selectedIndex()==1,"Continue expires to No");
    for(unsigned i=0;i<41;++i)visit.advance({});require(visit.route()==OriginalTimeAttackVisit::Route::Exit,"Expired continuation exits");
    setup.continuationEnabled=false;visit.beginAfterResults(setup);require(visit.route()==OriginalTimeAttackVisit::Route::Exit,"Disabled cabinet continuation goes to exit");
    setup.continuationEnabled=true;setup.resultStatus=0;setup.courseRankingQualified=false;
    visit.beginAfterResults(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Personal/model flags alone cannot enter course ranking");
    setup.recordFlags=0;visit.beginAfterResults(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Nonrecord finish does not manufacture qualifying flags");
    setup.courseRankingQualified=true;visit.beginAfterResults(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Ranking,"Non-best top-ten insertion still receives ranking page");
    require(visit.countdownTicks()==900,"ARankinTA exact900-frame expiry");
    for(unsigned i=0;i<29;++i)visit.advance({true});require(visit.phase()==0,"Original entry fade gates confirmation for29 ticks");
    visit.advance({true});require(visit.phase()==2,"Original entry fade permits confirm on30th tick");
    visit.beginAfterResults(setup);for(unsigned i=0;i<899;++i)visit.advance({});require(visit.phase()==1&&visit.countdownTicks()==1,"Ranking holds through899th owner tick");
    visit.advance({});require(visit.phase()==2&&visit.countdownTicks()==0,"900th tick starts exit fade");
    for(unsigned i=0;i<33;++i)visit.advance({});require(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Expired ranking exits through original fade/settle");
    setup.resultStatus=2;visit.beginAfterResults(setup);require(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Top-ten flag cannot qualify time-up");
    setup.courseRankingQualified=false;setup.resultStatus=0;
    setup.freePlay=false;setup.canContinue=false;visit.beginAfterResults(setup);visit.advance({true});for(unsigned i=0;i<41;++i)visit.advance({});require(visit.route()==OriginalTimeAttackVisit::Route::Exit,"No granted continuation cannot accept");
    const auto file=out/"local-records.csv";require(records.save(file),"Save named local rankings");TimeAttackRecords loaded;require(loaded.load(file),"Read named local rankings");
    require(loaded.entries().size()==4,"Record row count round trips");
    const auto own=std::find_if(loaded.entries().begin(),loaded.entries().end(),[](const auto& e){return e.ticks6000==987654;});
    require(own!=loaded.entries().end()&&own->nameGlyphs==first.nameGlyphs&&own->manual&&!own->night,"Persist original name and transmission metadata");
    const auto legacy=out/"legacy.csv";{std::ofstream f(legacy);f<<"condition,weather,car,finish_ticks6000\n6,0,0,999999\n";}
    require(loaded.load(legacy)&&loaded.entries()[0].nameGlyphs[0]==221,"Legacy CSV remains readable and unnamed");
    require(loaded.save(legacy),"Legacy saves migrate atomically");
    {std::ofstream f(file);f<<"condition,weather,car,finish_ticks6000,name0,name1,name2,name3,name4,manual,night\n6,0,0,987654,999,0,0,0,0,0,0\n";}
    require(!loaded.load(file)&&loaded.entries()[0].ticks6000==999999,"Bad glyph rejected without discarding live records");
    // A bank can contain five artwork pages while telemetry has four slots.
    // Exercise real rendering (including valid telemetry) through every page,
    // both directions, repeated wraps, and a fresh visit after each course.
    const unsigned pageCounts[]{2,4,4,4,4,4,5,5,4};
    std::vector<std::uint32_t> pixels(640*480);
    for(unsigned condition=0;condition<18;++condition){
        auto cycle=setup;cycle.condition=condition;cycle.resultStatus=0;
        cycle.telemetry.valid=true;cycle.sourceAnalysisAvailable=false;
        visit.beginLecture(cycle);for(unsigned i=0;i<16;++i)visit.advance({});
        const auto pages=pageCounts[condition/2];
        std::cout<<"Cycling condition "<<condition<<", "<<pages<<" pages"<<std::endl;
        for(unsigned step=0;step<pages*3;++step){
            require(visit.mapIndex()==step%pages,"All original artwork pages remain selectable across repeated wraps");
            visit.paint(pixels,640,480);
            require(visit.phase()==1,"Map cycling preserves active analysis");
            visit.advance({false,false,false,false,true});
        }
        require(visit.mapIndex()==0,"Repeated map cycles return to first page");
        visit.advance({true});for(unsigned i=0;i<20;++i)visit.advance({});
        require(visit.finished()&&visit.route()==OriginalTimeAttackVisit::Route::Points,"Cycling all maps still permits normal results exit");
    }
    std::cout<<"PASS "<<checks<<" bounded Time Attack visit checks; original artwork previews written. No guest execution.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
