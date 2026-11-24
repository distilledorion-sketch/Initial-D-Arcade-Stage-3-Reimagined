// Real exported scene input, real bridge assignment and actual App driver.
// No Unity renderer, audio device, network or personal settings are opened.
#include "../src/unity_bridge.cpp"
#include <iostream>
using namespace idas3::original;
namespace {
std::uint64_t checks=0;
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
OriginalVehicleInputs packet(App& a){const auto d=a.driver(false);OriginalHostInputState state;
 return adaptOriginalHostInput(state,{d.steer,d.throttle,d.brake,d.shiftDown,d.shiftUp,d.requestedGear,-1,d.originalThrottleAnalog,d.originalBrakeAnalog},false,true,0);}
unsigned sourceAdc(unsigned raw){unsigned value=raw*2;if(value>=0x8000&&value<0x8100)value=0x8100;return std::min(value,0xff7fu);}
std::string utf8(const fs::path& p){const auto s=p.u8string();return {s.begin(),s.end()};}
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Native asset root and new isolated output directory required");
 const auto output=fs::absolute(argv[2]);check(!fs::exists(output),"Fixture directory must be new");fs::create_directories(output);
 const auto assets=utf8(fs::absolute(argv[1])),saves=utf8(output/"userdata");
 check(Idas3SceneInitialize(assets.c_str(),saves.c_str(),640,480)==1,"Actual scene init failed");
 auto& runtime=unityRuntime();auto& a=*runtime.app;a.validationMode=true;a.audio.enabled=false;a.originalHandling=true;
 a.controllerResponse=ControllerResponse::FlycastWheel;
 // Exercise actual exported status publication at each presentation-owner
 // boundary. This flag enables mapped cabinet controls only while the local
 // original finish child still owns the visible course/car.
 const auto finishFlag=[&]{publish(runtime,0);Idas3UnityStatus s{};s.size=sizeof(s);
  check(Idas3UnityGetStatus(&s)==1,"Actual scene status read failed");return (s.flags&0x10000000u)!=0;};
 check(!finishFlag(),"Frontend advertised finish driving controls");
 const auto initialPhase=a.race.phase;const bool initialMenu=a.menu;
 a.menu=false;a.race.phase=RacePhase::Finished;
 check(finishFlag(),"Local original finish did not advertise mapped controls");
 a.finishBannerDone=true;
 check(finishFlag(),"Record hold lost driving mappings after FINISH banner");
 a.finishBannerDone=false;
 for(bool* owner:{&a.menu,&a.loadingActive,&a.legendVisitActive,&a.preRaceDialogueActive,
    &a.buntaVisitActive,&a.timeAttackVisitActive,&a.endingActive,&a.resultVisit.initialized,
    &a.fullTuneActive,&a.fullTuneSelecting,&a.replayPlaybackActive}){
  const bool old=*owner;*owner=true;check(!finishFlag(),"Replacement/modal owner retained finish driving mappings");*owner=old;
 }
 a.multiplayer.active=true;
 for(int page=-1;page<=2;++page){a.onlineResult.page=page;check(finishFlag()==(page<=0),"Online finish capability escaped its page owner");}
 a.onlineResult.page=0;a.onlineResult.finish.phase=original::OriginalLinkedFinishPresentation::Phase::Done;
 check(!finishFlag(),"Completed finish retained driving while waiting for EOS handoff");
 a.onlineResult.finish.begin(true,false);
 for(bool* modal:{&a.multiplayer.waiting,&a.multiplayer.disconnected}){
  *modal=true;check(!finishFlag(),"Online waiting/disconnect retained finish driving");*modal=false;
 }
 // The real exported API must accept the save-lifecycle bit; a managed
 // callback stub cannot prove native flag validation or nonmutation.
 a.onlineResult.begin(0,0);
 check(Idas3MultiplayerResultInput(0,0,0,16)==1&&a.onlineResult.savePending&&!a.onlineResult.blocked,
  "Export rejected nonmodal pending-save flag");
 check(finishFlag(),"Pending-save lifecycle removed original finish driving");
 for(unsigned flags:{32u,0x80000000u,48u}){
  const auto prior=a.onlineResult;
  check(Idas3MultiplayerResultInput(1,1,1,flags)==0,"Unknown result flag accepted");
  check(a.onlineResult.savePending==prior.savePending&&a.onlineResult.blocked==prior.blocked&&
   a.onlineResult.confirmPending==prior.confirmPending&&a.onlineResult.cancelPending==prior.cancelPending&&
   a.onlineResult.direction==prior.direction&&a.onlineResult.page==prior.page&&a.onlineResult.frame==prior.frame,
   "Rejected result flag mutated owner/input");
 }
 check(Idas3MultiplayerResultInput(0,0,0,1|16)==1&&a.onlineResult.blocked&&!finishFlag(),
  "Exported recovery/modal flag did not own finish controls");
 check(Idas3MultiplayerResultInput(0,0,0,0)==1&&finishFlag(),"Cleared modal retained stale capability block");
 // Read-only selectors must preserve a queued edge and every byte of owner
 // state across repeated observations, including a zero-tick presentation.
 a.onlineResult.points.frame=247;a.onlineResult.input(true,true,0,0);
 const auto beforeRead=a.onlineResult;
 const auto resultBytes=[&]{std::array<unsigned char,sizeof(a.onlineResult)> bytes{};
  std::memcpy(bytes.data(),&a.onlineResult,bytes.size());return bytes;};
 const auto beforeBytes=resultBytes();
 for(unsigned read=0;read<3;++read){
  check(Idas3MultiplayerResultScreen(8)==247,"Diagnostic points-frame selector mismatch");
  check(Idas3MultiplayerResultScreen(9)==6,"Diagnostic pending edge selector mismatch");
  check(Idas3MultiplayerResultScreen(10)==-1,"Unsupported diagnostic selector accepted");
  check(beforeBytes==resultBytes(),"Reading result diagnostics mutated owner/queued input");
 }
 a.onlineResult=beforeRead;a.onlineResult.input(false,false,0,1);
 const auto blockedBytes=resultBytes();
 check(Idas3MultiplayerResultScreen(9)==1&&blockedBytes==resultBytes(),
  "Blocked diagnostic selector changed its owner");
 a.onlineResult=beforeRead;a.onlineResult.input(false,false,0,0);
 check(a.onlineResult.confirmPending&&a.onlineResult.cancelPending,"Neutral diagnostic input cleared queued edges");
 a.onlineResult.input(false,false,0,1);a.onlineResult.input(false,false,0,0);

