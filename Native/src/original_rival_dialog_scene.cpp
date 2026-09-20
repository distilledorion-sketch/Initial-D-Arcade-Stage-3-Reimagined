#include "original_rival_dialog_scene.h"
#include "original_matrix.h"
#include "unity_ui_capture.h"
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace idas3::original {
namespace {
namespace fs=std::filesystem;
fs::path dialogRoot(const fs::path& root){return root/"data/original_assets/rival_dialog";}
// Every alphabet and namekana glyph is a single square sprite of its own cell.
constexpr float alphabetCell=32.f,namekanaCell=48.f;
}

bool OriginalRivalDialogScene::available(const fs::path& root){
    const auto base=dialogRoot(root);
    return fs::exists(base/"dialog.idasdialog")&&fs::exists(base/"banks/rival_scene/rival_scene.idasmesh")
        &&fs::exists(base/"fonts/alphabet/textures/textures.idastex")
        &&fs::exists(base/"banks/continue/continue.idasmesh")
        &&fs::exists(base/"banks/v3sK17continue/v3sK17continue.idasmesh");
}

void OriginalRivalDialogScene::load(const fs::path& root){
    root_=root;
    data_=OriginalRivalDialogData::load(root);
    const auto base=dialogRoot(root);
    scene_={NativeModel::load(base/"banks/rival_scene/rival_scene.idasmesh"),
        NativeTextureBank::load(base/"banks/rival_scene/textures/textures.idastex")};
    choice_={NativeModel::load(base/"banks/continue/continue.idasmesh"),
        NativeTextureBank::load(base/"banks/continue/textures/textures.idastex")};
    prompt_={NativeModel::load(base/"banks/v3sK17continue/v3sK17continue.idasmesh"),
        NativeTextureBank::load(base/"banks/v3sK17continue/textures/textures.idastex")};
    alphabet_=NativeTextureBank::load(base/"fonts/alphabet/textures/textures.idastex");
    namekana_=NativeTextureBank::load(base/"fonts/namekana/textures/textures.idastex");
    loaded_=true;
}

void OriginalRivalDialogScene::begin(const OriginalBattleProfile& profile,
    const OriginalRivalDialogSetup& setup){
    if(!loaded_)throw std::runtime_error("Original rival dialogue artwork is not loaded");
    const auto base=dialogRoot(root_);
    const auto portrait=setup.buntaChallenge?std::string("rival_bunta_challenge"):
        originalRivalDialogPortraitBank(data_,setup.enemy);
    const auto background=setup.buntaChallenge?
        std::string("rival_bg0")+std::to_string(setup.buntaCourse)+(setup.buntaCourse==8?"":"nf"):
        originalRivalDialogBackgroundBank(data_,setup.enemy,setup.weather);
    if(portrait!=portraitName_){
        for(unsigned i=0;i<portrait_.textures.size();++i)unityUiForgetTexture(portrait_.textures.at(i));
        portrait_={NativeModel::load(base/"banks"/portrait/(portrait+".idasmesh")),
            NativeTextureBank::load(base/"banks"/portrait/"textures/textures.idastex")};
        portraitName_=portrait;
    }
    if(background!=backgroundName_){
        for(unsigned i=0;i<background_.textures.size();++i)unityUiForgetTexture(background_.textures.at(i));
        background_={NativeModel::load(base/"banks"/background/(background+".idasmesh")),
            NativeTextureBank::load(base/"banks"/background/"textures/textures.idastex")};
        backgroundName_=background;
    }
    resetOriginalRivalDialog(state_,data_,profile,setup);
}

void OriginalRivalDialogScene::paint(std::span<std::uint32_t> target,int width,int height)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)
        throw std::invalid_argument("Invalid original rival dialogue destination");
    if(!loaded_||!state_.initialized)return;
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const float left=(float(width)-640.f*fit)*.5f,top=(float(height)-480.f*fit)*.5f;
    // The source submits the background and window panel on the opaque list
    // and the portrait, border and glyphs on the translucent one, so the
    // composite order is background, portrait, window, then text.
    const auto all=originalRivalDialogDraws(state_);
    std::vector<const OriginalRivalDialogDraw*> ordered;
    ordered.reserve(all.size());
    for(const auto pass:{OriginalRivalDialogBank::Background,OriginalRivalDialogBank::Portrait,
        OriginalRivalDialogBank::Scene,OriginalRivalDialogBank::Alphabet,
        OriginalRivalDialogBank::Namekana})
        for(const auto& draw:all)if(draw.bank==pass)ordered.push_back(&draw);
    for(const auto* entry:ordered){
        const auto& draw=*entry;
        const bool glyph=draw.bank==OriginalRivalDialogBank::Alphabet
            ||draw.bank==OriginalRivalDialogBank::Namekana;
        if(glyph){
            // The source glyph part is one square cell; its position is the
            // top-left corner in original pixels.
            const auto& bank=draw.bank==OriginalRivalDialogBank::Alphabet?alphabet_:namekana_;
            if(draw.chunk>=bank.size())continue;
            const float cell=draw.bank==OriginalRivalDialogBank::Alphabet?alphabetCell:namekanaCell;
            const float px=320.f+draw.x*100.f,py=240.f-draw.y*100.f;
            SpritePlacement placement;
            placement.scale=fit;placement.offsetX=left;placement.offsetY=top;
            OriginalSprite sprite;
            sprite.vertices={OriginalSpriteVertex{px,py,0,0,1,draw.color,0},
                {px,py+cell,0,0,0,draw.color,0},
                {px+cell,py,0,1,1,draw.color,0},
                {px+cell,py+cell,0,1,0,draw.color,0}};
            compositeOriginalSprite(target,width,height,bank.at(draw.chunk),sprite,placement);
            continue;
        }
        const Bank* bank=nullptr;
        switch(draw.bank){
        case OriginalRivalDialogBank::Portrait:bank=&portrait_;break;
        case OriginalRivalDialogBank::Background:bank=&background_;break;
        default:bank=&scene_;break;
        }
        if(!bank||draw.chunk>=bank->model.chunks.size())continue;
        // The draw's scale is the source depth compensation that the hardware
        // projection divides straight back out, so a layer vertex lands at
        // 320 + 100*(x + translation) and 240 - 100*(y + translation).
        SpritePlacement placement;
        placement.scale=100.f*fit;placement.invertY=true;placement.authoredHeight=0;
        placement.offsetX=left+fit*(320.f+100.f*draw.x);
        placement.offsetY=top+fit*(240.f-100.f*draw.y);
        compositeOriginalMenuChunk(target,width,height,bank->textures,
            bank->model.chunks[draw.chunk],placement);
    }
}
void OriginalRivalDialogScene::paintChoice(std::span<std::uint32_t> target,int width,int height,
    OriginalLegendChoiceKind kind,std::uint32_t selected,std::uint32_t timerTicks)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)
        throw std::invalid_argument("Invalid original legend choice destination");
    if(!loaded_)return;
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const float left=(float(width)-640.f*fit)*.5f,top=(float(height)-480.f*fit)*.5f;
    // The hardware sorts these layers by depth, so the dimmer sits behind the
    // prompt and the countdown digits sit in front. Each layer keeps its own
    // imported material, including the dimmer's half-transparent black.
    auto layers=originalLegendChoiceDraws(kind,selected,timerTicks);
    std::stable_sort(layers.begin(),layers.end(),
        [](const OriginalLegendChoiceDraw& a,const OriginalLegendChoiceDraw& b){return a.z<b.z;});
    for(const auto& draw:layers){
        const Bank& bank=draw.bank==OriginalLegendChoiceBank::Continue?choice_:prompt_;
        if(draw.chunk>=bank.model.chunks.size())continue;
        SpritePlacement placement;
        placement.scale=100.f*fit*draw.scale;placement.invertY=true;placement.authoredHeight=0;
        // These prompt layers are the original UI helper's VUR batches. Its
        // colour buffer holds white with no offset, so the imported artwork
        // shows its own shading; the material words left in the capture are
        // placeholders that would flatten the chrome to white.
        placement.defaultOriginalUiColors=true;
        placement.offsetX=left+fit*draw.x;
        placement.offsetY=top+fit*draw.y;
        compositeOriginalMenuChunk(target,width,height,bank.textures,
            bank.model.chunks[draw.chunk],placement);
    }
}
}
