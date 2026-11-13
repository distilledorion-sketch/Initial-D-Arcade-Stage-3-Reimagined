#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
 if(argc<3||argc>5)throw std::runtime_error("Native root, NEW evidence directory, optional language and boundary-only flag required");
 const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
 if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
 fs::create_directories(out);const int language=argc>=4?std::stoi(argv[3]):0;
 setOriginalUiLanguage(language==1?1:0);setCustomMenuLanguage(language);
 unsigned checks=0,cases=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";
 app->settings();app->frontend.initialize(root,true);app->audio.enabled=false;app->menu=true;
 const auto data=original::OriginalTuningData::load(root);
 const auto fixture=[&](unsigned car,unsigned package,unsigned gear=1,unsigned plays=0){
  auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);original::applyOriginalAcceptedCardFlag(p);original::finishOriginalDriverSetupFlag(p);
  p.setu(68,gear);p.setu(72,123456);p.setu(76,5);p.setu(64,0);for(unsigned i=0;i<5;++i)p.setu(44+4*i,162+i);
  p.setByte(152,std::uint8_t(package));p.setByte(153,3);p.setByte(154,4);p.setByte(155,2);
  for(unsigned i=156;i<164;++i)p.setByte(i,1);
  p.setByte(164,5);p.setByte(165,1);p.setu(1180,p.u(1180)|0xc00u);p.setu(1176,1);p.setu(1140,plays);p.setu(1160,0x31415926);p.setByte(1192,0);
  for(unsigned i=116;i<147;++i)p.setByte(i,0x11);
  p.setByte(148,3);p.setu(1080,7);p.setu(1120,100);p.setu(1124,80);p.setu(200,987654);return p;
 };
 const auto seed=[&](unsigned slot,const original::OriginalBattleProfile& p){const auto dir=app->saveSlots.profileDirectory(slot);const auto car=p.u(16);
  check(LocalDriverProfiles(dir).save(car,p),"Seed profile");check(LocalDriverSetup(dir).markComplete(car),"Seed setup");check(app->saveSlots.adopt(slot,p),"Seed slot");};
 const auto disk=[&]{std::map<fs::path,std::pair<std::vector<char>,fs::file_time_type>> result;
  for(const auto& item:fs::recursive_directory_iterator(app->saveRoot)){if(!item.is_regular_file()||item.path().extension()==".lock")continue;
   std::ifstream f(item.path(),std::ios::binary);result[item.path()]={{std::istreambuf_iterator<char>(f),{}},item.last_write_time()};}return result;};
 const auto tick=[&](unsigned count=1){for(unsigned i=0;i<count;++i){app->frontend.advance(1./60.);app->advanceCarInspection();
  check(!app->frontend.takeDriverProfileCommit()&&!app->frontend.takeDriverSetupCompleted(),"Renewal leaked a separate setup write");}};
 const auto choose=[&]{app->frontend.stage=FrontendStage::SaveSelect;app->frontend.saveSelected=0;app->openSaveFile(0);};
 const auto select=[&](unsigned package,unsigned gear){
  tick(20);app->frontend.automatic=gear==0;check(app->frontend.inputReady(),"Transmission not ready");app->frontend.confirm();
  for(unsigned n=0;n<180&&app->frontend.stage==FrontendStage::Transmission&&app->inspectionVisit;++n)tick();
  if(app->frontend.car!=29){check(app->frontend.stage==FrontendStage::TuningCourse,"Renewal must offer tuning package");tick(10);
   for(unsigned n=0;n<8&&app->frontend.tuningCourseState().selected496!=package;++n){app->frontend.change(1);tick();}
   check(app->frontend.tuningCourseState().selected496==package,"Package selection");check(app->frontend.inputReady(),"Package not ready");app->frontend.confirm();
   for(unsigned n=0;n<160&&app->frontend.stage==FrontendStage::TuningCourse&&app->inspectionVisit;++n)tick();}
 };
 const auto finish=[&]{check(app->inspectionVisit&&app->inspectionVisit->committed&&app->frontend.savedData()->inspectionCompletion(),"Completion must follow durable renewal");
  tick(160);app->frontend.confirm();unsigned remaining=950;while(app->inspectionVisit&&remaining--)tick();
  check(!app->inspectionVisit&&!app->frontend.inspectingCar()&&app->frontend.stage==FrontendStage::Mode,"Renewal did not finish in Mode");};
 seed(1,fixture(7,0,1,23));
 for(unsigned car=0;car<(argc==5?0u:35u);++car)for(unsigned package=0;package<data.car(car).packages.size();++package)
 for(unsigned next=0;next<data.car(car).packages.size();++next)for(unsigned gear=0;gear<2;++gear){
  const auto p=fixture(car,package,1-gear);seed(0,p);choose();
  check(app->inspectionVisit&&app->frontend.stage==FrontendStage::Transmission,"Zero plays must offer inspection choices");check(app->profiles.load(car).profile.words==p.words,"Opening renewal wrote the draft");
  auto expected=p;expected.setu(68,gear);expected.setu(1140,50);expected.setByte(152,std::uint8_t(next));
  if(package!=next){for(unsigned i:{154u,155u,156u,157u,158u,159u,160u,161u,162u,163u,165u})expected.setByte(i,0);
   if(next!=data.car(car).packages.size()-1){expected.setByte(153,0);expected.setu(1180,expected.u(1180)&~0xc00u);}}
  select(next,gear);check(app->profiles.load(car).profile.words==expected.words,"Renewal differs from arcade reset rules or altered unrelated data");
  const auto before=disk();finish();app->flushProfiles();check(app->frontend.battleProfile.words==expected.words&&app->battleProfile.words==expected.words,"Renewal live car differs from saved result");check(disk()==before,"Summary wrote after durable renewal");
  check(app->ensureSelectedCardAccepted()&&app->profiles.load(car).profile.u(1140)==50,"Renewed session charged an extra use");
  check(LocalDriverProfiles(app->saveSlots.profileDirectory(1)).load(7).profile.words==fixture(7,0,1,23).words,"Another driver changed");++cases;
 }
 for(unsigned plays:{1u,2u,49u,50u,51u}){seed(0,fixture(0,0,1,plays));choose();check(!app->inspectionVisit,"Positive plays entered renewal");check(app->profiles.load(0).profile.u(1140)==plays-1,"Card acceptance must consume exactly one use");app->battleProfile=app->frontend.battleProfile;
  for(bool timeout:{false,true,false}){app->race.phase=RacePhase::Finished;app->race.timeUp=timeout;app->saveResultProfile();check(app->ensureSelectedCardAccepted()&&!app->inspectionVisit,"Continue renewed an accepted card");check(app->profiles.load(0).profile.u(1140)==plays-1,"Continued race consumed another use");}}
 auto p=fixture(0,0);seed(0,p);choose();auto before=disk();tick(20);app->frontend.back();app->advanceCarInspection();
 check(!app->inspectionVisit&&disk()==before&&app->profiles.load(0).profile.words==p.words,"Cancellation changed expired card");
 choose();tick(20);app->frontend.confirm();tick(160);check(app->frontend.stage==FrontendStage::TuningCourse,"Cancel package fixture");tick(10);app->frontend.back();app->advanceCarInspection();check(!app->inspectionVisit&&disk()==before,"Package cancellation wrote draft");
 const auto lock=fs::path(app->profiles.path(0).wstring()+L".lock");auto handle=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 check(handle!=INVALID_HANDLE_VALUE,"Acquire lock");choose();select(1,0);check(!app->inspectionVisit&&app->frontend.stage==FrontendStage::SaveSelect&&disk()==before,"Failed renewal changed files or continued");
 CloseHandle(handle);choose();select(1,0);finish();check(app->profiles.load(0).profile.u(1140)==50,"Renewal retry failed");
 seed(0,p);choose();auto newer=p;newer.setu(72,765432);check(app->profiles.save(0,newer),"Concurrent writer fixture");before=disk();select(1,0);check(!app->inspectionVisit&&disk()==before&&app->frontend.battleProfile.words==newer.words,"Renewal replaced concurrent progress");
 seed(0,fixture(0,0,1,2));choose();check(app->profiles.load(0).profile.u(1140)==1,"First card entry");choose();check(app->profiles.load(0).profile.u(1140)==0&&!app->inspectionVisit,"Second card entry");choose();check(app->inspectionVisit.has_value(),"Next entry after last use must renew");select(0,1);finish();
 auto online=fixture(7,0,1,1);seed(1,online);before=disk();original::OriginalBattleProfile read;check(app->onlineCarProfile(onlineSlotCarSelection(1,7),read)==1&&disk()==before,"Online preview wrote a card");
 app->acceptCardUse(1,7,read);check(read.u(1140)==0,"Online acceptance failed");app->acceptCardUse(1,7,read);check(LocalDriverProfiles(app->saveSlots.profileDirectory(1)).load(7).profile.u(1140)==0,"Online rematch charged another use");
 app->acceptedCards.clear();bool rejected=false;try{app->acceptCardUse(1,7,read);}catch(const std::exception&){rejected=true;}check(rejected,"Expired online card bypassed renewal");
 before=disk();app->acceptCardUse(-1,7,read);check(disk()==before,"Guest/legacy online selection wrote a physical card");
 app->hud.loadOriginal(root);app->audio.configure(root);app->audio.enabled=false;app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
 check(app->renderer.initialize(nullptr,640,480,true),"Offscreen renderer initialization");app->demoPreparationStarted=true;app->replayRecordingFlags=0;seed(0,p);choose();
 const auto capture=[&](const char* name){check(app->renderMenu(1./60.),"Offscreen render failed");check(app->renderer.saveBitmap((out/(std::string(name)+".bmp")).wstring()),"Capture failed");};
 tick(20);capture("renewal-transmission");app->frontend.confirm();tick(160);tick(10);capture("renewal-package");app->frontend.confirm();tick(145);capture("renewal-complete");finish();capture("renewal-mode");
 // NORMAL's inherited basic level differs when the original basic-tune flag
 // is unset; exercise that source branch separately from fully tuned cards.
 p=fixture(0,0);p.setu(1180,p.u(1180)&~0xc00u);seed(0,p);choose();select(unsigned(data.car(0).packages.size()-1),1);finish();
 check(app->profiles.load(0).profile.byte(153)==5&&app->profiles.load(0).profile.byte(164)==5,"NORMAL lost inherited engine/basic level");
 // A fresh additional car's first issued session already has 50 remaining.
 app->frontend.changingSavedCar=true;app->frontend.car=34;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
 app->frontend.battleProfile.setu(16,34);
 if(argc==5){check(app->finishSavedCarSelection()&&app->ensureSelectedCardAccepted(),"Fresh garage card acceptance");check(app->profiles.load(34).profile.u(1140)==50,"Fresh card charged twice");}
 else app->frontend.changingSavedCar=false;
 // A natural source race reaches time-up. At zero remaining, Continue still
 // uses the accepted card; only returning to title and inserting it again renews.
 auto raceCard=original::makeOriginalFreshBattleProfile();raceCard.setu(16,0);raceCard.setu(68,1);raceCard.setu(1140,1);
 original::applyOriginalAcceptedCardFlag(raceCard);original::finishOriginalDriverSetupFlag(raceCard);seed(0,raceCard);
 app->acceptedCards.clear();app->useSaveSlot(0);app->frontend.car=0;app->frontend.make=6;app->frontend.automatic=false;
 app->frontend.gameMode=original::OriginalGameMode::TimeAttack;app->frontend.course=app->courseIndex=0;
 app->reverse=app->wet=app->night=false;app->loadedProfileCar=-1;app->start();
 check(app->battleProfile.u(1140)==0&&app->profiles.load(0).profile.u(1140)==0,"Starting without prior review must accept once");
 unsigned raceTicks=0;for(;raceTicks<20000&&app->race.phase!=RacePhase::Finished;++raceTicks)app->simulate({});
 check(app->race.phase==RacePhase::Finished&&app->race.timeUp&&app->profiles.load(0).profile.u(1140)==0,"Natural timeout changed remaining uses");
 app->returnToCourseSelection();capture("last-use-continue");check(!app->inspectionVisit,"Continue on final use opened renewal");
 app->frontend.stage=FrontendStage::Title;app->renderMenu(0);check(app->acceptedCards.empty(),"Title retained the previous card session");
 choose();check(app->inspectionVisit.has_value(),"New entry after final-use Continue failed to renew");
 tick(20);app->frontend.back();app->advanceCarInspection();check(app->profiles.load(0).profile.u(1140)==0,"Cancelled final renewal granted uses");
 std::cout<<"Natural final-use timeout: "<<raceTicks<<" source ticks; Continue retained zero; next entry requested inspection.\n";
 std::cout<<"PASS "<<checks<<" checks, "<<cases<<" car/package/transmission renewals; per-card acceptance, continued races, online rematches, cancellation, write failure, conflicts, isolation and offscreen rendering.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
