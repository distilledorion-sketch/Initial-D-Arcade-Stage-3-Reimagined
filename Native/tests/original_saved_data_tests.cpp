#include "frontend.h"
#include "original_record_rules.h"
#include "original_time_attack_visit.h"
#include "unity_ui_capture.h"
#include <fstream>
#include <iostream>
using namespace idas3;using namespace idas3::original;
unsigned checks=0;void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
void picture(std::filesystem::path path,const std::vector<unsigned>& pixels,int width=640,int height=480){std::ofstream f(path,std::ios::binary);f<<"P6\n"<<width<<' '<<height<<"\n255\n";for(auto p:pixels){char b[]{char(p>>16),char(p>>8),char(p)};f.write(b,3);}}
int main(int argc,char** argv){try{
 if(argc<3)throw std::runtime_error("Usage: original_saved_data_tests root output");setOriginalUiLanguage(argc>3?std::stoi(argv[3]):0);std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
 auto profile=makeOriginalFreshBattleProfile();profile.setu(16,0);profile.setu(40,0);profile.setu(76,5);for(unsigned i=0;i<5;++i)profile.setu(44+i*4,162);profile.setu(72,5000);profile.setu(1140,50);profile.setu(1180,3);profile.setByte(161,1);
 registerOriginalPersonalTimeAttackRecord(profile,originalRecordPartition(6,false),1185888,0,{});
 OriginalSavedData screen;screen.load(root);screen.begin(profile);for(unsigned i=0;i<16;++i)screen.advance(profile);check(screen.ready(),"Saved Data entry fade never completed");
 auto before=profile;for(unsigned page:{0u,1u,2u,3u,5u,6u,7u}){
  check(screen.state().page492==page,"Saved Data page order differs from supplied video");std::vector<unsigned> pixels(640*480,0xff000000);screen.paint(pixels,640,480,profile);picture(out/("page-"+std::to_string(page)+".ppm"),pixels);
  if(page==2){auto night=profile;registerOriginalPersonalTimeAttackRecord(night,originalRecordPartition(6,false),1185882,1,{});
   std::vector<unsigned> alternate(640*480,0xff000000);screen.paint(alternate,640,480,night);unsigned changed=0;
   for(unsigned y=295;y<310;++y)for(unsigned x=332;x<346;++x)changed+=alternate[y*640+x]!=pixels[y*640+x];
   check(changed>10,"Saved record lost its day/night symbol");picture(out/"page-2-night.ppm",alternate);
  }
  if(page==7){for(unsigned i=0;i<20;++i)screen.advance(profile);std::vector<unsigned> next(640*480,0xff000000);screen.paint(next,640,480,profile);
   unsigned changed=0;for(unsigned y=335;y<414;++y)for(unsigned x=115;x<165;++x)changed+=pixels[y*640+x]!=next[y*640+x];
   check(changed>50,"Configuration gear-shift illustration is missing or static");picture(out/"page-7-shift.ppm",next);
   // The content must retain the same centered 4:3 canvas in widescreen.
   std::vector<unsigned> wide(960*480,0xff000000);screen.paint(wide,960,480,profile);
   unsigned differences=0;for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)differences+=wide[y*960+x+160]!=next[y*640+x];
   // Fractional glyph UVs can round on an adjacent sample after translation;
   // permit sparse raster-edge differences, never a shifted row or panel.
   std::cout<<"Widescreen differing pixels: "<<differences<<" / 307200\n";
   check(differences<128,"Saved Data layout changes with viewport aspect ratio");
  }
  if(page!=7){screen.nextPage();screen.advance(profile);}
 }
 for(unsigned offset=0;offset<1228;offset+=4)check(profile.u(offset)==before.u(offset),"Browsing Saved Data changed profile bytes");
 // A highlighted setting must respond to steering/Left/Right without an
 // extra confirmation. Keep the edit transactional until Confirm or row exit.
 {auto options=screen;auto edited=profile;
  constexpr std::array<unsigned,5> offsets{1184,1186,1187,1188,1189},counts{3,3,9,2,2};
  for(unsigned row=0;row<5;++row){options.moveRow(-1);options.advance(edited);
   const unsigned old=edited.byte(offsets[row]);
   options.change(1);options.advance(edited);
   check(options.state().draft1544[row]==(old+1)%counts[row],"Highlighted option ignores Left/Right until an extra confirm");
   check(edited.byte(offsets[row])==old,"Unconfirmed option leaked into the profile");
   options.cancel();options.advance(edited);
   check(options.state().draft1544[row]==old,"Cancel failed on a directly edited option");
   options.change(-1);options.advance(edited);options.confirm();options.advance(edited);
   check(edited.byte(offsets[row])==(old+counts[row]-1)%counts[row],"Direct option edit was not committed");
  }
 }
 // Exercise every row, not just BGM: the cursor must enclose the selected
 // label in the authored 34-pixel rows at every aspect ratio and in edit mode.
 {auto selection=screen;auto selectedProfile=profile;
  for(unsigned row=0;row<5;++row){selection.moveRow(-1);selection.advance(selectedProfile);
   check(selection.state().row1460==row,"Configuration row input skipped a label");
   for(unsigned editing=0;editing<2;++editing){
    if(editing){selection.confirm();selection.advance(selectedProfile);}
    for(const auto size:std::array<std::array<int,2>,3>{{{640,480},{1280,720},{1920,1080}}}){
     const int w=size[0],h=size[1];const float fit=std::min(w/640.f,h/480.f),left=(w-640*fit)*.5f,top=(h-480*fit)*.5f;
     std::vector<unsigned> pixels(w*h,0xff000000);selection.paint(pixels,w,h,selectedProfile);
     int minY=h,maxY=-1,minX=w,maxX=-1;unsigned yellow=0;
     for(int y=int(top+130*fit);y<int(top+325*fit);++y)for(int x=int(left+78*fit);x<int(left+266*fit);++x){
      const auto c=pixels[y*w+x];if(((c>>16)&255)>170&&((c>>8)&255)>140&&(c&255)<100){minY=std::min(minY,y);maxY=std::max(maxY,y);minX=std::min(minX,x);maxX=std::max(maxX,x);++yellow;}
     }
     // Centers are taken from the five label strips in v3sS10conf21.
     constexpr std::array<float,5> labelCenters{160,194,228,262,296};
     const float expected=top+labelCenters[row]*fit;
     check(yellow>80&&std::abs((minY+maxY)*.5f-expected)<=1.5f*fit,"Configuration selection box is not centered on its label");
     const float expectedX=left+(164.f+(originalUiLanguage()==1?13.168f:0.f))*fit;
     check(std::abs((minX+maxX)*.5f-expectedX)<=1.5f*fit,"Configuration cursor misses the localized label column");
     if(w==640)picture(out/("config-row-"+std::to_string(row)+(editing?"-editing":"-focused")+".ppm"),pixels);
    }
    if(editing){selection.cancel();selection.advance(selectedProfile);}
   }
  }
  check(selectedProfile.words==profile.words,"Moving the selection changed saved settings");
 }
 // Select BGM, enter edit, change it, cancel, and then commit a different value.
 screen.moveRow(-1);screen.advance(profile);
 {std::vector<unsigned> pixels(640*480,0xff000000);screen.paint(pixels,640,480,profile);picture(out/"config-focused.ppm",pixels);}
 screen.confirm();screen.advance(profile);
 {std::vector<unsigned> pixels(640*480,0xff000000);screen.paint(pixels,640,480,profile);picture(out/"config-editing.ppm",pixels);}
 screen.change(1);screen.advance(profile);check(profile.byte(1184)==0,"Unconfirmed setting was persisted");
 screen.cancel();screen.advance(profile);check(screen.state().draft1544[0]==0,"Cancel failed to restore option");screen.confirm();screen.advance(profile);screen.change(2);screen.advance(profile);screen.confirm();const auto committed=screen.advance(profile);check(committed.profileWritten&&profile.byte(1184)==2,"Configuration commit did not update profile");
 screen.nextPage();screen.advance(profile);screen.confirm();bool exited=false;for(unsigned i=0;i<18;++i)exited|=screen.advance(profile).parentRequested;check(exited,"Saved Data End failed to return to setup");
 Frontend frontend;frontend.initialize(root);frontend.car=0;frontend.battleProfile=profile;frontend.selectSavedData();frontend.advance(16./60);check(frontend.stage==FrontendStage::SavedData,"Continue skipped Saved Data");
 // The frontend used to cover these margins with a different animation.
 // Compare both sides of the seam to one uninterrupted Saved Data painting,
 // including the animation wrap and non-integer viewport scaling.
 for(unsigned frame:{0u,1u,100u,320u,321u}){
  if(frame)frontend.advance((frame==1?1:frame==100?99:frame==320?220:1)/60.);
  for(const auto size:std::array<std::array<int,2>,5>{{{960,480},{1280,720},{2560,1080},{720,1280},{1365,768}}}){
   const int w=size[0],h=size[1];const float fit=std::min(w/640.f,h/480.f);
   const int dw=int(640*fit),dh=int(480*fit),left=(w-dw)/2,top=(h-dh)/2;
   const auto actual=frontend.paint(w,h);std::vector<unsigned> reference(w*h,0x12345678);
   frontend.savedData()->paint(reference,w,h,frontend.battleProfile);
   unsigned mismatch=0,gaps=0;
   for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(x<left||x>=left+dw||y<top||y>=top+dh){
    mismatch+=actual[y*w+x]!=reference[y*w+x];gaps+=reference[y*w+x]==0x12345678;
   }
   if(mismatch||gaps)std::cerr<<"Margin frame "<<frame<<" size "<<w<<'x'<<h<<" differences "<<mismatch<<" gaps "<<gaps<<'\n';
   check(mismatch==0&&gaps==0,"Saved Data margins use the wrong background or animation phase");
   if(frame==100&&w==2560)picture(out/"saved-data-ultrawide.ppm",actual,w,h);
   // Verify the GPU command path can export this same composition.
   Idas3UiEnable(1);Idas3UiBeginFrame(w+1,h);frontend.advance(1./60.);
   // Different dimensions invalidate the software-only cached frame.
   frontend.paint(640,480);const auto& gpu=frontend.paint(w+1,h);
   unityUiSubmit(gpu.data(),w+1,h,false,false,false);UnityUiFrame info{sizeof(UnityUiFrame)};
   check(Idas3UiGetFrame(&info)&&info.unresolvedSurfaces==0,"Saved Data extension lost a Unity UI surface");
   Idas3UiEnable(0);frontend.advance(1./60.);
  }
 }
 frontend.confirm();frontend.advance(18./60);check(frontend.stage==FrontendStage::Transmission,"Saved Data did not return to saved transmission");
 OriginalTimeAttackVisit visit;visit.load(root);OriginalTimeAttackVisit::Setup setup;setup.condition=6;setup.car=0;setup.ticks6000=1185888;setup.nameGlyphs.fill(162);
 visit.beginAfterResults(setup);check(visit.stage()==OriginalTimeAttackVisit::Stage::Password,"Completed unranked finish skipped password");for(unsigned i=0;i<16;++i)visit.advance({});std::vector<unsigned> pixels(640*480,0);visit.paint(pixels,640,480);picture(out/"password.ppm",pixels);visit.advance({true});for(unsigned i=0;i<20;++i)visit.advance({});check(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Password failed to continue");
 setup.resultStatus=2;visit.beginAfterResults(setup);check(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Time-up incorrectly issued a password");setup.resultStatus=0xffffffff;visit.beginAfterResults(setup);check(visit.stage()==OriginalTimeAttackVisit::Stage::Continue,"Invalid result issued a password");
 std::cout<<checks<<" Saved Data, profile isolation, configuration and finish-routing checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
