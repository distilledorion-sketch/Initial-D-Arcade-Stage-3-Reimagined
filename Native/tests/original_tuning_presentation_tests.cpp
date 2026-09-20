#include "original_tuning_presentation.h"
#include "original_menu_audio.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <fstream>
using namespace idas3;using namespace idas3::original;using namespace idas3::reference;
namespace {
unsigned checks=0;std::uint64_t instructions=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F&& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"Unsupported presentation input accepted");}
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Expected canonical image and project root");const std::filesystem::path root=argv[2];
    const auto data=OriginalTuningData::load(root);const auto ui=OriginalTuningUi::load(root);OriginalTuningPresentation presentation(root);RefMemory m(argv[1]);
    constexpr unsigned child=0x0d000000,stack=0x0d100000,stop=0x00ff0000;
    for(unsigned car=0;car<35;++car)for(unsigned package=0;package<data.car(car).packages.size();++package)for(unsigned selected=0;selected<data.car(car).packages[package].steps.size();++selected){
        OriginalTuningChild s;s.kind=OriginalTuningChildKind::basic;s.car=car;s.package=package;s.selected=selected;
        m.clear();m.zeroRegion(child,0x10000);m.zeroRegion(stack,0x10000);m.write32(stack+4144,child);m.write32(child+20,car);m.write32(child+28,package);m.write32(child+32,selected);
        RefCpu c(m);c.r[14]=stack;c.r[15]=stack+0xf000;c.r[0]=child+0x4000;instructions+=c.run(0x0c115ba6,0x0c115bcc,1000);
        check(float(m.read32(stack+4148))==originalTuningDescriptionSize(s),"Source constructor font size mismatch");
        presentation.begin(s,data);const auto size=presentation.description().size;check(size==originalTuningDescriptionSize(s),"Presentation lost constructor text size");
        OriginalTuningChildFrame event;event.descriptionChanged=true;event.descriptionAddress=data.car(car).packages[package].steps[selected].words[3];event.descriptionX=240;event.descriptionY=388;
        presentation.consume(event);presentation.consume(event);presentation.consume({});
        check(presentation.description().address==event.descriptionAddress&&presentation.description().x==240&&presentation.description().y==388&&presentation.description().size==size,"Description did not persist across repeated renders/chained updates");
    }
    // The actual force-play dispatch, not an inferred enum-name translation.
    for(unsigned cue:{2u,5u,6u,11u,12u,13u}){
        m.clear();m.zeroRegion(child,0x10000);m.zeroRegion(stack,0x10000);m.write32(child,1);m.write32(child+40,0x0c31eb68);
        RefCpu c(m);c.r[4]=child;c.r[5]=cue;c.r[6]=1;c.r[15]=stack+0xf000;c.pr=stop;unsigned command=0;
        c.callHooks[0x0c1ed9c0]=[&](auto&cpu){command=cpu.r[4];};instructions+=c.run(0x0c1435c0,stop,1000);
        const auto sound=loadOriginalTuningSound(root,cue);check(sound.command==command,"Original tuning sound command mismatch");
        const unsigned expectedSample=cue==5?7:cue==6?8:cue;
        check(sound.playbackId==cue&&sound.sampleId==expectedSample&&sound.clip.channels==1&&!sound.clip.samples.empty(),"Wrong original tuning sample");
        check(sound.sequenceVolume==(cue==2?120:127),"Original sequence volume missing");
    }
    rejects([&]{loadOriginalTuningSound(root,0);});rejects([&]{loadOriginalTuningSound(root,14);});
    for(unsigned scenario=0;scenario<4;++scenario){
        OriginalTuningChild s;s.kind=scenario<2?(scenario?OriginalTuningChildKind::performance:OriginalTuningChildKind::basic):OriginalTuningChildKind::optionalPart;
        s.current=0;s.next=1;s.flags=scenario?53:63;s.balance=125000;s.nextThreshold=150000;s.choice=scenario==2?0:1;
        s.extraIndex=data.car(0).packages[0].steps[0].words[4];if(std::int32_t(s.extraIndex)<0)s.flags&=~2u;
        presentation.begin(s,data);OriginalTuningChildFrame event;event.descriptionChanged=true;
        if(s.kind!=OriginalTuningChildKind::optionalPart){event.descriptionAddress=scenario?data.car(0).performance[0].words[2]:data.car(0).packages[0].steps[0].words[3];event.descriptionX=scenario?90:250;presentation.consume(event);}
        else check(presentation.description().address==data.car(0).optional[0].words[4]&&presentation.description().x==250&&presentation.description().y==390,"Optional constructor text missing before first update");
        const auto initialFrame=s.frame;std::vector<unsigned> overlay(640*480),again(640*480),wide(960*480),opaque(640*480,0xff246890);
        presentation.paintOverlay(overlay,640,480,s,data,879);presentation.paintOverlay(again,640,480,s,data,879);presentation.paintOverlay(wide,960,480,s,data,879);
        check(overlay==again&&s.frame==initialFrame,"Paint advanced or mutated source animation");
        for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)check(overlay[y*640+x]==wide[y*960+x+160],"Widescreen stretched original tuning composition");
        unsigned transparent=0,visible=0,partial=0;for(auto p:overlay){transparent+=(p>>24)==0;visible+=(p>>24)!=0;partial+=(p>>24)>0&&(p>>24)<255;}
        check(transparent>100000&&visible>10000,"Original tuning overlay lost aperture or alpha edges");
        // Opaque reference RGB should remain unchanged by the opt-in carrier.
        auto carrier=opaque;ui.paint(opaque,640,480,s,data,presentation.description(),879);ui.paintOverlay(carrier,640,480,s,data,presentation.description(),879);
        for(unsigned i=0;i<opaque.size();++i)for(unsigned shift:{0u,8u,16u})check(std::abs(int((opaque[i]>>shift)&255)-int((carrier[i]>>shift)&255))<=2,"Overlay changed opaque source artwork");
        auto invalid=s;invalid.car=1;rejects([&]{presentation.drawList(invalid,data,879);});presentation.clear();rejects([&]{presentation.consume({});});
    }
    std::cout<<"Original tuning presentation passed "<<checks<<" checks, "<<instructions<<" source instructions; persistent descriptions/font exception, original cue mappings, four authentic overlay states and widescreen aperture.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
