#include "frontend.h"
#include <iostream>
using namespace idas3;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
int main(int argc,char**argv)try{
    require(argc==2,"Game root required");Frontend f;f.initialize(argv[1]);f.advance(0);
    require(f.attractChild()==3,"Fresh game does not enter original Sega owner");
    std::vector<unsigned> route{3};unsigned logoStarts=0,demoStarts=0,demoStops=0;
    bool gasChecked=false,rankingChecked=false,demoChecked=false,demoEndingChecked=false;
    auto consume=[&]{for(const auto& e:f.takeAttractAudioEvents()){
        if(e.child==4&&e.sourceFrame==31)++logoStarts;
        if(e.child==7&&e.sourceFrame==0)++demoStarts;
        if(e.child==7&&e.finished){require(e.sourceFrame==5837,"Demo completion skipped the source pose-wrap gate");++demoStops;}
    }};consume();
    for(unsigned tick=0;tick<40000&&route.size()<9;++tick){
        const auto old=f.attractChild();f.advance(1./60.);consume();
        if(f.attractChild()!=old)route.push_back(f.attractChild());
        if(f.attractChild()==7&&f.attractFrame()==701){
            require(f.demoCursor().frame==700,"Demo camera and actor cursor are not on rendered source frame");
            require(f.demoData().hasCamera(),"Authored demo cameras absent");
            require(f.demoData().camera(700).world!=f.demoData().camera(701).world,"Demo camera static");demoChecked=true;
        }
        if(f.attractChild()==7&&f.attractFrame()==5837){
            require(f.demoCursor().frame==5836,"Demo did not display its final recorded frame");demoEndingChecked=true;
        }
        if(f.showingGasstand()&&f.attractFrame()==20){
            const auto talk=f.paint(640,480);f.advance(1./60.);consume();const auto stable=f.paint(640,480);
            require(stable!=talk,"Recovered gasstand background not moving");require(f.paint(640,480)==stable,"Painting advances timing");
            const auto wide=f.paint(1280,720);require((wide[360*1280+20]&0xffffff)!=0,"Widescreen backdrop missing");
            for(int y=0;y<720;++y)for(int x=0;x<960;++x)require(wide[y*1280+x+160]==stable[(y*480/720)*640+x*640/960],"Widescreen stretches original central composition");
            gasChecked=true;
        }
        if(f.attractChild()==12&&f.attractFrame()==150){require(f.rankingState().events.drawCar,"Ranking never creates top car");rankingChecked=true;}
    }
    require(route==std::vector<unsigned>({3,4,5,6,7,8,11,12,3}),"Full original attract route disconnected");
    require(gasChecked&&rankingChecked&&demoChecked&&demoEndingChecked,"Attract owner or final demo pose skipped");
    require(logoStarts==1&&demoStarts==2&&demoStops==1,"Source audio events lost or repeated");
    // Entry and first demo update intentionally duplicate frame0; mixer deduplicates.
    for(unsigned desired:{3u,4u,5u,6u,7u,8u,11u,12u}){
        f.stage=FrontendStage::Make;f.advance(0);f.stage=FrontendStage::Title;f.advance(0);
        unsigned guard=0;while(f.attractChild()!=desired&&guard++<30000)f.advance(1./60.);
        require(f.attractChild()==desired,"Cannot reach attract owner");f.takeAttractAudioEvents();
        f.confirm();f.advance(1./60.);require(f.screenFadeArgb()==0xff000000,"Start does not clear attract");
        bool stopped=false;for(const auto& e:f.takeAttractAudioEvents())stopped|=e.child==~0u;require(stopped,"Start left audio playing");
        f.advance(2./60.);require(f.stage==FrontendStage::Title,"Start skipped parent wait");
        // Start lands on the save picker, not straight on the maker menu:
        // choosing a file is what rebinds the per-file stores and moves the
        // stage on, so the walk into the game runs through it.
        f.advance(1./60.);require(f.stage==FrontendStage::SaveSelect,"Cannot enter game from attract owner");
        f.confirm();require(f.takeSaveFileChosen(),"Save file choice never reaches the host");
    }
    std::cout<<"PASS all8 attract owners, full loop, camera/data clock, audio events, widescreen conversation and Start during every owner\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
