#include "frontend.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
using Op=OriginalSelectionMusicOperation;
void command(const OriginalSelectionMusicCommand& c,Op op,unsigned cue,unsigned word,unsigned argument=0){check(c.operation==op&&c.cue==cue&&c.word==word&&c.argument==argument,"Selection command/order differs");}
void initialLoad(Frontend& f,unsigned cue){const auto c=f.takeSelectionMusicCommands();check(c.size()==1,"Stage Init must emit one load");command(c[0],Op::Load,cue,cue?0xa8:0x1a8,cue?103:109);}
void startTick(Frontend& f,unsigned cue){const auto c=f.takeSelectionMusicCommands();check(c.size()==2,"First owner tick must start and write source level");command(c[0],Op::Start,cue,cue?0xa8:0x1a8);command(c[1],Op::Control,cue,0x4a0,cue?103:109);}
}
int main(){try{
    {
        Frontend f;f.stage=FrontendStage::Make;f.advance(0);initialLoad(f,0);
        check(f.selectionMusicState().pendingStart33==1&&!f.selectionMusicState().playing4,"Zero elapsed invented a music service tick");
        f.advance(0);check(f.takeSelectionMusicCommands().empty(),"Zero elapsed repeated Init");
        f.advance(1./120.);check(f.takeSelectionMusicCommands().empty(),"Half-frame started music early");
        f.advance(1./120.);startTick(f,0);
        check(f.selectionMusicState().volumeCounter0C31EB34==1,"Start tick did not include first volume write");
        f.advance(5./60.);const auto c=f.takeSelectionMusicCommands();check(c.size()==5,"Source needs six volume writes including start tick");for(const auto& e:c)command(e,Op::Control,0,0x4a0,109);
        check(!f.selectionMusicState().volumePending34&&!f.selectionMusicState().volumeCounter0C31EB34,"Source volume initialization did not finish after six ticks");
        f.advance(20./60.);check(f.takeSelectionMusicCommands().empty(),"Steady menu restarted music");
    }
    for(const auto stage:{FrontendStage::Make,FrontendStage::Car,FrontendStage::Transmission,FrontendStage::Mode,FrontendStage::Course,FrontendStage::Route,FrontendStage::Weather,FrontendStage::Time,FrontendStage::Rival}){
        const unsigned cue=stage<=FrontendStage::Transmission?0:1;Frontend f;f.stage=stage;f.advance(0);initialLoad(f,cue);f.advance(1./60.);startTick(f,cue);
        f.advance(0);check(f.takeSelectionMusicCommands().empty(),"Repeated render emitted music");
    }
    {
        Frontend f;f.stage=FrontendStage::Course;f.advance(0);initialLoad(f,1);f.advance(1.);
        const auto commands=f.takeSelectionMusicCommands();check(commands.size()==7,"Batched cosmetic course updates skipped source music ticks");command(commands.front(),Op::Start,1,0xa8);for(unsigned i=1;i<commands.size();++i)command(commands[i],Op::Control,1,0x4a0,103);
        check(f.selectionMusicState().playing4&&!f.selectionMusicState().volumePending34,"Batched course music did not settle");
    }
    {
        Frontend f;f.confirm();f.advance(3./60.);check(f.stage==FrontendStage::Title&&f.takeSelectionMusicCommands().empty(),"Attract Start wait requested TYPE early");
        f.advance(1./60.);check(f.stage==FrontendStage::SaveSelect,"Original Start handoff boundary moved");const auto c=f.takeSelectionMusicCommands();check(c.size()==3,"Actual Start must Init and service TYPE on the same owner update");
        command(c[0],Op::Load,0,0x1a8,109);command(c[1],Op::Start,0,0x1a8);command(c[2],Op::Control,0,0x4a0,109);
        // The save picker stands in for the card reader and is the first owner
        // after Start, so TYPE loads there and must carry into the maker menu:
        // picking a file services the cue, it does not reload it.
        f.confirm();check(f.takeSaveFileChosen(),"Save file choice never reaches the host");
        f.stage=FrontendStage::Make;f.advance(1./60.);const auto entry=f.takeSelectionMusicCommands();
        check(entry.size()==1,"Entering the maker menu reloaded the TYPE cue");command(entry[0],Op::Control,0,0x4a0,109);
        f.advance(8./60.);f.takeSelectionMusicCommands();check(f.inputReady(),"Maker entry timing changed");f.confirm();f.advance(133./60.);check(f.stage==FrontendStage::Make,"Maker confirmation ended early");f.advance(1./60.);check(f.stage==FrontendStage::Car,"Maker confirmation boundary changed");check(f.takeSelectionMusicCommands().empty(),"Car reloaded the same TYPE cue");
        f.advance(8./60.);check(f.inputReady(),"Car entry timing changed");f.confirm();f.advance(163./60.);check(f.stage==FrontendStage::Car,"Car confirmation ended before164 source ticks");f.advance(1./60.);check(f.stage==FrontendStage::Transmission,"Car confirmation failed at164 source ticks");check(f.takeSelectionMusicCommands().empty(),"Transmission restarted TYPE");
        f.advance(16./60.);check(f.inputReady(),"Transmission entry timing changed");f.confirm();f.advance(141./60.);check(f.stage==FrontendStage::Transmission,"Transmission confirmation ended early");f.advance(1./60.);check(f.stage==FrontendStage::Mode,"Transmission completion boundary changed");
        const auto mode=f.takeSelectionMusicCommands();check(mode.size()==5,"Mode Init must switch bank and service it on the same update");command(mode[0],Op::Control,0,0x1200a0);command(mode[1],Op::Unload,0,0);command(mode[2],Op::Load,1,0xa8,103);command(mode[3],Op::Start,1,0xa8);command(mode[4],Op::Control,1,0x4a0,103);
        f.advance(16./60.);f.takeSelectionMusicCommands();
        for(const auto stage:{FrontendStage::Course,FrontendStage::Route,FrontendStage::Weather,FrontendStage::Time,FrontendStage::Rival}){f.stage=stage;f.advance(1./60.);check(f.takeSelectionMusicCommands().empty(),"SELECT navigation restarted the same bank");}
        f.endSelectionMusic();auto exit=f.takeSelectionMusicCommands();check(exit.size()==2,"Race exit must stop and unload SELECT");command(exit[0],Op::Control,1,0x1200a0);command(exit[1],Op::Unload,1,0);
        check(f.selectionMusicState().scene==4&&f.selectionMusicState().handle0==-1,"Race exit kept selection resource ownership");f.endSelectionMusic();check(f.takeSelectionMusicCommands().empty(),"Repeated race exit emitted commands");
        f.advance(0);initialLoad(f,1);f.advance(1./60.);startTick(f,1);
        f.stage=FrontendStage::Title;f.advance(0);auto title=f.takeSelectionMusicCommands();check(title.size()==2,"Returning to attract must stop and unload");check(f.selectionMusicState().scene==0&&f.selectionMusicState().selectedCue28==-1,"Attract retained a pending selection cue");
    }
    for(const int course:{3,4,8}){
        Frontend f;f.course=course;f.stage=course==4?FrontendStage::Weather:course==8?FrontendStage::Route:FrontendStage::Time;
        f.advance(6./60.);f.takeSelectionMusicCommands();f.confirm();
        check(f.takeSelectionMusicCommands().empty(),"Input emitted a source owner command before its tick");
        f.advance(31./60.);
        check(!f.takeStartRequest()&&f.confirmationInProgress(),"TA skipped phase3 after its final choice");
        check(f.takeSelectionMusicCommands().empty(),"TA faded before owner+440 reached1");
        f.advance(0);check(f.takeSelectionMusicCommands().empty(),"Repeated rendering advanced the exit");
        f.advance(1./60.);auto events=f.takeSelectionMusicCommands();
        check(events.size()==1,"TA must issue one exit fade on confirmation tick32");command(events[0],Op::Control,1,0xaa0,8);
        check(!f.takeStartRequest(),"TA race started at fade request instead of phase4");
        f.confirm();f.change(1);check(f.takeSelectionMusicCommands().empty(),"Blocked exit input repeated the fade");
        f.advance(14./60.);check(f.takeSelectionMusicCommands().empty()&&!f.takeStartRequest(),"TA stopped before owner+440 exceeded15");
        f.advance(1./60.);events=f.takeSelectionMusicCommands();
        check(events.size()==1,"TA phase4 must emit one Stop on confirmation tick47");command(events[0],Op::Control,1,0x1200a0);
        check(f.takeStartRequest()&&!f.takeStartRequest(),"TA exit did not request exactly one start");
        f.endSelectionMusic();events=f.takeSelectionMusicCommands();
        check(events.size()==1,"App race handoff repeated the already-completed Stop");command(events[0],Op::Unload,1,0);
        if(course==4)check(f.night,"Happo lost its source night restriction");
        if(course==8)check(f.wet&&f.night,"Snow lost its source weather restrictions");
    }
    {
        Frontend f;f.stage=FrontendStage::Time;f.advance(6./60.);f.takeSelectionMusicCommands();f.confirm();f.advance(47./60.);
        const auto c=f.takeSelectionMusicCommands();check(c.size()==2&&f.takeStartRequest(),"Batched TA exit lost source phases");command(c[0],Op::Control,1,0xaa0,8);command(c[1],Op::Control,1,0x1200a0);
    }
    {
        Frontend f;f.gameMode=OriginalGameMode::LegendOfTheStreets;f.stage=FrontendStage::Rival;
        f.advance(6./60.);f.takeSelectionMusicCommands();f.confirm();f.advance(0);
        check(f.takeSelectionMusicCommands().empty(),"Legend faded before accepted input was serviced");
        f.advance(1./60.);const auto c=f.takeSelectionMusicCommands();
        check(c.size()==1,"Legend accepted confirmation must emit one AA0");command(c[0],Op::Control,1,0xaa0,8);
        f.advance(133./60.);check(f.takeSelectionMusicCommands().empty()&&!f.takeStartRequest(),"Legend repeated fade or skipped its hold");
        f.advance(1./60.);check(f.takeStartRequest()&&f.takeSelectionMusicCommands().empty(),"Legend completion timing changed");
        f.endSelectionMusic();const auto exit=f.takeSelectionMusicCommands();check(exit.size()==2,"Legend handoff must stop and unload");command(exit[0],Op::Control,1,0x1200a0);command(exit[1],Op::Unload,1,0);
    }
    std::cout<<"Frontend selection music passed:"<<checks<<" checks; zero-time Init, post-owner servicing, nine menus, Legend accepted-confirm fade and TA tick32 fade/tick47 Stop\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
