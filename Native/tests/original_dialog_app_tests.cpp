// Actual App visit scheduling. Boundary states deliberately select the last
// closing tick and Ready phase; the independent original051EE0 dispatch oracle
// proves source skip/Ready/closed ordering. No game window or audio device.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../src/main.cpp"
#include <iostream>
namespace {
unsigned checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::unique_ptr<App> fixture(const fs::path& root,unsigned course){
 auto app=std::make_unique<App>();app->validationMode=true;app->preRaceDialogueSmoke=true;
 app->root=fs::absolute(root);app->audio.enabled=false;app->active=true;app->menu=true;
 app->audio.configure(app->root);
 app->frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
 app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
 original::selectOriginalRival(app->frontend.battleProfile,course==8?17:0);
 app->frontend.battleProfile.setu(4,course);
 app->originalSession.withSharedRandom([](auto& seed){seed=0x12345678;});
 check(app->beginPreRaceDialogue(),"Actual App pre-race owner failed to begin");
 check(!app->originalSession.ready(),"Menu fixture unexpectedly initialized driving solver");
 return app;
}
}
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Native asset root required");
 for(unsigned course:{0u,8u}){
  auto app=fixture(argv[1],course);auto& state=app->preRaceDialogue.state();
  // End the scene on the first accepted tick of a six-tick catch-up batch.
  state.phase=3;state.fade=20;state.closed=false;state.spectators=1;
  const auto before=app->originalSession.sharedRandomSeed();const auto draws=state.particles.acceptedDraws;
  auto expectedParticles=state.particles;auto expected=before;
  original::prepareOriginalDialogParticles(expectedParticles,course,state.spectators,expected);
  app->advancePreRaceDialogue(.1,false);
  check(state.closed&&state.fade==21,"Catch-up updated the closed dialogue again");
  check(state.particles.acceptedDraws==draws+1,"Catch-up drew old scene after its terminal tick");
  check(app->originalSession.sharedRandomSeed()==expected,"Catch-up consumed shared RNG after terminal draw");
  app->advancePreRaceDialogue(.1,true);
  check(state.particles.acceptedDraws==draws+1&&app->originalSession.sharedRandomSeed()==expected,"Already closed owner accepted another Main");
 }
 auto app=fixture(argv[1],0);auto& state=app->preRaceDialogue.state();
 state.phase=1;state.fade=0;state.skip=false;state.completed=false;
 // No script execution is needed for this ownership check; phase2 of the
 // text script is an ordinary in-progress page, and held skip ends it now.
 state.scriptPhase=2;state.endFlag=false;state.spectators=1;
 auto seed=app->originalSession.sharedRandomSeed();const auto draws=state.particles.acceptedDraws;
 app->paused=true;app->advancePreRaceDialogue(.1,true);
 check(!state.skip&&state.phase==1&&state.particles.acceptedDraws==draws,"Paused App advanced dialogue");
 check(app->originalSession.sharedRandomSeed()==seed,"Paused App consumed dialogue RNG");
 app->paused=false;app->advancePreRaceDialogue(0,true);
 check(!state.skip&&state.phase==1&&state.particles.acceptedDraws==draws,"Zero-time repaint accepted input or Draw");
 const auto kind=state.kind;app->advancePreRaceDialogue(FixedClock::step,true);
 check(state.skip&&state.phase==3,"Held pre-race skip was not applied before same-tick child Main");
 check(state.kind==kind,"Ready pre-race dialogue advanced to a different authored kind");
 check(state.particles.acceptedDraws==draws+1,"Single accepted skip tick must draw exactly once");
 auto ready=fixture(argv[1],0);auto& readyState=ready->preRaceDialogue.state();
 readyState.phase=2;readyState.fade=0;const auto readyKind=readyState.kind;
 ready->advancePreRaceDialogue(FixedClock::step,false);
 check(readyState.phase==3&&readyState.kind==readyKind,"Ready must close current intro instead of switching kinds");
 std::cout<<"PASS "<<checks<<" actual App pre-race dispatch checks: closed catch-up, pause/repaint, same-tick skip, Ready close, retained shared RNG. No window/audio/network/user save opened.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
