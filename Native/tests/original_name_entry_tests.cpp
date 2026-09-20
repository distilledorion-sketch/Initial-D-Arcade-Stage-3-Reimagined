#include "original_name_entry.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0d000000,outer=0x0d001000,inner=0x0d002000,coordinates=0x0d003000;
constexpr unsigned profile=0x0c31c99c;
std::uint64_t checks=0,instructions=0,ticks=0;
std::string scenario;
void equal(unsigned a,unsigned b,const std::string& label){++checks;if(a!=b)throw std::runtime_error(scenario+" "+label+" source="+hex(a)+" native="+hex(b));}
void seed(RefMemory&m,const OriginalNameEntryState&s){
    m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(0x0d100000,0x10000);m.zeroRegion(profile,0x1000);m.zeroRegion(0x0c92ed00,0x50);
    for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{64,s.parentEvent64},{76,s.previousScreen76},{80,s.alternateScreen80},{460,s.selected460},{464,outer},{468,s.blinkArgb468},{472,s.blinkPhase472},{476,s.blinkFrame476},{528,s.length528},{572,s.frame572},{576,s.fade576},{584,s.fadeEnabled584},{588,s.phase588},{592,s.hold592},{600,s.page600}})m.write32(owner+offset,value);
    m.writeFloat(owner+452,s.cursorX452);m.writeFloat(owner+456,s.cursorY456);m.write8(owner+596,s.activeOverlay596);m.write8(owner+604,s.timedOut604);
    for(unsigned i=0;i<5;++i){m.write32(owner+480+i*8,s.glyphIds480[i]);m.write32(owner+484+i*8,s.keyboardIds484[i]);}
    m.write32(profile+1176,s.sharedCountdown1176);m.write32(profile+1180,s.profileFlags1180);m.write8(profile+1192,s.profileKind1192);
    m.write32(outer+4,inner);m.write32(outer+8,coordinates);m.writeFloat(outer+12,s.selector.progress);m.write32(outer+24,s.selector.selected);m.writeFloat(outer+28,s.selector.deltaX);m.writeFloat(outer+32,s.selector.deltaY);
    m.write32(inner+4,53);m.write32(inner+8,s.selector.selected);m.writeFloat(inner+12,.2f);m.writeFloat(inner+16,.06f);m.writeFloat(inner+20,.4f);m.writeFloat(inner+24,s.selector.velocity);m.writeFloat(inner+28,.2f);
    float accel=.06f*.06f;accel*=.5f;accel*=.9f;m.writeFloat(inner+32,accel);m.write32(inner+36,s.selector.phase);m.write32(inner+40,s.selector.minimum);m.write32(inner+44,s.selector.maximum);m.write8(inner+48,s.selector.wrap);
    for(unsigned i=0;i<53;++i){const auto p=originalNameEntryTilePosition(i);m.writeFloat(coordinates+i*8,p[0]);m.writeFloat(coordinates+i*8+4,p[1]);}
}
void compare(RefMemory&m,const OriginalNameEntryState&s){
    for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{64,s.parentEvent64},{460,s.selected460},{468,s.blinkArgb468},{472,s.blinkPhase472},{476,s.blinkFrame476},{528,s.length528},{576,s.fade576},{584,s.fadeEnabled584},{588,s.phase588},{592,s.hold592},{600,s.page600}})equal(m.read32(owner+offset),value,"owner+"+std::to_string(offset));
    equal(m.read32(owner+452),std::bit_cast<unsigned>(s.cursorX452),"cursorX");equal(m.read32(owner+456),std::bit_cast<unsigned>(s.cursorY456),"cursorY");
    equal(m.read8(owner+596),s.activeOverlay596,"overlay");equal(m.read8(owner+604),s.timedOut604,"timeout");
    for(unsigned i=0;i<5;++i){equal(m.read32(owner+480+i*8),s.glyphIds480[i],"glyph"+std::to_string(i));equal(m.read32(owner+484+i*8),s.keyboardIds484[i],"keyboard"+std::to_string(i));}
    equal(m.read32(profile+1176),s.sharedCountdown1176,"countdown");equal(m.read32(profile+1180),s.profileFlags1180,"flags");
    equal(m.read32(inner+8),s.selector.selected,"inner selected");equal(m.read32(inner+36),s.selector.phase,"inner phase");equal(m.read32(inner+40),s.selector.minimum,"inner min");equal(m.read32(inner+44),s.selector.maximum,"inner max");equal(m.read8(inner+48),s.selector.wrap,"wrap");
    equal(m.read32(outer+12),std::bit_cast<unsigned>(s.selector.progress),"progress");equal(m.read32(inner+24),std::bit_cast<unsigned>(s.selector.velocity),"velocity");equal(m.read32(outer+28),std::bit_cast<unsigned>(s.selector.deltaX),"deltaX");equal(m.read32(outer+32),std::bit_cast<unsigned>(s.selector.deltaY),"deltaY");
}
OriginalNameEntryEvents tick(RefMemory&m,OriginalNameEntryState&s,const OriginalNameEntryInput&i,const OriginalNameEntryTables&t){
    seed(m,s);RefCpu c(m);c.r[4]=owner;c.r[15]=0x0d10ff00;c.pr=0x0dffffff;std::vector<unsigned> cues;
    c.callHooks[0x0c0d4340]=[&](RefCpu&v){v.setFloat(0,i.steering);};c.callHooks[0x0c0d4300]=[&](RefCpu&v){v.r[0]=v.r[5]==1?i.confirmPressed:i.backPressed;};
    m.write8(0x0c92ed40,i.rowJump>0?32:i.rowJump<0?16:0);
    c.callHooks[0x0c141f80]=[&](RefCpu&v){equal(v.r[5],1,"cue selector");cues.push_back(v.r[4]);};
    c.callHooks[0x0c1fa9e0]=[](RefCpu&){};c.callHooks[0x0c16d680]=[&](RefCpu&v){v.r[0]=unsigned(s.selectedCardState96);};bool promoted=false;c.callHooks[0x0c16db80]=[&](RefCpu&v){equal(v.r[4],10,"card promotion argument");promoted=true;};
    instructions+=c.run(0x0c1281e0,0x0c128454,1000000);const auto oldFrame=s.frame572;auto e=tickOriginalNameEntry(s,i,t);equal(s.frame572,oldFrame+1,"frame");compare(m,s);equal(promoted,e.requestCardState10,"card promotion event");equal(unsigned(cues.size()),unsigned(e.cueIds.size()),"cue count");for(unsigned n=0;n<cues.size();++n)equal(cues[n],e.cueIds[n],"cue order");
    if(e.nameCommitted){equal(m.read32(profile+76),s.length528,"committed length");for(unsigned n=0;n<s.length528;++n)equal(m.read32(profile+44+n*4),s.glyphIds480[n],"committed glyph");}
    ++ticks;return e;
}
void select(OriginalNameEntryState&s,unsigned tile){s.selector={};s.selector.selected=s.selected460=tile;const auto p=originalNameEntryTilePosition(tile);s.cursorX452=p[0];s.cursorY456=p[1];}
void initReference(RefMemory&m){
    constexpr unsigned stack=0x0d010000;
    scenario="source initialized coordinates";m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);m.write32(stack+300,owner);m.write32(owner+464,outer);
    RefCpu c(m);c.r[14]=stack;c.r[15]=stack+0xf000;unsigned positions=0;
    c.callHooks[0x0c09c0c0]=[&](RefCpu&v){const auto p=originalNameEntryTilePosition(v.r[5]);equal(v.fr[4],std::bit_cast<unsigned>(p[0]),"source x");equal(v.fr[5],std::bit_cast<unsigned>(p[1]),"source y");++positions;};
    c.callHooks[0x0c021960]=[](RefCpu&v){v.r[0]=0x0d008000;};instructions+=c.run(0x0c127124,0x0c1273e6,20000);equal(positions,53,"position count");
    OriginalNameEntryState native;initializeOriginalNameEntry(native);
    for(auto[offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{468,native.blinkArgb468},{472,native.blinkPhase472},{476,native.blinkFrame476},{572,native.frame572},{576,native.fade576},{588,native.phase588},{592,native.hold592}})equal(m.read32(owner+offset),value,"initial owner+"+std::to_string(offset));
    equal(m.read32(owner+452),std::bit_cast<unsigned>(native.cursorX452),"initial x");equal(m.read32(owner+456),std::bit_cast<unsigned>(native.cursorY456),"initial y");equal(m.read8(owner+596),native.activeOverlay596,"initial overlay");
    for(unsigned kind=0;kind<3;++kind)for(unsigned length=0;length<6;++length){scenario="source import kind"+std::to_string(kind)+" length"+std::to_string(length);m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);m.zeroRegion(profile,0x1000);m.write32(stack+300,owner);m.write32(stack+452,profile+1148);m.write8(profile+1192,std::uint8_t(kind));m.write32(profile+76,length);std::array<unsigned,5> glyphs{162,163,164,165,166};for(unsigned n=0;n<5;++n)m.write32(profile+44+n*4,glyphs[n]);
        RefCpu r(m);r.r[14]=stack;r.r[15]=stack+0xf000;instructions+=r.run(0x0c126e76,kind==2?0x0c126efa:0x0c12704a,2000);OriginalNameEntryState state;state.profileKind1192=std::uint8_t(kind);initializeOriginalNameEntry(state,glyphs,length);equal(m.read32(owner+528),state.length528,"import length");if(kind==2)for(unsigned n=0;n<5;++n){equal(m.read32(owner+480+n*8),state.glyphIds480[n],"import glyph");equal(m.read32(owner+484+n*8),state.keyboardIds484[n],"import keyboard");}
    }
    // Independent constructor proof: no native selector fields seed this RAM.
    // Real126C20 import/branch code constructs real09BD80/09B9E0 selectors;
    // only allocator, TLS and the empty base-object constructor are bounded.
    constexpr unsigned tls=owner+0x8000;
    for(unsigned kind=0;kind<3;++kind)for(unsigned length=0;length<6;++length){
        scenario="independent selector Init kind"+std::to_string(kind)+" length"+std::to_string(length);m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);m.zeroRegion(profile,0x1000);
        m.write32(stack+300,owner);m.write32(stack+452,profile+1148);m.write32(stack+448,tls);m.write32(stack+456,tls+4);m.write32(stack+476,stack+64);m.write32(tls+4,tls+256);m.write32(profile+76,length);m.write8(profile+1192,std::uint8_t(kind));
        constexpr std::array<unsigned,5> sega{180,166,168,162,220};for(unsigned i=0;i<5;++i)m.write32(profile+44+i*4,sega[i]);
        RefCpu r(m);r.r[14]=stack;r.r[15]=stack+0xf000;unsigned allocation=owner+0x4000;
        r.callHooks[0x0c221fc0]=[&](RefCpu&v){v.r[0]=tls;};r.callHooks[0x0c0219a0]=[](RefCpu&){};r.callHooks[0x0c021960]=[&](RefCpu&v){v.r[0]=allocation;allocation+=(v.r[5]+15)&~15u;if(allocation>=tls)throw std::runtime_error("Isolated Init allocation bound");};
        instructions+=r.run(0x0c126e76,0x0c127124,30000);const auto sourceOuter=m.read32(owner+464),sourceInner=m.read32(sourceOuter+4);
        OriginalNameEntryState native;native.profileKind1192=std::uint8_t(kind);initializeOriginalNameEntry(native,sega,length);
        equal(m.read32(sourceInner+4),53,"constructed count");equal(m.read32(sourceInner+8),native.selector.selected,"constructed selected");equal(m.read32(sourceInner+36),native.selector.phase,"constructed phase");equal(m.read32(sourceInner+40),native.selector.minimum,"constructed minimum");equal(m.read32(sourceInner+44),native.selector.maximum,"constructed maximum");equal(m.read8(sourceInner+48),native.selector.wrap,"constructed wrap");equal(m.read32(sourceOuter+24),native.selector.selected,"constructed outer selection");
        equal(m.read32(sourceOuter+12),std::bit_cast<unsigned>(native.selector.progress),"constructed progress");equal(m.read32(sourceInner+24),std::bit_cast<unsigned>(native.selector.velocity),"constructed velocity");equal(m.read32(owner+528),native.length528,"constructed name length");
    }
}
}
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("usage: original_name_entry_tests ROOT CANONICAL_IMAGE");const auto tables=OriginalNameEntryTables::load(argv[1]);RefMemory m(argv[2]);
    initReference(m);OriginalNameEntryState s;initializeOriginalNameEntry(s);
    scenario="fade and timeout";for(unsigned n=0;n<5000&&!s.parentEvent64;++n)tick(m,s,{},tables);equal(s.parentEvent64,8,"completion");equal(s.length528,4,"default length");
    for(unsigned kind=0;kind<3;++kind)for(unsigned flags:{0u,1u,2u,3u,0x10002u}){s={};s.profileKind1192=std::uint8_t(kind);s.profileFlags1180=flags;initializeOriginalNameEntry(s,{162,163,220,220,220},2);s.phase588=3;s.fade576=0;scenario="route";for(unsigned n=0;n<22;++n)tick(m,s,{},tables);}
    for(unsigned page=0;page<3;++page)for(unsigned tile=0;tile<53;++tile)for(unsigned length=0;length<6;++length){s={};initializeOriginalNameEntry(s);s.phase588=1;s.page600=page;s.length528=length;s.glyphIds480={162,163,164,165,166};s.keyboardIds484={122,123,124,125,126};select(s,tile);scenario="tile "+std::to_string(tile)+" page "+std::to_string(page)+" length "+std::to_string(length);tick(m,s,{0,true,false,0},tables);}
    for(unsigned tile=0;tile<53;++tile)for(int row:{-1,1})for(bool confirm:{false,true}){s={};initializeOriginalNameEntry(s);s.phase588=1;select(s,tile);scenario="row "+std::to_string(tile)+":"+std::to_string(row);tick(m,s,{0,confirm,false,row},tables);tick(m,s,{},tables);}
    for(unsigned tile=0;tile<53;++tile)for(float axis:{-1.f,-.21f,-.2f,0.f,.2f,.21f,1.f})for(bool wrap:{false,true}){s={};initializeOriginalNameEntry(s);s.phase588=1;select(s,tile);s.length528=5;s.selector.minimum=wrap?0:59;s.selector.maximum=52;s.selector.wrap=wrap;scenario="source directional range "+std::to_string(tile);tick(m,s,{axis,false,false,0},tables);}
    for(unsigned length=0;length<6;++length){s={};s.profileKind1192=2;initializeOriginalNameEntry(s,{162,163,164,165,166},length);scenario="import first active tick";for(unsigned n=0;n<18;++n)tick(m,s,{},tables);tick(m,s,{0,false,true,-1},tables);for(unsigned n=0;n<30;++n)tick(m,s,{1,false,false,0},tables);}
    // A short imported name is not the source general-purpose rename route.
    s={};s.profileKind1192=2;initializeOriginalNameEntry(s,{180,166,168,162,220},4);scenario="imported SEGA locked END";for(unsigned n=0;n<17;++n)tick(m,s,{},tables);for(unsigned n=0;n<80;++n)tick(m,s,{-1,false,false,0},tables);equal(s.selected460,52,"Source short import stays END");
    tick(m,s,{0,false,true,0},tables);equal(s.length528,3,"Source short import backspace");equal(s.selector.minimum,59,"Short delete preserves imported range");equal(s.selector.wrap,false,"Short delete preserves imported lock");
    for(unsigned n=0;n<10;++n)tick(m,s,{-1,false,false,0},tables);equal(s.selected460,52,"Wheel still cannot leave END");
    tick(m,s,{0,false,false,1},tables);equal(s.selector.selected,16,"Source digital row escape");tick(m,s,{1,false,false,0},tables);equal(s.selected460,17,"Source positive step after row escape");tick(m,s,{0,true,false,0},tables);equal(s.length528,4,"Source append after row escape");equal(s.glyphIds480[3],179,"Source appended R");
    s={};s.profileKind1192=2;initializeOriginalNameEntry(s,{162,163,164,165,166},5);scenario="five-letter import delete restores range";for(unsigned n=0;n<17;++n)tick(m,s,{},tables);tick(m,s,{0,false,true,0},tables);equal(s.length528,4,"Full import delete length");equal(s.selector.minimum,0,"Full import delete minimum");equal(s.selector.maximum,52,"Full import delete maximum");equal(s.selector.wrap,true,"Full import delete wrap");
    std::mt19937 rng(0x1281e0);for(unsigned run=0;run<30;++run){s={};initializeOriginalNameEntry(s);s.phase588=1;scenario="random "+std::to_string(run);for(unsigned n=0;n<600&&!s.parentEvent64;++n){const auto r=rng();OriginalNameEntryInput input;input.steering=float(int(r%201)-100)/100;input.confirmPressed=r%71==0;input.backPressed=r%43==0;input.rowJump=r%79==0?-1:r%83==0?1:0;scenario="random "+std::to_string(run)+" tick "+std::to_string(n)+" selected "+std::to_string(s.selected460)+" inner "+std::to_string(s.selector.selected)+" length "+std::to_string(s.length528)+" steer "+std::to_string(input.steering)+" row "+std::to_string(input.rowJump);tick(m,s,input,tables);}}
    for(int status:{-1,0,9,10,16}){s={};initializeOriginalNameEntry(s);s.phase588=1;s.selectedCardState96=status;select(s,52);scenario="card state "+std::to_string(status);tick(m,s,{0,true,false,0},tables);}
    // Each bounded source substitution that fits the actual five-glyph field.
    unsigned replacements=0;for(const auto&[from,to]:tables.substitutions){if(from.size()>10)continue;s={};initializeOriginalNameEntry(s);s.phase588=1;s.length528=unsigned(from.size()/2);bool valid=true;
        for(unsigned n=0;n<s.length528;++n){unsigned id=0;for(;id<221;++id)if(tables.glyphs[id].bytes[0]==std::uint8_t(from[2*n])&&tables.glyphs[id].bytes[1]==std::uint8_t(from[2*n+1]))break;if(id==221){valid=false;break;}s.glyphIds480[n]=id;}
        if(!valid)continue;select(s,52);scenario="substitution "+std::to_string(replacements++);tick(m,s,{0,true,false,0},tables);
    }
    std::cout<<"PASS "<<checks<<" comparisons / "<<ticks<<" source ticks / "<<instructions<<" bounded SH4 instructions / "<<replacements<<" source substitutions\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
