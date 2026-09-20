#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <map>

// Actual App race/result/input/save integration. This deliberately uses a new
// writable root, with only data/ shared through a junction. It never opens an
// audio device or a visible window. Race ticks are simulated at the original
// fixed rate without drawing every road frame; results use the actual WARP
// renderer. No timer, finish flag, result points or RNG is injected.
namespace {
using Profile=original::OriginalBattleProfile;
using Kind=original::OriginalTuningChildKind;
constexpr double tickSeconds=1.0/60.0;
void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
struct SavedFile {
    std::vector<char> bytes;
    fs::file_time_type written;
    bool operator==(const SavedFile&)const=default;
};
using Files=std::map<std::string,SavedFile>;
Files savedFiles(const App& app){
    Files result;const auto directory=app.root/"userdata";
    if(!fs::exists(directory))return result;
    for(const auto& entry:fs::recursive_directory_iterator(directory))if(entry.is_regular_file()){
        std::ifstream f(entry.path(),std::ios::binary);
        result.emplace(fs::relative(entry.path(),directory).generic_string(),
            SavedFile{{std::istreambuf_iterator<char>(f),{}},entry.last_write_time()});
    }
    return result;
}
void mixFrame(App& app){
    app.audio.scene(app.menu,app.race.phase==RacePhase::Finished,app.paused,app.race.timeUp);
    for(unsigned sample=0;sample<735;++sample)
        (void)app.audio.renderStereo(app.vehicle.rpm,app.vehicle.throttle,app.vehicle.speed,app.vehicle.slip,!app.menu&&!app.paused);
}
void render(App& app,double elapsed=tickSeconds){
    require(app.render(elapsed),"Actual App render failed: "+app.renderer.error);
    mixFrame(app);
}
void capture(App& app,const fs::path& output,const std::string& name){
    require(app.render(0),"Capture render failed: "+app.renderer.error);
    require(app.renderer.saveBitmap((output/(name+".bmp")).wstring()),"Cannot save application capture");
}
std::unique_ptr<App> makeApp(const fs::path& isolatedRoot){
    auto app=std::make_unique<App>();app->root=isolatedRoot;
    // validationMode intentionally remains false: these are real isolated
    // profile commits, not the application's validation no-save bypass.
    app->settings();
    app->originalCamera=OriginalChaseCamera::load(isolatedRoot);
    app->bumperCamera=OriginalChaseCamera::load(isolatedRoot,OriginalDrivingView::Bumper);
    app->frontend.initialize(isolatedRoot,true);app->hud.loadOriginal(isolatedRoot);
    app->audio.configure(isolatedRoot);app->audio.enabled=true;
    require(app->renderer.initialize(nullptr,640,480,true),"Cannot initialize offscreen WARP: "+app->renderer.error);
    return app;
}
void chooseLegend(App& app,unsigned car){
    app.frontend.car=int(car);app.loadSelectedProfile();
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    app.frontend.battleProfile.setu(0,0);
    // First original Myogi opponent is available on an untouched profile.
    original::selectOriginalRival(app.frontend.battleProfile,0);
    app.frontend.automatic=true;app.start();mixFrame(app);
    require(app.battle&&!app.bunta&&app.originalHandling,"Legend selection did not start original battle");
}
void chooseTimeAttack(App& app,unsigned car){
    app.frontend.car=int(car);app.loadSelectedProfile();
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app.frontend.course=app.courseIndex=3;app.frontend.reverse=app.reverse=false;
    app.frontend.wet=app.wet=false;app.frontend.night=app.night=false;
    app.frontend.automatic=app.automatic=true;app.start();mixFrame(app);
    require(!app.battle&&app.originalHandling,"Time Attack did not start original single-player race");
}
unsigned naturalTimeout(App& app){
    DriverInput neutral;neutral.automatic=true;
    unsigned frames=0;
    while(app.race.phase!=RacePhase::Finished&&frames<18000){app.simulate(neutral);mixFrame(app);++frames;}
    require(app.race.phase==RacePhase::Finished,"Stationary original race did not finish within300seconds");
    require(app.race.timeUp,"Stationary race ended by another rule; this fixture expects natural timeout");
    require(app.pendingResultSetup.has_value(),"Natural result did not queue original result owner");
    require(!app.resultVisit.initialized,"Result owner advanced before first result render");
    render(app,0);
    require(app.resultVisit.initialized&&!app.pendingResultSetup,"First render did not create result owner exactly once");
    return frames;
}
void frozenChecks(App& app,const fs::path& output,const std::string& name){
    require(app.resultVisit.initialized&&!app.menu,"Freeze check requires active result owner");
    const auto files=savedFiles(app);const auto profile=app.battleProfile.words;
    const auto frame=app.battleResults.frame60,phase=app.resultVisit.animation.phase;
    const auto childFrame=app.resultVisit.child.frame,timeout=app.battleProfile.u(1176);
    const auto preview=app.tuningPreview->nextState(),visible=app.tuningPreview->visibleState();
    const auto compare=[&]{
        require(app.battleProfile.words==profile&&app.battleResults.frame60==frame&&app.resultVisit.animation.phase==phase,
            "Pause/repaint changed result points/profile/owner clock");
        require(app.resultVisit.child.frame==childFrame&&app.battleProfile.u(1176)==timeout,"Pause/repaint advanced tuning child timeout");
        const auto& now=app.tuningPreview->nextState();const auto& shown=app.tuningPreview->visibleState();
        require(now.frames==preview.frames&&now.angle==preview.angle&&now.focus==preview.focus&&shown.frames==visible.frames&&shown.angle==visible.angle&&shown.focus==visible.focus,
            "Pause/repaint advanced result preview");
        require(savedFiles(app)==files,"Pause/repaint wrote a profile, backup or other userdata file");
    };
    capture(app,output,name+"-before-repaint");
    for(unsigned i=0;i<4;++i)render(app,0);
    compare();app.paused=true;
    for(unsigned i=0;i<31;++i)render(app);
    compare();app.paused=false;render(app,0);compare();
    capture(app,output,name+"-after-pause");
}
void finishVisit(App& app){
    unsigned frames=0;
    while(!app.menu&&frames<2500){render(app);++frames;}
    require(app.menu&&app.resultAnimationFrame.finished,"Original child command14/fade did not return to course selection");
    const auto saved=app.profiles.load(unsigned(app.frontend.car));
    require(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.words==app.frontend.battleProfile.words,
        "Completed result did not persist exact final profile");
    const auto files=savedFiles(app);
    for(unsigned i=0;i<3;++i)render(app,0);
    require(savedFiles(app)==files,"Completed result repaint saved/awarded again");
}
void waitForChild(App& app,Kind expected){
    require(app.resultVisit.tuning.kind==expected,"Wrong source tuning child selected");
    for(unsigned i=0;!app.resultAnimationFrame.showUpgradeChild&&i<600;++i)render(app);
    require(app.resultAnimationFrame.showUpgradeChild,"Result did not reveal original tuning child");
    require(app.resultVisit.child.frame==0,"Child advanced on the result-to-child transition draw");
}
void confirm(App& app,bool left){
    // Exercise App's existing keyboard bridge, not direct child commands.
    app.input.down.fill(false);app.input.pressed.fill(false);
    app.input.down[left?VK_LEFT:VK_RIGHT]=true;
    app.commands(tickSeconds);render(app);
    app.input.pressed[VK_RETURN]=true;
    app.commands(tickSeconds);render(app);
    app.input.down.fill(false);app.input.pressed.fill(false);app.commands(tickSeconds);
}
void verifyNextRace(App& app,const Profile& expected){
    chooseTimeAttack(app,expected.u(16));
    require(app.battleProfile.byte(164)==expected.byte(164),"Next race lost saved tuning level");
    for(unsigned offset=156;offset<168;++offset)
        require(app.battleProfile.byte(offset)==expected.byte(offset),"Next race lost saved appearance byte");
    const auto& selected=app.originalSession.selection().physics;
    require(selected.upgradeIndex0C9015F0==expected.byte(164),"Next race solver ignored saved tuning level");
    auto appearance=app.battleProfile;appearance.setu(64,app.frontend.selectedColor());
    require(app.carPresentation.usesPlayerProfile()&&app.loadedAppearanceWord==original::originalPlayerAppearanceConfig(appearance).word,
        "Next race did not rebuild saved car appearance");
}
}

