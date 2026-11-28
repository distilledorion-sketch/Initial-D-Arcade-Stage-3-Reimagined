#include "original_rival_dialog_scene.h"
#include "original_matrix.h"
#include "unity_ui_capture.h"
#include <vector>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
namespace fs=std::filesystem;
fs::path dialogRoot(const fs::path& root){return root/"data/original_assets/rival_dialog";}
// Every alphabet and namekana glyph is a single square sprite of its own cell.
constexpr float alphabetCell=32.f,namekanaCell=48.f;

unsigned speechStyle(const OriginalRivalDialogState& state,unsigned speaker){
    // Rev C 0C0F6012 overrides the record's packed styles for guest speakers.
    if(speaker==3){
        constexpr std::array<unsigned,13> blue{4,5,8,9,14,15,18,19,21,22,23,26,28};
        if(std::find(blue.begin(),blue.end(),state.character)!=blue.end())return 1;
    }
    if(state.character==28){
        if(speaker==4||speaker==7)return 2;
        if(speaker==5)return 0;
    }
    if(state.character==29){
        if(speaker==3||speaker==5)return 0;
        if(speaker==4||speaker==6)return 1;
        if(speaker==7)return 2;
    }
    return speaker<4?(state.speechStyles>>(4*speaker))&3:0;
}

void paintSpeechPanel(std::span<std::uint32_t> target,int width,int height,
    const NativeModel& model,const NativeTextureBank& textures,const SpritePlacement& placement,
    const OriginalDialogLine& line,unsigned style){
    if(line.glyphs.empty())return;
    float minX=line.glyphs.front().x,maxX=minX,minY=line.glyphs.front().y,maxY=minY;
    for(const auto& g:line.glyphs){
        minX=std::min(minX,g.x);maxX=std::max(maxX,g.x);
        minY=std::min(minY,g.y);maxY=std::max(maxY,g.y);
    }
    // Rev C 0C0F6B6E measures the occupied 16px cells; 0C0F54C0 places
    // the four frame joints around them. The frame parts are 16px quads.
    const float left=minX-9,right=maxX+25,top=minY-9,bottom=maxY+25;
    const float innerWidth=right-left-16,innerHeight=bottom-top-16;
    constexpr unsigned frames[]{8,4,13,8};
    const unsigned frame=frames[style&3];
    const auto part=[&](unsigned id,float x,float y,float sx,float sy){
        const auto& chunk=model.chunks.at(id);
        std::array<Vec3,4> vertices;
        const auto& source=chunk.batches.at(0).vertices;
        if(chunk.batches.size()!=1||source.size()!=vertices.size())
            throw std::runtime_error("Invalid rival speech panel quad");
        for(unsigned i=0;i<vertices.size();++i){
            const auto& p=source[i].position;
            vertices[i]={x+100.f*sx*p.x,y-100.f*sy*p.y,p.z};
        }
        compositeOriginalMenuChunk(target,width,height,textures,chunk,placement,vertices);
    };
    part(12,right,top,(right-left)/16,(bottom-top)/16);
    part(frame,right,top,1,1);part(frame,left,top,-1,1);
    part(frame,right,bottom,1,-1);part(frame,left,bottom,-1,-1);
    part(frame+1,right,top+8,1,innerHeight/16);
    part(frame+1,left,top+8,-1,innerHeight/16);
    part(frame+3,right-8,top,innerWidth/16,1);
    part(frame+3,right-8,bottom,innerWidth/16,-1);
}
}

bool OriginalRivalDialogScene::available(const fs::path& root){
    const auto base=dialogRoot(root);
    return fs::exists(base/"dialog.idasdialog")&&fs::exists(base/"banks/rival_scene/rival_scene.idasmesh")
        &&(fs::exists(base/"fonts/alphabet/textures/textures.idastex")||
           fs::exists(base/"fonts/msg_itsuki/textures/textures.idastex"))
        &&fs::exists(base/"banks/continue/continue.idasmesh")
        &&fs::exists(base/"banks/v3sK17continue/v3sK17continue.idasmesh");
}

