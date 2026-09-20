#include "../src/original_course_lighting.h"
// Harness-only mutation gate: both Apps run the actual start, owner update,
// path bookkeeping, solver and rendering code. The control omits only the
// pure native light-state mutation. No live application switch is introduced.
namespace idas3::original {
struct ApplicationLightFixture {
    inline static bool enabled=true;
    inline static unsigned calls=0;
    inline static OriginalLightMatrix matrix{};
    inline static OriginalLightVector reference{};
};
OriginalIrohazakaLightUpdate applicationFixtureUpdateCourseLighting(OriginalCourseLighting& state,
    const OriginalLightMatrix& matrix,const OriginalLightVector& reference){
    ++ApplicationLightFixture::calls;ApplicationLightFixture::matrix=matrix;ApplicationLightFixture::reference=reference;
    if(ApplicationLightFixture::enabled)return updateOriginalCourseLighting(state,matrix,reference);
    return {};
}
}
#define updateOriginalCourseLighting applicationFixtureUpdateCourseLighting
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#undef updateOriginalCourseLighting
#include <iostream>
#include <map>

namespace {
using Bytes=std::vector<char>;
Bytes readBytes(const fs::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Unreadable fixture file");return Bytes(std::istreambuf_iterator<char>(f),{});}
struct SavedFile{Bytes data;fs::file_time_type time;bool operator==(const SavedFile&)const=default;};
std::map<fs::path,SavedFile> snapshot(const fs::path& directory){
    std::map<fs::path,SavedFile> result;
    if(fs::exists(directory))for(const auto& file:fs::recursive_directory_iterator(directory))if(file.is_regular_file())
        result.emplace(file.path().lexically_relative(directory),SavedFile{readBytes(file.path()),file.last_write_time()});
    return result;
}
bool equalFog(const original::OriginalCourseFog& a,const original::OriginalCourseFog& b){
    return a.sourceRow==b.sourceRow&&a.sourceAddress==b.sourceAddress&&a.density==b.density&&a.maximum==b.maximum&&
        a.colorRgb==b.colorRgb&&a.packedDensity==b.packedDensity&&a.samples==b.samples&&a.table==b.table;
}
bool equalLight(const original::OriginalCourseLighting& a,const original::OriginalCourseLighting& b){
    if(a.sourceRow!=b.sourceRow||a.sourceAddress!=b.sourceAddress||a.count!=b.count||a.ambient!=b.ambient)return false;
    for(unsigned i=0;i<a.count;++i){const auto&x=a.lights[i];const auto&y=b.lights[i];
        if(x.kind!=y.kind||x.sourceSlot!=y.sourceSlot||x.enabled!=y.enabled||x.position!=y.position||x.incomingDirection!=y.incomingDirection||
            x.color!=y.color||x.distance0!=y.distance0||x.distance1!=y.distance1||x.angle0!=y.angle0||x.angle1!=y.angle1||
            x.coefficientWords!=y.coefficientWords||x.angleCosines!=y.angleCosines)return false;
    }return true;
}
original::OriginalLightMatrix priorPlayerMatrix(const App& app,const original::OriginalFscaTable& table){
    const auto& actor=app.originalSession.actor();
    auto matrix=original::originalActorMatrix({actor.f(0),actor.f(4)-std::bit_cast<float>(0x3ca3d70au),actor.f(8)},
        {actor.f(24),actor.f(28),actor.f(32)},table);
    original::rotateOriginalMatrixPhase(matrix,1,0x8000,table);return matrix.elements;
}
original::OriginalLightVector priorPathReference(const App& app){
    // Source099460 uses period=count-1, not a clamped interpolated point.
    // Course::load has already applied its source reverse-point ordering.
    const int period=int(app.course.points.size()-1);int index=app.courseLightPathIndex%period;if(index<0)index+=period;
    const auto& p=app.course.points[unsigned(index)];return {p.x,p.y,p.z};
}
DriverInput scriptedInput(const App& app,unsigned tick){
    DriverInput input;input.automatic=true;input.throttle=tick%300<250?.6f:.15f;
    const auto at=app.course.project(app.vehicle.position,app.segment);
    const float ahead=std::max(12.f,app.vehicle.speed*.65f);const auto direction=app.course.sample(at.sample.distance+ahead).center-app.vehicle.position;
    const float error=wrapAngle(std::atan2(direction.x,direction.z)-app.vehicle.yaw);
    input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(error),ahead)/recoveredSteeringLimit,-.6f,.6f);return input;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Marked isolated root and output directory required");
    const auto isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"COURSE_ENVIRONMENT_TEST_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked or non-work App root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Fixture userdata must not redirect to actual saves");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),privateSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0,pairedTicks=0,discriminatingMatrices=0,discriminatingPaths=0;
    auto check=[&](bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);};
    const auto trig=original::OriginalFscaTable::load(isolated/"data/original_physics/fsca_table.bin");
    const auto bootstrap=original::originalBootstrapFog();
    auto initialize=[&](){
        auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
        app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
        app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
        app->frontend.car=0;app->automatic=app->frontend.automatic=true;app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();app->drivingView=OriginalDrivingView::Chase;
        check(equalFog(app->raceFog,bootstrap),"New App did not retain source graphics bootstrap fog");return app;
    };
    auto compareDriving=[&](const App&a,const App&b){
        const auto&x=a.originalSession;const auto&y=b.originalSession;
        check(x.vehicle().drive.words==y.vehicle().drive.words&&x.actor().words==y.actor().words,"Course lighting changed original player state");
        check(x.publishedActors().player0C8FF388==y.publishedActors().player0C8FF388&&x.publishedActors().secondary0C8FF430==y.publishedActors().secondary0C8FF430,"Course lighting changed published actors");
        check(x.recovery().drive0C9009F0.words==y.recovery().drive0C9009F0.words&&x.recovery().actor0C8FF580==y.recovery().actor0C8FF580,"Course lighting changed recovery state");
        const auto&p=x.contactCompletion();const auto&q=y.contactCompletion();
        check(p.randomSeed0C37C778==q.randomSeed0C37C778&&p.elapsedFrames0C900E84==q.elapsedFrames0C900E84&&p.steeringMask0C900EBC==q.steeringMask0C900EBC,"Course lighting changed shared RNG/counters");
        check(p.cues0C900E5C==q.cues0C900E5C&&p.snapshot0CAA9718==q.snapshot0CAA9718&&p.impactPositions==q.impactPositions&&p.impactFrames==q.impactFrames&&p.positionCursor==q.positionCursor&&p.frameCursor==q.frameCursor,"Course lighting changed sound/contact history");
        check(x.platformFrame()==y.platformFrame()&&a.originalRaceOwnerFrame==b.originalRaceOwnerFrame,"Course lighting changed owner/solver clocks");
        check(a.race.phase==b.race.phase&&a.race.ticks==b.race.ticks&&a.race.remaining6000==b.race.remaining6000&&a.race.elapsed6000==b.race.elapsed6000&&a.race.countdown==b.race.countdown,"Course lighting changed race timing");
    };
    auto verifyScope=[&](App& app){
        check(app.renderer.courseFog==&app.raceFog&&app.raceLighting&&app.renderer.courseLighting==&*app.raceLighting,"Race did not bind its retained source fog/light owners");
        Mesh prefix;NativeAssembly background;
        if(app.catalogScenery)background=app.courseScene.backgroundAssembly(app.camera);else background.instances.push_back(originalAkinaBackgroundInstance(app.camera));
        prefix.originalCar(app.originalBackgroundModel,background,{},0,unsigned(app.originalCourseTextures.size()));
        app.scenery(prefix,app.progress);if(app.courseCrows)prefix.originalCar(app.courseCrows->model,app.courseCrows->assembly(),{},0,app.crowTextureBase);
        check(!prefix.ranges.empty()&&app.raceMesh.ranges.size()>prefix.ranges.size(),"Actual race needs course and distinct car ranges");
        for(unsigned i=0;i<app.raceMesh.ranges.size();++i)check(app.raceMesh.ranges[i].courseLighting==(i<prefix.ranges.size()),"Course lightset leaked beyond source geometry/background/crow scope");
    };
    auto condition=[&](App& app,unsigned course,bool reverse,bool night,bool wet){
        app.courseIndex=int(course);app.reverse=reverse;app.night=night;app.wet=wet;
    };
    auto start=[&](App& app){
        const auto retained=app.raceFog;original::ApplicationLightFixture::calls=0;app.start();
        auto expected=retained;if(app.courseIndex==4)original::applyHappoFogRegisters(expected,app.night,app.wet);
        else expected=original::originalCourseFog(unsigned(app.courseIndex),app.night,app.wet);
        check(equalFog(app.raceFog,expected),"App activation selected or retained the wrong fog table");
        check(app.raceLighting&&equalLight(*app.raceLighting,original::originalCourseLighting(unsigned(app.courseIndex),app.night,app.wet)),"App activation did not retain constructor lighting state");
        check(app.courseLightPathIndex==0&&original::ApplicationLightFixture::calls==0,"Course lighting advanced during source solver-only warmup");
        check(app.originalRaceOwnerFrame==0&&app.originalHandling,"Fixture did not start the actual original race owner");
    };
    std::ofstream csv(output/"application.csv");csv<<"label,course,direction,night,wet,owner_frame,light_row,path_cache,lights,fog_density,table_first,seed\n";
    auto capture=[&](App& app,const std::string& label,bool save=true){
        const auto lights=*app.raceLighting;const auto fog=app.raceFog;const int path=app.courseLightPathIndex;
        const auto calls=original::ApplicationLightFixture::calls;
        check(app.render(0,false),"Actual environment App render failed");verifyScope(app);
        check(equalLight(lights,*app.raceLighting)&&equalFog(fog,app.raceFog)&&path==app.courseLightPathIndex&&calls==original::ApplicationLightFixture::calls,"Rendering advanced a source environment owner");
        if(save)check(app.renderer.saveBitmap((output/(label+".bmp")).wstring()),"Could not save actual App environment capture");
        csv<<label<<','<<app.courseIndex<<','<<app.reverse<<','<<app.night<<','<<app.wet<<','<<app.originalRaceOwnerFrame<<','<<app.raceLighting->sourceRow<<','<<app.courseLightPathIndex<<','<<app.raceLighting->count<<','<<app.raceFog.packedDensity<<','<<app.raceFog.table.front()<<','<<app.originalSession.contactCompletion().randomSeed0C37C778<<'\n';csv.flush();
    };
    auto tick=[&](App& app,App* control,unsigned frame){
        const auto matrix=priorPlayerMatrix(app,trig);const auto reference=priorPathReference(app);
        const int oldCoordinate=app.originalCoordinate.index;auto expected=*app.raceLighting;
        original::updateOriginalCourseLighting(expected,matrix,reference);
        const auto fog=app.raceFog;const auto input=scriptedInput(app,frame);
        original::ApplicationLightFixture::enabled=true;original::ApplicationLightFixture::calls=0;app.simulate(input);
        check(original::ApplicationLightFixture::calls==1,"Missing or repeated course update within one source frame");
        check(original::ApplicationLightFixture::matrix==matrix,"Lighting consumed a camera/current-solver matrix instead of the prior player visual matrix");
        check(original::ApplicationLightFixture::reference==reference,"Lighting consumed a current/clamped/re-reversed path reference");
        check(equalLight(*app.raceLighting,expected)&&equalFog(app.raceFog,fog),"App environment update differs from recovered pure source state");
        check(app.courseLightPathIndex==oldCoordinate,"Course path cache did not publish the old race coordinate after lighting");
        discriminatingMatrices+=matrix!=priorPlayerMatrix(app,trig);discriminatingPaths+=reference!=priorPathReference(app);
        if(control){
            original::ApplicationLightFixture::enabled=false;control->simulate(input);original::ApplicationLightFixture::enabled=true;
            compareDriving(app,*control);check(app.courseLightPathIndex==control->courseLightPathIndex,"Lighting mutation gate changed source path bookkeeping");++pairedTicks;
        }
    };
    // New process bootstrap -> hidden asset preloads -> first Happo. Asset
    // selection is deliberately exercised without starting a race.
    auto history=initialize();check(history->renderer.initialize(nullptr,960,720,true),"Cannot initialize offscreen WARP renderer");
    for(unsigned course:{3u,5u,4u}){
        condition(*history,course,false,false,false);history->load();
        check(equalFog(history->raceFog,bootstrap)&&!history->raceLighting,"Hidden course preload changed source graphics state");
    }
    condition(*history,4,false,true,false);start(*history);capture(*history,"happo-first-bootstrap");
    check(history->raceFog.table==bootstrap.table,"First Happo did not inherit graphics bootstrap table");
    // Direct restart retains the previous table. This is a controlled native
    // restart boundary, distinct from the result/selection graphics reset.
    condition(*history,3,false,false,false);start(*history);const auto priorGeneric=history->raceFog;
    check(priorGeneric.table!=bootstrap.table,"Retained-table fixture needs a distinguishable authored cap");
    condition(*history,4,true,true,true);start(*history);capture(*history,"happo-retained-akina-day-table");
    check(history->raceFog.table==priorGeneric.table&&history->raceFog.samples==priorGeneric.samples,"Happo replaced retained authored table data");
    history->beginResultVisit({});
    check(equalFog(history->raceFog,bootstrap)&&!history->renderer.courseLighting&&!history->renderer.courseFog,"Result Init did not reset/clear course graphics state immediately");
    check(history->render(0,false),"Result render failed");
    check(!history->renderer.courseLighting&&!history->renderer.courseFog&&equalFog(history->raceFog,bootstrap),"Result preview inherited race environment bindings");
    history->returnToCourseSelection();
    check(!history->raceLighting&&equalFog(history->raceFog,bootstrap)&&!history->renderer.courseLighting&&!history->renderer.courseFog,"Selection return failed to reset course state");
    check(history->renderMenu(0),"Course menu return render failed");
    check(!history->renderer.courseLighting&&!history->renderer.courseFog&&equalFog(history->raceFog,bootstrap),"Menu rendering or preload selected course graphics state");
    condition(*history,4,false,true,true);start(*history);capture(*history,"happo-after-result-bootstrap");
    check(history->raceFog.table==bootstrap.table,"Happo after result retained obsolete race table");

    // All72 requested loader combinations, including native diagnostics.
    // App::load forces Snow wet, giving68 distinct resulting combinations.
    // Normal menu availability is independently covered by the source test.
    for(unsigned course=0;course<9;++course)for(bool reverse:{false,true})for(bool night:{false,true})for(bool wet:{false,true}){
        condition(*history,course,reverse,night,wet);start(*history);tick(*history,nullptr,0);
        const auto label="loader-c"+std::to_string(course)+"-d"+std::to_string(reverse)+"-n"+std::to_string(night)+"-w"+std::to_string(wet);
        capture(*history,label,reverse==false&&wet==false);
    }
    // Both directions on the two position-dependent owners. Every input and
    // drive-state comparison includes the180-frame countdown then moving car.
    for(unsigned course:{4u,5u})for(bool reverse:{false,true}){
        auto app=initialize(),control=initialize();const bool wet=course==4;
        condition(*app,course,reverse,true,wet);condition(*control,course,reverse,true,wet);
        start(*app);start(*control);compareDriving(*app,*control);
        check(app->renderer.initialize(nullptr,960,720,true),"Cannot initialize paired App WARP renderer");
        const auto label="paired-c"+std::to_string(course)+"-d"+std::to_string(reverse);
        for(unsigned frame=0;frame<600;++frame){
            tick(*app,control.get(),frame);
            if(frame==179){check(app->race.phase==RacePhase::Running&&app->race.ticks==1,"Environment fixture GO boundary differs");capture(*app,label+"-go180");}
        }
        capture(*app,label+"-600");
        app->paused=true;app->clock.reset();check(app->render(0,false),"Paused baseline render failed");
        const auto before=output/(label+"-pause-before.bmp"),after=output/(label+"-pause-after.bmp");
        check(app->renderer.saveBitmap(before.wstring()),"Could not save paused baseline");
        const auto lights=*app->raceLighting;const auto fog=app->raceFog;const auto path=app->courseLightPathIndex;
        const auto calls=original::ApplicationLightFixture::calls;
        for(unsigned repaint=0;repaint<4;++repaint){
            app->commands(1./60.);if(!app->menu&&!app->paused)app->clock.advance(1./60.,[&]{app->simulate(app->driver());});else app->clock.reset();
            check(app->render(1./60.,false),"Paused repaint failed");
        }
        check(app->renderer.saveBitmap(after.wstring())&&readBytes(before)==readBytes(after),"Paused source environment repaint changed pixels");
        check(equalLight(lights,*app->raceLighting)&&equalFog(fog,app->raceFog)&&path==app->courseLightPathIndex&&calls==original::ApplicationLightFixture::calls,"Paused rendering advanced source environment state");
        compareDriving(*app,*control);start(*app);start(*control);compareDriving(*app,*control);
        std::cout<<"Checked "<<label<<"600 paired updates, pause and restart.\n"<<std::flush;
    }
    check(discriminatingMatrices>100&&discriminatingPaths>10,"Test route did not distinguish prior/current matrix and path inputs");
    // Actual battle draws submit both player and rival ranges. The bumper
    // mirror is rendered with the same immutable source lightset and fog.
    auto battle=initialize();battle->frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    battle->frontend.battleProfile.setu(0,0);original::selectOriginalRival(battle->frontend.battleProfile,19);start(*battle);
    check(battle->battle&&battle->rivalVisible,"Battle fixture did not create both source cars");
    check(battle->renderer.initialize(nullptr,960,720,true),"Cannot initialize battle WARP renderer");
    capture(*battle,"battle-happo-player-rival-scope");battle->drivingView=OriginalDrivingView::Bumper;
    const auto lightState=*battle->raceLighting;const auto fogState=battle->raceFog;const auto lightCalls=original::ApplicationLightFixture::calls;
    check(battle->render(0,true),"Battle bumper rear-view render failed");verifyScope(*battle);
    check(equalLight(lightState,*battle->raceLighting)&&equalFog(fogState,battle->raceFog)&&lightCalls==original::ApplicationLightFixture::calls,"Mirror render advanced environment state");
    check(battle->renderer.saveBitmap((output/"battle-happo-bumper-mirror.bmp").wstring()),"Could not save battle mirror evidence");
    check(snapshot(realRoot/"userdata")==realSaves&&snapshot(isolated/"userdata")==privateSaves,"Driver saves changed during isolated fixture");
    std::ostringstream report;report<<"PASS "<<checks<<" checks / "<<pairedTicks<<" paired App updates; prior/current matrix/path discrimination "<<discriminatingMatrices<<'/'<<discriminatingPaths
        <<"; bootstrap/Happo retained tables, result/menu reset,72 requested loader cases, scope/repaint/pause/restart/mirror; "<<realSaves.size()<<" real save files unchanged in bytes/timestamps. No audio device opened.\n";
    std::cout<<report.str();std::ofstream(output/"result.txt")<<report.str();
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
