#include "original_time_attack_visit.h"
#include "original_time_attack_background.h"
#include "original_time_attack_stats.h"
#include "original_ranking_data.h"
#include "original_gasstand_data.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace idas3::original {
namespace {
using Stage=OriginalTimeAttackVisit::Stage;
using Route=OriginalTimeAttackVisit::Route;
using Command=OriginalLegendReturnCommand;
constexpr std::array<const char*,8> mapNames{"myogi","usui","akagi","akina","happo","iroha","syomaru","tuchizaka"};
float f(unsigned word){return std::bit_cast<float>(word);}
bool valid(const OriginalTimeAttackVisit::Setup& s){return (s.condition<18||(s.condition<24&&!s.customCourseName.empty()&&!s.customMaps.empty()&&s.customMaps.size()<=5))&&s.weather<2&&s.car<35;}
void putWord(OriginalRankingRecord& r,unsigned offset,std::uint32_t word){
    for(unsigned i=0;i<4;++i)r.bytes[offset+i]=std::uint8_t(word>>(i*8));
}
struct Canvas {
    std::span<std::uint32_t> target;int width,height;float fit,left,top;
    Canvas(std::span<std::uint32_t> t,int w,int h):target(t),width(w),height(h),
        fit(std::min(float(w)/640.f,float(h)/480.f)),left((w-640.f*fit)*.5f),top((h-480.f*fit)*.5f){}
    void chunk(const NativeModel& model,const NativeTextureBank& textures,unsigned index,
        float x=0,float y=0,float scaleX=1,float scaleY=1)const{
        if(index>=model.chunks.size())return;
        SpritePlacement placement;placement.scale=100.f*fit;placement.invertY=true;
        placement.authoredHeight=0;placement.offsetX=left+x*fit;placement.offsetY=top+y*fit;
        if(scaleX==1&&scaleY==1){compositeOriginalMenuChunk(target,width,height,textures,model.chunks[index],placement);return;}
        auto item=model.chunks[index];for(auto& batch:item.batches)for(auto& v:batch.vertices){v.position.x*=scaleX;v.position.y*=scaleY;}
        compositeOriginalMenuChunk(target,width,height,textures,item,placement);
    }
    void text(const MenuFont& font,const char* value,float x,float y,float size,std::uint32_t color=0xffffffff)const{
        font.paint(target,width,height,value,left+x*fit,top+y*fit,size*fit,color);
    }
    void alphabet(const NativeTextureBank& bank,std::string_view text,float x,float y)const{
        //1CB200(mode1,color4,full strlen): exact62-glyph alphabet/width table,
        // also recovered by the original gas-station conversation renderer.
        SpritePlacement placement;placement.scale=fit;placement.offsetX=left+x*fit;placement.offsetY=top+y*fit;
        float advance=0;
        for(char value:text){const auto found=std::find(gasstand_data::fontCharacters.begin(),gasstand_data::fontCharacters.end(),value);
            if(found==gasstand_data::fontCharacters.end()){advance+=5;continue;}
            const auto i=std::size_t(found-gasstand_data::fontCharacters.begin());
            constexpr std::uint32_t ink=0xff010101;OriginalSprite sprite;
            sprite.vertices={OriginalSpriteVertex{advance,0,0,0,1,ink,0},{advance,32,0,0,0,ink,0},
                {advance+32,0,0,1,1,ink,0},{advance+32,32,0,1,0,ink,0}};
            compositeOriginalSprite(target,width,height,bank.at(unsigned(i)),sprite,placement);
            advance=std::fma(gasstand_data::word(gasstand_data::fontWidthWords[i]),1.f,advance);
        }
    }
    void rectangle(float x,float y,float w,float h,std::uint32_t color)const{
        x=left+x*fit;y=top+y*fit;w*=fit;h*=fit;
        if(unityUiEnabled()){unityUiSolid(target.data(),width,height,x,y,w,h,color);return;}
        for(int yy=std::max(0,int(y));yy<std::min(height,int(y+h));++yy)
            for(int xx=std::max(0,int(x));xx<std::min(width,int(x+w));++xx)target[std::size_t(yy)*width+xx]=color;
    }
    void line(float x,float y,float xx,float yy,std::uint32_t color,float thickness=1)const{
        x=left+x*fit;y=top+y*fit;xx=left+xx*fit;yy=top+yy*fit;
        if(unityUiEnabled()){unityUiLine(target.data(),width,height,x,y,xx,yy,thickness*fit,color);return;}
        const int steps=std::max(1,int(std::ceil(std::max(std::abs(xx-x),std::abs(yy-y)))));
        const int radius=std::max(0,int(thickness*fit*.5f));
        for(int i=0;i<=steps;++i){const int px=int(std::lround(x+(xx-x)*i/steps)),py=int(std::lround(y+(yy-y)*i/steps));
            for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx)
                if(px+dx>=0&&px+dx<width&&py+dy>=0&&py+dy<height)target[std::size_t(py+dy)*width+px+dx]=color;
        }
    }
};
}

