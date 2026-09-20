#include "frontend.h"
#include "unity_ui_capture.h"
#include "original_ranking_presentation.h"
#include "original_tuning_presentation.h"
#include "original_results.h"
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace idas3;
// Ranking's translation unit also contains3D mesh assembly. This UI-only test
// deliberately has no renderer: attempting that unrelated boundary is a failure.
void idas3::Mesh::originalCar(const NativeModel&,const NativeAssembly&,Vec3,float,std::uint32_t){throw std::logic_error("UI-only fixture crossed the3D renderer boundary");}

unsigned checks=0;
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
UnityUiFrame snapshot(){UnityUiFrame f{sizeof(f)};check(Idas3UiGetFrame(&f)==1,"UI frame");return f;}
std::uint64_t hashFrame(const UnityUiFrame& f){
 std::vector<UnityUiDraw>d(f.drawCount);std::vector<UnityUiVertex>v(f.vertexCount);Idas3UiCopyDraws(d.data(),int(d.size()));Idas3UiCopyVertices(v.data(),int(v.size()));std::uint64_t h=14695981039346656037ull;
 auto add=[&](const void* p,std::size_t n){const auto* b=static_cast<const unsigned char*>(p);while(n--){h^=*b++;h*=1099511628211ull;}};add(d.data(),d.size()*sizeof(d[0]));add(v.data(),v.size()*sizeof(v[0]));return h;
}
int main(int argc,char** argv)try{
 if(argc!=3)throw std::invalid_argument("unity_ui_screens_tests asset_root NEW-output-dir");
 const std::filesystem::path output=argv[2];check(!std::filesystem::exists(output),"evidence folder must be new");std::filesystem::create_directories(output);
 std::ofstream log(output/"screen-commands.csv");log<<"kind,stage,car,width,height,draws,vertices,textures,unresolved\n";
 Idas3UiEnable(1);Frontend f;f.initialize(argv[1],false);
 auto capture=[&](const char* kind,int width,int height){
  Idas3UiBeginFrame(width,height);const auto& a=f.paint(width,height);unityUiSubmit(a.data(),width,height,false,false,false);const auto one=snapshot();
  check(one.unresolvedSurfaces==0,"UI lost a surface while painting a playable screen");check(one.drawCount>0,"playable menu has no sprite commands");const auto before=hashFrame(one);
  log<<kind<<','<<int(f.stage)<<','<<f.car<<','<<width<<','<<height<<','<<one.drawCount<<','<<one.vertexCount<<','<<one.textureCount<<','<<one.unresolvedSurfaces<<'\n';
  Idas3UiBeginFrame(width,height);const auto& b=f.paint(width,height);unityUiSubmit(b.data(),width,height,false,false,false);const auto two=snapshot();
  check(one.drawCount==two.drawCount&&one.vertexCount==two.vertexCount&&!two.unresolvedSurfaces&&before==hashFrame(two),"paused/cached screen capture changed");
 };
 for(unsigned stage=0;stage<12;++stage){
  f.battleProfile=original::makeOriginalFreshBattleProfile();f.battleProfile.setu(16,0);f.car=0;f.make=6;f.course=3;f.stage=FrontendStage(stage);f.advance(0);f.advance(16./60.);
  capture("all-stages",640,480);capture("wide",1280,720);
 }
 for(int make=0;make<7;++make)for(int car:Frontend::carsForMake(make)){
  f.make=make;f.car=car;f.battleProfile.setu(16,unsigned(car));f.stage=FrontendStage::Car;f.advance(0);f.driverProfileLoaded();capture("all-cars",640,480);
  f.changeColor(1);capture("car-colors",640,480);
 }
 // Imported-name path, then the actual separate layers consumed by Main.
 f.stage=FrontendStage::Transmission;f.advance(0);f.battleProfile.setByte(1192,2);f.battleProfile.setu(76,4);
 const unsigned name[]={18,4,6,0};for(unsigned i=0;i<4;++i)f.battleProfile.setu(44+4*i,name[i]);
 f.stage=FrontendStage::Name;f.advance(0);f.advance(16./60.);capture("imported-name",1280,720);
 std::array<std::vector<unsigned>,6> layers;for(auto& l:layers)l.resize(640*480);
 Idas3UiBeginFrame(640,480);for(auto& l:layers)unityUiClear(l.data(),640,480);
 const auto& p=f.nameEntryPresentation();const auto& s=f.nameEntryState();p.paintBackground(layers[0],640,480);p.paintNameBacking(layers[1],640,480);p.paintGlow(layers[2],640,480,s);p.paint(layers[3],640,480,s,s.sharedCountdown1176);p.paintCursor(layers[4],640,480,s);p.paintLegacy(layers[5],640,480,s);
 for(unsigned i=0;i<6;++i)unityUiSubmit(layers[i].data(),640,480,i==0,i==2||i==4,false);
 check(snapshot().unresolvedSurfaces==0,"separate Name layers missing capture");
 f.stage=FrontendStage::TuningCourse;f.advance(0);f.advance(16./60.);Idas3UiBeginFrame(640,480);
 auto passes=f.tuningCoursePresentation().overlays(640,480,f.tuningCourseState(),f.battleProfile.u(1176));for(auto& pass:passes)unityUiSubmit(pass.pixels.data(),640,480,false,pass.additive,false);
 check(snapshot().unresolvedSurfaces==0&&snapshot().drawCount>0,"separate tuning layers missing capture");
 // Natural complete attract progression; every child retains animated elements.
 f.stage=FrontendStage::Title;f.advance(0);std::set<unsigned> owners;
 for(unsigned tick=0;tick<22000;++tick){f.advance(1./60.);if(owners.insert(f.attractChild()).second||tick%180==0)capture("natural-attract",1280,720);if(owners.size()==8&&f.attractChild()==3)break;}
 check(owners==std::set<unsigned>({3,4,5,6,7,8,11,12}),"all eight original attract owners were not reached");
 // Actual ranking painter includes its temporary640canvas and fitted-copy boundary.
 original::OriginalRankingPresentation ranking;ranking.load(argv[1]);original::OriginalRankingPlayback playback;playback.reset(0);
 for(unsigned course=0;course<9;++course)for(unsigned condition=0;condition<2;++condition){playback.page.courseIndex=course;playback.page.conditionIndex=condition;playback.boardFrame=83;
  Idas3UiBeginFrame(1280,720);const auto& art=ranking.paint(1280,720,playback,f.rankingData());unityUiSubmit(art.data(),1280,720,false,false,false);
  check(snapshot().drawCount>40&&!snapshot().unresolvedSurfaces,"ranking board/fitted canvas missing commands");
 }
 const auto tuningData=original::OriginalTuningData::load(argv[1]);OriginalTuningPresentation tuning(argv[1]);std::vector<unsigned> panel(1280*720);
 for(auto kind:{original::OriginalTuningChildKind::basic,original::OriginalTuningChildKind::performance,original::OriginalTuningChildKind::optionalPart}){
  original::OriginalTuningChild child;child.kind=kind;child.car=0;child.package=0;child.selected=0;child.current=0;child.next=1;child.balance=180000;child.flags=21;child.choice=1;child.optionalIndex=0;
  if(kind==original::OriginalTuningChildKind::basic){child.extraIndex=tuningData.cars[0].packages[0].steps[0].words[4];child.nextThreshold=tuningData.cars[0].packages[0].steps[1].words[2];}
  else if(kind==original::OriginalTuningChildKind::performance)child.nextThreshold=tuningData.cars[0].performance[1].words[1];
  tuning.begin(child,tuningData);
  if(kind!=original::OriginalTuningChildKind::optionalPart){original::OriginalTuningChildFrame update;update.descriptionChanged=true;update.descriptionAddress=kind==original::OriginalTuningChildKind::basic?tuningData.cars[0].packages[0].steps[0].words[3]:tuningData.cars[0].performance[0].words[2];tuning.consume(update);}
  Idas3UiBeginFrame(1280,720);unityUiClear(panel.data(),1280,720);tuning.paintOverlay(panel,1280,720,child,tuningData,879);unityUiSubmit(panel.data(),1280,720,false,false,false);
  check(snapshot().drawCount>20&&!snapshot().unresolvedSurfaces,"result tuning owner/description capture missing");
 }
 const auto results=OriginalBattleResults::load(argv[1]);
 for(unsigned mode=0;mode<3;++mode)for(bool highlighted:{false,true}){OriginalBattleResultsState rs;rs.profileMode=mode;rs.points={1000,2000,300,3300,180000};rs.balanceHighlighted=highlighted;rs.totalTicks6000=123456;rs.signedAdvantage=12.34f;rs.frame60=300;
  Idas3UiBeginFrame(1280,720);unityUiClear(panel.data(),1280,720);results.paint(panel,1280,720,rs,true);unityUiSubmit(panel.data(),1280,720,false,false,false);check(snapshot().drawCount>20&&!snapshot().unresolvedSurfaces,"TA/Legend/Bunta result commands missing");
 }
 Idas3UiEnable(0);std::ofstream(output/"summary.txt")<<"PASS "<<checks<<" checks; all12stages,35cars/colors, importedName/separateName+tuninglayers, eight natural attract owners,18rankingboards,3resulttuningkinds/descriptions,6points/resultsstates, exact repeated command hashes,0unresolved surfaces. No profile writes or graphics device.\n";
 std::cout<<"PASS "<<checks<<" screen command checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
