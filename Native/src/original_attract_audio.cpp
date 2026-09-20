#include "original_attract_audio.h"
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<OriginalStreamDescriptor,18> streams{{
    {"02_speedy_speed_boy.bin",1,1,116},
    {"03_remember_me.bin",1,1,121},
    {"04_save_me.bin",1,1,119},
    {"05_over_the_rainbow.bin",1,1,105},
    {"06_stop_your_self_control.bin",1,1,109},
    {"07_crazy_for_love.bin",1,1,126},
    {"08_express_love.bin",1,1,120},
    {"09_blackout.bin",1,1,114},
    {"10_fall_in_the_web.bin",1,1,112},
    {"11_pamela.bin",1,1,111},
    {"12_fight_for_love_tonight.bin",1,1,113},
    {"01_gamble_rumble.bin",0,1,100},
    {"13_dancin_in_my_dreams.bin",0,1,120},
    {"WIN.bin",0,1,110}, {"LOSE.bin",0,1,110},
    {"TIMEUP.bin",0,1,106}, {"cr.bin",0,1,127},
    {"logo.bin",0,1,127}
}};
}
std::span<const OriginalStreamDescriptor> originalStreamDescriptors(){return streams;}
const OriginalStreamDescriptor& originalStreamDescriptor(unsigned id){
    if(id>=streams.size())throw std::out_of_range("Original stream ID");
    return streams[id];
}
OriginalAttractSoundCommands originalAttractSoundCommands(unsigned child,
    std::uint32_t frame,bool enabled,bool demoFinished){
    OriginalAttractSoundCommands out;
    const auto append=[&](OriginalAttractSoundOperation op,unsigned value=0){
        out.commands[out.count++]={op,value};
    };
    if(child==4){
        //073A5A..073A92: old frame31; incremented frame390 stops audio.
        if(frame==31&&enabled)append(OriginalAttractSoundOperation::Play,17);
        if(frame+1u==390)append(OriginalAttractSoundOperation::Stop);
    }else if(child==7){
        //0E7080..0E7092: stream11 followed by explicit volume125.
        if(frame==0&&enabled){
            append(OriginalAttractSoundOperation::Play,11);
            append(OriginalAttractSoundOperation::Volume,125);
        }
        if(demoFinished)append(OriginalAttractSoundOperation::Stop);
    }
    return out;
}
}