std::string originalTimeAttackStartIntervalText(std::uint32_t ticks6000){
    if(!ticks6000)return "--";
    const auto milliseconds=ticks6000/6u;char value[32];
    std::snprintf(value,sizeof(value),"%u.%03u",milliseconds/1000u,milliseconds%1000u);return value;
}
std::vector<OriginalTimeAttackCountdownDraw> originalTimeAttackCountdownDraws(std::uint32_t ticks){
    //18B192 constructs110A60(mode1).111000 selects the one/two-digit
    // widget at800ticks;1926A0 returns80, and110B46 supplies its authored
    // 5.68/5.20,-.16 anchors. This original timer is not a60Hz clock.
    const unsigned seconds=std::min(ticks/80u,99u);
    std::vector<OriginalTimeAttackCountdownDraw> draws{{seconds%10u+1u,f(0x40b5c28f)*100.f,-f(0xbe23d70a)*100.f}};
    if(seconds>=10)draws.push_back({seconds/10u+1u,f(0x40a66666)*100.f,-f(0xbe23d70a)*100.f});
    return draws;
}
void OriginalTimeAttackDrivingTrace::recordFrame(float speed,float distance,float throttle,float brake,bool wall,std::uint32_t ticks){
    // Thirty minutes at 60 Hz is also the local record validity ceiling. Do
    // not permit bad host samples to turn a graph into unbounded geometry.
    if(samples_.size()>=108000||!std::isfinite(speed)||!std::isfinite(distance)||
        !std::isfinite(throttle)||!std::isfinite(brake))return;
    if(!samples_.empty()&&ticks<samples_.back().ticks6000)return;
    samples_.push_back({std::clamp(speed,0.f,600.f),distance,std::clamp(throttle,0.f,1.f),
        std::clamp(brake,0.f,1.f),ticks,wall});
}

