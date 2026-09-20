// Actual-App fixture: intercept only the pure original car-light boundaries.
// The paired control omits their state mutations; neither path alters solver,
// input, source RNG, road queries, projected geometry or saved user data.
#include "../src/original_car_lighting.h"
#include "../src/original_car_light_gain.h"
#include <vector>
namespace idas3::original {
struct ApplicationCarLightEvent {
    enum Kind { Publish,Ambient,Enable } kind{};
    OriginalCarLighting* owner{};
    OriginalCarLighting before{},after{};
    OriginalLightMatrix matrix{};
    OriginalCarAmbientInputs ambient{};
    bool enabled{};
    unsigned projectorEvents{};
};
struct ApplicationCarLightFixture {
    inline static bool enabled=true;
    inline static std::vector<ApplicationCarLightEvent> events;
    inline static unsigned(*projectorEventCount)()=nullptr;
    inline static void(*publicationCheck)(OriginalCarLighting*)=nullptr;
    static unsigned projectorEvents(){return projectorEventCount?projectorEventCount():0;}
};
void applicationPublishCarLight(OriginalCarLighting&state,const OriginalLightMatrix&matrix){
    if(ApplicationCarLightFixture::publicationCheck)ApplicationCarLightFixture::publicationCheck(&state);
    ApplicationCarLightEvent e;e.kind=ApplicationCarLightEvent::Publish;e.owner=&state;e.before=state;e.matrix=matrix;
    if(ApplicationCarLightFixture::enabled)publishOriginalCarLight(state,matrix);
    e.after=state;e.projectorEvents=ApplicationCarLightFixture::projectorEvents();ApplicationCarLightFixture::events.push_back(e);
}
void applicationCarLightAmbient(OriginalCarLighting&state,const OriginalCarAmbientInputs&inputs){
    ApplicationCarLightEvent e;e.kind=ApplicationCarLightEvent::Ambient;e.owner=&state;e.before=state;e.ambient=inputs;
    if(ApplicationCarLightFixture::enabled)updateOriginalCarAmbient(state,inputs);
    e.after=state;e.projectorEvents=ApplicationCarLightFixture::projectorEvents();ApplicationCarLightFixture::events.push_back(e);
}
void applicationEnableCarLight(OriginalCarLighting&state,bool enabled){
    ApplicationCarLightEvent e;e.kind=ApplicationCarLightEvent::Enable;e.owner=&state;e.before=state;e.enabled=enabled;
    if(ApplicationCarLightFixture::enabled)setOriginalCarLightEnabled(state,enabled);
    e.after=state;e.projectorEvents=ApplicationCarLightFixture::projectorEvents();ApplicationCarLightFixture::events.push_back(e);
}
struct ApplicationGainEvent {std::int32_t index{};float fraction{},value{};unsigned carEvents{},projectorEvents{};};
class ApplicationCarLightGain:public OriginalCarLightGain {
public:
    inline static std::vector<ApplicationGainEvent> events;
    static ApplicationCarLightGain load(const std::filesystem::path&root,unsigned course,bool night,bool wet,bool reverse){
        ApplicationCarLightGain result;static_cast<OriginalCarLightGain&>(result)=OriginalCarLightGain::load(root,course,night,wet,reverse);return result;
    }
    float evaluate(std::int32_t index,float fraction)const{
        const auto value=OriginalCarLightGain::evaluate(index,fraction);
        events.push_back({index,fraction,value,unsigned(ApplicationCarLightFixture::events.size()),ApplicationCarLightFixture::projectorEvents()});
        return ApplicationCarLightFixture::enabled?value:1.f;
    }
};
}
#define publishOriginalCarLight applicationPublishCarLight
#define updateOriginalCarAmbient applicationCarLightAmbient
#define setOriginalCarLightEnabled applicationEnableCarLight
#define OriginalCarLightGain ApplicationCarLightGain
#define main includedProjectedHeadlightApplicationFixture
#include "check_projected_headlight_application.cpp"
#undef main
#undef publishOriginalCarLight
#undef updateOriginalCarAmbient
#undef setOriginalCarLightEnabled
#undef OriginalCarLightGain

