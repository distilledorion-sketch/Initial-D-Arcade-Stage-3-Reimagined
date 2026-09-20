#include "original_name_entry.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace idas3::original {
namespace {
std::int32_t si(std::uint32_t v){return std::bit_cast<std::int32_t>(v);}
void selectorSet(OriginalNameEntryState&s,unsigned tile){
    s.selector.selected=tile;s.selector.phase=0;s.selector.velocity=s.selector.progress=0;
}
void selectorTick(OriginalNameEntryState&s,float steering){
    auto& c=s.selector;const auto previous=c.selected;
    if(c.phase==0){
        const auto magnitude=std::abs(steering);
        if(magnitude>0.2f){
            auto next=si(c.selected)+(steering>0?1:-1);bool move=true;
            if(steering>0&&next>si(c.maximum)){if(c.wrap)next=si(c.minimum);else move=false;}
            else if(steering<0&&next<si(c.minimum)){if(c.wrap)next=si(c.maximum);else move=false;}
            if(move){c.selected=std::uint32_t(next);c.velocity=-std::max((magnitude-0.2f)*0.2f,0.06f);c.progress=1;c.phase=1;}
        }
    }else if(c.phase==1){
        float acceleration=0.06f*0.06f;acceleration*=0.5f;acceleration*=0.9f;
        c.velocity+=acceleration;c.progress+=c.velocity;
        if(!(c.progress>0)||!(0>c.velocity)){c.velocity=c.progress=0;c.phase=0;}
    }
    const auto position=originalNameEntryTilePosition(c.selected);
    if(c.selected!=previous){const auto old=originalNameEntryTilePosition(previous);c.deltaX=old[0]-position[0];c.deltaY=old[1]-position[1];}
    s.cursorX452=std::fma(c.progress,c.deltaX,position[0]);s.cursorY456=std::fma(c.progress,c.deltaY,position[1]);s.selected460=c.selected;
}
void setFullRange(OriginalNameEntryState&s){s.selector.minimum=59;s.selector.maximum=52;s.selector.wrap=false;selectorSet(s,52);}
void restoreRange(OriginalNameEntryState&s){s.selector.minimum=0;s.selector.maximum=52;s.selector.wrap=true;}
void eraseLast(OriginalNameEntryState&s){if(!s.length528)return;if(s.length528>4)restoreRange(s);--s.length528;}
void append(OriginalNameEntryState&s,const OriginalNameEntryTables&t,unsigned offset){
    if(s.length528>4)return;unsigned tile=s.selected460;if(tile>999)tile-=2;tile+=offset;
    if(tile>=t.keyboard.size())throw std::out_of_range("Original name keyboard entry");
    s.keyboardIds484[s.length528]=tile;s.glyphIds480[s.length528]=t.keyboard[tile].words[0];++s.length528;
    if(s.length528>4)setFullRange(s);
}
void commit(OriginalNameEntryState&s,const OriginalNameEntryTables&t,OriginalNameEntryEvents&e){
    if(!s.length528){constexpr std::array<unsigned,4> defaults{180,166,168,162};for(unsigned n=0;n<4;++n)s.glyphIds480[n]=s.keyboardIds484[n]=defaults[n];s.length528=4;}
    while(s.length528&&s.glyphIds480[s.length528-1]==220)--s.length528;
    auto normalized=s.glyphIds480;for(auto& id:normalized)if(id>=81&&id<=161)id-=81;
    const auto encoded=t.encodedName(normalized,s.length528);
    for(const auto& [from,to]:t.substitutions)if(encoded==from){
        s.profileFlags1180|=0x10000;s.length528=unsigned(to.size()/2);
        if(s.length528>5)throw std::runtime_error("Original name replacement exceeds five glyphs");
        for(unsigned i=0;i<s.length528;++i){const auto first=std::uint8_t(to[2*i]),second=std::uint8_t(to[2*i+1]);
            auto glyph=std::find_if(t.glyphs.begin(),t.glyphs.end(),[&](const auto&g){return g.bytes[0]==first&&g.bytes[1]==second;});
            if(glyph==t.glyphs.end())throw std::runtime_error("Original replacement glyph missing");s.glyphIds480[i]=unsigned(glyph-t.glyphs.begin());}
        e.substituted=true;break;
    }
    s.blinkPhase472=1;++s.phase588;e.nameCommitted=true;
}
void inputTick(OriginalNameEntryState&s,const OriginalNameEntryInput&i,const OriginalNameEntryTables&t,OriginalNameEntryEvents&e){
    const auto old=s.selected460;selectorTick(s,i.steering);if(old!=s.selected460)e.cueIds.push_back(5);
    if(s.length528>4){s.selector.minimum=s.selected460<=19?19:s.selected460<=35?35:s.page600==2?51:59;s.selector.maximum=s.selected460<=19?19:s.selected460<=35?35:52;}
    const unsigned pageOffset=s.page600==1?61:s.page600==2?122:0;
    if(s.length528<=4||s.selected460<51||s.selected460>52){
        int next=int(s.selected460);
        if(i.rowJump<0){if(next<=19)next=std::min(next+36,52);else if(next<=35)next-=20;else{next-=16;if(next>35)next=16;}}
        else if(i.rowJump>0){if(next<=19){next+=20;if(next>35)next=52;}else if(next<=35)next+=16;else next-=36;}
        if(i.rowJump)selectorSet(s,unsigned(next));
    }
    if(!s.sharedCountdown1176){s.timedOut604=1;setFullRange(s);selectorTick(s,i.steering);s.selected460=52;}
    if(i.backPressed&&s.length528){eraseLast(s);return;}
    if(!i.confirmPressed&&!s.timedOut604)return;
    if(s.timedOut604||s.selected460==52){selectorSet(s,52);selectorTick(s,i.steering);s.selected460=52;e.cueIds.push_back(3);}
    else e.cueIds.push_back(2);
    switch(s.selected460){
    case 998:s.page600=s.page600==1?0:1;break;
    case 999:s.page600=s.page600==2?0:2;break;
    case 51:eraseLast(s);break;
    case 52:commit(s,t,e);break;
    case 50:
        if(s.length528&&s.length528<=4){s.keyboardIds484[s.length528]=50;s.glyphIds480[s.length528]=220;++s.length528;
            if(s.length528>4)setFullRange(s);}
        else if(s.length528>4){restoreRange(s);--s.length528;}
        break;
    case 59:case 60:
        if(s.page600==2){append(s,t,pageOffset);break;}
        if(s.length528){const auto& row=t.keyboard.at(s.keyboardIds484[s.length528-1]).words;const auto modifier=s.selected460==59?1:2;
            if(row[modifier+2])s.glyphIds480[s.length528-1]=row[0]+row[modifier];}
        break;
    default:append(s,t,pageOffset);break;
    }
}
}
OriginalNameEntryTables OriginalNameEntryTables::load(const std::filesystem::path&root){
    std::ifstream in(root/"data/original_frontend/name_entry.bin",std::ios::binary);if(!in)throw std::runtime_error("Original name tables missing");
    const std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};std::size_t at=0;
    const auto byte=[&](){if(at>=b.size())throw std::runtime_error("Truncated original name tables");return b[at++];};
    const auto word=[&](){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(byte())<<(i*8);return v;};
    if(word()!=0x454e4449||word()!=1||word()!=221||word()!=173)throw std::runtime_error("Unknown original name tables");const auto count=word();if(count!=293)throw std::runtime_error("Original name substitution count");
    OriginalNameEntryTables t;for(auto&g:t.glyphs)for(auto&v:g.bytes)v=byte();for(auto&r:t.keyboard)for(auto&v:r.words)v=word();
    for(unsigned i=0;i<count;++i){const auto a=word(),z=word();if(a>30||z>10||a%2||z%2)throw std::runtime_error("Original name string length");std::string from,to;for(unsigned j=0;j<a;++j)from+=char(byte());for(unsigned j=0;j<z;++j)to+=char(byte());t.substitutions.emplace_back(std::move(from),std::move(to));}
    if(at!=b.size())throw std::runtime_error("Original name table tail");return t;
}
std::string OriginalNameEntryTables::encodedName(const std::array<std::uint32_t,5>&ids,unsigned length)const{
    if(length>5)throw std::out_of_range("Original name length");std::string out;for(unsigned i=0;i<length;++i){const auto&g=glyphs.at(ids[i]);out+=char(g.bytes[0]);out+=char(g.bytes[1]);}return out;
}
std::array<float,2> originalNameEntryTilePosition(unsigned tile){
    constexpr std::array<std::uint32_t,20> xs{0x3e570a3d,0x3eeb851e,0x3f35c28f,0x3f75c28f,0x3f9ae148,0x3fe00000,0x40000000,0x40100000,0x40200000,0x40300000,0x40528f5c,0x40628f5c,0x40728f5c,0x408147ae,0x408947ae,0x409a8f5c,0x40a28f5c,0x40aa8f5c,0x40b28f5c,0x40ba8f5c};
    if(tile>52)throw std::out_of_range("Original name selector tile");const auto column=tile<20?tile:tile<36?tile-20:tile-36;
    return {std::bit_cast<float>(xs[column]),std::bit_cast<float>(tile<20?0xc05851ecu:tile<36?0xc06b851fu:0xc07eb852u)};
}
void initializeOriginalNameEntry(OriginalNameEntryState&s,const std::array<std::uint32_t,5>&existing,unsigned length){
    if(s.profileKind1192==2&&(length>5||std::any_of(existing.begin(),existing.begin()+length,[](auto id){return id>220;})))
        throw std::invalid_argument("Invalid original saved name");
    s.frame572=s.phase588=s.hold592=s.blinkPhase472=s.blinkFrame476=0;s.fade576=15;s.fadeEnabled584=0;s.blinkArgb468=0xffffffff;
    s.activeOverlay596=1;s.timedOut604=0;s.page600=2;s.sharedCountdown1176=4879;s.parentEvent64=0;s.selector={};s.keyboardIds484={};s.glyphIds480.fill(220);
    s.length528=s.profileKind1192==2?length:0;if(s.profileKind1192==2){std::copy_n(existing.begin(),s.length528,s.glyphIds480.begin());setFullRange(s);}else selectorSet(s,0);
    //126C20 sets these cursor constants even when the imported inner selector
    //starts atEND. It does not initialize outer selected460; retain the owner
    //value (zero in a fresh native state) until1279C0 publishes the selection.
    const auto p=originalNameEntryTilePosition(0);s.cursorX452=p[0];s.cursorY456=p[1];
}
OriginalNameEntryEvents tickOriginalNameEntry(OriginalNameEntryState&s,const OriginalNameEntryInput&i,const OriginalNameEntryTables&t){
    OriginalNameEntryEvents e;
    switch(s.phase588){
    case 0:s.fadeEnabled584=1;--s.fade576;if(si(s.fade576)<0){s.fade576=0;++s.phase588;}break;
    case 1:
        s.sharedCountdown1176=si(s.sharedCountdown1176)>0?s.sharedCountdown1176-1:0;if(!s.sharedCountdown1176)s.timedOut604=1;
        s.fadeEnabled584=0;inputTick(s,i,t,e);if(e.nameCommitted&&s.selectedCardState96<=9)e.requestCardState10=true;break;
    case 2:s.fadeEnabled584=0;s.activeOverlay596=0;if(si(++s.hold592)>30){s.hold592=0;++s.phase588;}break;
    case 3:
        s.fadeEnabled584=1;if(si(++s.fade576)>15){s.fade576=15;const auto old=s.hold592++;if(si(old)>3){
            s.parentEvent64=!(s.profileFlags1180&2)?8:((s.profileKind1192==2?s.alternateScreen80:s.previousScreen76)<<16)|4;e.parentRequested=true;}}
        break;
    default:break;
    }
    if(s.blinkPhase472==1){s.blinkArgb468=0xffffff00;if(++s.blinkFrame476>5){s.blinkFrame476=0;s.blinkPhase472=2;}}
    else if(s.blinkPhase472==2){s.blinkArgb468=0xffffffff;if(++s.blinkFrame476>5){s.blinkFrame476=0;s.blinkPhase472=1;}}
    ++s.frame572;return e;
}
std::uint32_t originalNameEntryFadeArgb(const OriginalNameEntryState&s){return std::uint32_t(float(si(s.fade576))/15.f*255.f)<<24;}
}