 a.multiplayer.active=false;a.onlineResult={};
 a.originalHandling=false;check(!finishFlag(),"Experimental handling opted into source finish mappings");a.originalHandling=true;
 a.race.phase=RacePhase::Running;check(!finishFlag(),"Running race incorrectly advertised finish capability");
 a.race.phase=initialPhase;a.menu=initialMenu;
 Idas3UnityInput frame{};frame.size=sizeof(frame);frame.flags=1;frame.padConnected=1;
 Idas3UnityPedals precise{sizeof(precise),1,3,0,0};
 for(unsigned raw=0;raw<=32767;++raw){
  precise.throttleAdc=std::uint16_t(sourceAdc(raw));precise.brakeAdc=std::uint16_t(sourceAdc(32767-raw));
  frame.rightTrigger=unsigned(std::lround(float(raw)/32767.f*255));frame.leftTrigger=unsigned(std::lround(float(32767-raw)/32767.f*255));
  validatePedals(precise);assignInput(a.input,frame,true,&precise);a.setHostDrivingControlsBlocked(false);
  const auto got=packet(a);check(got.analog.throttle==precise.throttleAdc&&got.analog.brake==precise.brakeAdc,"Actual bridge/App lost a precise pedal word");
 }
 for(unsigned t=0;t<256;++t)for(unsigned b=0;b<256;++b){
  frame.rightTrigger=t;frame.leftTrigger=b;assignInput(a.input,frame,true);const auto old=packet(a);
  Idas3UnityPedals absent{sizeof(absent),1,0,0,0};assignInput(a.input,frame,true,&absent);const auto next=packet(a);
  check(next.analog.throttle==old.analog.throttle&&next.analog.brake==old.analog.brake,"Absent extension changed legacy byte behavior");
  check(a.input.originalThrottleAnalog==-1&&a.input.originalBrakeAnalog==-1,"Legacy frame retained stale precise pedals");
 }
 precise={sizeof(precise),1,3,16578,0xff7f};frame.rightTrigger=65;frame.leftTrigger=255;
 for(unsigned mode=0;mode<3;++mode)for(unsigned keys=0;keys<4;++keys){
  a.controllerResponse=ControllerResponse(mode);for(auto& key:frame.keys)key=0;frame.keys['W'/32]=keys&1?1u<<('W'%32):0;frame.keys['S'/32]|=keys&2?1u<<('S'%32):0;
  assignInput(a.input,frame,true,&precise);const auto got=packet(a);
  assignInput(a.input,frame,true);const auto byteOnly=packet(a);
  check(got.analog.throttle==(mode==2&&!(keys&1)?precise.throttleAdc:byteOnly.analog.throttle),"Throttle precedence/profile changed");
  check(got.analog.brake==(mode==2&&!(keys&2)?precise.brakeAdc:byteOnly.analog.brake),"Brake precedence/profile changed");
 }
 for(auto& key:frame.keys)key=0;a.controllerResponse=ControllerResponse::FlycastWheel;
 for(unsigned gate=0;gate<4;++gate){frame.flags=gate;assignInput(a.input,frame,(gate&1)!=0,&precise);a.setHostDrivingControlsBlocked((gate&2)!=0);
  check((a.input.originalThrottleAnalog>=0)==(gate==1),"Focus/block retained precise pedals");}
 a.setHostDrivingControlsBlocked(false);frame.flags=1;frame.padConnected=0;assignInput(a.input,frame,true,&precise);
 check(a.input.originalThrottleAnalog==-1&&a.input.originalBrakeAnalog==-1,"Disconnected pedal survived");frame.padConnected=1;
 a.originalHandling=false;assignInput(a.input,frame,true,&precise);check(a.driver(false).originalThrottleAnalog==-1,"Experimental handling received original ADC override");a.originalHandling=true;
 // Call the actual exported functions with zero solver delta. This exercises
 // locked submission/commands/render ownership without a window or audio.
 check(Idas3SceneStepWithPedals(&frame,&precise)==1,"Actual precise export failed");
 check(a.input.originalThrottleAnalog==16578&&a.input.originalBrakeAnalog==0xff7f,"Export did not assign accepted sidecar");
 const auto retained=packet(a);const auto ticks=runtime.ticks,frames=runtime.frames;const auto owner=runtime.app.get();
 for(unsigned kind=0;kind<7;++kind){auto bad=precise;
  if(kind==0)bad.size=12;if(kind==1)bad.version=2;if(kind==2)bad.validMask=4;if(kind==3)bad.throttleAdc=0xffff;if(kind==4)bad.brakeAdc=0xff80;if(kind==5){bad.validMask=2;bad.throttleAdc=1;}if(kind==6){bad.validMask=1;bad.brakeAdc=1;}
  check(Idas3SceneStepWithPedals(&frame,&bad)==0,"Malformed sidecar accepted");
  check(runtime.app.get()==owner&&runtime.ticks==ticks&&runtime.frames==frames,"Rejected sidecar mutated frame/lifecycle");
  const auto after=packet(a);check(after.analog.throttle==retained.analog.throttle&&after.analog.brake==retained.analog.brake,"Rejected sidecar changed accepted controls");
 }
 check(Idas3SceneStep(&frame)==1,"Legacy export failed after precision");check(a.input.originalThrottleAnalog==-1&&a.input.originalBrakeAnalog==-1,"Legacy exported frame leaked sidecar");
 check(Idas3SceneStepWithPedals(&frame,nullptr)==1,"Explicit null fallback failed");
 // Join the exported input owner to the real running solo solver, rather
 // than only checking its intermediate host packet. Start with the actual
 // F5 command and let normal loading/countdown reach GO without state writes.
 precise={sizeof(precise),1,3,0,0};frame.rightTrigger=frame.leftTrigger=0;
 frame.deltaSeconds=1./60;frame.keys[VK_F5/32]=1u<<(VK_F5%32);
 check(Idas3SceneStepWithPedals(&frame,&precise)==1,"Exported solo start failed");
 for(auto& key:frame.keys)key=0;
 unsigned startup=0;
 while(a.menu||a.loadingActive||a.race.phase!=RacePhase::Running){
  check(++startup<=600,"Exported solo startup did not reach normal GO");
  check(Idas3SceneStepWithPedals(&frame,&precise)==1,"Exported loading/countdown failed");
 }
 check(a.originalSession.ready()&&!a.multiplayer.active,"Actual original solo solver unavailable");
 unsigned solverSteps=0;
 const auto assertSolver=[&](std::uint16_t throttle,std::uint16_t brake){
  const auto& controls=a.originalSession.vehicle().controls;
  if(controls.throttleHighByte!=(throttle>>8)||controls.brakeHighByte!=(brake>>8)){
   const auto d=a.driver(false);std::cerr<<"solver expected "<<(throttle>>8)<<','<<(brake>>8)<<" got "<<controls.throttleHighByte<<','<<controls.brakeHighByte<<" driverraw "<<d.originalThrottleAnalog<<','<<d.originalBrakeAnalog<<" hostraw "<<a.input.originalThrottleAnalog<<','<<a.input.originalBrakeAnalog<<" response "<<int(a.controllerResponse)<<" original "<<a.originalHandling<<'\n';
  }
  check(controls.throttleHighByte==(throttle>>8)&&controls.brakeHighByte==(brake>>8),"Exported pedals did not reach actual solver ADC conditioner");
  check(controls.throttle==originalHostPedalAmount(throttle)&&controls.brake==originalHostPedalAmount(brake),"Actual solver resolved different pedal values");
  const auto driver=a.driver(false);
  check(driver.throttle==controls.throttle&&driver.brake==controls.brake,"Actual App telemetry pedals differ from running solver");
 };
 for(const auto words:std::array<std::array<std::uint16_t,2>,8>{{{0,0xff7f},{0xff7f,0},{0x8100,0x807e},{16578,0xaaff},{0x4000,0x4100},{0x7ffe,0x8100},{0xaaaa,0x5554},{0xff7f,0xff7f}}}){
  precise.throttleAdc=words[0];precise.brakeAdc=words[1];
  // Deliberately disagreeing fallback bytes prove the accepted extension
  // wins at the final solver boundary, including simultaneous pedals.
  frame.rightTrigger=17;frame.leftTrigger=29;
  for(const double delta:{1./60,3./60}){
   frame.deltaSeconds=delta;const auto before=a.originalSession.platformFrame();
   check(Idas3SceneStepWithPedals(&frame,&precise)==1,"Exported running precise frame failed");
   const auto advanced=a.originalSession.platformFrame()-before;
   check(advanced==unsigned(std::lround(delta*60)),"Actual solver did not consume the expected catch-up steps");solverSteps+=advanced;
   assertSolver(words[0],words[1]);
  }
 }
 frame.deltaSeconds=1./60;frame.keys['W'/32]|=1u<<('W'%32);frame.keys['S'/32]|=1u<<('S'%32);
 precise.throttleAdc=precise.brakeAdc=0;
 check(Idas3SceneStepWithPedals(&frame,&precise)==1,"Exported keyboard override failed");assertSolver(171u<<8,171u<<8);
 for(auto& key:frame.keys)key=0;
 frame.rightTrigger=65;frame.leftTrigger=100;
 check(Idas3SceneStep(&frame)==1,"Running legacy export failed after precise frames");
 const auto oldByte=[](unsigned value){return std::uint16_t(std::clamp(sourceAdc(unsigned(std::lround(value*(32767.f/255.f))))>>8,64u,171u)<<8);};
 assertSolver(oldByte(frame.rightTrigger),oldByte(frame.leftTrigger));
 check(Idas3SceneShutdown()==1,"Isolated scene shutdown failed");
 std::cout<<"PASS "<<checks<<" actual exported API/bridge/App precision, old bytes, rejection atomicity, source precedence and lifecycle checks; "<<solverSteps+2<<" running solo solver frames after "<<startup<<" normal startup frames; no hardware or network.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