namespace {
bool equalCarLightSet(const original::OriginalCourseLighting&a,const original::OriginalCourseLighting&b){
    const auto x=original::originalCourseLightingPacket(a,original::originalLightIdentityMatrix);
    const auto y=original::originalCourseLightingPacket(b,original::originalLightIdentityMatrix);
    return x.count==y.count&&x.glm==y.glm&&x.lights==y.lights;
}
// Publication callback is a test-only function pointer so source helpers need
// no knowledge of App. These expected path words belong only to this fixture.
App* coordinateApp=nullptr;
std::array<std::optional<original::OriginalPathCoordinate>,2> expectedCoordinates;
unsigned coordinateChecks=0;
void checkPublishedCoordinate(original::OriginalCarLighting*owner){
    if(!coordinateApp)return;auto&app=*coordinateApp;const unsigned car=owner==&app.rivalCarLight?1u:0u;
    auto&expected=expectedCoordinates[car];if(!expected){const auto condition=app.originalSession.selection().physics.conditionCode;
        expected=original::OriginalPathCoordinate{original::originalRaceRuleRow(original::originalRaceRuleRowIndex(condition,app.battle?0u:2u)).startIndex,0};}
    const auto&actors=app.originalSession.publishedActors();const auto&actor=car?actors.secondary0C8FF430:actors.player0C8FF388;
    app.originalPath.project({std::bit_cast<float>(actor[0]),std::bit_cast<float>(actor[1]),std::bit_cast<float>(actor[2])},*expected);
    const auto&actual=car?app.rivalLightCoordinate:app.playerLightCoordinate;
    ++coordinateChecks;if(actual.index!=expected->index||std::bit_cast<unsigned>(actual.fraction)!=std::bit_cast<unsigned>(expected->fraction))
        throw std::runtime_error("Private car-light path did not use current raw actor XYZ/source start index/retained coordinate");
}
struct DrivingSnapshot {
    std::vector<std::uint32_t> words;
    explicit DrivingSnapshot(const App&app){
        const auto&session=app.originalSession;
        const auto append=[&](const auto&values){words.insert(words.end(),values.begin(),values.end());};
        append(session.vehicle().drive.words);append(session.actor().words);
        append(session.publishedActors().player0C8FF388);append(session.publishedActors().secondary0C8FF430);
        append(session.recovery().drive0C9009F0.words);append(session.recovery().actor0C8FF580);
        const auto&contact=session.contactCompletion();append(contact.cues0C900E5C);append(contact.snapshot0CAA9718);
        append(app.playerBody.query().words);append(app.rivalBody.query().words);
        words.insert(words.end(),{contact.randomSeed0C37C778,contact.elapsedFrames0C900E84,contact.steeringMask0C900EBC,
            unsigned(session.platformFrame()),unsigned(app.originalRaceOwnerFrame),unsigned(app.race.phase),unsigned(app.race.ticks),
            unsigned(app.race.remaining6000),unsigned(app.race.elapsed6000),unsigned(app.race.countdown),unsigned(app.courseLightPathIndex)});
    }
    bool operator==(const DrivingSnapshot&)const=default;
};
}

