#include "../src/original_headlight_projection.h"
#include <stdexcept>
// Test-only boundary instrumentation. Both Apps execute the actual source
// solver and lifecycle. The control suppresses only projector working-state
// updates; it cannot change the shared driver RNG or collision scratch.
namespace idas3::original {
struct ApplicationHeadlightEvent {
    struct Result { unsigned point;OriginalCollisionQuery query;bool hit; };
    enum Kind { Bind,Publish,Advance } kind{};
    const void* owner{};
    OriginalHeadlightProjection::Matrix matrix{};
    std::array<OriginalHeadlightProjection::Record,9> recordsBefore{},recordsAfter{};
    std::vector<Result> results;
};
struct ApplicationHeadlightFixture {
    inline static bool enabled=true;
    inline static std::vector<ApplicationHeadlightEvent> events;
    static std::vector<std::uint32_t> vertexWords(const NativeModel& model){
        std::vector<std::uint32_t> words;
        for(const auto& chunk:model.chunks)for(const auto& batch:chunk.batches)for(const auto&v:batch.vertices){
            words.insert(words.end(),{v.header,std::bit_cast<unsigned>(v.position.x),std::bit_cast<unsigned>(v.position.y),std::bit_cast<unsigned>(v.position.z),
                std::bit_cast<unsigned>(v.u),std::bit_cast<unsigned>(v.v),v.color0,v.color1});
        }return words;
    }
};
class ApplicationHeadlightProjection:public OriginalHeadlightProjection {
public:
    void bindRoad(const OriginalCollisionQuery& query){
        ApplicationHeadlightEvent event;event.kind=ApplicationHeadlightEvent::Bind;event.owner=this;event.results.push_back({0,query,true});
        OriginalHeadlightProjection::bindRoad(query);
        for(const auto&q:queries())if(q.words!=query.words)throw std::runtime_error("LightON failed to bind the complete road query");
        ApplicationHeadlightFixture::events.push_back(std::move(event));
    }
    void publish(){
        ApplicationHeadlightEvent event;event.kind=ApplicationHeadlightEvent::Publish;event.owner=this;event.recordsBefore=records();
        OriginalHeadlightProjection::publish();event.recordsAfter=records();
        if(event.recordsBefore!=event.recordsAfter)throw std::runtime_error("Publication changed working headlight records");
        ApplicationHeadlightFixture::events.push_back(std::move(event));
    }
    unsigned advance(const Matrix& matrix,const Query& query){
        ApplicationHeadlightEvent event;event.kind=ApplicationHeadlightEvent::Advance;event.owner=this;event.matrix=matrix;event.recordsBefore=records();
        const auto published=ApplicationHeadlightFixture::vertexWords(model());
        unsigned count=0;if(ApplicationHeadlightFixture::enabled)count=OriginalHeadlightProjection::advance(matrix,[&](OriginalCollisionQuery&q){
            const unsigned point=unsigned(&q-queries().data());const bool hit=query(q);event.results.push_back({point,q,hit});return hit;});
        event.recordsAfter=records();
        if(published!=ApplicationHeadlightFixture::vertexWords(model()))throw std::runtime_error("Working update changed the published headlight mesh");
        ApplicationHeadlightFixture::events.push_back(std::move(event));return count;
    }
};
}
#define OriginalHeadlightProjection ApplicationHeadlightProjection
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#undef OriginalHeadlightProjection
#include <iostream>
#include <map>
#include <cstring>