bool OriginalTimeAttackVisit::available(const std::filesystem::path& root){
    const auto base=root/"data/original_assets/time_attack";
    return std::filesystem::exists(base/"lecture/lecture.idasmesh")&&
        std::filesystem::exists(base/"lecture/textures/textures.idastex")&&
        std::filesystem::exists(root/"data/original_assets/attract/ranking/v3sT13rankin/v3sT13rankin.idasmesh")&&
        OriginalRivalDialogScene::available(root);
}
void OriginalTimeAttackVisit::load(const std::filesystem::path& root){
    loaded_=false;
    auto loadBank=[](const std::filesystem::path& base,const std::string& name){return Bank{
        NativeModel::load(base/name/(name+".idasmesh")),
        NativeTextureBank::load(base/name/"textures/textures.idastex")};};
    const auto base=root/"data/original_assets/time_attack";
    lecture_=loadBank(base,"lecture");
    for(unsigned i=0;i<maps_.size();++i)maps_[i]=loadBank(base,std::string("lecmap_")+mapNames[i]);
    const auto rank=root/"data/original_assets/attract/ranking";
    ranking_=loadBank(rank,"v3sT13rankin");common_=loadBank(rank,"v3sT00common");
    select_=loadBank(root/"data/original_assets/name_entry","select");
    alphabet_=NativeTextureBank::load(root/"data/original_assets/attract/alphabet/textures/textures.idastex");
    choices_.load(root);font_=MenuFont::load(root);loaded_=true;
}
void OriginalTimeAttackVisit::finish(Route route){route_=route;stage_=Stage::Finished;}
void OriginalTimeAttackVisit::beginLecture(const Setup& s){
    if(!valid(s))throw std::invalid_argument("Invalid Time Attack visit selector");
    setup_=s;events_.clear();rows_.clear();route_=Route::None;stage_=Stage::Lecture;
    for(unsigned map=0;map<4;++map)drivingLines_[map]=s.telemetry.valid&&s.customMaps.empty()?originalTimeAttackDrivingLines(s.condition/2,map,s.telemetry):std::vector<OriginalTimeAttackDrivingLine>{};
    frame_=0;phase_=0;fade_=15;selected_=0;mapIndex_=0;
    backgroundFrame_=displayedBackgroundFrame_=0;
    timerTicks_=s.sourceAnalysisAvailable?s.analysis.countdownTicks:1279;
    if(s.suppressLecture){finish(Route::Points);return;}
    events_.push_back({Command::SoundSet,1,0});events_.push_back({Command::MusicRequest,2,1});
}
void OriginalTimeAttackVisit::prepareRanking(){
    rows_.clear();rowMetadata_.clear();std::vector<TimeAttackEntry> local;
    for(const auto& e:setup_.localRecords)if(e.condition==setup_.condition&&e.weather==setup_.weather&&
        e.car<35&&e.ticks6000>0&&e.ticks6000<10800000)local.push_back(e);
    std::stable_sort(local.begin(),local.end(),[](const auto& a,const auto& b){return a.ticks6000<b.ticks6000;});
    if(local.size()>10)local.resize(10);
    for(const auto& e:local){
        OriginalRankingRecord r;putWord(r,0,e.ticks6000);
        for(unsigned i=0;i<5;++i)r.bytes[4+i]=std::min<std::uint8_t>(e.nameGlyphs[i],221);
        // Source record8 contains name's fifth byte below packed car bits26+.
        putWord(r,8,r.word(8)|(e.car<<26));putWord(r,12,(e.manual?1u:0u)|(e.night||setup_.condition/2==8?2u:0u));
        rows_.push_back(r);
        rowMetadata_.push_back(!e.metadataUnknown&&(e.manual||e.night||std::any_of(e.nameGlyphs.begin(),e.nameGlyphs.end(),[](auto c){return c!=221;})));
    }
}
void OriginalTimeAttackVisit::beginAfterResults(const Setup& s){
    if(!valid(s))throw std::invalid_argument("Invalid Time Attack visit selector");
    setup_=s;events_.clear();route_=Route::None;frame_=0;phase_=0;fade_=0;selected_=0;
    prepareRanking();
    // 07AD20 CHECK accepts qualifying course/model/personal flags only for
    // signed result statuses<=1. Host local persistence replaces the card
    // writer and existing named profiles replace a second name-entry visit.
    // ARankinTA Init07EE74..07EE8C independently requires course rank<10.
    // Beating only a personal/model best does not qualify this child.
    if(std::int32_t(s.resultStatus)<=1&&s.courseRankingQualified&&!rows_.empty()){
        stage_=Stage::Ranking;timerTicks_=900;rankingFadeAlpha_=255;rankingSettle_=0;
    }else beginContinue();
}
void OriginalTimeAttackVisit::beginContinue(){
    frame_=0;phase_=0;fade_=0;selected_=0;timerTicks_=879;
    // Parent07ADC0/07ACE0 tests cabinet continuation bit5. Native builds have
    // no physical card to eject; Exit is explicitly routed to the title.
    if(!setup_.continuationEnabled){finish(Route::Exit);return;}
    stage_=Stage::Continue;
}
void OriginalTimeAttackVisit::advance(const Input& input){
    if(!active())return;
    ++frame_;
    if(stage_==Stage::Lecture){
        displayedBackgroundFrame_=backgroundFrame_;
        backgroundFrame_=advanceOriginalTimeAttackBackground(backgroundFrame_);
        // HLecture18D940..18D9F2; countdown1341A0 saturates. The source final
        // settle phase increments its own counter twice per owner update.
        if(timerTicks_)--timerTicks_;
        switch(phase_){
        case 0:if(--fade_<0){fade_=0;phase_=1;}break;
        case 1:
            if(input.nextMap&&std::int32_t(setup_.resultStatus)<=1){
                const auto course=setup_.condition/2;
                const auto parts=setup_.customMaps.empty()?maps_[course==8?3:course].model.chunks.size():setup_.customMaps.size();
                if(parts)mapIndex_=unsigned((mapIndex_+1)%parts);
            }
            if(input.confirm||input.skip||timerTicks_==0){phase_=2;}break;
        case 2:if(++fade_>15){fade_=15;phase_=3;selected_=0;}break;
        case 3:if(selected_++>3)phase_=4;++selected_;break;
        case 4:finish(Route::Points);break;
        }
        return;
    }
    if(stage_==Stage::Ranking){
        if(timerTicks_)--timerTicks_;
        //07F340 calls04BE00 then04BF00 before testing input. Source starts
        // fade phase1 at counter0, accepts confirm in phase2, and waits for
        // four settled black frames in phase4 after the30-tick exit fade.
        if(phase_==0){rankingFadeAlpha_=unsigned((30-fade_)*255/30);if(++fade_==30)phase_=1;}
        else if(phase_==1)rankingFadeAlpha_=0;
        else if(phase_==2){rankingFadeAlpha_=unsigned((30-fade_)*255/30);if(--fade_==0){phase_=3;rankingSettle_=0;}}
        else if(phase_==3)rankingFadeAlpha_=255;
        if(phase_==3&&++rankingSettle_>3){beginContinue();return;}
        if((phase_==1&&(input.confirm||input.skip))||timerTicks_==0){if(phase_<2)phase_=2;}
        return;
    }
    if(stage_==Stage::Continue){
        if(phase_==0){
            if(timerTicks_)--timerTicks_;
            const auto previous=selected_;
            if(input.previous)selected_=0;if(input.next)selected_=1;
            if(previous!=selected_)events_.push_back({Command::Cue,2,1});
            if(input.confirm||input.skip||!timerTicks_){
                if(!timerTicks_)selected_=1;phase_=1;frame_=0;
                events_.push_back({Command::Cue,3,1});
            }
        }else if(frame_>40){ // 081320..081336 confirmation dwell
            if(selected_==0&&timerTicks_>0&&(setup_.freePlay||setup_.canContinue)){
                events_.push_back({Command::ContinueAccepted,0,0});
                events_.push_back({Command::SoundSet,1,0});finish(Route::Retry);
            }else finish(Route::Exit);
        }
    }
}
std::uint32_t OriginalTimeAttackVisit::fadeArgb()const{
    return stage_==Stage::Lecture?std::uint32_t(std::clamp(fade_,0,15)*255/15)<<24:
        stage_==Stage::Ranking?rankingFadeAlpha_<<24:0;
}
void OriginalTimeAttackVisit::paintLecture(std::span<std::uint32_t> target,int width,int height)const{
    const Canvas c(target,width,height);
    for(const auto& draw:originalTimeAttackBackgroundDraws(displayedBackgroundFrame_))
        c.chunk(select_.model,select_.textures,draw.chunk,draw.position.x*100.f,-draw.position.y*100.f);
    // 18DA60: imported parts preserve authored coordinates. The source makes
    // only the listed Y=-.24 translation and1.1 X scale on the speech panel.
    c.chunk(lecture_.model,lecture_.textures,37,0,24);
    c.chunk(lecture_.model,lecture_.textures,21,0,24);
    const auto course=setup_.condition/2;
    if(setup_.customMaps.empty()){
        const auto& map=maps_[course==8?3:course];c.chunk(map.model,map.textures,mapIndex_,0,24);
    }else{
        c.rectangle(195,195,240,240,0xff202c39);
        const auto& page=setup_.customMaps.at(mapIndex_);
        for(const auto& line:page.road)c.line(line.from[0],line.from[1],line.to[0],line.to[1],line.color,3.f);
        for(const auto& line:page.driving)c.line(line.from[0],line.from[1],line.to[0],line.to[1],line.color,1.25f);
        for(const auto& p:page.walls)c.chunk(lecture_.model,lecture_.textures,15,p[0],p[1],.65f,.65f);
        for(const auto& p:page.ditches)c.chunk(lecture_.model,lecture_.textures,27,p[0],p[1],.65f,.65f);
    }
    // Artwork banks are not telemetry arrays: Shomaru and Tsuchisaka each
    // have five pages, while the recovered driving/marker cache has four.
    // Keep every artwork page reachable, but only draw available telemetry.
    if(setup_.customMaps.empty()&&setup_.telemetry.valid&&mapIndex_<drivingLines_.size()){
        for(const auto& line:drivingLines_.at(mapIndex_))
            c.line(line.from[0],line.from[1],line.to[0],line.to[1],line.color,1.25f);
        const auto marker=[&](const OriginalTimeAttackMapMarker& point,unsigned chunk,bool section){
            const float x=section?315.f+point.position[0]*256.f:point.position[0];
            const float y=24.f+(section?291.f+point.position[1]*256.f:point.position[1]);
            if(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(point.scale)&&point.scale>0)
                c.chunk(lecture_.model,lecture_.textures,chunk,x,y,point.scale,point.scale);
        };
        if(mapIndex_==0)for(const auto& point:setup_.telemetry.fullMapWallMarkers)marker(point,15,false);
        else if(mapIndex_<=3){
            for(const auto& point:setup_.telemetry.sectionWallMarkers[mapIndex_-1])marker(point,14,true);
            for(const auto& point:setup_.telemetry.sectionDitchMarkers[mapIndex_-1])marker(point,27,true);
        }
    }
    c.chunk(lecture_.model,lecture_.textures,22,0,24);
    c.chunk(lecture_.model,lecture_.textures,40,-20,24,1.1f);
    c.chunk(lecture_.model,lecture_.textures,2);
    c.chunk(lecture_.model,lecture_.textures,std::int32_t(setup_.resultStatus)>1?0:1);
    //18E160 uses the authored map instruction part, with the common-.24Y.
    const auto kind=setup_.sourceAnalysisAvailable?setup_.analysis.kind:(std::int32_t(setup_.resultStatus)>1?0u:2u);
    c.chunk(lecture_.model,lecture_.textures,kind==0?(course==0?33u:35u):36u,0,24);
    for(const auto& draw:originalTimeAttackCountdownDraws(timerTicks_))
        c.chunk(select_.model,select_.textures,draw.chunk,draw.x,draw.y);
    //18E0A0 passes172,70/93/116;1CB200 adds font mode1's24-pixel Y offset.
    // The original letters/widths fit the original panel, without host fonts
    // or an invented performance graph occupying the advice window.
    if(setup_.sourceAnalysisAvailable)for(unsigned i=0;i<3;++i)
        c.alphabet(alphabet_,setup_.analysis.lines[i],172.f,94.f+23.f*i);
    auto timing=setup_.analysisInput;timing.course=course;
    for(const auto& draw:originalTimeAttackStatDraws(setup_.telemetry,timing))
        c.chunk(lecture_.model,lecture_.textures,draw.chunk,draw.x,draw.y);
}
void OriginalTimeAttackVisit::paintRanking(std::span<std::uint32_t> target,int width,int height)const{
    using namespace ranking_data;const Canvas c(target,width,height);const auto course=setup_.condition/2,direction=setup_.condition&1;
    std::vector<OriginalRankingBoardDraw> draws;
    auto fixed=[&](unsigned chunk,Vec3 pos={}){OriginalRankingBoardDraw d;d.chunk=chunk;d.position=pos;draws.push_back(d);};
    fixed(48);if(setup_.customCourseName.empty())fixed(courseChunks[course]);fixed(routeChunks[(setup_.customCourseName.empty()?course:3)*2+direction]);if(course!=8)fixed(setup_.weather?43:42);fixed(47);fixed(3);
    // ARankinTA supplies a nonnegative qualified row, suppressing attract's
    // gear/model hint50. That control belongs only to attract child12.
    float y=f(lit_0C1BDBC8);
    const auto current=std::find_if(rows_.begin(),rows_.end(),[&](const auto& row){
        return row.word(0)==setup_.ticks6000&&row.car()==setup_.car&&
            std::equal(setup_.nameGlyphs.begin(),setup_.nameGlyphs.end(),row.bytes.begin()+4);
    });
    const int highlighted=current==rows_.end()?-1:int(current-rows_.begin());
    for(unsigned i=0;i<rows_.size();++i){if((i&1)&&int(i)!=highlighted)fixed(0,{0,y+f(lit_0C1BDBCC),f(lit_0C1BDBD4)});
        const unsigned age=stage_==Stage::Continue?std::max(frame_,90u):frame_;
        const bool activeRow=int(i)==highlighted;
        if(activeRow&&int(age)>int(i*8))fixed(44,{0,y+f(lit_0C1BDBCC),f(lit_0C1BDBD8)});
        //07F378 /1BDB64: hide only the newly inserted row during the first
        //15 ticks of each60-tick flash, beginning after its18+rank*8 reveal.
        if(activeRow&&age>=18+i*8&&(age-(18+i*8))%60<=14){y-=f(lit_0C1BDBE0);continue;}
        auto row=originalRankingRowDraws(rows_[i],y,int(age)-int(i*8));
        // V1 local records did not store transmission/night. Leave those
        // cells empty rather than claiming every old run was AT/day.
        if(!rowMetadata_[i]&&row.size()>=2)row.resize(row.size()-2);
        draws.insert(draws.end(),row.begin(),row.end());y-=f(lit_0C1BDBE0);}
    for(const auto& draw:draws){const auto& bank=draw.bank==OriginalRankingBoardDraw::Bank::ranking?ranking_:common_;
        auto chunk=bank.model.chunks.at(draw.chunk);unsigned vertex=0;
        for(auto& batch:chunk.batches){if(draw.overrideMaterial)batch.material[3]=batch.material[5]=draw.color;
            for(auto& v:batch.vertices){v.position+=draw.position;if(draw.overrideUv){if(vertex>=4)throw std::runtime_error("Time Attack ranking glyph bounds");v.u=draw.u[vertex];v.v=draw.v[vertex];++vertex;}}}
        SpritePlacement p;p.scale=100*c.fit;p.invertY=true;p.authoredHeight=0;p.offsetX=c.left;p.offsetY=c.top;
        compositeOriginalMenuChunk(target,width,height,bank.textures,chunk,p);
    }
    if(!setup_.customCourseName.empty())c.text(font_,setup_.customCourseName.c_str(),28,13,28.f);
}
void OriginalTimeAttackVisit::paint(std::span<std::uint32_t> target,int width,int height)const{
    if(width<=0||height<=0||target.size()<std::size_t(width)*height)throw std::invalid_argument("Invalid Time Attack visit destination");
    if(!loaded_||!active())return;
    if(stage_==Stage::Lecture)paintLecture(target,width,height);
    else if(stage_==Stage::Ranking)paintRanking(target,width,height);
    else if(stage_==Stage::Continue){
        // AContinue owns a fresh077C80(selcrs2) car scene. The race owner
        // renders it behind this choice; prior lecture/ranking panels close.
        choices_.paintChoice(target,width,height,OriginalLegendChoiceKind::Continue,selected_,timerTicks_);
    }
}
}
