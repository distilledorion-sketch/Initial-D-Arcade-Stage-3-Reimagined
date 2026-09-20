#include "../src/original_course_crows.h"
// Test-only factory gate. The control App runs the identical production
// start/load/simulate code with its optional crow owner absent, including
// during initialization. All owner methods use the production implementation.
namespace idas3 {
struct ApplicationCrowFixtureOwner : OriginalCourseCrows {
    inline static bool creationEnabled=true;
    static bool availableFor(unsigned course,bool night,bool wet){return creationEnabled&&OriginalCourseCrows::availableFor(course,night,wet);}
    static ApplicationCrowFixtureOwner load(const std::filesystem::path& root){
        ApplicationCrowFixtureOwner result;
        static_cast<OriginalCourseCrows&>(result)=OriginalCourseCrows::load(root);return result;
    }
};
}
#define OriginalCourseCrows ApplicationCrowFixtureOwner
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#undef OriginalCourseCrows
#include <iostream>
#include <map>

namespace {
using Bytes=std::vector<char>;
Bytes bytes(const fs::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Unreadable fixture file");return Bytes(std::istreambuf_iterator<char>(f),{});}
struct SavedFile {Bytes data;fs::file_time_type time;bool operator==(const SavedFile&)const=default;};
std::map<fs::path,SavedFile> snapshot(const fs::path& directory){
    std::map<fs::path,SavedFile> result;
    if(fs::exists(directory))for(const auto& entry:fs::recursive_directory_iterator(directory))if(entry.is_regular_file())
        result.emplace(entry.path().lexically_relative(directory),SavedFile{bytes(entry.path()),entry.last_write_time()});
    return result;
}
bool sameAssembly(const NativeAssembly& a,const NativeAssembly& b){
    if(a.instances.size()!=b.instances.size())return false;
    for(unsigned i=0;i<a.instances.size();++i)if(a.instances[i].chunk!=b.instances[i].chunk||a.instances[i].transform!=b.instances[i].transform)return false;
    return true;
}
Mesh withoutCrowRanges(const Mesh& mesh,unsigned texture){
    Mesh result;for(const auto& range:mesh.ranges)if(range.texture!=texture){
        auto copy=range;copy.first=unsigned(result.vertices.size());result.ranges.push_back(copy);
        result.vertices.insert(result.vertices.end(),mesh.vertices.begin()+range.first,mesh.vertices.begin()+range.first+range.count);
    }return result;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Marked isolated-root and output-directory required");
    const fs::path isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"COURSE_CROWS_TEST_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked or non-work App root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Isolated userdata must not redirect to real saves");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),isolatedSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0,pairedTicks=0;
    auto check=[&](bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);};
    auto initialize=[&](bool reverse){
        auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
        app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
        app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
        app->courseIndex=0;app->reverse=reverse;app->night=false;app->wet=false;app->automatic=true;
        app->frontend.car=0;app->frontend.automatic=true;app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();return app;
    };
    auto compare=[&](const App& a,const App& b){
        const auto& x=a.originalSession;const auto& y=b.originalSession;
        check(x.vehicle().drive.words==y.vehicle().drive.words,"Crow presence changed original drive words");
        check(x.actor().words==y.actor().words,"Crow presence changed original actor words");
        check(x.publishedActors().player0C8FF388==y.publishedActors().player0C8FF388&&x.publishedActors().secondary0C8FF430==y.publishedActors().secondary0C8FF430,"Crow presence changed published actors");
        check(x.recovery().drive0C9009F0.words==y.recovery().drive0C9009F0.words&&x.recovery().actor0C8FF580==y.recovery().actor0C8FF580,"Crow presence changed recovery state");
        const auto& p=x.contactCompletion();const auto& q=y.contactCompletion();
        check(p.randomSeed0C37C778==q.randomSeed0C37C778&&p.elapsedFrames0C900E84==q.elapsedFrames0C900E84&&p.steeringMask0C900EBC==q.steeringMask0C900EBC,"Crow presence changed shared driving RNG or completion counters");
        check(p.cues0C900E5C==q.cues0C900E5C&&p.snapshot0CAA9718==q.snapshot0CAA9718&&p.impactPositions==q.impactPositions&&p.impactFrames==q.impactFrames&&p.positionCursor==q.positionCursor&&p.frameCursor==q.frameCursor,"Crow presence changed completion cues/history");
        check(x.platformFrame()==y.platformFrame()&&a.originalRaceOwnerFrame==b.originalRaceOwnerFrame,"Crow presence changed driving frame counts");
        check(a.race.phase==b.race.phase&&a.race.ticks==b.race.ticks&&a.race.remaining6000==b.race.remaining6000&&a.race.elapsed6000==b.race.elapsed6000&&a.race.countdown==b.race.countdown,"Crow presence changed race/countdown timing");
    };
    std::ofstream csv(output/"application.csv");csv<<"direction,label,crow_frame,owner_frame,race_ticks,seed,crow_texture_base,crow_vertices\n";
    for(bool reverse:{false,true}){
        auto app=initialize(reverse),control=initialize(reverse);
        ApplicationCrowFixtureOwner::creationEnabled=true;app->start();
        ApplicationCrowFixtureOwner::creationEnabled=false;control->start();ApplicationCrowFixtureOwner::creationEnabled=true;
        check(app->originalHandling&&app->courseCrows.has_value()&&!control->courseCrows,"Optional owner isolation failed during actual App start");
        check(app->courseCrows->state().flightFrame==0&&app->originalRaceOwnerFrame==0,"Actual initialization/warmup advanced crow cursor");
        compare(*app,*control);
        OriginalCourseCrowState expected;resetOriginalCourseCrowsBeforeDrivingSeed(expected,1u);
        check(expected.animationFrames==app->courseCrows->state().animationFrames,"Actual fresh App did not preserve driving-entry seed boundary");
        const auto firstAssembly=app->courseCrows->assembly();
        check(app->renderer.initialize(nullptr,960,720,true),"Cannot initialize offscreen WARP renderer");
        const auto prefix=reverse?std::string("reverse-"):std::string("forward-");
        auto capture=[&](const std::string& label){
            check(app->render(0,false),"Actual App render failed");
            check(app->renderer.saveBitmap((output/(prefix+label+".bmp")).wstring()),"Could not save App capture");
            Mesh crowMesh;crowMesh.originalCar(app->courseCrows->model,app->courseCrows->assembly(),{},0,app->crowTextureBase);
            unsigned visibleVertices=0;
            for(const auto& range:app->raceMesh.ranges){
                check(range.texture==0xffffffffu||range.texture<app->crowTextureBase+1,"App mesh references texture beyond uploaded crow bank");
                if(range.texture==app->crowTextureBase){check(range.original&&range.courseLighting,"Crow range lost source/course material scope");visibleVertices+=range.count;}
            }
            check(app->crowTextureBase==app->originalCourseTextures.size()+app->originalBackgroundTextures.size()+app->originalTextures.size()+app->numberPlate.textures.size(),"Crow texture base overlaps another bank");
            check(crowMesh.vertices.size()==5130&&visibleVertices==crowMesh.vertices.size(),"Actual App did not append all19 original crows");
            csv<<(reverse?1:0)<<','<<label<<','<<app->courseCrows->state().flightFrame<<','<<app->originalRaceOwnerFrame<<','<<app->race.ticks<<','<<app->originalSession.contactCompletion().randomSeed0C37C778<<','<<app->crowTextureBase<<','<<visibleVertices<<'\n';csv.flush();
        };
        capture("natural-start");
        for(unsigned frame=0;frame<900;++frame){
            DriverInput input;input.automatic=true;input.throttle=frame%300<250?.55f:.15f;
            const auto projection=app->course.project(app->vehicle.position,app->segment);
            const float ahead=std::max(12.f,app->vehicle.speed*.65f);
            const auto direction=app->course.sample(projection.sample.distance+ahead).center-app->vehicle.position;
            const float error=wrapAngle(std::atan2(direction.x,direction.z)-app->vehicle.yaw);
            input.steer=-std::clamp(std::atan2(2*app->config.wheelbase*std::sin(error),ahead)/recoveredSteeringLimit,-.6f,.6f);
            app->simulate(input);control->simulate(input);++pairedTicks;compare(*app,*control);
            check(app->courseCrows->state().flightFrame==(frame+1)%900,"Actual App crow owner missed or duplicated a source update");
            if(frame==179){check(app->race.phase==RacePhase::Running&&app->race.ticks==1&&app->courseCrows->state().flightFrame==180,"GO180 crow/source timing mismatch");capture("natural-go180");}
        }
        check(sameAssembly(firstAssembly,app->courseCrows->assembly()),"Actual900-frame loop did not recover original model poses");capture("natural-loop900");
        // Actual paused host gate: commands and redraw continue while the
        // fixed simulation clock is held. This is not a direct paused call
        // to App::simulate, which the production host never makes.
        app->paused=true;app->clock.reset();check(app->render(0,false),"Pause baseline render failed");
        const auto frozenState=app->courseCrows->state();const auto frozenDrive=app->originalSession.vehicle().drive.words;
        const auto frozenSeed=app->originalSession.contactCompletion().randomSeed0C37C778;
        const auto before=output/(prefix+"paused-before.bmp"),after=output/(prefix+"paused-after.bmp");
        check(app->renderer.saveBitmap(before.wstring()),"Pause baseline save failed");
        for(unsigned repaint=0;repaint<4;++repaint){
            app->commands(1./60.);if(!app->menu&&!app->paused)app->clock.advance(1./60.,[&]{app->simulate(app->driver());});else app->clock.reset();
            check(app->render(1./60.,false),"Paused repaint failed");
        }
        check(app->renderer.saveBitmap(after.wstring()),"Pause repaint save failed");
        check(bytes(before)==bytes(after),"Frozen App repaint changed WARP pixels");
        check(app->courseCrows->state().flightFrame==frozenState.flightFrame&&app->courseCrows->state().animationFrames==frozenState.animationFrames&&app->originalSession.vehicle().drive.words==frozenDrive&&app->originalSession.contactCompletion().randomSeed0C37C778==frozenSeed,"Paused repaint advanced crows or driving");
        // Close-view diagnostic uses the actual App mesh and uploaded texture
        // table, changing only the camera. It is labelled separately from
        // the natural bumper-camera captures above.
        Vec3 centre{};for(const auto& instance:app->courseCrows->assembly().instances)centre+=Vec3{instance.transform[3],instance.transform[7],instance.transform[11]};centre=centre/19.f;
        app->renderer.cameraUp={0,1,0};app->renderer.verticalFieldOfView=.95f;app->renderer.projectionAspect=4.f/3;app->renderer.screenFadeArgb=0;
        const auto close=output/(prefix+"close-diagnostic.bmp"),without=output/(prefix+"close-without-crows.bmp");
        const auto eye=centre+Vec3{0,7,34};
        check(app->renderer.draw(app->raceMesh,eye,centre,false,false),"Close-view actual App mesh failed");
        check(app->renderer.saveBitmap(close.wstring()),"Close-view capture failed");
        const auto empty=withoutCrowRanges(app->raceMesh,app->crowTextureBase);
        check(app->renderer.draw(empty,eye,centre,false,false),"Close-view control render failed");
        check(app->renderer.saveBitmap(without.wstring()),"Close-view control capture failed");
        const auto visible=bytes(close),absent=bytes(without);check(visible.size()==absent.size(),"Close-view image extent changed");
        unsigned changed=0;for(unsigned i=54;i+3<visible.size();i+=4)changed+=std::memcmp(visible.data()+i,absent.data()+i,3)!=0;
        check(changed>100,"App crows submitted geometry but did not produce visible textured pixels");
        // Restart uses the exact currently owned stream. The disabled App
        // follows the same original reset/warmup without creating the flock.
        const auto restartSeed=app->originalSession.contactCompletion().randomSeed0C37C778;
        compare(*app,*control);ApplicationCrowFixtureOwner::creationEnabled=true;app->start();
        ApplicationCrowFixtureOwner::creationEnabled=false;control->start();ApplicationCrowFixtureOwner::creationEnabled=true;
        resetOriginalCourseCrowsBeforeDrivingSeed(expected,restartSeed);
        check(app->courseCrows->state().flightFrame==0&&app->courseCrows->state().animationFrames==expected.animationFrames,"Restart did not reconstruct phases from its unchanged driving-entry seed");compare(*app,*control);
        std::cout<<"Checked "<<prefix<<"900 paired original App updates; close-view changed "<<changed<<" pixels.\n"<<std::flush;
        // Actual loading gates. The transition also exercises removal of a
        // previously active owner and invalidation of the texture table.
        for(unsigned c=0;c<9;++c)for(unsigned variant=0;variant<(c==0?4u:1u);++variant){
            app->courseIndex=int(c);app->night=(variant&1)!=0;app->wet=(variant&2)!=0;app->reverse=reverse;app->load();
            check(bool(app->courseCrows)==(c==0&&variant==0),"Actual course load retained crows in another course/weather owner");
        }
    }
    check(snapshot(realRoot/"userdata")==realSaves,"Actual saved driver files changed");
    check(snapshot(isolated/"userdata")==isolatedSaves,"Validation App unexpectedly wrote isolated save files");
    std::ofstream summary(output/"result.txt");summary<<"PASS "<<checks<<" checks; "<<pairedTicks<<" paired original App updates; both Myogi directions; initialization+restart isolation, GO180,900loop,pause pixels,texture ranges,close-view visibility,and course/weather gates. "<<realSaves.size()<<" real saved files unchanged in bytes and timestamps. No audio device opened.\n";
    std::cout<<"PASS "<<checks<<" checks / "<<pairedTicks<<" paired App ticks; "<<realSaves.size()<<" actual save files unchanged.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