void OriginalRivalDialogScene::load(const fs::path& root){
    loaded_=false;
    portraitName_.clear();
    backgroundName_.clear();
    dialogueFontMap_.clear();
    root_=root;
    data_=OriginalRivalDialogData::load(root);
    const auto base=dialogRoot(root);
    scene_={NativeModel::load(base/"banks/rival_scene/rival_scene.idasmesh"),
        NativeTextureBank::load(base/"banks/rival_scene/textures/textures.idastex")};
    choice_={NativeModel::load(base/"banks/continue/continue.idasmesh"),
        NativeTextureBank::load(base/"banks/continue/textures/textures.idastex")};
    prompt_={NativeModel::load(base/"banks/v3sK17continue/v3sK17continue.idasmesh"),
        NativeTextureBank::load(base/"banks/v3sK17continue/textures/textures.idastex")};
    if(!data_.japanese())alphabet_=NativeTextureBank::load(base/"fonts/alphabet/textures/textures.idastex");
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
        if(data_.japanese()){
            const auto name="msg_"+portrait.substr(6);
            const auto font=base/"fonts"/name;
            std::ifstream input(font/(name+"_font.bin.nz"),std::ios::binary);
            std::vector<std::uint16_t> map(96*96);
            if(!input.read(reinterpret_cast<char*>(map.data()),std::streamsize(map.size()*2))||
               input.peek()!=std::ifstream::traits_type::eof())throw std::runtime_error("Invalid Japanese rival font map");
            auto textures=NativeTextureBank::load(font/"textures/textures.idastex");
            for(auto index:map)if(index!=0xffff&&index>=textures.size())throw std::runtime_error("Invalid Japanese rival glyph");
            for(unsigned i=0;i<alphabet_.size();++i)unityUiForgetTexture(alphabet_.at(i));
            alphabet_=std::move(textures);
            dialogueFontMap_=std::move(map);
        }
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

void OriginalRivalDialogScene::beginWithSharedRandom(const OriginalBattleProfile& profile,
    const OriginalRivalDialogSetup& setup,std::uint32_t& seed){
    auto selected=setup;
    //0F8780 virtual Init is0F8B00, whose cheer draw precedes both arrays.
    //0F88C0 calls0F9A00 instead and does not execute that cheer selection.
    if(!selected.buntaChallenge)selected.cheer=selectOriginalRivalSpectator(seed);
    begin(profile,selected);
    resetOriginalDialogParticles(state_.particles,seed);
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
    bool paintedSpeech=false;
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
            if(state_.text.vertical&&!paintedSpeech&&state_.phase==1){
                SpritePlacement panel;panel.scale=fit;panel.offsetX=left;panel.offsetY=top;
                for(unsigned i=0;i<state_.otherSpeech.size();++i){
                    if(!state_.slotFlags[i])continue;
                    const auto& line=i==state_.speaker?state_.text:state_.otherSpeech[i];
                    paintSpeechPanel(target,width,height,scene_.model,scene_.textures,panel,line,speechStyle(state_,i));
                }
                paintedSpeech=true;
            }
            // The source glyph part is one square cell; its position is the
            // top-left corner in original pixels.
            const auto& bank=draw.bank==OriginalRivalDialogBank::Alphabet?alphabet_:namekana_;
            auto texture=draw.chunk;
            if(draw.bank==OriginalRivalDialogBank::Alphabet&&data_.japanese()){
                if(texture>=dialogueFontMap_.size())continue;
                texture=dialogueFontMap_[texture];
            }
            if(texture>=bank.size())continue;
            const float cell=draw.bank==OriginalRivalDialogBank::Namekana?namekanaCell:
                state_.text.vertical?state_.text.cell:alphabetCell;
            const float px=320.f+draw.x*100.f,py=240.f-draw.y*100.f;
            SpritePlacement placement;
            placement.scale=fit;placement.offsetX=left;placement.offsetY=top;
            OriginalSprite sprite;
            sprite.vertices={OriginalSpriteVertex{px,py,0,0,1,draw.color,0},
                {px,py+cell,0,0,0,draw.color,0},
                {px+cell,py,0,1,1,draw.color,0},
                {px+cell,py+cell,0,1,0,draw.color,0}};
            compositeOriginalSprite(target,width,height,bank.at(texture),sprite,placement);
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
int OriginalRivalDialogScene::hitContinue(float x,float y,unsigned selected)const{
    if(!loaded_)return -1;
    for(const auto& draw:originalLegendChoiceDraws(OriginalLegendChoiceKind::Continue,selected,879)){
        if(draw.bank!=OriginalLegendChoiceBank::Prompt||draw.chunk>1||draw.chunk>=prompt_.model.chunks.size())continue;
        float left=1e9f,top=1e9f,right=-1e9f,bottom=-1e9f;
        for(const auto& batch:prompt_.model.chunks[draw.chunk].batches)for(const auto& v:batch.vertices){
            const float px=draw.x+v.position.x*100.f*draw.scale,py=draw.y-v.position.y*100.f*draw.scale;
            left=std::min(left,px);right=std::max(right,px);top=std::min(top,py);bottom=std::max(bottom,py);
        }
        if(x>=left&&x<=right&&y>=top&&y<=bottom)return draw.chunk==1?0:1;
    }
    return -1;
}
void OriginalRivalDialogScene::paintChoice(std::span<std::uint32_t> target,int width,int height,
    OriginalLegendChoiceKind kind,std::uint32_t selected,std::uint32_t timerTicks,bool confirming,std::uint32_t confirmFrame)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)
        throw std::invalid_argument("Invalid original legend choice destination");
    if(!loaded_)return;
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const float left=(float(width)-640.f*fit)*.5f,top=(float(height)-480.f*fit)*.5f;
    // The hardware sorts these layers by depth, so the dimmer sits behind the
    // prompt and the countdown digits sit in front. Each layer keeps its own
    // imported material, including the dimmer's half-transparent black.
    auto layers=originalLegendChoiceDraws(kind,selected,timerTicks,confirming,confirmFrame);
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
