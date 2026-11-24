#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
using namespace idas3::reference;

// Private, bounded scheduling oracle. Starts from an explicitly initialized
// Finish Hold (counter120 or600, ready=1, no pending record announcement).
// Runs original numeric0/2 child Main, phase selector/dispatcher, Exit Init/
// Main/End, ordinary wrapper Main/End, mode parent, and close Init/Main/End.
// It does not claim to construct a race or independently test result readiness,
// the below120 pose branch, record queue, numerical physics, or result assets.
// Race Main's device/render prefix and End's resource cleanup tail are omitted
// at stated slice boundaries; all semantic scheduling between them executes.
namespace {
unsigned checks{},cases{};std::uint64_t instructions{};
void require(bool b,const std::string& what){++checks;if(!b)throw std::runtime_error(what);}
constexpr unsigned owner=0x0d000000,finish=0x0d002000,exitChild=0x0d002200;
constexpr unsigned innerSentinel=0x0d002400,aux=0x0d003000,auxSentinel=0x0d003100;
constexpr unsigned callback=0x0d004000,callbackVt=0x0d004100,timer=0x0d005000,tls=0x0d006000;
constexpr unsigned parent=0x0d010000,wrapper=0x0d012000,closeChild=0x0d013000,resultBase=0x0d015000;
constexpr unsigned parentSentinel=0x0d017000,stack=0x0d030000,stop=0x0f000000,callbackHook=0x0f000100;
struct Route {const char* name;unsigned parentMain,mode,wrapperId,wrapperVt,closeVt,resultVt,resultInit,resultMain;};
const Route routes[]{
 {"TA",0x0c07ab60,2,2,0x0c384804,0x0c38472c,0x0c38d22c,0x0c18ace0,0x0c18d880},
 {"Legend",0x0c08e400,0,3,0x0c385e4c,0x0c385d74,0x0c385ccc,0x0c095880,0x0c0958a0},
 {"Bunta",0x0c185c60,0,3,0x0c385e4c,0x0c385d74,0x0c38cdd4,0x0c18a760,0x0c18a780}
};
}
int main(int argc,char** argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);
 for(const auto& route:routes)for(bool goal:{false,true})for(bool manual:{false,true}){
  ++cases;m.clear();m.zeroRegion(owner,0x20000);m.zeroRegion(stack-0x1000,0x11000);
  // Canonical vtable identities are asserted, not silently replaced by a
  // hand-written selector. 18AB72 installs the TA HLecture vtable38D22C.
  unsigned resultVt=route.resultVt;
  require(m.read32(resultVt+20)==route.resultInit&&m.read32(resultVt+28)==route.resultMain,"canonical result callback identity");
  const unsigned result=route.name==std::string("Bunta")?resultBase+0x134:resultBase;
  const unsigned wrapperId=route.wrapperId,closeId=wrapperId+1,resultId=closeId+1;
  m.write32(parent+12,0x0c383c74);m.write32(parent+32,parentSentinel);
  m.write32(parent+36,wrapper);m.write32(parent+68,wrapper);
  m.write32(wrapper+4,closeChild);m.write32(wrapper+12,route.wrapperVt);
  m.write32(wrapper+16,wrapperId);m.write32(wrapper+20,2);
  m.write32(wrapper+(route.mode==2?92:88),owner);
  m.write32(closeChild+4,result);m.write32(closeChild+12,route.closeVt);m.write32(closeChild+16,closeId);
  m.write32(result+4,parentSentinel);m.write32(result+12,resultVt);m.write32(result+16,resultId);
  m.write32(owner+12,0x0c3831c4);m.write32(owner+32,innerSentinel);
  m.write32(owner+36,finish);m.write32(owner+68,finish);
  m.write32(finish+4,exitChild);m.write32(finish+12,0x0c383314);m.write32(finish+16,3);
  m.write32(finish+20,2);m.write32(finish+76,manual?120:600);m.write32(finish+80,owner);m.write8(finish+88,1);
  m.write32(exitChild+4,innerSentinel);m.write32(exitChild+12,0x0c38326c);
  m.write32(exitChild+16,4);m.write32(exitChild+80,owner);
  m.write32(owner+0x640,aux);m.write32(aux,auxSentinel);m.write32(aux+4,auxSentinel);
  m.write32(owner+0x664,callback);m.write32(callback,callbackVt);m.write32(callbackVt+12,callbackHook);
  m.write32(owner+0x49c,timer);m.write32(owner+0x668,route.mode);m.write8(owner+0x624,goal);
  m.write32(owner+0x66c,goal?0:1);m.write32(owner+0x630,3);
  m.write32(0x0c31c99c+4,0); // ordinary course: TA selects HLecture, not course9 branch
  m.write32(0x0c31c99c,route.name==std::string("Bunta")?2:route.mode);
  m.write32(0x0c31ce18+32,1);m.write8(0x0c92ed00,manual?128:0);
  unsigned tick=0,solvers=0,poses=0,recorders=0,resultInit=0,resultMain=0,raceEnds=0;
  bool blank=false;std::vector<std::string> events;
  std::function<void(RefCpu&)> configure;
  auto slice=[&](unsigned from,unsigned to,unsigned object,bool raceCore){
   RefCpu c(m);configure(c);c.callHooks.erase(from);c.r[15]=stack+0xa000;c.pr=stop;c.r[4]=object;
   if(raceCore){c.r[12]=owner;c.r[13]=0x0c156700;}
   instructions+=c.run(from,to,20000);
  };
  configure=[&](RefCpu& c){
   // Explicit service / resource / device boundaries, not scheduling owners.
   for(unsigned a:{0x0c156700u,0x0c055d60u,0x0c020460u,0x0c141d80u,
     0x0c159f40u,0x0c15a400u,0x0c057700u,0x0c061700u,
     0x0c066840u,0x0c069020u,0x0c069e40u})c.callHooks[a]=[](auto&){};
   c.callHooks[0x0c221fc0]=[](auto& q){q.r[0]=tls;};
   c.callHooks[0x0c160160]=[](auto& q){q.r[0]=0;};
   c.callHooks[0x0c16de80]=[](auto& q){q.r[0]=0;}; // ordinary, no challenger interrupt
   c.callHooks[0x0c06a420]=[](auto& q){q.setFloat(0,0.f);};
   c.callHooks[callbackHook]=[&](auto&){events.push_back("race.outcome.callback");};
   c.callHooks[0x0c0654e0]=[&](auto&){events.push_back("hud.previous");};
   c.callHooks[0x0c024100]=[&](auto& q){events.push_back(q.r[4]==owner+76?"camera.draw":"other.draw");};
   c.callHooks[0x0c062de0]=[&](auto&){++solvers;events.push_back("solver");};
   c.callHooks[0x0c063ce0]=[&](auto&){++poses;events.push_back("publish");};
   c.callHooks[0x0c191b20]=[](auto& q){q.r[0]=0;};
   c.callHooks[0x0c190d00]=[&](auto&){++recorders;events.push_back("recorder");};
   c.callHooks[0x0c211c80]=[&](auto& q){blank=q.r[4]!=0;events.push_back(blank?"blank":"unblank");};
   c.callHooks[0x0c05cc60]=[&](auto&){events.push_back("finish.end");}; // statistics/resource tail is not scheduling
   c.callHooks[0x0c05fdc0]=[&](auto&){slice(0x0c05fe6e,0x0c060080,owner,true);};
   c.callHooks[0x0c0600e0]=[&](auto&){++raceEnds;events.push_back("race.end.begin");slice(0x0c0600e0,0x0c060106,owner,false);events.push_back("race.end.cleanup");};
   c.callHooks[0x0c05f900]=[&](auto&){events.push_back("race.destructor");};
   const auto initAddress=route.mode==2?m.read32(resultVt+20):route.resultInit;
   const auto mainAddress=route.mode==2?m.read32(resultVt+28):route.resultMain;
   c.callHooks[initAddress]=[&](auto&){++resultInit;events.push_back("result.init");};
   c.callHooks[mainAddress]=[&](auto&){++resultMain;events.push_back("result.main");};
  };
  const unsigned holdTicks=manual?1:2,finalRaceTick=holdTicks+6;
  for(tick=1;tick<=finalRaceTick+3;++tick){
   events.clear();const auto oldSolvers=solvers,oldPoses=poses,oldRecorder=recorders;
   RefCpu c(m);configure(c);c.r[15]=stack+0xf000;c.pr=stop;c.r[4]=parent;
   instructions+=c.run(route.parentMain,stop,30000);
   const auto count=[&](const char* s){return unsigned(std::count(events.begin(),events.end(),s));};
   const auto where=[&](const char* s){return std::find(events.begin(),events.end(),s)-events.begin();};
   require(solvers-oldSolvers==unsigned(tick<=finalRaceTick),"one solver only through final Exit update");
   require(poses-oldPoses==unsigned(tick<=finalRaceTick),"one publication only through final Exit update");
   require(recorders-oldRecorder==unsigned(route.mode==2&&tick<=finalRaceTick),"TA recorder follows every final solver, no recorder for mode0");
   require(count("camera.draw")==unsigned(tick<=holdTicks),"Finish camera only in Hold, not Exit/Close/Result");
   if(tick<=holdTicks)require(where("camera.draw")<where("solver"),"Finish draw precedes solver");
   if(tick<=finalRaceTick)require(where("solver")<where("publish"),"solver precedes publication");
   if(route.mode==2&&tick<=finalRaceTick)require(where("publish")<where("recorder"),"publication precedes recorder");
   if(tick==holdTicks){
    require(m.read32(owner+68)==exitChild,"Hold selects Exit child");
    require(m.read32(exitChild+20)==0&&m.read32(exitChild+76)==0&&!blank,"selector does not initialize Exit in same dispatch");
    require(count("finish.end")==1,"Finish End exactly once on transition");
   }
   if(tick>holdTicks&&tick<finalRaceTick){require(blank,"hardware blank retained during Exit");require(resultInit==0,"no result init during Exit");}
   if(tick==holdTicks+1)require(count("blank")==1&&m.read32(exitChild+76)==2,"first Exit dispatch runs Init then Main");
   if(tick==finalRaceTick){
    require(m.read32(exitChild+76)==7,"sixth Exit Main completes at prior counter6");
    require(raceEnds==1&&!blank&&count("unblank")==1,"wrapper ends race and unblanks during final source update");
    require(where("solver")<where("race.end.begin")&&where("publish")<where("unblank"),"final solver/publication precede End/unblank");
    if(route.mode==2)require(where("recorder")<where("race.end.begin"),"final TA recorder precedes race End");
    require(m.read32(parent+68)==closeChild&&m.read32(closeChild+20)==0,"Close selected but not initialized on race completion update");
    require(resultInit==0&&resultMain==0,"result not dispatched on final race update");
   }
   if(tick==finalRaceTick+1){
    require(m.read32(parent+68)==result&&m.read32(result+20)==0,"one Close update selects result without dispatching it");
    require(resultInit==0&&resultMain==0,"Close completion does not run result Init/Main early");
   }
   if(tick==finalRaceTick+2)require(resultInit==1&&resultMain==1,"result Init then Main first occur on following dispatch");
   if(tick==finalRaceTick+3)require(resultInit==1&&resultMain==2,"result initializes exactly once");
   std::cout<<route.name<<" goal="<<goal<<" manual="<<manual<<" tick="<<tick<<" parentChild="<<m.read32(m.read32(parent+68)+16)<<" blank="<<blank<<" events=";
   for(const auto& e:events)std::cout<<e<<',';std::cout<<'\n';
  }
 }
 std::cout<<"PASS "<<cases<<" parent route cases, "<<checks<<" assertions, "<<instructions<<" original instructions. Started at declared ready Hold boundary; scheduling is original, numerical/render/resource and result-content leaves are explicit hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