namespace idas3 {
struct CourseMapTestAccess {
    static std::vector<std::uint32_t> pixels(const Hud&hud){return {hud.pixels,hud.pixels+std::size_t(hud.width)*hud.height};}
};
}
namespace {
using Bytes=std::vector<char>;
Bytes readBytes(const fs::path&path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Unreadable headlight fixture file");return Bytes(std::istreambuf_iterator<char>(in),{});}
struct SavedFile{Bytes bytes;fs::file_time_type time;bool operator==(const SavedFile&)const=default;};
std::map<fs::path,SavedFile> snapshot(const fs::path&root){
    std::map<fs::path,SavedFile> files;
    if(fs::exists(root))for(const auto&entry:fs::recursive_directory_iterator(root))if(entry.is_regular_file())files.emplace(entry.path().lexically_relative(root),SavedFile{readBytes(entry.path()),entry.last_write_time()});
    return files;
}
original::OriginalHeadlightProjection::Matrix actorMatrix(const std::array<std::uint32_t,42>&actor,const original::OriginalFscaTable&trig){
    const auto value=[&](unsigned offset){return std::bit_cast<float>(actor[offset/4]);};
    auto matrix=original::originalActorMatrix({value(0),value(4)-std::bit_cast<float>(0x3ca3d70au),value(8)},
        {value(24),value(28),value(32)},trig);
    original::rotateOriginalMatrixPhase(matrix,1,0x8000,trig);return matrix.elements;
}
DriverInput scriptedInput(const App&app,unsigned frame){
    DriverInput input;input.automatic=true;input.throttle=frame%300<250?.6f:.15f;
    const auto at=app.course.project(app.vehicle.position,app.segment);const float ahead=std::max(12.f,app.vehicle.speed*.65f);
    const auto direction=app.course.sample(at.sample.distance+ahead).center-app.vehicle.position;
    const float error=wrapAngle(std::atan2(direction.x,direction.z)-app.vehicle.yaw);
    input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(error),ahead)/recoveredSteeringLimit,-.6f,.6f);return input;
}
bool projectorRange(const MeshRange&range){return range.original&&range.tsp==0x4489a464u&&(range.gmp&0x200u)!=0;}
std::uint64_t changedPixels(const Bytes&a,const Bytes&b){
    if(a.size()!=b.size()||a.size()<54)throw std::runtime_error("Incompatible headlight image pair");
    std::uint32_t offset;std::memcpy(&offset,a.data()+10,4);if(offset>a.size())throw std::runtime_error("Bad headlight bitmap offset");
    std::uint64_t changed=0;for(std::size_t p=offset;p+3<a.size();p+=4)changed+=a[p]!=b[p]||a[p+1]!=b[p+1]||a[p+2]!=b[p+2];return changed;
}
struct FrozenDraw {
    Vec3 eye,target;
    std::optional<OriginalRearViewFrame> rear;
    std::vector<std::uint32_t> overlay;
    explicit FrozenDraw(const App&app,bool mirror){
        const bool latest=app.paused||app.race.phase==RacePhase::Finished;
        const bool bumper=app.drivingView==OriginalDrivingView::Bumper;
        const auto&view=latest?(bumper?app.bumperCamera.frame():app.originalCamera.frame()):(bumper?app.previousBumperFrame:app.previousCameraFrame);
        eye=app.camera;target=view.target;overlay=idas3::CourseMapTestAccess::pixels(app.hud);
        if(mirror&&app.battle&&bumper){rear=app.rearCameraFrame;const auto&prior=latest?app.rearCameraFrame:app.previousRearCameraFrame;
            rear->eye=prior.eye;rear->target=prior.target;rear->up=normalized(prior.up);}
    }
    bool draw(App&app,const Mesh&mesh)const{return app.renderer.draw(mesh,eye,target,app.night,app.wet,overlay.data(),false,nullptr,rear?&*rear:nullptr);}
};
struct PointChoice {const char*name;CourseSample sample;};
std::array<PointChoice,3> chooseRoadSamples(const Course&course){
    std::array<PointChoice,3> result{{{"flat",{}},{"slope",{}},{"curve",{}}}};
    std::array<float,3> best{std::numeric_limits<float>::max(),-1,-1};
    // Select from authored course shape, away from seams and finish. These
    // are controlled placement fixtures; natural driving is tested separately.
    for(float distance=60;distance<course.length-80;distance+=12){const auto point=course.sample(distance);
        const float flat=std::abs(point.grade)+std::abs(point.curvature)*5;
        if(flat<best[0]){best[0]=flat;result[0].sample=point;}
        if(std::abs(point.grade)>best[1]&&std::abs(point.curvature)<.015f){best[1]=std::abs(point.grade);result[1].sample=point;}
        if(std::abs(point.curvature)>best[2]&&std::abs(point.curvature)<.06f){best[2]=std::abs(point.curvature);result[2].sample=point;}
    }return result;
}
}