int main(int argc,char** argv)try{
    require(argc==4,"isolated-game-root output-directory --isolated-tuning-profile required");
    const bool supplement=std::string(argv[3])=="--isolated-tuning-supplement";
    require(supplement||std::string(argv[3])=="--isolated-tuning-profile","Explicit isolated-profile test flag missing");
    const auto isolatedRoot=fs::weakly_canonical(argv[1]);const fs::path output=argv[2];
    require(isolatedRoot.filename()=="tuning-application-root"&&isolatedRoot.parent_path().filename()=="work",
        "Refusing profile writes outside work/tuning-application-root");
    require(fs::exists(isolatedRoot/".isolated-tuning-application"),"Isolated-root marker missing");
    require(supplement||!fs::exists(isolatedRoot/"userdata"),"Harness requires a new isolated root; preserve prior run and use a fresh directory");
    fs::create_directories(output);
    std::ofstream report(output/"application.csv",supplement?std::ios::app:std::ios::out);
    if(!supplement)report<<"case,fixture,car,race_frames,owner_kind,before_points,earned,spent,after_points,upgrade,wheel,child_frames\n";
    auto app=makeApp(isolatedRoot);const auto tables=original::OriginalTuningData::load(isolatedRoot);
    const auto first=tables.car(0).packages.at(0).steps.at(0).words;
    require(first[0]==7&&first[2]==5000,"Canonical AE86 first basic award is no longer the5000-point wheel row");
    if(!supplement){
    require(app->profiles.load(0).origin==LocalDriverProfiles::Origin::Fresh,"Natural reward fixture was not fresh");
    for(unsigned raceNumber=1;raceNumber<=5;++raceNumber){
        chooseLegend(*app,0);const auto before=app->battleProfile.u(72);const auto frames=naturalTimeout(*app);
        require(app->battlePoints.total==1000&&app->battlePoints.win==0&&app->battlePoints.advantage==0,
            "Natural Legend timeout did not earn exactly source1000 participation points");
        require(app->battleProfile.u(72)==raceNumber*1000,"Natural Legend points were not committed exactly once");
        const auto kind=app->resultVisit.tuning.kind;
        if(raceNumber<5)require(kind==Kind::none&&app->battleProfile.byte(163)==0,"Basic wheel award occurred before5000points");
        else{
            waitForChild(*app,Kind::basic);frozenChecks(*app,output,"legend-wheel-child");
            for(unsigned i=0;app->battleProfile.byte(163)==0&&!app->menu&&i<1200;++i)render(*app);
            require(app->battleProfile.byte(163)==1&&app->battleProfile.u(72)==5000,"Earned wheel upgrade missing or incorrectly charged points");
            capture(*app,output,"legend-wheel-awarded");
        }
        if(raceNumber==1){for(unsigned i=0;i<45;++i)render(*app);frozenChecks(*app,output,"legend-points");}
        finishVisit(*app);
        require(app->frontend.battleProfile.u(72)==raceNumber*1000,"Leaving results awarded points twice");
        report<<"legend-"<<raceNumber<<",fresh-natural,0,"<<frames<<','<<unsigned(kind)<<','<<before<<",1000,0,"<<app->frontend.battleProfile.u(72)<<','<<unsigned(app->frontend.battleProfile.byte(164))<<','<<unsigned(app->frontend.battleProfile.byte(163))<<','<<app->resultVisit.child.frame<<'\n';report.flush();
        std::cout<<"Natural Legend timeout "<<raceNumber<<" passed, balance="<<app->frontend.battleProfile.u(72)<<"\n"<<std::flush;
    }
    auto earned=app->profiles.load(0).profile;
    app.reset();app=makeApp(isolatedRoot);verifyNextRace(*app,earned);
    const auto taFrames=naturalTimeout(*app);
    require(app->timeAttackPoints.participation==1000&&app->timeAttackPoints.total==1000&&app->timeAttackPoints.finish==0&&app->timeAttackPoints.recordBonus==0,
        "Natural TA timeout points differ from source1000-only award");
    require(app->battleResults.profileMode==1&&app->battleProfile.u(72)==6000,"TA result visit did not bind source points UI/commit");
    for(unsigned i=0;i<45;++i)render(*app);
    frozenChecks(*app,output,"time-attack-points");finishVisit(*app);
    report<<"ta-timeout,fresh-natural,0,"<<taFrames<<','<<unsigned(app->resultVisit.tuning.kind)<<",5000,1000,0,6000,"<<unsigned(app->frontend.battleProfile.byte(164))<<",1,"<<app->resultVisit.child.frame<<'\n';report.flush();
    }else{
        // Explicit completed-basic fixture assembled from original mutations.
        // Only the final1000point threshold crossing is earned in this run.
        auto fixture=original::makeOriginalFreshBattleProfile();fixture.setu(16,0);
        fixture.setu(1180,fixture.u(1180)|1);fixture.setu(72,134000);
        for(unsigned i=0;!(fixture.u(1180)&0x400)&&i<128;++i)
            (void)original::applyOriginalTuningCommand(fixture,tables,1);
        const auto row=tables.car(0).performance.at(fixture.byte(153)).words;
        require(fixture.byte(164)==5&&row[0]==6&&row[1]==135000,"Performance prerequisite did not match original level6/135000row");
        require(app->profiles.save(0,fixture),"Cannot save labeled completed-basic performance fixture");
        chooseTimeAttack(*app,0);const auto frames=naturalTimeout(*app);waitForChild(*app,Kind::performance);
        require(app->timeAttackPoints.total==1000&&app->battleProfile.u(72)==135000,"Natural TA award did not reach135000threshold");
        for(unsigned i=0;app->battleProfile.byte(164)==5&&!app->menu&&i<1200;++i)render(*app);
        require(app->battleProfile.byte(164)==6&&app->battleProfile.u(72)==135000,"Performance reward failed or spent tuning points");
        frozenChecks(*app,output,"performance-level6-awarded");finishVisit(*app);
        const auto saved=app->profiles.load(0).profile;
        require(saved.byte(164)==6&&saved.byte(153)==1,"Performance award was not persisted exactly once");
        report<<"performance-level6,completed-basic-saved-fixture,0,"<<frames<<','<<unsigned(Kind::performance)<<",134000,1000,0,135000,6,"<<unsigned(saved.byte(163))<<','<<app->resultVisit.child.frame<<'\n';report.flush();
        app.reset();app=makeApp(isolatedRoot);verifyNextRace(*app,saved);
        require(app->originalSession.selection().physics.overrideMode0C9015F4==1,"AE86 level6 source engine override did not reach next race");
    }
    // Optional shops require a long completed tuning history. These two
    // explicit saved-profile fixtures test the genuine next race/owner/input
    // path; they are not evidence that the whole history was naturally earned.
    for(unsigned car=supplement?3u:1u;car<=(supplement?3u:2u);++car){
        const bool accept=car==1,timeout=car==3;
        const std::string caseName=accept?"optional-accept":timeout?"optional-timeout":"optional-decline";
        auto fixture=original::makeOriginalFreshBattleProfile();
        fixture.setu(16,car);fixture.setu(72,1000000);fixture.setu(1180,fixture.u(1180)|0xc01);
        fixture.setByte(164,63);fixture.setByte(153,57);fixture.setByte(155,2);
        require(app->profiles.save(car,fixture),"Cannot save labeled optional-shop prerequisite fixture");
        chooseLegend(*app,car);const auto frames=naturalTimeout(*app);waitForChild(*app,Kind::optionalPart);
        const auto sourceRow=tables.car(car).optional.at(app->resultVisit.child.optionalIndex).words;
        const auto before=app->battleProfile;const auto beforeFiles=savedFiles(*app);
        app->input.down.fill(false);app->input.down[VK_LEFT]=true;app->commands(tickSeconds);render(*app);
        app->input.down.fill(false);app->commands(tickSeconds);
        require(app->resultVisit.child.choice==0,"App LEFT did not select original accept choice");
        require(app->battleProfile.u(72)==before.u(72),"Optional preview spent points before confirmation");
        for(unsigned o=156;o<168;++o)require(app->battleProfile.byte(o)==before.byte(o),"Optional preview mutated saved appearance");
        require(savedFiles(*app)==beforeFiles,"Optional preview persisted a transaction before confirmation");
        frozenChecks(*app,output,caseName+"-preview");
        if(!timeout)confirm(*app,accept);
        finishVisit(*app);
        const auto saved=app->profiles.load(car).profile;const auto expected=1001000u-(accept?sourceRow[3]:0u);
        require(saved.u(72)==expected,"Optional accept/decline charged an incorrect price or committed twice");
        for(unsigned offset=156;offset<168;++offset){
            const auto value=accept&&offset==156+sourceRow[1]?std::uint8_t(sourceRow[0]):before.byte(offset);
            require(saved.byte(offset)==value,"Optional accept/decline saved an incorrect appearance");
        }
        if(timeout)require(saved.u(1176)==0,"Optional timeout ended before exhausting the original879tick choice clock");
        report<<caseName<<",completed-tuning-saved-fixture,"<<car<<','<<frames<<','<<unsigned(Kind::optionalPart)<<",1000000,1000,"<<(accept?sourceRow[3]:0)<<','<<expected<<','<<unsigned(saved.byte(164))<<','<<unsigned(saved.byte(163))<<','<<app->resultVisit.child.frame<<'\n';report.flush();
        verifyNextRace(*app,saved);
    }
    if(supplement)std::cout<<"PASS actual App supplement: natural TA timeout crossed authored135000threshold, awarded/persisted level6 without spending points, reloaded next-race solver level6/AE86 override; optional879tick timeout declined without charge/appearance mutation. Pause/repaint inert. Explicit saved prerequisites, offscreen WARP, no audio device, isolated root only.\n";
    else std::cout<<"PASS actual App: five unmodified-clock Legend timeouts naturally earned5000points and the AE86 wheel; TA timeout earned1000; labeled optional accept/decline fixtures used source price and profile writes; pauses/repaints were inert; saved tuning and appearance reached the next race. Offscreen WARP only, no audio device, isolated profile root only.\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
