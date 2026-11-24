// Actual App startup owners. All filesystem writes are confined to a new
// verification directory; no audio device/window/network is opened.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "original_loading_selection.h"
#include "original_race_overlay_resources.h"
#include "original_race_start.h"
#include <vector>
namespace owner_fixture {
struct Event {const char* name;unsigned before,after,value;};
inline std::vector<Event> events;
inline bool omitLoading=false,omitOverlay=false;
inline unsigned warmupBefore=0,warmupAfter=0,warmupEbc=0,initial21c=0;
}
namespace idas3::original {
inline std::uint32_t observedLoading(OriginalLoadingOwner owner,unsigned mode,unsigned enemy,unsigned& seed){
 const auto before=seed;const auto value=owner_fixture::omitLoading?0u:selectOriginalLoadingArtwork(owner,mode,enemy,seed);
 owner_fixture::events.push_back({"loading",before,seed,value});return value;
}
inline OriginalRaceOverlayResources observedOverlay(unsigned mode,unsigned& seed){
 const auto before=seed;auto temporary=seed;
 const auto result=prepareOriginalRaceOverlayResources(mode,owner_fixture::omitOverlay?temporary:seed);
 owner_fixture::events.push_back({"overlay",before,seed,mode});return result;
}
inline void observedWarmup(OriginalDrivingSession& session,const OriginalVehicleInputs& input,unsigned frame,
 std::uint8_t digital=0,const std::function<void(const OriginalDrivingStepEffects&)>& publish={}){
 owner_fixture::warmupBefore=session.sharedRandomSeed();owner_fixture::initial21c=session.vehicle().drive.u(0x21c);
 warmupOriginalRaceSession(session,input,frame,digital,publish);
 owner_fixture::warmupAfter=session.sharedRandomSeed();owner_fixture::warmupEbc=session.contactCompletion().steeringMask0C900EBC;
 owner_fixture::events.push_back({"warmup",owner_fixture::warmupBefore,owner_fixture::warmupAfter,owner_fixture::warmupEbc});
}
}
#define selectOriginalLoadingArtwork observedLoading
#define prepareOriginalRaceOverlayResources observedOverlay
#define warmupOriginalRaceSession observedWarmup
#include "../src/main.cpp"
#undef selectOriginalLoadingArtwork
#undef prepareOriginalRaceOverlayResources
#undef warmupOriginalRaceSession
#include <iostream>
namespace {
unsigned checks=0;
void check(bool condition,const std::string& what){++checks;if(!condition)throw std::runtime_error(what);}
unsigned next(unsigned seed){return seed*1103515245u+12345u;}
void seed(App& a,unsigned value){a.originalSession.withSharedRandom([&](auto& s){s=value;});}
std::unique_ptr<App> fixture(const fs::path& root,const fs::path& out){
 auto app=std::make_unique<App>();app->root=fs::absolute(root);app->saveRoot=out;
 app->validationMode=true;app->settings();app->audio.enabled=false;app->audio.configure(root);
 app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
 app->frontend.initialize(root,true);app->frontend.stage=FrontendStage::Mode;
 app->frontend.car=0;app->frontend.course=app->courseIndex=3;app->frontend.automatic=app->automatic=false;
 app->frontend.gameMode=original::OriginalGameMode::TimeAttack;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
 app->active=true;app->menu=true;app->demoPreparationStarted=true;app->replayRecordingFlags=0;
 return app;
}
void selectTa(App& a,unsigned course=3){
 a.validationMode=true;a.activeSaveSlot=-1;a.inspectionVisit.reset();a.multiplayer.active=false;a.replayPlaybackActive=false;
 a.frontend.gameMode=original::OriginalGameMode::TimeAttack;a.frontend.course=a.courseIndex=int(course);
 a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();a.frontend.battleProfile.setu(0,0);a.frontend.battleProfile.setu(24,29);
 a.frontend.car=0;a.frontend.automatic=a.automatic=false;a.reverse=a.wet=a.night=false;
 a.loadingActive=true;a.loadingStarted=false;a.menu=true;a.loadedProfileCar=0;
 owner_fixture::events.clear();owner_fixture::omitLoading=owner_fixture::omitOverlay=false;
}
void testEligibility(App& a){
 constexpr unsigned initial=0x12345678;
 for(unsigned mode=0;mode<3;++mode)for(int course:{-1,0,3,8,9,17,18})for(unsigned blocked=0;blocked<4;++blocked){
  selectTa(a);a.frontend.gameMode=original::OriginalGameMode(mode);a.frontend.course=a.courseIndex=course;
  a.frontend.battleProfile.setu(0,mode);a.frontend.battleProfile.setu(4,unsigned(course));a.frontend.battleProfile.setu(24,0);
  a.multiplayer.active=blocked==1;a.replayPlaybackActive=blocked==2;seed(a,initial);
  const auto chosen=a.prepareOriginalRaceLoading(blocked==3);
  const bool eligible=blocked==0&&course>=0&&course<9;
  check(chosen.has_value()==eligible,"Selected mode/course extension admission");
  check(a.originalSession.sharedRandomSeed()==(eligible?next(initial):initial),"Rejected selection consumed RNG");
 }
 selectTa(a);a.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;a.frontend.battleProfile.setu(0,0);a.frontend.battleProfile.setu(4,7);a.frontend.battleProfile.setu(24,29);seed(a,initial);
 check(a.prepareOriginalRaceLoading()==12&&a.originalSession.sharedRandomSeed()==initial,"Final visitor must use fixed12 without draw");
 for(auto mode:{original::OriginalGameMode::LegendOfTheStreets,original::OriginalGameMode::BuntaChallenge}){
  a.frontend.gameMode=mode;a.frontend.course=17;a.courseIndex=3;a.frontend.battleProfile.setu(0,unsigned(mode));a.frontend.battleProfile.setu(4,0);a.frontend.battleProfile.setu(24,0);
  a.importedCourse.emplace();seed(a,initial);check(a.prepareOriginalRaceLoading().has_value()&&a.originalSession.sharedRandomSeed()==next(initial),"Stale imported race suppressed selected original battle");
 }
 a.importedCourse.reset();selectTa(a);a.frontend.course=17;a.courseIndex=3;seed(a,initial);
 check(!a.prepareOriginalRaceLoading()&&a.originalSession.sharedRandomSeed()==initial,"Imported mapped course3 admitted original owner");
 selectTa(a);a.frontend.course=3;a.courseIndex=8;seed(a,initial);check(a.prepareOriginalRaceLoading().has_value(),"Direct diagnostic original course override rejected");
}
void testRejection(App& a,const fs::path& out){
 constexpr unsigned initial=0x71932391;selectTa(a);
 a.inspectionVisit.emplace(App::InspectionVisit{0,0,a.frontend.battleProfile});seed(a,initial);a.start();
 check(a.originalSession.sharedRandomSeed()==initial&&owner_fixture::events.empty(),"Already open inspection consumed owner");a.inspectionVisit.reset();
 // An actual failed durable write, not a mocked return value. A regular file
 // occupies the directory where the private profile store would be created.
 const auto blocked=out/"blocked-store";std::ofstream(blocked)<<"fixture";
 a.validationMode=false;a.activeSaveSlot=0;a.loadedProfileCar=0;
 original::finishOriginalDriverSetupFlag(a.frontend.battleProfile);a.frontend.battleProfile.setByte(1192,0);
 a.profiles=LocalDriverProfiles(blocked/"profiles");a.pendingProfiles[0]=a.frontend.battleProfile;
 seed(a,initial);a.start();
 check(a.originalSession.sharedRandomSeed()==initial&&owner_fixture::events.empty(),"Failed pending write consumed loading/overlay");
 check(a.pendingProfiles[0].has_value(),"Failed private write was silently acknowledged");
 seed(a,initial);a.enterRaceAfterPrompt();
 check(a.originalSession.sharedRandomSeed()==initial&&owner_fixture::events.empty(),"Failed entry-path pending write consumed owner");
 a.pendingProfiles={};a.saveSlots=LocalSaveSlots(blocked/"saves");a.menu=true;
 seed(a,initial);a.start();
 check(a.originalSession.sharedRandomSeed()==initial&&owner_fixture::events.empty(),"Failed card acceptance consumed loading/overlay");
 check(a.acceptedCards.empty(),"Failed card acceptance inserted accepted token");
 // A genuinely expired private card opens inspection before loading.
 a.saveSlots=LocalSaveSlots(out/"expired-saves");a.frontend.battleProfile.setu(1140,0);
 check(a.saveSlots.adopt(0,a.frontend.battleProfile),"Could not seed private slot");
 const auto directory=a.saveSlots.profileDirectory(0);LocalDriverProfiles store(directory);LocalDriverSetup setup(directory);
 check(store.save(0,a.frontend.battleProfile)&&setup.markComplete(0),"Could not seed private expired card");
 a.useSaveSlot(0);a.loadedProfileCar=0;seed(a,initial);a.start();
 check(a.inspectionVisit.has_value()&&a.originalSession.sharedRandomSeed()==initial&&owner_fixture::events.empty(),"Expired card did not stop before owner");
 a.closeCarInspection();seed(a,initial);a.start();
 check(a.inspectionVisit.has_value()&&a.originalSession.sharedRandomSeed()==initial&&owner_fixture::events.empty(),"Cancelled expired card retry consumed owner");
 a.closeCarInspection();a.useSaveSlot(-1);selectTa(a);
}
struct Run {unsigned seed,ebc,initial21c;std::uint64_t digest;std::vector<unsigned> masks;};
Run run(App& a,unsigned initial,bool prepared,bool missingOwners,std::ostream& report,const char* label){
 selectTa(a);a.originalSession=original::OriginalDrivingSession{};owner_fixture::omitLoading=owner_fixture::omitOverlay=missingOwners;seed(a,initial);
 if(prepared)a.prepareOriginalRaceLoading();a.start(false,prepared);
 check(a.originalHandling&&a.originalSession.ready(),"App failed to start original TA");
 check(owner_fixture::events.size()==3,"App duplicated or omitted loading/overlay/warmup owner");
 check(std::string(owner_fixture::events[0].name)=="loading"&&std::string(owner_fixture::events[1].name)=="overlay"&&std::string(owner_fixture::events[2].name)=="warmup","App owner order differs");
 check(a.originalRaceOverlayResources.has_value()&&!a.originalRaceOverlayResources->driverLevelUp,"TA profile normalization did not reach resource owner");
 check(a.originalLoadingArtwork.has_value(),"App did not retain chosen source art");
 auto expected=owner_fixture::warmupBefore;unsigned expectedEbc=0;
 for(unsigned i=0;i<60;++i){expected=next(expected);expectedEbc+=(expected>>16)&3;}
 check(owner_fixture::warmupAfter==expected&&owner_fixture::warmupEbc==expectedEbc,"Warmups did not follow owner seed for 60 real contact steps");
 Run result{a.originalSession.sharedRandomSeed(),a.originalSession.contactCompletion().steeringMask0C900EBC,owner_fixture::initial21c,a.originalSession.rollbackDigest(),{}};
 report<<label<<",initial="<<initial<<",loading="<<owner_fixture::events[0].after<<",overlayBefore="<<owner_fixture::events[1].before<<",overlayAfter="<<owner_fixture::events[1].after<<",warmupSeed="<<result.seed<<",warmupEBC="<<result.ebc<<",initial21cBits="<<result.initial21c<<",digest="<<result.digest<<'\n';
 // Controlled solver boundary after the real App setup. Source engine/tire
 // frame owners are deliberately excluded in BOTH variants; no claim of a
 // complete live input recording or boot/menu RNG history is made here.
 a.originalSession.setEngineOutput({});a.originalSession.enableRaceStart(2);
 original::OriginalHostInputState inputState{};
 for(unsigned frame=0;frame<360;++frame){
  original::OriginalHostControls controls{frame<120?0.f:frame<240?.35f:-.35f,1.f,0.f,false,false};
  const auto input=original::adaptOriginalHostInput(inputState,controls,false,true,frame);
  a.originalSession.tick(input);result.masks.push_back(a.originalSession.contactCompletion().steeringMask0C900EBC);
 }
 report<<label<<",after360Seed="<<a.originalSession.sharedRandomSeed()<<",after360EBC="<<a.originalSession.contactCompletion().steeringMask0C900EBC<<",speedBits="<<a.originalSession.vehicle().drive.u(0x238)<<'\n';
 return result;
}
}
int main(int argc,char** argv)try{
 if(argc!=3)throw std::runtime_error("Native asset root and private evidence root required");
 const auto root=fs::absolute(argv[1]),evidence=fs::absolute(argv[2]);
 fs::create_directories(evidence);
 const auto runId=std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 const auto out=evidence/("run-"+runId);if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
 auto app=fixture(root,out/"userdata");testEligibility(*app);testRejection(*app,out);
 std::ofstream report(out/"controlled-ta.csv");
 const auto direct=run(*app,0x12345678,false,false,report,"owners-direct");
 const auto prepared=run(*app,0x12345678,true,false,report,"owners-prepared");
 check(direct.seed==prepared.seed&&direct.ebc==prepared.ebc&&direct.digest==prepared.digest&&direct.masks==prepared.masks,"Prepared load and direct start differ/double-consume");
 const auto omitted=run(*app,0x12345678,false,true,report,"counterfactual-missing-two-owners");
 check(direct.seed!=omitted.seed&&direct.initial21c!=omitted.initial21c,"Added source loading owner did not change initialized random state");
 unsigned changed=0;for(unsigned i=0;i<direct.masks.size();++i)changed+=direct.masks[i]!=omitted.masks[i];
 check(changed!=0,"Controlled EBC history unexpectedly identical");report<<"differentEBCFrames="<<changed<<"/360\n";
 OriginalLoadingScreen art;art.load(root);for(unsigned id=0;id<13;++id){check(art.beginArtwork(id)&&art.artwork()==id,"Authored art number remapped to compact index");}
 // The normal entry actually paints once and invokes start(false,true).
 // The offscreen D3D target has no window and no audio device is initialized.
 check(app->renderer.initialize(nullptr,640,480,true),"Offscreen loading target failed");
 app->loadingScreen.load(root);selectTa(*app);app->validationMode=false;
 app->originalSession=original::OriginalDrivingSession{};seed(*app,0x12345678);
 check(app->enterRaceAfterPrompt(),"Actual loading entry failed");
 check(app->loadingStarted&&app->loadingActive&&owner_fixture::events.size()==3,"Loading frame did not start precisely once");
 check(app->originalSession.sharedRandomSeed()==direct.seed,"Loading renderer path differs from direct startup");
 const auto paintedSeed=app->originalSession.sharedRandomSeed();const auto paintedEvents=owner_fixture::events.size();
 for(unsigned repaint=0;repaint<4;++repaint)check(app->renderLoading(repaint?.1:0),"Loading repaint failed");
 check(app->originalSession.sharedRandomSeed()==paintedSeed&&owner_fixture::events.size()==paintedEvents,"Loading repaint duplicated source owners");
 // An abandoned loading is not a persistent token: a later direct retry owns
 // a new choice, even though the old diagnostic artwork remains available.
 app->loadingActive=false;app->validationMode=true;owner_fixture::events.clear();seed(*app,0x98765432);app->start();
 check(owner_fixture::events.size()==3&&owner_fixture::events[0].before==0x98765432&&owner_fixture::events[0].after==next(0x98765432),"Cancelled load reused a stale preparation token");
 const auto oldArt=art.artwork();check(!art.beginArtwork(999)&&art.artwork()==oldArt,"Missing authored art changed current artwork");
 std::cout<<"PASS "<<checks<<" App startup checks; original/imported/nonoriginal/online/replay admissions, actual private card write failures, renewal cancellation/retry, direct/prepared equality, real warmups, source artwork selection. Controlled EBC frames differ "<<changed<<"/360 when the two owners are omitted. Offscreen loading/repaints only; no window/audio/network/user saves.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
