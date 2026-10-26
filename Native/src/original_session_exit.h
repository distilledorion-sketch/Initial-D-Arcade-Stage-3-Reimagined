#pragma once
#include "native_assets.h"
#include "original_time_attack_background.h"
#include <algorithm>
#include <stdexcept>

namespace idas3::original {
// A_VISIT_EJECTCARD -> A_VISIT_GAMEOVER (08E400), also used by A_TA.
// The cabinet waits for physical card hardware. Desktop saves supply that
// acknowledgement, followed by a bounded retrieval presentation.
class OriginalSessionExit {
public:
    enum class Stage { Inactive, SavingCard, RetrieveCard, GameOver, Finished };
    Stage stage=Stage::Inactive;
    unsigned frame=0;
    void begin(const std::filesystem::path& root,bool hasCard){
        if(gameOver_.chunks.empty()){
            const auto path=root/"data/original_assets/session_exit";
            gameOver_=NativeModel::load(path/"gameover_v3/gameover_v3.idasmesh");
            gameOverTextures_=NativeTextureBank::load(path/"gameover_v3/textures/textures.idastex");
            card_=NativeModel::load(path/"ejectcard/ejectcard.idasmesh");
            cardTextures_=NativeTextureBank::load(path/"ejectcard/textures/textures.idastex");
            const auto select=root/"data/original_assets/name_entry/select";
            background_=NativeModel::load(select/"select.idasmesh");
            backgroundTextures_=NativeTextureBank::load(select/"textures/textures.idastex");
            if(gameOver_.chunks.size()!=11||card_.chunks.size()!=4)throw std::runtime_error("Incomplete session-end artwork");
        }
        frame=backgroundFrame_=displayedBackgroundFrame_=0;stage=hasCard?Stage::SavingCard:Stage::GameOver;
    }
    bool active()const{return stage!=Stage::Inactive&&stage!=Stage::Finished;}
    void step(bool confirm,bool saved){
        if(!active())return;
        if(stage==Stage::SavingCard||stage==Stage::RetrieveCard){
            displayedBackgroundFrame_=backgroundFrame_;
            backgroundFrame_=advanceOriginalTimeAttackBackground(backgroundFrame_);
        }
        ++frame;
        if(stage==Stage::SavingCard){if(saved&&frame>=60){stage=Stage::RetrieveCard;frame=0;}}
        else if(stage==Stage::RetrieveCard){if(frame>=120||(confirm&&frame>=30)){stage=Stage::GameOver;frame=0;}}
        // 095780: accelerator fast-forwards at270; the owner finishes after300.
        else if(stage==Stage::GameOver){if(confirm&&frame<270)frame=270;if(frame>301)stage=Stage::Finished;}
    }
    void paint(std::span<std::uint32_t> pixels,int width,int height)const{
        const float fit=std::min(float(width)/640.f,float(height)/480.f);
        SpritePlacement p;p.invertY=true;p.authoredHeight=0;p.scale=100.f*fit;
        p.offsetX=(width-640.f*fit)*.5f;p.offsetY=(height-480.f*fit)*.5f;p.defaultOriginalUiColors=true;
        const auto draw=[&](const NativeModel& model,const NativeTextureBank& textures,unsigned chunk,float x=0,float y=0){
            auto placement=p;placement.offsetX+=x*100.f*fit;placement.offsetY-=y*100.f*fit;
            compositeOriginalMenuChunk(pixels,width,height,textures,model.chunks.at(chunk),placement);
        };
        if(stage==Stage::SavingCard||stage==Stage::RetrieveCard){
            // AEjectCard04783C selects tile45.0483DC calls the same112E60
            // owner as HLecture, before the eject-card panel. These are full
            // original screens, not a card panel floating on an empty backdrop.
            for(const auto& tile:originalTimeAttackBackgroundDraws(displayedBackgroundFrame_))
                draw(background_,backgroundTextures_,tile.chunk,tile.position.x,tile.position.y);
            draw(card_,cardTextures_,1);draw(card_,cardTextures_,2);
            if(stage==Stage::RetrieveCard){draw(card_,cardTextures_,0);draw(card_,cardTextures_,3);}
        }else if(stage==Stage::GameOver){
            draw(gameOver_,gameOverTextures_,10);
            // 0CFF60 and source table25DE50: ten alternating horizontal strips.
            for(unsigned strip=0;strip<10;++strip){
                const unsigned first=1+2*strip,last=20+2*strip;
                if(frame<first)continue;
                const float start=(strip&1)?6.399999618530273f:-6.399999618530273f;
                const float x=frame>last?0.f:start+(0.f-start)/float(last-first)*float(frame-first);
                draw(gameOver_,gameOverTextures_,strip,x);
            }
        }
    }
private:
    NativeModel gameOver_,card_,background_;
    NativeTextureBank gameOverTextures_,cardTextures_,backgroundTextures_;
    unsigned backgroundFrame_=0,displayedBackgroundFrame_=0;
};
}
