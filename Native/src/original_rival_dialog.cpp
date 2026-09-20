#include "original_rival_dialog.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <string_view>
#include <utility>

namespace idas3::original {
namespace {
#include "original_rival_dialog_tables.inc"
#include "original_bunta_dialog_tables.inc"

// Screen origin of every dialogue layer: 0C66A8/0C66CC/0C66D0 map one model
// unit to 100 pixels about (320,240), so (-3.2,2.4) is the top-left corner.
// The model constructor (0C1464A8) seeds every part with this exact triple,
// and a still dialogue element keeps it, so the authored words are used.
constexpr std::uint32_t sceneAnchorXWord=0xc04cccccu,sceneAnchorYWord=0x40199999u,
    sceneAnchorZWord=0xbe19999au;
constexpr float textBaselineY=416.25f;    // 0C0F7058
constexpr float nameOriginX=10.0f;        // 0C0F655C
constexpr float glyphDepth=-0.014f;       // 0C0C6C0C
constexpr float glyphZOffset=0.00001f;    // 0C0F6554
constexpr float nameSlideStep=0.04f;      // 0C0FB0B4, dialogue phase
constexpr float nameSettleStep=0.02f;     // 0C0FB0B8, ready phase
constexpr float nameSlideLimit=-2.0f;     // dialog+484
constexpr std::uint32_t sceneFrameChunk=18,sceneBorderChunk=19; // 0FB0C0
constexpr std::uint16_t projectionAngle=0x1000;                 // half of the 0x2000 field
// Player name plate (0FB0C0): 0C6C20(object, 320 + 100*slide, 15) then a
// fixed 48-pixel advance. Its glyph depth is the object's own 0A8240 offset.
constexpr float plateOriginX=320.f,plateSlideScale=100.f,plateOriginY=15.f,plateCell=48.f;
constexpr std::uint32_t plateDepthWord=0xbe18fc51u;

float word(std::uint32_t bits){return std::bit_cast<float>(bits);}

// 191060 mode 4: the alphabet advance table at 339524. A code with no row
// keeps the full cell, which is what the source lookup leaves behind.
float alphabetWidth(std::uint16_t jis){
    for(const auto& entry:alphabetAdvances)if(entry.jis==jis)return word(entry.widthWord);
    return 0.f;
}
bool alphabetTexture(std::uint32_t jisIndex,std::uint16_t& texture){
    for(const auto& entry:alphabetGlyphs)if(entry.first==jisIndex){texture=entry.second;return true;}
    return false;
}
bool namekanaTexture(std::uint32_t jisIndex,std::uint16_t& sprite){
    for(const auto& entry:namekanaGlyphs)if(entry.first==jisIndex){sprite=entry.second;return true;}
    return false;
}
// Per-rival result byte at 31CA10 (profile+116): 134500 is the high nibble,
// 134520 the low nibble.
std::uint32_t rivalHigh(const OriginalBattleProfile& p,std::uint32_t n){
    return n<31?std::uint32_t(p.byte(116+n))>>4:0u;
}
std::uint32_t rivalLow(const OriginalBattleProfile& p,std::uint32_t n){
    return n<31?std::uint32_t(p.byte(116+n))&15u:0u;
}
// 134200(n): every rival of course group n already beaten.
bool courseGroupCleared(const OriginalBattleProfile& p,std::uint32_t group){
    if(group>=rivalGroups.size())return false;
    const auto first=std::uint32_t(rivalGroups[group].first),count=std::uint32_t(rivalGroups[group].second);
    std::uint32_t beaten=0;
    for(std::uint32_t i=0;i<count;++i)if(rivalHigh(p,first+i))++beaten;
    return beaten==count;
}
std::uint32_t defeatedRivalCount(const OriginalBattleProfile& p){
    std::uint32_t count=0;
    for(std::uint32_t i=0;i<29;++i)if(rivalHigh(p,i))++count;
    return count;
}

// ---- source text-line object (0C5EE0/0C60C0 family) ------------------------
void resetCursor(OriginalDialogLine& line){line.cursorX=line.startX;line.cursorY=line.startY;}
void clearLine(OriginalDialogLine& line){line.glyphs.clear();line.revealed=0;resetCursor(line);}
void clearText(OriginalDialogLine& line){clearLine(line);line.tick=0;line.waitHead=0;line.waitCount=0;}
void setLineOrigin(OriginalDialogLine& line,float x,float y){
    line.startX=x;line.startY=y;line.cursorX=x;line.cursorY=y;line.lineStart=x;
}
// Every cursor advance is one source FMAC, a true fused multiply-add.
void newLine(OriginalDialogLine& line){
    line.cursorX=line.lineStart;
    line.cursorY=std::fma(line.linePitch,line.scale,line.cursorY);
}
void pushGlyph(OriginalDialogLine& line,std::uint16_t texture){
    line.glyphs.push_back({texture,line.cursorX,line.cursorY,line.zOffset+glyphDepth});
}
// 0C6A00(line, jis, 4): proportional advance from the alphabet width table.
// 0C68E0(line, jis): the same append with a fixed full-cell advance.
void appendJis(OriginalDialogLine& line,std::string_view jis,bool proportional){
    for(std::size_t i=0;i<jis.size();){
        const auto b0=static_cast<unsigned char>(jis[i]);
        if(b0==10){newLine(line);++i;continue;}
        if(i+1>=jis.size())break;
        const auto b1=static_cast<unsigned char>(jis[i+1]);
        const std::uint32_t index=(std::uint32_t(b0)-0xA0u)*96u+(std::uint32_t(b1)-0xA0u);
        if(index<=9216u){
            std::uint16_t texture=0;
            if(alphabetTexture(index,texture))pushGlyph(line,texture);
            const auto code=std::uint16_t((std::uint32_t(b0)<<8)|b1);
            line.cursorX=proportional?std::fma(line.cell,1.f-alphabetWidth(code),line.cursorX)
                                     :std::fma(line.cell,line.scale,line.cursorX);
        }
        i+=2;
    }
}
void pushWait(OriginalDialogLine& line,std::uint32_t frames){   // 0C70E0
    if(line.waitCount>=line.waitGlyph.size())return;
    line.waitGlyph[line.waitCount]=std::uint32_t(line.glyphs.size());
    line.waitFrames[line.waitCount]=frames;
    ++line.waitCount;
}
// 0C6FE0 page timer; the return is 0C6FA0's completion test.
bool stepTextTimer(OriginalDialogLine& line){
    ++line.tick;
    if(line.waitHead<line.waitCount&&line.waitGlyph[line.waitHead]==line.revealed){
        if(line.tick>line.waitFrames[line.waitHead]){line.tick=0;++line.waitHead;}
    }else if(line.tick>line.interval){
        line.tick=0;
        if(line.revealed<std::uint32_t(line.glyphs.size()))++line.revealed;
    }
    return line.revealed>=std::uint32_t(line.glyphs.size())&&line.waitHead>=line.waitCount;
}

// ---- background element list (0F4660 / 0F4D20) -----------------------------
struct ElementView { std::uint32_t portrait=0,background=0,spectators=0; };
ElementView elementAt(std::uint32_t character,std::uint32_t index){
    ElementView view;
    if(character>=scrollTables.size())return view;
    const auto first=std::uint32_t(scrollTables[character].first);
    const auto count=std::uint32_t(scrollTables[character].second);
    if(!count)return view;
    const auto& record=scrollRecords[first+index%count].words;
    const auto raw=record[0];
    // 31A6A8[character] remaps the authored portrait part (element ctor).
    const auto remap=raw<count?portraitPartRemap[first+raw]:std::int8_t(-1);
    view.portrait=remap>=0?std::uint32_t(remap):raw;
    view.background=record[1];
    view.spectators=record[2];
    return view;
}

// ---- script interpreter 0F6D40 --------------------------------------------
// Flag words +384..+464; the slot index is (offset-384)/4.
enum : std::size_t {
    flagCar=0,flagBeaten=1,flagLost=2,flagRaced=3,flagStage=4,flagCheer=5,flagScene=6,
    flagUnraced=7,flagExactCount=8,flagAtLeastCount=9,flagUnbeaten=10,flagStageAtLeast=11,
    flagLevel=12,flagRank=13,flagSelection=14,flagGroup=15,flagLevelAtLeast=16,
    flagPointsHigh=17,flagPointsLow=18,flagCardA=19,flagCardB=20
};
constexpr std::string_view inertCommands="!#)->]abfghijloqstvyz|}";

struct Interpreter {
    OriginalRivalDialogState& s;
    const OriginalRivalDialogRecord& record;
    const OriginalBattleProfile& profile;
    std::uint32_t mode=0;
    bool loop=true,yield=false,positioned=false;
    std::int32_t prescan=-1;