int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("Marked isolated root and output directory required");
    const auto isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"PROJECTED_HEADLIGHT_TEST_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked or non-work headlight fixture root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Headlight fixture userdata must not redirect to real saves");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),privateSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0,pairedTicks=0,roadHits=0,roadMisses=0;
    auto check=[&](bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);};
    using Fixture=original::ApplicationHeadlightFixture;using Event=original::ApplicationHeadlightEvent;
    const auto trig=original::OriginalFscaTable::load(isolated/"data/original_physics/fsca_table.bin");
    auto initialize=[&](unsigned course,bool reverse,bool night,bool battle=false){
        auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
        app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
        app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
        app->frontend.car=0;app->automatic=app->frontend.automatic=true;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.gameMode=battle?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
        if(battle){app->frontend.battleProfile.setu(0,0);original::selectOriginalRival(app->frontend.battleProfile,19);}
        app->courseIndex=int(course);app->reverse=reverse;app->night=night;app->wet=false;app->drivingView=OriginalDrivingView::Bumper;
        return app;
    };
    auto compareDriving=[&](const App&a,const App&b){
        const auto&x=a.originalSession;const auto&y=b.originalSession;
        check(x.vehicle().drive.words==y.vehicle().drive.words&&x.actor().words==y.actor().words,"Headlight effect changed player state");
        check(x.publishedActors().player0C8FF388==y.publishedActors().player0C8FF388&&x.publishedActors().secondary0C8FF430==y.publishedActors().secondary0C8FF430,"Headlight effect changed published actors");
        check(x.recovery().drive0C9009F0.words==y.recovery().drive0C9009F0.words&&x.recovery().actor0C8FF580==y.recovery().actor0C8FF580,"Headlight effect changed recovery state");
        const auto&p=x.contactCompletion();const auto&q=y.contactCompletion();
        check(p.randomSeed0C37C778==q.randomSeed0C37C778&&p.elapsedFrames0C900E84==q.elapsedFrames0C900E84&&p.steeringMask0C900EBC==q.steeringMask0C900EBC,"Headlight effect changed shared RNG/counters");
        check(p.cues0C900E5C==q.cues0C900E5C&&p.snapshot0CAA9718==q.snapshot0CAA9718&&p.impactPositions==q.impactPositions&&p.impactFrames==q.impactFrames&&p.positionCursor==q.positionCursor&&p.frameCursor==q.frameCursor,"Headlight effect changed audio/contact history");
        check(x.platformFrame()==y.platformFrame()&&a.originalRaceOwnerFrame==b.originalRaceOwnerFrame,"Headlight effect changed owner clocks");
        check(a.race.phase==b.race.phase&&a.race.ticks==b.race.ticks&&a.race.remaining6000==b.race.remaining6000&&a.race.elapsed6000==b.race.elapsed6000&&a.race.countdown==b.race.countdown,"Headlight effect changed race timing");
        check(a.playerBody.query().words==b.playerBody.query().words&&a.rivalBody.query().words==b.rivalBody.query().words,"Projector queries changed body contact history");
        check(a.courseLightPathIndex==b.courseLightPathIndex,"Headlight effect changed course path state");
    };
    auto start=[&](App&app){
        Fixture::enabled=true;Fixture::events.clear();app.start();unsigned bindings=0,advances=0;
        for(const auto&e:Fixture::events){bindings+=e.kind==Event::Bind;advances+=e.kind==Event::Advance;}
        check(bindings==(app.night?(app.battle?2u:1u):0u),"LightON must bind each enabled projection once on start");
        check(advances==0,"Solver-only startup advanced the projected headlight scene");
        check(app.originalRaceOwnerFrame==0&&app.originalHandling,"Fixture did not enter original driving");
    };
    auto verifyTick=[&](App&app){
        std::vector<const void*> publications;unsigned primaryAdvances=0,rivalAdvances=0;
        bool advancing=false;
        for(const auto&e:Fixture::events){
            check(e.kind!=Event::Bind,"Steady lights rebound their road query");
            if(e.kind==Event::Publish){check(!advancing,"Headlight publication occurred after scene advance");publications.push_back(e.owner);continue;}
            advancing=true;check(!publications.empty(),"Scene advance omitted prior ACar publication");
            const bool rival=e.owner!=publications.front();if(rival)++rivalAdvances;else ++primaryAdvances;
            const auto&actors=app.originalSession.publishedActors();
            check(e.matrix==actorMatrix(rival?actors.secondary0C8FF430:actors.player0C8FF388,trig),"Projector used prior/interpolated/body-adjusted matrix instead of current ACar pose");
            for(const auto&result:e.results){
                const auto&r=e.recordsAfter.at(result.point);const auto&q=result.query;
                if(result.hit){++roadHits;const float height=q.f(36)+(-q.f(24)+.1f);
                    check(r[2]==std::bit_cast<unsigned>(height),"Projected headlight point does not follow the actual collision height plus source lift");
                    if(q.u(28)&0x8000)check(r[6]==0x007f7f7f&&r[7]==0x007f7f7f,"Projected special road flags lost source colors");
                }else{++roadMisses;check(r[2]==std::bit_cast<unsigned>(e.matrix[13])&&r[6]==0&&r[7]==0,"Road miss failed to suppress this source point");}
            }
        }
        const unsigned owners=app.night?(app.battle?2u:1u):0u;
        check(publications.size()==owners&&primaryAdvances==unsigned(app.night),"Player projection lifecycle count differs from source");
        const unsigned rivalPasses=app.night&&app.battle?(app.drivingView==OriginalDrivingView::Bumper?2u:1u):0u;
        check(rivalAdvances==rivalPasses,"Rival projection did not follow active primary/rear-view scope");
    };
    std::ofstream csv(output/"application.csv");csv<<"label,course,direction,night,frame,view,projection_ranges,changed_pixels\n";
    auto capture=[&](App&app,const std::string&label,bool expectEffect){
        app.clock.reset();const auto eventCount=Fixture::events.size();
        const auto state=app.originalSession.actor().words;const auto seed=app.originalSession.contactCompletion().randomSeed0C37C778;
        check(app.render(0,true),"Actual headlight App render failed");
        const auto file=output/(label+".bmp");check(app.renderer.saveBitmap(file.wstring()),"Cannot save App headlight capture");
        const auto baseline=readBytes(file);const FrozenDraw frozen(app,true);
        check(frozen.draw(app,app.raceMesh),"Frozen actual headlight draw failed");
        const auto same=output/(label+"-direct.bmp");check(app.renderer.saveBitmap(same.wstring())&&readBytes(same)==baseline,"Frozen direct draw changed actual App view/HUD/mirror");
        Mesh without=app.raceMesh;unsigned ranges=0;
        for(auto&r:without.ranges)if(projectorRange(r)){++ranges;
            check(!r.courseLighting&&r.emissive,"Projected material inherited native course/beam lighting");
            check(r.pcw==0x8a00071eu&&r.isp==0x93800000u&&r.gmp==0x222u,"Projected strip material changed original source state");
            check(r.texture==app.projectedHeadlightTextureBase&&r.count==24,"Projected source image binding or eight triangles changed");
            check(r.viewMask==(ranges==1?1u:3u),"Player/rival projection main/rear scope changed");
            r.count=0;
        }
        check(!app.renderer.vehicleLights&&!app.renderer.opponentLights,"Projected headlights retained native beam double-light");
        check(frozen.draw(app,without),"Frozen projector-excluded draw failed");
        const auto removed=output/(label+"-projection-disabled.bmp");check(app.renderer.saveBitmap(removed.wstring()),"Cannot save projector-excluded capture");
        const auto changed=changedPixels(baseline,readBytes(removed));
        if(expectEffect&&changed<=100){std::ofstream debug(output/(label+"-projection-state.txt"));
            debug<<"Vehicle "<<app.vehicle.position.x<<' '<<app.vehicle.position.y<<' '<<app.vehicle.position.z<<" yaw "<<app.vehicle.yaw<<" progress "<<app.progress<<'\n';
            unsigned point=0;for(const auto&r:app.playerProjectedHeadlight.projection().records()){const auto&q=app.playerProjectedHeadlight.projection().queries()[point];
                debug<<point++<<" xyz "<<std::bit_cast<float>(r[1])<<' '<<std::bit_cast<float>(r[2])<<' '<<std::bit_cast<float>(r[3])<<" color "<<std::hex<<r[6]<<' '<<r[7]<<std::dec
                    <<" tri "<<std::bit_cast<int>(q.u(60))<<" hit "<<q.f(12)<<' '<<q.f(16)<<' '<<q.f(20)<<" distance "<<q.f(24)<<'\n';}}
        if(expectEffect)check(ranges>0&&changed>100,"Source projection did not visibly illuminate the actual road");
        else check(ranges==0&&changed==0,"Daytime projection changed scene pixels");
        check(Fixture::events.size()==eventCount&&state==app.originalSession.actor().words&&seed==app.originalSession.contactCompletion().randomSeed0C37C778,"Rendering changed source headlight/driving lifecycle");
        csv<<label<<','<<app.courseIndex<<','<<app.reverse<<','<<app.night<<','<<app.originalRaceOwnerFrame<<','<<int(app.drivingView)<<','<<ranges<<','<<changed<<'\n';csv.flush();
    };
    auto pairedTick=[&](App&app,App&control,unsigned frame){
        const auto input=scriptedInput(app,frame);Fixture::events.clear();Fixture::enabled=true;app.simulate(input);verifyTick(app);
        Fixture::events.clear();Fixture::enabled=false;control.simulate(input);Fixture::enabled=true;compareDriving(app,control);++pairedTicks;
    };
    for(bool reverse:{false,true}){
        auto app=initialize(3,reverse,true),control=initialize(3,reverse,true);start(*app);start(*control);compareDriving(*app,*control);
        check(app->renderer.initialize(nullptr,960,720,true),"Offscreen headlight renderer initialization failed");
        const auto label="natural-akina-d"+std::to_string(reverse);
        for(unsigned frame=0;frame<600;++frame){pairedTick(*app,*control,frame);if(frame==179)capture(*app,label+"-go180",true);}
        capture(*app,label+"-bumper600",true);app->drivingView=OriginalDrivingView::Chase;capture(*app,label+"-chase600",true);
        app->paused=true;app->clock.reset();check(app->render(0,true),"Paused headlight baseline failed");
        const auto before=output/(label+"-pause-before.bmp"),after=output/(label+"-pause-after.bmp");
        check(app->renderer.saveBitmap(before.wstring()),"Cannot capture paused headlight baseline");
        Fixture::events.clear();const auto records=app->playerProjectedHeadlight.projection().records();
        for(unsigned repaint=0;repaint<4;++repaint){app->commands(1./60.);if(!app->menu&&!app->paused)app->clock.advance(1./60.,[&]{app->simulate(app->driver());});else app->clock.reset();
            check(app->render(1./60.,true),"Paused headlight repaint failed");}
        check(app->renderer.saveBitmap(after.wstring())&&readBytes(before)==readBytes(after),"Paused headlight repaint changed pixels");
        check(Fixture::events.empty()&&records==app->playerProjectedHeadlight.projection().records(),"Paused render advanced projection state");compareDriving(*app,*control);
        start(*app);start(*control);compareDriving(*app,*control);
        check(app->playerProjectedHeadlight.projection().records()!=records,"Restart did not restore authored projection state");
        app->night=false;control->night=false;start(*app);start(*control);
        for(unsigned frame=0;frame<240;++frame)pairedTick(*app,*control,frame);
        capture(*app,label+"-day240",false);
        std::cout<<"Verified "<<label<<"840 paired ticks, cameras, pause, restart and day.\n"<<std::flush;
    }
    auto battle=initialize(4,true,true,true),control=initialize(4,true,true,true);start(*battle);start(*control);compareDriving(*battle,*control);
    check(battle->renderer.initialize(nullptr,960,720,true),"Battle headlight renderer initialization failed");
    for(unsigned frame=0;frame<600;++frame){
        if(frame==300){battle->drivingView=control->drivingView=OriginalDrivingView::Chase;}
        pairedTick(*battle,*control,frame);if(frame==239)capture(*battle,"battle-happo-bumper-mirror240",true);
    }
    capture(*battle,"battle-happo-chase600",true);
    // Exercise LightOFF/ON against the existing source body's complete road
    // query. This is a controlled activation edge, not invented gameplay input.
    const auto&actor=battle->originalSession.actor();const std::array<float,3> position{actor.f(0),actor.f(4),actor.f(8)};
    const auto bodyQuery=battle->playerBody.query().words;
    Fixture::events.clear();battle->playerProjectedHeadlight.request(true,battle->originalSession.collision(),battle->playerBody.query(),position);
    check(Fixture::events.empty(),"Repeated LightON rebound an enabled projection");
    battle->playerProjectedHeadlight.request(false,battle->originalSession.collision(),battle->playerBody.query(),position);
    battle->playerProjectedHeadlight.request(true,battle->originalSession.collision(),battle->playerBody.query(),position);
    check(Fixture::events.size()==1&&Fixture::events[0].kind==Event::Bind,"LightOFF/ON did not bind exactly once");
    const auto&binding=Fixture::events[0].results[0].query;
    for(unsigned i=0;i<3;++i)check(binding.f(32+4*i)==position[i]&&binding.f(44+4*i)==position[i],"LightON did not refresh both source position histories");
    check(battle->playerBody.query().words==bodyQuery,"LightON modified body/handling query history");
    battle->returnToCourseSelection();Fixture::events.clear();check(battle->renderMenu(0),"Menu after race failed");
    check(Fixture::events.empty(),"Menu advanced race projection");
    const auto menuBefore=output/"menu-retained-projection.bmp",menuAfter=output/"menu-reset-projection.bmp";
    check(battle->renderer.saveBitmap(menuBefore.wstring()),"Cannot capture menu projection isolation baseline");
    battle->playerProjectedHeadlight.reset();battle->rivalProjectedHeadlight.reset();
    check(battle->renderMenu(0)&&battle->renderer.saveBitmap(menuAfter.wstring())&&readBytes(menuBefore)==readBytes(menuAfter),"Retained race projection changed menu pixels");
    // Actual App tail boundary for enemy29's temporary headlight shutdown.
    // These are explicitly test-owned rule/counter inputs, not a claim that
    // the normal scripted drive below traverses this distant race section.
    auto blackout=initialize(7,false,true,true);
    original::selectOriginalRival(blackout->frontend.battleProfile,29);start(*blackout);
    check(blackout->battleProfile.u(24)==29&&blackout->rivalProjectedHeadlight.enabled(),"Enemy29 initial projection fixture failed");
    check(blackout->rivalLightState.frames1732==61,"Enemy29 initial ROM-backed request plus sixty warmup calls must leave counter61");
    const auto* rivalOwner=&blackout->rivalProjectedHeadlight.projection();
    const auto* playerOwner=&blackout->playerProjectedHeadlight.projection();
    auto motor=blackout->rivalPresentation.headlightState();motor.visible=true;motor.counter=40;motor.fraction=1;
    blackout->rivalPresentation.restoreHeadlightState(motor);
    auto&privateRuleState=const_cast<original::OriginalRaceRuleState&>(blackout->originalRace.state());privateRuleState.progress.index=1440;
    blackout->rivalLightState.frames1732=240;blackout->projectedLightPriorAdvantage=0;
    const auto unchangedDrive=blackout->originalSession.vehicle().drive.words;
    const auto unchangedSeed=blackout->originalSession.contactCompletion().randomSeed0C37C778;
    auto expectOrder=[&](std::initializer_list<std::pair<Event::Kind,const void*>> expected){
        check(Fixture::events.size()==expected.size(),"Enemy29 boundary event count changed");unsigned i=0;
        for(const auto&[kind,owner]:expected){check(Fixture::events[i].kind==kind&&Fixture::events[i].owner==owner,"Enemy29 publication/request/advance order changed");++i;}
    };
    Fixture::events.clear();blackout->advanceProjectedHeadlights();
    expectOrder({{Event::Publish,playerOwner},{Event::Publish,rivalOwner},{Event::Advance,playerOwner}});
    check(!blackout->rivalProjectedHeadlight.enabled()&&blackout->rivalLightState.frames1732==241,"Enemy29 source shutdown threshold not applied");
    check(blackout->rivalPresentation.headlightState().visible&&blackout->rivalPresentation.headlightState().counter==40,"Outer light request incorrectly waited for or changed popup motor visibility");
    Fixture::events.clear();blackout->advanceProjectedHeadlights();expectOrder({{Event::Publish,playerOwner},{Event::Advance,playerOwner}});
    blackout->projectedLightPriorAdvantage=20;
    Fixture::events.clear();blackout->advanceProjectedHeadlights();
    expectOrder({{Event::Publish,playerOwner},{Event::Bind,rivalOwner},{Event::Advance,playerOwner},{Event::Advance,rivalOwner},{Event::Advance,rivalOwner}});
    check(blackout->rivalProjectedHeadlight.enabled()&&blackout->rivalLightState.frames1732==0,"Enemy29 off-to-on tail boundary failed");
    Fixture::events.clear();blackout->advanceProjectedHeadlights();
    expectOrder({{Event::Publish,playerOwner},{Event::Publish,rivalOwner},{Event::Advance,playerOwner},{Event::Advance,rivalOwner},{Event::Advance,rivalOwner}});
    check(blackout->originalSession.vehicle().drive.words==unchangedDrive&&blackout->originalSession.contactCompletion().randomSeed0C37C778==unchangedSeed,"Enemy29 presentation request changed driving/RNG");
    // Representative collision surfaces are selected from real course shape.
    // Reinitializing the private session at those points is a placement fixture,
    // separate from the paired normal-input runs above. No saved profile changes.
    for(bool reverse:{false,true}){
        auto app=initialize(3,reverse,true);start(*app);
        check(app->renderer.initialize(nullptr,960,720,true),"Road-shape headlight renderer initialization failed");
        const auto choices=chooseRoadSamples(app->course);
        check(std::abs(choices[1].sample.grade)>.03f&&std::abs(choices[2].sample.curvature)>.01f,"Road fixture lacks meaningful slope/curve samples");
        for(const auto&choice:choices){
            const auto selection=app->originalSession.selection();const auto&p=choice.sample.center;const auto&t=choice.sample.tangent;
            app->originalSession.reset(isolated,selection,{p.x,p.y,p.z},{std::atan(choice.sample.grade),wrapAngle(std::atan2(t.x,t.z)-pi),0});
            // reset() alone is the solver's constructor boundary. Its actor
            // publication must complete before camera/mesh comparison, just
            // as normal App::start executes the original sixty warmup calls.
            app->playerBody.reset();app->originalInput={};
            const auto frozenInputs=original::adaptOriginalHostInput(app->originalInput,{},true,false,0);
            original::warmupOriginalRaceSession(app->originalSession,frozenInputs,app->originalSession.platformFrame(),0,
                [&](const auto&){app->projectOriginalPose(false);});
            app->previous=app->vehicle;
            app->previousPitch=app->bodyPitch;app->previousRoll=app->bodyRoll;app->previousWheelPose=app->wheelPose;
            app->progress=choice.sample.distance;app->segment=choice.sample.segmentIndex;
            app->originalCamera.reset();app->bumperCamera.reset();app->advanceOriginalCamera();
            app->initializeProjectedHeadlights();
            for(unsigned tick=0;tick<2;++tick){Fixture::events.clear();app->advanceProjectedHeadlights();verifyTick(*app);}
            const auto label=std::string("placed-akina-d")+std::to_string(reverse)+"-"+choice.name;
            app->drivingView=OriginalDrivingView::Bumper;capture(*app,label+"-bumper",true);
            app->drivingView=OriginalDrivingView::Chase;capture(*app,label+"-chase",true);
        }
    }
    check(roadHits>1000,"Actual projection fixture did not exercise road contacts");
    check(snapshot(realRoot/"userdata")==realSaves&&snapshot(isolated/"userdata")==privateSaves,"Driver save files changed during headlight fixture");
    std::ostringstream report;report<<"PASS "<<checks<<" checks / "<<pairedTicks<<" paired App ticks; "<<roadHits<<" actual road hits / "<<roadMisses
        <<" misses; original activation/publication/working updates, main/rear scope, source texture/material, direct App pixel identity, projected-road differences, pause/restart/day/menu, both directions and controlled flat/slope/curve poses. "
        <<realSaves.size()<<" real save files unchanged in bytes/timestamps. Offscreen WARP, no audio device.\n";
    std::cout<<report.str();std::ofstream(output/"result.txt")<<report.str();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}


