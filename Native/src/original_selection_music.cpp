#include "original_selection_music.h"
#include <bit>
#include <stdexcept>
namespace idas3::original {
namespace {
using Op=OriginalSelectionMusicOperation;
// Every cue of the source BGM table at 31DF04, recovered by running its
// startup constructors on the reference CPU. 0..2 are the menu scores; 3..33
// are the 31 rivals' themes, which the next-rival step asks for as the enemy
// number plus three; 34..38 are the after-race tracks.
constexpr std::array<OriginalSelectionMusicDescriptor,39> descriptors{{
    {"TYPE.bin",0x000001a8,0x100,109},{"SELECT.bin",0x000000a8,0x200,103},{"RESULT.bin",0x000001a8,0x100,108},
    {"ITSUKI.bin",0x00000ba8,0x1900,102},{"KENJI2.bin",0x000006a8,0x600,127},{"SHINGO.bin",0x000014a8,0x2200,103},
    {"SUETSUGI.bin",0x000010a8,0x1e00,101},{"ATSUO2.bin",0x000007a8,0x700,110},{"MAKO.bin",0x000016a8,0x2400,108},
    {"DEBU.bin",0x000006a8,0x700,102},{"KENTA.bin",0x000001a8,0x200,107},{"KEISUKE.bin",0x000005a8,0x600,102},
    {"IKETANI.bin",0x000003a8,0x400,106},{"NAKAZATO.bin",0x000015a8,0x2300,110},{"SUDOU.bin",0x000013a8,0x2000,92},
    {"RYOSUKE.bin",0x000004a8,0x500,103},{"TAKUMI1.bin",0x000000a8,0x0,113},{"SEIJI.bin",0x000012a8,0x1f00,101},
    {"SUDOU.bin",0x000013a8,0x2000,92},{"KAI.bin",0x000019a8,0xf00,107},{"MIKI.bin",0x000005a8,0x500,105},
    {"DAIKI.bin",0x000011a8,0x2100,103},{"SAKAI.bin",0x00000aa8,0xb00,109},{"TACHI.bin",0x000009a8,0xa00,114},
    {"NOBUHIKO.bin",0x00000ea8,0x1c00,112},{"SAKAMOTO.bin",0x000017a8,0x2500,104},{"WATARU.bin",0x000007a8,0x800,117},
    {"KYOKO2.bin",0x000001a8,0x0,98},{"RYOSUKE.bin",0x000004a8,0x500,109},{"LANEVO5.bin",0x000003a8,0x300,113},
    {"LANEVO6.bin",0x000004a8,0x400,103},{"KEISUKE2.bin",0x00000da8,0xc00,100},{"TAKUMI3.bin",0x000018a8,0xe00,113},
    {"BUNTA.bin",0x000008a8,0x900,110},{"AFTER_L.bin",0x00001ba8,0x1100,111},{"AFTER_W.bin",0x00001aa8,0x1000,117},
    {"AFTER_W2.bin",0x00001ca8,0x1200,120},{"WIN2.bin",0x000000a8,0x100,117},{"LOSE3.bin",0x000000a8,0x100,118}}};
constexpr std::array<OriginalSelectionMusicBinding,12> bindings{{
    {0x0c11bf20,0x0c11c54e,OriginalSelectionMusicCue::Type,"iSelSub01/card entry"},
    {0x0c11cfe0,0x0c11d15a,OriginalSelectionMusicCue::Type,"maker group"},
    {0x0c11d720,0x0c11db14,OriginalSelectionMusicCue::Type,"car group"},
    {0x0c11e260,0x0c11ea36,OriginalSelectionMusicCue::Type,"mission/tuning/name group"},
    {0x0c11f400,0x0c11f8d4,OriginalSelectionMusicCue::Type,"Integra color group"},
    {0x0c120080,0x0c120600,OriginalSelectionMusicCue::Type,"card-check group"},
    {0x0c121040,0x0c12126c,OriginalSelectionMusicCue::Select,"iSelSub0402/mode"},
    {0x0c121900,0x0c121c4c,OriginalSelectionMusicCue::Select,"iSelSub05/rival"},
    {0x0c122360,0x0c122b46,OriginalSelectionMusicCue::Type,"combined selection group"},
    {0x0c135300,0x0c135408,OriginalSelectionMusicCue::Select,"Time Attack owner"},
    {0x0c1375e0,0x0c1379aa,OriginalSelectionMusicCue::Select,"Time Attack course owner"},
    {0x0c183ce0,0x0c183f70,OriginalSelectionMusicCue::Select,"Bunta course owner"}}};
void emit(OriginalSelectionMusicCommands& out,Op operation,const OriginalSelectionMusicState& s,
    std::uint32_t word=0,std::uint32_t argument=0){
    if(out.count>=out.commands.size())throw std::logic_error("Selection music command bound");
    out.commands[out.count++]={operation,std::uint32_t(s.selectedCue28),word,argument,
        operation==Op::Control?std::int32_t(std::uint16_t(s.handle0)):s.handle0};
}
void stop(OriginalSelectionMusicState&s,OriginalSelectionMusicCommands&out){
    if(s.handle0>=0&&s.slotRegistered&&s.loaded32&&s.playing4==1){
        emit(out,Op::Control,s,0x001200a0,0);s.playing4=0;
    }
}
void unload(OriginalSelectionMusicState&s,OriginalSelectionMusicCommands&out){
    if(s.handle0>=0){stop(s,out);emit(out,Op::Unload,s);s.handle0=-1;s.loaded32=0;s.slotRegistered=false;}
}
}
const OriginalSelectionMusicDescriptor& originalSelectionMusicDescriptor(OriginalSelectionMusicCue cue){return descriptors.at(std::uint32_t(cue));}
const OriginalSelectionMusicDescriptor& originalMusicCueDescriptor(unsigned cue){return descriptors.at(cue);}
std::span<const OriginalSelectionMusicBinding> originalSelectionMusicBindings(){return bindings;}
std::optional<OriginalSelectionMusicCue> originalSelectionMusicCueForInit(std::uint32_t address){
    for(const auto& binding:bindings)if(binding.initAddress==address)return binding.cue;return {};
}
OriginalSelectionMusicCommands requestOriginalSelectionMusic(OriginalSelectionMusicState&s,
    OriginalSelectionMusicCue cue,const OriginalSelectionMusicLoadResult& load){
    const auto& d=originalSelectionMusicDescriptor(cue);OriginalSelectionMusicCommands out;
    if(s.selectedCue28==std::int32_t(cue)){s.sourceLevel24=d.sourceLevel;return out;}
    if(s.loaded32)unload(s,out);
    s.selectedCue28=std::int32_t(cue);s.command8=d.command;s.loadBusy20=load.loadingBusy?1:0;
    s.controlArgument22=8;s.sourceLevel24=d.sourceLevel;s.handle0=load.handle;
    s.playing4=0;s.loaded32=load.loaded?1:0;s.slotRegistered=load.slotRegistered;
    emit(out,Op::Load,s,d.command,d.sourceLevel);
    s.pendingStart33=1;s.delay36=0;out.cueChanged=true;return out;
}
OriginalSelectionMusicCommands tickOriginalSelectionMusic(OriginalSelectionMusicState&s,bool loaderBusy){
    OriginalSelectionMusicCommands out;
    if(s.loadBusy20==1&&!loaderBusy)s.loadBusy20=0;
    if(s.pendingStart33==1&&s.loadBusy20!=1){
        const auto old=s.delay36;--s.delay36;
        if(std::bit_cast<std::int32_t>(old)<=0&&s.handle0>=0){
            if(s.playing4==1)stop(s,out);
            s.playing4=1;emit(out,Op::Start,s,s.command8);
            s.pendingStart33=0;s.volumePending34=1;s.volumeCounter0C31EB34=0;
        }
    }
    if(s.volumePending34==1){
        s.volumeCounter0C31EB34=std::bit_cast<std::int32_t>(std::uint32_t(s.volumeCounter0C31EB34)+1);
        if(s.volumeCounter0C31EB34>0){
            emit(out,Op::Control,s,0x000004a0,std::uint32_t(std::int32_t(std::int16_t(s.sourceLevel24))));
            if(s.volumeCounter0C31EB34>5){s.volumePending34=0;s.volumeCounter0C31EB34=0;}
        }
    }
    return out;
}
OriginalSelectionMusicCommands stopOriginalSelectionMusic(OriginalSelectionMusicState&s){OriginalSelectionMusicCommands out;stop(s,out);return out;}
OriginalSelectionMusicCommands exitOriginalSelectionMusic(OriginalSelectionMusicState&s){
    OriginalSelectionMusicCommands out;if(s.handle0>=0)emit(out,Op::Control,s,0x00000aa0,std::uint32_t(std::int32_t(std::int16_t(s.controlArgument22))));return out;
}
OriginalSelectionMusicCommands changeOriginalSelectionMusicScene(OriginalSelectionMusicState&s,std::int32_t scene){
    OriginalSelectionMusicCommands out;if(s.scene==scene)return out;
    unload(s,out);s=OriginalSelectionMusicState{};s.scene=scene;return out;
}
}