    std::string_view token(std::uint32_t index)const{
        return index<record.tokens.size()?std::string_view(record.tokens[index].bytes):std::string_view();
    }
    std::uint32_t argument()const{
        const auto t=token(s.cursor);
        std::uint32_t value=0;
        for(std::size_t i=2;i<t.size();++i){
            const auto c=static_cast<unsigned char>(t[i]);
            if(c<'0'||c>'9')break;
            value=value*10u+std::uint32_t(c-'0');
        }
        return value;
    }
    bool bare()const{return token(s.cursor).size()==1;}
    void skipTo(char close){
        while(s.cursor+1<std::uint32_t(record.tokens.size())){
            const auto t=token(s.cursor);
            if(!t.empty()&&t[0]==close)break;
            ++s.cursor;
        }
    }
    // Bare conditional: an unset flag skips to the closing token; either way
    // the flag is cleared and the step yields.
    void closeConditional(std::size_t slot,char close){
        if(!s.flags[slot])skipTo(close);
        s.flags[slot]=0;
        yield=true;
    }
    void setConditional(std::size_t slot,bool condition){if(condition)s.flags[slot]=1;}
    // 0F8020/0F73A0/0F7F20 share this lazy page placement.
    void positionText(){
        if(positioned)return;
        positioned=true;
        setLineOrigin(s.text,float(s.textAnchorX),textBaselineY);
        s.text.color=speakerColours[s.speaker<speakerColours.size()?s.speaker:0];
        clearText(s.text);
        resetCursor(s.text);
    }
    void appendAscii(std::string_view text){
        // 0F6CC0: skip the leading command byte, map ASCII through 31B684.
        std::string converted;
        for(std::size_t i=1;i<text.size();++i){
            auto c=static_cast<unsigned char>(text[i]);
            if(c>=128)c=32;
            if(c>31){
                const auto jis=asciiToJis[c-32];
                converted.push_back(char(jis>>8));
                converted.push_back(char(jis&0xff));
            }else if(c==10)converted.push_back('\n');
        }
        appendJis(s.text,converted,true);
    }
    std::int32_t run();
    void dispatch(char command);
};

void Interpreter::dispatch(char command){
    const auto n=argument();
    switch(command){
    case 'C':{                                        // 0F6E60 speaker change
        s.speaker=n;
        const auto colour=speakerColours[n<speakerColours.size()?n:0];
        s.text.color=colour;s.name.color=colour;
        clearLine(s.name);
        if(s.buntaChallenge)appendJis(s.name,buntaSpeakerName,true);
        else{
            const auto first=std::uint32_t(speakerNameLists[s.character<31?s.character:0].first);
            const auto count=std::uint32_t(speakerNameLists[s.character<31?s.character:0].second);
            if(count)appendJis(s.name,speakerNameStrings[first+(n<count?n:0)],true);
        }
        appendJis(s.name,nameSeparator,true);
        s.slotFlags.fill(0);
        clearText(s.text);resetCursor(s.text);
        if(!s.name.glyphs.empty())s.textAnchorX=std::int32_t(s.name.glyphs.back().x);
        positioned=true;
        setLineOrigin(s.text,float(s.textAnchorX),textBaselineY);
        s.text.color=colour;
        clearText(s.text);resetCursor(s.text);
        yield=true;
        break;}
    case 'W':                                         // 0F6F40 page dwell
        pushWait(s.text,n?n:std::uint32_t(std::int32_t(float(s.text.glyphs.size())*4.f))+40u);
        break;
    case 'E':                                         // 0F6FA0 end of page
        s.endFlag=true;yield=true;break;
    case 'K':                                         // 0F6FC0 clear page
        if(n<s.slotFlags.size())s.slotFlags[n]=0;
        clearText(s.text);resetCursor(s.text);clearLine(s.name);
        break;
    case 'R':                                         // 0F7020 name-plate cue
        s.revealThreshold=std::uint32_t(s.text.glyphs.size());s.revealCounter=1;break;
    case 'P':                                         // 0F7080 picture select
        if(bare()){if(s.request2)s.request4=true;}
        else{
            s.picture=n;
            if(mode==1){prescan=std::int32_t(n);return;}
        }
        s.request2=true;yield=true;break;
    case 'B':                                         // 0F70E0 player car is n
        if(bare())closeConditional(flagCar,'b');
        else setConditional(flagCar,s.playerCar==n);
        break;
    case 'X':                                         // 0F7140 player car is not n
        if(bare())s.flags[flagCar]=1;
        else if(s.playerCar==n)s.flags[flagCar]=0;
        break;
    case 'V':                                         // 0F71A0 rival n beaten
        if(bare())closeConditional(flagBeaten,'v');
        else setConditional(flagBeaten,rivalHigh(profile,n)!=0);
        break;
    case 'Y':                                         // 0F7220 rival n unbeaten
        if(bare())closeConditional(flagUnbeaten,'y');
        else setConditional(flagUnbeaten,rivalHigh(profile,n)==0);
        break;
    case 'L':                                         // 0F72A0 lost to rival n
        if(bare())closeConditional(flagLost,'l');
        else setConditional(flagLost,rivalLow(profile,n)!=0);
        break;
    case 'H':                                         // 0F7320 raced rival n
        if(bare())closeConditional(flagRaced,'h');
        else setConditional(flagRaced,rivalHigh(profile,n)+rivalLow(profile,n)!=0);
        break;
    case 'M':                                         // 0F73A0 player car name
        positionText();
        appendJis(s.text,carNames[s.playerCar<carNames.size()?s.playerCar:0],true);
        break;
    case 'T':                                         // 0F7400 stage byte equals n
        if(bare())closeConditional(flagStage,'t');
        else setConditional(flagStage,std::uint32_t(profile.byte(164))==n);
        break;
    case 'O':                                         // 0F74C0 stage byte at least n
        if(bare())closeConditional(flagStageAtLeast,'o');
        else setConditional(flagStageAtLeast,std::uint32_t(profile.byte(164))>=n);
        break;
    case 'G':                                         // 0F7540 spectator draw
        if(bare())closeConditional(flagCheer,'g');
        else setConditional(flagCheer,s.cheer==n);
        break;
    case 'S':                                         // 0F75C0 scene byte 31CE46
        if(bare())closeConditional(flagScene,'s');
        else setConditional(flagScene,s.sceneByte46==n);
        break;
    case 'D':                                         // 0F7660 restore full scale
        s.text.scale=1.f;break;
    case 'I':                                         // 0F7680 scene byte 31CE48
        if(bare())closeConditional(flagLevel,'i');
        else setConditional(flagLevel,s.sceneByte48==n);
        break;
    case 'J':                                         // 0F7700 gate on 31CE47
        if((s.sceneByte47!=0)!=(n!=0))skipTo('j');
        break;
    case 'A':                                         // 0F77C0 profile flag 0x02000000
        if(bare())closeConditional(flagCardA,'a');
        else setConditional(flagCardA,((profile.u(1180)&0x02000000u)!=0u)==(n!=0u));
        break;
    case '$':                                         // 0F7840 profile flag 0x01000000
        if(bare())closeConditional(flagCardB,'#');
        else setConditional(flagCardB,((profile.u(1180)&0x01000000u)!=0u)==(n!=0u));
        break;
    case 'F':                                         // 0F78C0 never raced rival n
        if(bare())closeConditional(flagUnraced,'f');
        else setConditional(flagUnraced,rivalHigh(profile,n)==0&&rivalLow(profile,n)==0);
        break;
    case '[':                                         // 0F7960 profile byte 31CA34
        if(bare())closeConditional(flagRank,']');
        else setConditional(flagRank,std::uint32_t(profile.byte(152))==n);
        break;
    case '(':                                         // 0F79E0 selection word 31C9DC
        if(bare())closeConditional(flagSelection,')');
        else setConditional(flagSelection,profile.u(64)==n);
        break;
    case '@':                                         // 0F7A40 course group cleared
        if(bare())closeConditional(flagGroup,'!');
        else setConditional(flagGroup,courseGroupCleared(profile,n));
        break;
    case '{':                                         // 0F7BE0 course group not cleared
        if(bare())closeConditional(flagGroup,'}');
        else setConditional(flagGroup,!courseGroupCleared(profile,n));
        break;
    case '/':                                         // 0F7AE0 progress word above 14
        if(bare())closeConditional(flagPointsHigh,'|');
        else setConditional(flagPointsHigh,profile.u(1080+4*n)>14u);
        break;
    case '+':                                         // 0F7B60 progress word at most 14
        if(bare())closeConditional(flagPointsLow,'-');
        else setConditional(flagPointsLow,profile.u(1080+4*n)<=14u);
        break;
    case 'Q':                                         // 0F7C60 at least n rivals beaten
        if(bare())closeConditional(flagAtLeastCount,'q');
        else if(defeatedRivalCount(profile)>=n){s.flags[flagAtLeastCount]=1;yield=true;}
        break;
    case 'Z':                                         // 0F7E60 exactly n rivals beaten
        if(bare())closeConditional(flagExactCount,'z');
        else if(defeatedRivalCount(profile)==n){s.flags[flagExactCount]=1;yield=true;}
        break;
    case '<':                                         // 0F7D00 level at least n
        if(bare())closeConditional(flagLevelAtLeast,'>');
        else setConditional(flagLevelAtLeast,n<=s.sceneByte48);
        break;
    case 'N':                                         // 0F7D3E page break
        yield=true;break;
    case 'U':{                                        // 0F7F20 player name
        positionText();
        const auto flags=profile.u(1180);
        if((flags&2u)||(flags&1u)){
            const auto count=profile.u(76);
            for(std::uint32_t i=0;i<count&&i<32u;++i){
                const auto glyph=profile.u(44+4*i);
                if(glyph<nameEntryGlyphs.size())appendJis(s.text,nameEntryGlyphs[glyph],true);
            }
        }else appendJis(s.text,playerWord,true);
        break;}
    case '_':{                                        // 0F7DA0 authored race count
        const auto value=s.sceneByte47;
        if(value>98)appendJis(s.text,levelSuffix,false);
        else{
            const auto tens=value/10u;
            if(tens==1u)appendJis(s.text,levelUnit,false);
            else if(tens>1u){
                appendJis(s.text,digitStrings[tens<digitStrings.size()?tens:0],false);
                appendJis(s.text,levelUnit,false);
            }
            appendJis(s.text,levelSuffix,false);
        }
        break;}
    default:
        if(inertCommands.find(command)==std::string_view::npos){
            positionText();
            appendAscii(token(s.cursor));
        }
        break;
    }
}

std::int32_t Interpreter::run(){
    while(true){
        const auto t=token(s.cursor);
        if(t.empty())return 0;
        yield=false;
        dispatch(t[0]);
        if(mode!=0){
            if(prescan>=0)return prescan;
            if(s.endFlag)return -1;
        }else if(yield)loop=false;
        ++s.cursor;
        if(!loop)return 0;
    }
}

// 0F81A0: clear the conditional flags and rewind the cursor.
void resetScriptFields(OriginalRivalDialogState& s){
    s.flags.fill(0);
    s.cursor=0;
}
}   // namespace

float originalDialogTangent(std::uint16_t angle){
    // 1FA280, preserving the source float order and continued fraction.
    if((angle&0x7fffu)==0x4000u)return word(0x7f7fffffu);
    float value=float(angle)*word(0x40c90fdbu);
    value*=word(0x37800000u);
    const float quarterTurn=word(0x3fc90fdbu);
    float quarter=value/quarterTurn;
    quarter+=.5f;
    const int quadrant=int(quarter);
    value-=float(quadrant)*quarterTurn;
    const float square=value*value;
    float term=0.f;
    for(int divisor=13;divisor>2;divisor-=2){
        const float denominator=float(divisor)-term;
        term=square/denominator;
    }
    const float tangent=value/(1.f-term);
    if((quadrant&1)==0)return tangent;
    if(tangent==0.f)return word(0x7f7fffffu);
    return word(0xbf800000u)/tangent;
}

namespace {
// 145920/0C6560: the depth-compensating scale, computed in double exactly as
// 224000/222D00/222A00/223400 do.
float projectionScale(float z){
    const double tangent=double(originalDialogTangent(projectionAngle));
    return float(double(-z)/2.4*tangent);
}
void applyPicture(OriginalRivalDialogState& s){
    const auto element=s.buntaChallenge?
        ElementView{buntaDialogElements[s.picture%buntaDialogElements.size()][0],
            buntaDialogElements[s.picture%buntaDialogElements.size()][1],0}:
        elementAt(s.character,s.picture);
    s.portraitChunk=element.portrait;
    s.backgroundChunk=element.background;
    s.spectators=element.spectators;
}
void buildScript(OriginalRivalDialogState& s,const OriginalRivalDialogData& data,
    const OriginalBattleProfile& profile){
    const auto& record=s.buntaChallenge?data.buntaRecord(s.course,s.kind):data.record(s.character,s.kind);
    s.text=OriginalDialogLine{};
    s.name=OriginalDialogLine{};
    s.text.interval=1;s.text.zOffset=glyphZOffset;
    s.name.interval=1;s.name.zOffset=glyphZOffset;
    setLineOrigin(s.name,nameOriginX,textBaselineY);
    s.speaker=0;s.endFlag=false;s.scriptPhase=0;
    s.frame68=0;s.frame72=0;s.revealCounter=0;s.revealThreshold=0;
    s.request2=false;s.request4=false;s.slotFlags.fill(0);s.textAnchorX=0;
    resetScriptFields(s);
    // 0F8160: the prescan reports the opening picture of this page.
    Interpreter prescan{s,record,profile,1};
    const auto picture=prescan.run();
    if(picture>=0)s.picture=std::uint32_t(picture);
    s.requestedPicture=s.picture;
    applyPicture(s);
}
}   // namespace

void resetOriginalRivalDialog(OriginalRivalDialogState& s,const OriginalRivalDialogData& data,
    const OriginalBattleProfile& profile,const OriginalRivalDialogSetup& setup){
    const auto& identity=data.enemy(setup.enemy);
    s=OriginalRivalDialogState{};
    s.buntaChallenge=setup.buntaChallenge;
    s.character=setup.buntaChallenge?31u:identity.character;
    s.kind=setup.kind;
    s.course=setup.buntaChallenge?setup.buntaCourse:identity.backgroundCourse;
    s.night=setup.buntaChallenge?1u:identity.backgroundNight;
    s.weather=setup.buntaChallenge?0u:setup.weather;
    s.variant=setup.buntaChallenge?34u+s.course:s.course*4u+s.night*2u+s.weather;
    s.playerCar=setup.playerCar;
    s.cheer=setup.cheer;
    s.playerName=setup.playerName;
    s.sceneByte46=profile.byte(1194);
    s.sceneByte47=profile.byte(1195);
    s.sceneByte48=profile.byte(1196);
    s.phase=0;s.fade=20;s.fadeEnabled=true;
    s.nameSlide=2.0f;s.nameSlideRate=0.2f;
    buildScript(s,data,profile);
    s.initialized=true;
}

void advanceOriginalRivalDialogPage(OriginalRivalDialogState& s,const OriginalRivalDialogData& data,
    const OriginalBattleProfile& profile,std::int32_t delta){
    const auto next=std::int32_t(s.kind)+delta;
    if(next<0||next>=(s.buntaChallenge?51:24))return;
    s.kind=std::uint32_t(next);
    // 0FA6AE..0FA6BA writes scene+0x1A4=1 when replacing a page.
    // Leaving phase2 here reports the new refusal/reply as already ready.
    s.phase=1;s.skip=false;s.completed=false;
    buildScript(s,data,profile);
}

void closeOriginalRivalDialog(OriginalRivalDialogState& s){s.fadeEnabled=true;s.phase=3;}
void skipOriginalRivalDialog(OriginalRivalDialogState& s){if(s.phase>0)s.skip=true;}

std::uint32_t stepOriginalRivalDialog(OriginalRivalDialogState& s,const OriginalRivalDialogData& data,
    const OriginalBattleProfile& profile){
    if(!s.initialized)return 0;
    std::uint32_t status=0;
    switch(s.phase){
    case 0:                                            // 0FADC0 opening fade
        if(--s.fade<0){s.fade=0;s.fadeEnabled=false;s.phase=1;}
        break;
    case 1:{                                           // 0FADE0
        const auto& record=s.buntaChallenge?data.buntaRecord(s.course,s.kind):data.record(s.character,s.kind);
        // 0F69E0
        s.request2=false;s.request4=false;
        if(s.scriptPhase==0){
            Interpreter step{s,record,profile,0};
            step.run();
            s.scriptPhase=1;
        }else if(s.scriptPhase==1)s.scriptPhase=2;
        else if(s.scriptPhase==2){
            if(stepTextTimer(s.text)){
                if(!s.endFlag)s.scriptPhase=0;
                else{s.frame68=0;s.scriptPhase=3;}
            }
            if(s.revealCounter&&s.text.revealed>=s.revealThreshold)++s.revealCounter;
        }else if(s.scriptPhase==3){
            if(s.frame68==60){
                s.slotFlags.fill(0);
                clearText(s.text);resetCursor(s.text);
                clearLine(s.name);
            }
            // 0F6C38 returns before the frame counters advance.
            if(s.frame68>120)status=3;
        }
        if(status!=3){++s.frame68;++s.frame72;}
        if(!status){
            if(s.request4)status=4;
            else if(s.request2)status=2;
            else if(s.revealCounter>1)status=1;
        }
        // The applied part change trails the request by two updates and the
        // scene is not drawn while it is pending.
        if(s.backgroundWait){if(!--s.backgroundWait)applyPicture(s);}
        switch(status){
        case 1:s.nameVisible=true;break;
        case 2:
        case 4:
            if(s.picture!=s.requestedPicture){s.requestedPicture=s.picture;s.backgroundWait=2;}
            break;
        case 3:s.completed=true;break;
        default:break;
        }
        if(s.completed||s.skip){if(s.phase>0)++s.phase;}
        else if(s.nameVisible&&s.nameSlideRate>0.f){
            s.nameSlide-=s.nameSlideRate;
            if(nameSlideLimit>s.nameSlide)s.nameSlideRate-=nameSlideStep;
        }
        break;}
    case 2:                                            // 0FB000 label settle
        if(s.nameSlideRate>0.f){
            s.nameSlide-=s.nameSlideRate;
            if(0.f>s.nameSlide)s.nameSlideRate-=nameSettleStep;
        }
        break;
    default:                                           // 0FB040 closing fade
        if(++s.fade>20)s.closed=true;
        break;
    }
    return status;
}

std::vector<OriginalRivalDialogDraw> originalRivalDialogDraws(const OriginalRivalDialogState& s){
    std::vector<OriginalRivalDialogDraw> draws;
    if(!s.initialized)return draws;
    const auto sceneAnchorX=word(sceneAnchorXWord),sceneAnchorY=word(sceneAnchorYWord),sceneAnchorZ=word(sceneAnchorZWord);
    const auto sceneScale=projectionScale(sceneAnchorZ);
    const auto scenePart=[&](OriginalRivalDialogBank bank,std::uint32_t chunk){
        OriginalRivalDialogDraw draw;
        draw.bank=bank;draw.chunk=chunk;
        draw.scaleX=sceneScale;draw.scaleY=sceneScale;draw.scaleZ=1.f;
        draw.x=sceneAnchorX;draw.y=sceneAnchorY;draw.z=sceneAnchorZ;
        return draw;
    };
    // 0F56E0 draws the portrait then the background, and is skipped while a
    // part change is pending.
    if(!s.backgroundWait){
        draws.push_back(scenePart(OriginalRivalDialogBank::Portrait,s.portraitChunk));
        draws.push_back(scenePart(OriginalRivalDialogBank::Background,s.backgroundChunk));
    }
    if(s.phase==1){                                   // 0F6880: page then label
        const auto emit=[&](const OriginalDialogLine& line){
            for(const auto& glyph:line.glyphs){
                OriginalRivalDialogDraw draw;
                draw.bank=OriginalRivalDialogBank::Alphabet;
                draw.chunk=glyph.texture;
                const auto scale=projectionScale(glyph.z);
                draw.scaleX=scale;draw.scaleY=scale;draw.scaleZ=1.f;
                draw.x=(glyph.x-320.f)/100.f;
                draw.y=(240.f-glyph.y)/100.f;
                draw.z=glyph.z;
                draw.color=line.color;draw.tinted=true;
                draws.push_back(draw);
            }
        };
        emit(s.text);
        emit(s.name);
    }
    if(s.nameVisible){
        // The plate is cleared, repositioned and refilled every update, so it
        // is a pure function of the slide and the stored name.
        float cursor=std::fma(s.nameSlide,plateSlideScale,plateOriginX);
        const auto depth=word(plateDepthWord);
        const auto scale=projectionScale(depth);
        for(std::size_t i=0;i+1<s.playerName.size();i+=2){
            const auto b0=static_cast<unsigned char>(s.playerName[i]);
            const auto b1=static_cast<unsigned char>(s.playerName[i+1]);
            const std::uint32_t index=(std::uint32_t(b0)-0xA0u)*96u+(std::uint32_t(b1)-0xA0u);
            std::uint16_t sprite=0;
            if(index<=9216u&&namekanaTexture(index,sprite)){
                OriginalRivalDialogDraw draw;
                draw.bank=OriginalRivalDialogBank::Namekana;
                draw.chunk=sprite;
                draw.scaleX=scale;draw.scaleY=scale;draw.scaleZ=1.f;
                draw.x=(cursor-320.f)/100.f;
                draw.y=(240.f-plateOriginY)/100.f;
                draw.z=depth;
                draws.push_back(draw);
            }
            cursor=std::fma(plateCell,1.f,cursor);
        }
    }
    draws.push_back(scenePart(OriginalRivalDialogBank::Scene,sceneFrameChunk));
    draws.push_back(scenePart(OriginalRivalDialogBank::Scene,sceneBorderChunk));
    return draws;
}

std::uint32_t originalRivalDialogFadeArgb(const OriginalRivalDialogState& s){
    if(!s.fadeEnabled)return 0;
    const auto counter=std::min<std::int32_t>(s.fade,15);
    auto alpha=std::int32_t(float(counter)/15.f*255.f);
    alpha=std::clamp(alpha,0,255);
    return std::uint32_t(alpha)<<24;
}

std::string originalRivalDialogPlayerName(const OriginalBattleProfile& profile){
    const auto flags=profile.u(1180);
    std::string name;
    if((flags&2u)||(flags&1u)){
        const auto count=profile.u(76);
        for(std::uint32_t i=0;i<count&&i<32u;++i){
            const auto glyph=profile.u(44+4*i);
            if(glyph<nameEntryGlyphs.size())name+=std::string(nameEntryGlyphs[glyph]);
        }
    }
    return name;
}
std::string originalRivalDialogPortraitBank(const OriginalRivalDialogData& data,std::uint32_t enemy){
    return "rival_"+data.enemy(enemy).portrait;
}
std::string originalRivalDialogBackgroundBank(const OriginalRivalDialogData& data,std::uint32_t enemy,
    std::uint32_t weather){
    const auto& identity=data.enemy(enemy);
    if(identity.backgroundCourse==8)return "rival_bg08";
    char name[32]={};
    std::snprintf(name,sizeof name,"rival_bg%02u%c%c",identity.backgroundCourse,
        identity.backgroundNight?'n':'d',weather?'r':'f');
    return name;
}
}