int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("Marked isolated root and output directory required");
    const auto isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"CAR_LIGHTING_TEST_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked or non-work car-light root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Car-light userdata must not redirect to real saves");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),privateSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0,pairedTicks=0,publishedSpots=0,ambientUpdates=0;
    auto check=[&](bool value,const char*why){++checks;if(!value)throw std::runtime_error(why);};
    using Fixture=original::ApplicationCarLightFixture;using Event=original::ApplicationCarLightEvent;
    using Projector=original::ApplicationHeadlightFixture;
    Fixture::projectorEventCount=[](){return unsigned(Projector::events.size());};
    const auto trig=original::OriginalFscaTable::load(isolated/"data/original_physics/fsca_table.bin");
    using Gain=original::ApplicationCarLightGain;
    Fixture::publicationCheck=checkPublishedCoordinate;
    auto clearEvents=[](){Fixture::events.clear();Projector::events.clear();Gain::events.clear();};
    std::map<const App*,std::array<std::optional<original::OriginalPathCoordinate>,2>> coordinates;
    auto activateCoordinates=[&](App&app,bool reset=false){if(reset)coordinates[&app]={};coordinateApp=&app;expectedCoordinates=coordinates[&app];};
    auto retainCoordinates=[&](){coordinates[coordinateApp]=expectedCoordinates;};
    auto initialize=[&](unsigned course,bool reverse,bool night,bool wet,bool battle,unsigned enemy=19){
        auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
        app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
        app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
        app->frontend.car=0;app->automatic=app->frontend.automatic=true;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.gameMode=battle?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
        if(battle){app->frontend.battleProfile.setu(0,0);original::selectOriginalRival(app->frontend.battleProfile,enemy);}
        app->courseIndex=int(course);app->reverse=reverse;app->night=night;app->wet=wet;app->drivingView=OriginalDrivingView::Chase;
        return app;
    };
    auto compareDriving=[&](const App&a,const App&b){
        check(DrivingSnapshot(a)==DrivingSnapshot(b),"Car-light mutation changed original driver/RNG/query/race state");
        const auto&x=a.originalSession.contactCompletion();const auto&y=b.originalSession.contactCompletion();
        check(x.impactPositions==y.impactPositions&&x.impactFrames==y.impactFrames&&x.positionCursor==y.positionCursor&&x.frameCursor==y.frameCursor,
            "Car-light mutation changed contact history");
        check(a.playerProjectedHeadlight.projection().records()==b.playerProjectedHeadlight.projection().records()&&
              a.rivalProjectedHeadlight.projection().records()==b.rivalProjectedHeadlight.projection().records(),"Car-light update changed projected working geometry");
    };
    auto verifySets=[&](const App&app){
        check(app.raceLighting&&app.raceLightSets,"Car-light scope snapshots missing");
        const auto expected=original::composeOriginalRaceLighting(*app.raceLighting,app.playerCarLight,app.rivalCarLight,app.carLightingSetup());
        check(equalCarLightSet(expected.course,app.raceLightSets->course)&&equalCarLightSet(expected.player,app.raceLightSets->player)&&
              equalCarLightSet(expected.rival,app.raceLightSets->rival)&&expected.hasRival==app.raceLightSets->hasRival,"App scope snapshot differs from source registration");
        check(app.playerCarLight.ownSpot.enabled==app.playerProjectedHeadlight.enabled()&&app.rivalCarLight.ownSpot.enabled==app.rivalProjectedHeadlight.enabled(),
            "Embedded SPOT and projected outer light81 disagree");
    };
    auto verifyEvents=[&](const App&app,bool startup){
        unsigned playerPubs=0,rivalPubs=0,playerAmbients=0,rivalAmbients=0;
        const Event*lastPlayer=nullptr,*lastRival=nullptr;
        for(const auto&e:Fixture::events){const bool rival=e.owner==&app.rivalCarLight;
            check(rival||e.owner==&app.playerCarLight,"Unknown App car-light owner");
            if(e.kind==Event::Publish){++publishedSpots;if(rival){++rivalPubs;lastRival=&e;}else{++playerPubs;lastPlayer=&e;}
                for(unsigned axis=0;axis<3;++axis){double value=double(e.matrix[axis])*0.;value+=double(e.matrix[4+axis])*(-.5);value+=double(e.matrix[8+axis])*(-3.);value+=e.matrix[12+axis];
                    check(e.after.ownSpot.position[axis]==float(value),"Embedded SPOT translation differs from world*T(0,-.5,-3)");
                    check(e.after.ownSpot.incomingDirection[axis]==e.matrix[8+axis],"Embedded SPOT did not retain world third column");}
            }else if(e.kind==Event::Ambient){++ambientUpdates;if(rival)++rivalAmbients;else ++playerAmbients;
                check(e.ambient.course==unsigned(app.courseIndex)&&e.ambient.wet==app.wet&&e.ambient.night==app.night&&e.ambient.rival==rival,
                    "Car ambient condition/owner arguments do not match App");
                check(e.ambient.courseAmbient==app.raceCarAmbient,"Car ambient used course ARRAY rather than source HEADER RGB");
                check(!rival||e.ambient.rivalLightBeforeRequest==e.before.ownSpot.enabled,"Rival ambient did not consume prior outer lamp81");
                auto expectedAmbient=e.ambient.courseAmbient;
                if(e.ambient.course==6&&e.ambient.wet)for(auto&v:expectedAmbient)v=.6f-v;
                if(rival&&e.ambient.night&&!e.ambient.rivalLightBeforeRequest){const auto factor=e.ambient.signedAdvantage>-5?std::clamp(-.2f*e.ambient.signedAdvantage,0.f,1.f):1.f;
                    for(auto&v:expectedAmbient)v*=factor;}
                check(e.after.ambient==expectedAmbient&&e.after.gain==e.before.gain,"App ambient differs from source formula or altered independent gain");
                check(rival?rivalPubs==rivalAmbients:playerPubs==playerAmbients,"Ambient did not follow each source pose publication");
                if(!startup){const unsigned expectedProjectors=unsigned(app.playerProjectedHeadlight.enabled())+(rival?unsigned(e.ambient.rivalLightBeforeRequest):0u);
                    check(e.projectorEvents==expectedProjectors,"Car ambient/projector publication ordering changed");}
            }
        }
        check(playerPubs==(startup?61u:1u)&&playerAmbients==playerPubs,"Player publication/ambient source count changed");
        check(rivalPubs==(app.rivalVisible?playerPubs:0u)&&rivalAmbients==rivalPubs,"Rival publication/ambient source count changed");
        check(lastPlayer&&lastPlayer->matrix==actorMatrix(app.originalSession.publishedActors().player0C8FF388,trig),"Car light used stale/interpolated/body-only player matrix");
        if(app.rivalVisible)check(lastRival&&lastRival->matrix==actorMatrix(app.originalSession.publishedActors().secondary0C8FF430,trig),"Car light used stale/interpolated/body-only rival matrix");
        const unsigned expectedGains=!startup&&!app.night?(app.rivalVisible?2u:1u):0u;
        check(Gain::events.size()==expectedGains,"Car gain ran during night/warmup or missed normal owner update");
        for(unsigned i=0;i<Gain::events.size();++i){const auto&e=Gain::events[i];const auto&coord=i?app.rivalLightCoordinate:app.playerLightCoordinate;
            check(e.index==coord.index&&e.fraction==coord.fraction,"Gain did not consume private current car path coordinate");
            check(e.carEvents==Fixture::events.size(),"Gain evaluation preceded pose/ambient/light requests");
            check(e.projectorEvents==0,"Day gain evaluation followed projected working queries");
            const auto values=app.carLightGain.values();float expected=1;
            if(!values.empty()){auto index=e.index;auto fraction=e.fraction;if(app.reverse){index=int(values.size())-index-2;fraction=1-fraction;}
                expected=std::clamp(std::fma(values[index],1-fraction,values[index+1]*fraction),0.f,1.f);}
            check(e.value==expected&&(i?app.rivalCarLight:app.playerCarLight).gain==expected,"App gain differs from authored interpolation");
        }
        if(startup)check(app.playerCarLight.gain==1&&app.rivalCarLight.gain==1,"Warmups changed initial ARRAY gain");
        verifySets(app);
    };
    auto start=[&](App&app,bool active){Fixture::enabled=active;clearEvents();activateCoordinates(app,true);app.start();retainCoordinates();if(active)verifyEvents(app,true);};
    auto pairedTick=[&](App&a,App&b,unsigned frame){const auto input=scriptedInput(a,frame);
        Fixture::enabled=true;clearEvents();activateCoordinates(a);a.simulate(input);retainCoordinates();verifyEvents(a,false);
        Fixture::enabled=false;clearEvents();activateCoordinates(b);b.simulate(input);retainCoordinates();Fixture::enabled=true;compareDriving(a,b);++pairedTicks;
    };
    std::ofstream csv(output/"application.csv");csv<<"label,course,night,wet,player_ranges,rival_ranges,course_ranges,changed_pixels\n";
    auto capture=[&](App&app,const std::string&label){
        app.clock.reset();clearEvents();const auto state=DrivingSnapshot(app);
        check(app.render(0,true),"Actual car-light render failed");check(Fixture::events.empty()&&Gain::events.empty(),"Race repaint advanced source car-light state");
        check(app.renderer.courseLighting==&app.raceLightSets->course&&app.renderer.playerLighting==&app.raceLightSets->player&&
              app.renderer.rivalLighting==(app.rivalVisible?&app.raceLightSets->rival:nullptr),"Renderer did not bind three composed ARRAY scopes");
        const auto file=output/(label+".bmp");check(app.renderer.saveBitmap(file.wstring()),"Cannot save actual car-light image");const auto image=readBytes(file);
        const FrozenDraw frozen(app,true);check(frozen.draw(app,app.raceMesh),"Frozen source lighting draw failed");
        const auto direct=output/(label+"-direct.bmp");check(app.renderer.saveBitmap(direct.wstring())&&readBytes(direct)==image,"Frozen draw changed actual App camera/HUD/mirror");
        unsigned player=0,rival=0,course=0,projection=0;
        for(const auto&r:app.raceMesh.ranges){check(r.carLighting<=2,"Invalid App car-light scope");player+=r.carLighting==1;rival+=r.carLighting==2;course+=r.courseLighting;
            check(!r.carLighting||!r.courseLighting,"Car range also inherited course light scope");
            if(projectorRange(r)){++projection;check(!r.carLighting&&!r.courseLighting,"Projected headlight inherited a car ARRAY");}}
        check((player!=0)==(app.drivingView==OriginalDrivingView::Chase)&&course!=0,"Player/course ranges do not follow camera scope");
        check((rival!=0)==app.rivalVisible,"Rival body/plate ranges lack source ARRAY scope");
        check(!app.renderer.vehicleLights&&!app.renderer.opponentLights,"Native beams still enabled alongside source lights");
        app.renderer.playerLighting=app.renderer.rivalLighting=nullptr;app.renderer.courseLighting=&*app.raceLighting;
        check(frozen.draw(app,app.raceMesh),"Legacy-light comparison draw failed");const auto without=output/(label+"-without-car-arrays.bmp");
        check(app.renderer.saveBitmap(without.wstring()),"Cannot save car-light comparison");const auto difference=changedPixels(image,readBytes(without));
        check(difference>100,"Actual visible source car ARRAYs did not change image");
        check(app.render(0,true)&&Fixture::events.empty()&&Gain::events.empty()&&DrivingSnapshot(app)==state,"Frozen lighting render changed simulation state");
        csv<<label<<','<<app.courseIndex<<','<<app.night<<','<<app.wet<<','<<player<<','<<rival<<','<<course<<','<<difference<<'\n';
    };
    struct Scene{const char*label;unsigned course;bool reverse,night,wet,battle;unsigned enemy=19;};
    const std::array scenes{Scene{"akina-night",3,false,true,false,false},Scene{"akina-reverse-wet",3,true,true,true,false},
        Scene{"happo-battle",4,true,true,false,true},Scene{"myogi-day",0,false,false,false,false},Scene{"shomaru-wet",6,false,true,true,false},
        Scene{"akina-day-battle",3,false,false,false,true,10},Scene{"akina-day-reverse-wet",3,true,false,true,false}};
    for(const auto&scene:scenes){auto app=initialize(scene.course,scene.reverse,scene.night,scene.wet,scene.battle,scene.enemy),control=initialize(scene.course,scene.reverse,scene.night,scene.wet,scene.battle,scene.enemy);
        start(*app,true);start(*control,false);Fixture::enabled=true;compareDriving(*app,*control);
        check(app->courseIndex==int(scene.course)&&app->night==scene.night&&app->reverse==scene.reverse&&app->wet==scene.wet,"Source rival selection remapped the requested coverage scene");
        check(app->renderer.initialize(nullptr,960,720,true),"Car-light WARP renderer initialization failed");
        for(unsigned frame=0;frame<420;++frame)pairedTick(*app,*control,frame);
        capture(*app,std::string(scene.label)+"-chase420");
        if(scene.battle){app->drivingView=OriginalDrivingView::Bumper;capture(*app,std::string(scene.label)+"-bumper420");}
        if(!app->night&&app->carLightGain.hasTable()){
            // Controlled presentation-coordinate fixture, separate from the
            // normal 420-tick drive. Exercise an actual authored shadow region
            // without teleporting/modifying a driver or its rule coordinate.
            const auto values=app->carLightGain.values();const auto lowest=std::min_element(values.begin(),values.end()-1);
            check(*lowest<.95f,"Day source stream has no discriminating shade region");
            const auto oldPlayer=app->playerLightCoordinate,oldRival=app->rivalLightCoordinate;
            const int sourceIndex=int(lowest-values.begin());const int index=app->reverse?int(values.size())-sourceIndex-2:sourceIndex;
            const auto driverBefore=DrivingSnapshot(*app);
            app->playerLightCoordinate={index,app->reverse?1.f:0.f};app->rivalLightCoordinate=app->playerLightCoordinate;
            clearEvents();app->advanceCarLightGain();check(app->playerCarLight.gain==*lowest&&(!app->rivalVisible||app->rivalCarLight.gain==*lowest),"Authored shade region did not reach both car ARRAY gains");
            verifySets(*app);check(DrivingSnapshot(*app)==driverBefore,"Presentation shade coordinate changed original driving/RNG/rules");
            app->drivingView=OriginalDrivingView::Chase;capture(*app,std::string(scene.label)+"-authored-shade");
            app->playerLightCoordinate=oldPlayer;app->rivalLightCoordinate=oldRival;clearEvents();app->advanceCarLightGain();
        }
        app->paused=true;clearEvents();const auto paused=DrivingSnapshot(*app);const auto lights=*app->raceLightSets;
        check(app->render(0,true),"Paused car-light image failed");const auto before=output/(std::string(scene.label)+"-pause-before.bmp");check(app->renderer.saveBitmap(before.wstring()),"Paused capture failed");
        for(unsigned i=0;i<6;++i)check(app->render(1./60,true),"Repeated pause render failed");
        const auto after=output/(std::string(scene.label)+"-pause-after.bmp");check(app->renderer.saveBitmap(after.wstring())&&readBytes(before)==readBytes(after),"Paused car-light pixels changed");
        check(Fixture::events.empty()&&Gain::events.empty()&&DrivingSnapshot(*app)==paused&&equalCarLightSet(app->raceLightSets->player,lights.player)&&equalCarLightSet(app->raceLightSets->rival,lights.rival),"Paused rendering consumed car-light state");
        start(*app,true);start(*control,false);Fixture::enabled=true;compareDriving(*app,*control);
        app->returnToCourseSelection();clearEvents();check(app->renderMenu(0),"Menu return failed");
        check(!app->renderer.playerLighting&&!app->renderer.rivalLighting&&!app->renderer.courseLighting&&Fixture::events.empty()&&Gain::events.empty(),"Race car-light ARRAY leaked into menu return");
        const auto menuBefore=output/(std::string(scene.label)+"-menu-retained.bmp"),menuAfter=output/(std::string(scene.label)+"-menu-cleared.bmp");
        check(app->renderer.saveBitmap(menuBefore.wstring()),"Menu snapshot failed");app->playerCarLight={};app->rivalCarLight={};app->raceLightSets.reset();
        check(app->renderMenu(0)&&app->renderer.saveBitmap(menuAfter.wstring())&&readBytes(menuBefore)==readBytes(menuAfter),"Retained source race car state changed menu pixels");
        std::cout<<"Verified "<<scene.label<<"420 paired ticks, source publication, scope, pause/restart/menu.\n"<<std::flush;
    }
    // Actual App request tail, with controlled private race-state boundaries:
    // current ambient reads prior byte81, so the Off and On requests affect
    // this frame's shared SPOT masks before they affect next-frame ambient.
    auto blackout=initialize(7,false,true,false,true,29);start(*blackout,true);
    auto&rules=const_cast<original::OriginalRaceRuleState&>(blackout->originalRace.state());rules.progress.index=1440;
    blackout->rivalLightState.frames1732=240;blackout->projectedLightPriorAdvantage=0;
    auto motor=blackout->rivalPresentation.headlightState();motor.visible=true;motor.counter=40;motor.fraction=1;blackout->rivalPresentation.restoreHeadlightState(motor);
    const auto unchanged=DrivingSnapshot(*blackout);const auto ambient=blackout->raceCarAmbient;
    clearEvents();blackout->advanceProjectedHeadlights();verifyEvents(*blackout,false);
    check(!blackout->rivalCarLight.ownSpot.enabled&&blackout->rivalCarLight.ambient==ambient,"Off request dimmed current ambient before prior81 consumption");
    check(blackout->rivalPresentation.headlightState().visible&&blackout->rivalPresentation.headlightState().counter==40,"Source outer lamp incorrectly followed popup visibility");
    clearEvents();blackout->advanceProjectedHeadlights();verifyEvents(*blackout,false);
    check(blackout->rivalCarLight.ambient==original::OriginalLightVector{},"Steady-off rival ambient ignored signed-advantage zero");
    blackout->projectedLightPriorAdvantage=20;
    clearEvents();blackout->advanceProjectedHeadlights();verifyEvents(*blackout,false);
    check(blackout->rivalCarLight.ownSpot.enabled&&blackout->rivalCarLight.ambient==original::OriginalLightVector{},"On request rewrote prior-off ambient on the same frame");
    clearEvents();blackout->advanceProjectedHeadlights();verifyEvents(*blackout,false);
    check(blackout->rivalCarLight.ambient==ambient&&DrivingSnapshot(*blackout)==unchanged,"Next-frame ambient did not recover, or request changed driving/RNG");
    check(snapshot(realRoot/"userdata")==realSaves&&snapshot(isolated/"userdata")==privateSaves,"Car-light fixture changed driver save files");
    std::ostringstream report;report<<"PASS "<<checks<<" checks / "<<pairedTicks<<" paired App ticks / "<<publishedSpots<<" published car spots / "<<ambientUpdates
        <<" source ambient updates; world-matrix publication, prior light81, source scopes, main/mirror, original projection exclusion, pause/restart/menu and driving/RNG/query invariance. "
        <<coordinateChecks<<" exact private path publications; daytime authored gain and reverse/wet mapping, no startup/night gain tick. "
        <<realSaves.size()<<" real save files unchanged in bytes/timestamps. WARP960x720, no audio device.\n";
    std::cout<<report.str();std::ofstream(output/"result.txt")<<report.str();return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
