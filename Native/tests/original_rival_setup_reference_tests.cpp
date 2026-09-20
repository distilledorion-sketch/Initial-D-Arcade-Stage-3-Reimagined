#include "original_rival_setup.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned stack=0x0cfff000,stop=0x0f000000,actorBase=0x0c901c6c;
constexpr unsigned profile=0x0cd00000,course=0x0cd01000,paths=0x0cd02000,authoring=0x0cd03000;
constexpr unsigned primaryIn=0x0cd10000,alternateIn=0x0cd20000,primaryOut=0x0cd30000,alternateOut=0x0cd40000;
struct Checks {
    std::size_t count=0,steps=0,cases=0,hooks=0;
    void equal(std::uint32_t a,std::uint32_t b,const std::string& label){++count;if(a!=b)throw std::runtime_error(label+" actual="+hex(a)+" expected="+hex(b));}
};
void reset(RefMemory& m){m.clear();m.zeroRegion(0x0c8ff000,0x1c0000);m.zeroRegion(0x0cd00000,0x10000);m.zeroRegion(0x0cff0000,0x10000);}
void putString(RefMemory& m,unsigned p,const std::string& s){for(unsigned i=0;i<s.size();++i)m.write8(p+i,std::uint8_t(s[i]));m.write8(p+unsigned(s.size()),0);}
std::string stringAt(const RefMemory& m,unsigned p){std::string s;for(unsigned i=0;i<1024;++i){auto v=m.read8(p+i);if(!v)return s;s+=char(v);}throw std::runtime_error("Invalid reference string");}
void seedPaths(RefMemory& m,unsigned direction){m.write32(course+28,paths);m.write32(course+24,authoring);m.write32(course+32,0x0cb01000);m.write32(paths+8,primaryIn);m.write32(paths+12,alternateIn);m.write32(paths+16,primaryOut);m.write32(paths+20,alternateOut);m.write32(paths+24,direction);}
void disable(RefMemory& m,Checks& c,std::mt19937& random){
    for(unsigned trial=0;trial<32;++trial){reset(m);std::array<OriginalRivalState,8> actors;for(unsigned a=0;a<8;++a)for(unsigned w=0;w<179;++w){actors[a].words[w]=random();m.write32(actorBase+a*716+w*4,actors[a].words[w]);}
        auto frame=random();m.write32(0x0caa986c,frame);RefCpu cpu(m);cpu.r[15]=stack;cpu.pr=stop;c.steps+=cpu.run(0x0c15adc0,stop);
        disableOriginalRivals(actors,frame);for(unsigned a=0;a<8;++a)for(unsigned w=0;w<179;++w)c.equal(actors[a].words[w],m.read32(actorBase+a*716+w*4),"sparse reset");c.equal(frame,m.read32(0x0caa986c),"frame reset");++c.cases;
    }
}
void loaderAndDirection(RefMemory& m,Checks& c){
    reset(m);putString(m,profile,"/binary/PATH_tu");std::vector<std::string> requests;RefCpu cpu(m);cpu.r[4]=paths;cpu.r[5]=profile;cpu.r[6]=0;cpu.r[15]=stack;cpu.pr=stop;
    cpu.callHooks[0x0c221fc0]=[&](RefCpu& x){++c.hooks;x.r[0]=profile+0x800;};
    cpu.callHooks[0x0c226980]=[&](RefCpu& x){++c.hooks;auto format=stringAt(m,x.r[5]);auto at=format.find("%d");if(at==std::string::npos)throw std::runtime_error("Unexpected loader format");format.replace(at,2,std::to_string(x.r[6]));putString(m,x.r[4],format);x.r[0]=unsigned(format.size());};
    cpu.callHooks[0x0c226aa0]=[&](RefCpu& x){++c.hooks;putString(m,x.r[4],stringAt(m,x.r[5]));x.r[0]=x.r[4];};
    cpu.callHooks[0x0c226a00]=[&](RefCpu& x){++c.hooks;putString(m,x.r[4],stringAt(m,x.r[4])+stringAt(m,x.r[5]));x.r[0]=x.r[4];};
    cpu.callHooks[0x0c04e480]=[&](RefCpu& x){++c.hooks;c.equal(x.r[5],0,"sync fileload argument");requests.push_back(stringAt(m,x.r[4]));x.r[0]=0x0cd10000+unsigned(requests.size()-1)*0x10000;};
    c.steps+=cpu.run(0x0c046f40,stop,5000);const std::vector<std::string> expected{"/binary/PATH_tui_0.bin","/binary/PATH_tuo_0.bin","/binary/PATH_tui_1.bin","/binary/PATH_tuo_1.bin"};if(requests!=expected)throw std::runtime_error("Original PATH construction mismatch");
    c.equal(m.read32(paths+8),0x0cd10000,"i_0 slot");c.equal(m.read32(paths+16),0x0cd20000,"o_0 slot");c.equal(m.read32(paths+12),0x0cd30000,"i_1 slot");c.equal(m.read32(paths+20),0x0cd40000,"o_1 slot");c.equal(m.read8(paths+28),1,"loader completion");++c.cases;
    for(unsigned direction=0;direction<2;++direction){reset(m);seedPaths(m,7);const unsigned frame=0x0cffe000;m.write32(frame+2508,0);m.write32(frame+2500,direction);m.write32(frame+2516,course);m.write32(frame+2504,1);m.write32(frame+2732,1);
        RefCpu x(m);x.r[14]=frame;x.r[15]=stack;x.pr=stop;c.steps+=x.run(0x0c0437a8,0x0c0437cc);c.equal(m.read32(paths+24),direction,"PATH direction");c.equal(m.read32(authoring+28),direction,"authoring direction");c.equal(m.read32(course+48),direction,"course direction");c.equal(m.read32(course+52),1,"time");c.equal(m.read32(course+56),1,"weather");++c.cases;
    }
}
void callerPolicy(RefMemory& m,Checks& c){
    for(unsigned mode=0;mode<5;++mode)for(unsigned condition=0;condition<18;++condition){reset(m);const unsigned race=profile,frame=0x0cffe000;seedPaths(m,condition&1);m.write32(race+1036,course);m.write32(race+1640,mode);m.write32(race+1652,condition/2);m.write32(race+1660,condition&1);m.write32(0x0c31c99c+16,0x12345678);m.write32(0x0c31c99c+20,3);
        unsigned calls=0;RefCpu x(m);x.r[13]=race;x.r[14]=frame;x.r[15]=stack;x.pr=stop;x.callHooks[0x0c159720]=[&](RefCpu& q){++calls;++c.hooks;c.equal(q.r[4],condition,"caller condition");c.equal(q.r[5],0x0cb01000,"collision pointer");c.equal(q.r[6],condition&1?primaryOut:primaryIn,"primary PATH");c.equal(q.r[7],condition&1?alternateOut:alternateIn,"alternate PATH");c.equal(m.read32(q.r[15]+12),mode==0?3:mode==1?0xfffffffe:0xffffffff,"enable control");c.equal(m.read32(q.r[15]+4),frame,"player position pointer");c.equal(m.read32(q.r[15]+8),frame+84,"player angles pointer");c.equal(m.read32(q.r[15]+16),frame+28,"rival position pointer");c.equal(m.read32(q.r[15]+20),frame+84,"rival angles pointer");};
        c.steps+=x.run(0x0c062a44,0x0c062c20,5000);c.equal(calls,mode==4?0:1,"numeric mode caller");++c.cases;
    }
}
void setup(RefMemory& m,Checks& c){
    for(auto control:{-3,-2,-1,0,1,9})for(unsigned condition:{0u,6u,14u,17u})for(unsigned variant=0;variant<3;++variant){reset(m);std::vector<unsigned> order;const auto car=variant==0?0u:34u;
        m.write32(profile,variant);m.write32(profile+16,car);m.write32(profile+20,7);m.write32(profile+24,26);m.write32(profile+32,variant&1);m.write32(profile+148,8);m.write8(profile+142,11);m.write8(profile+164,6);m.write8(profile+152,2);m.write32(0x0c4004dc,variant==0?0xffffffff:variant==1?5:12);m.write32(0x0c4004d8,0x3456);
        for(unsigned a=0;a<8;++a)for(unsigned w=0;w<179;++w)m.write32(actorBase+a*716+w*4,0xa0000000+a*256+w);
        for(unsigned w=0;w<42;++w){m.write32(0x0c9008a4+w*4,0x10000000+w);m.write32(0x0c9017d4+w*4,0x20000000+w);m.write32(0x0c8ff430+w*4,0x30000000+w);}
        for(unsigned w=0;w<272;++w)m.write32(0x0c900f00+w*4,0x40000000+w);
        m.write32(stack+4,profile+0x500);m.write32(stack+8,profile+0x600);m.write32(stack+12,std::uint32_t(control));m.write32(stack+16,profile+0x700);m.write32(stack+20,profile+0x800);
        RefCpu x(m);x.r[4]=condition;x.r[5]=0x0cb01000;x.r[6]=primaryIn;x.r[7]=alternateIn;x.r[15]=stack;x.pr=stop;
        x.callHooks[0x0c133240]=[&](RefCpu& q){++c.hooks;q.r[0]=profile;};
        x.callHooks[0x0c1595c0]=[&](RefCpu& q){++c.hooks;order.push_back(0x1595c0);c.equal(q.r[4],profile+0x500,"player init position");c.equal(q.r[5],profile+0x600,"player init angles");c.equal(m.read32(actorBase),0xa0000000,"disable occurs after player init");};
        x.callHooks[0x0c15ae00]=[&](RefCpu& q){++c.hooks;order.push_back(0x15ae00);c.equal(q.r[4],profile+0x700,"rival init position");c.equal(q.r[5],profile+0x800,"rival init angles");c.equal(q.r[6],primaryIn,"rival init primary");c.equal(q.r[7],alternateIn,"rival init alternate");for(unsigned a=0;a<8;++a)c.equal(m.read32(actorBase+a*716),0,"disable before rival init");for(unsigned i=0;i<3;++i)c.equal(m.read32(q.r[15]+i*4),1,"original extra arguments");m.write32(actorBase+716,1);};
        c.steps+=x.run(0x0c159720,stop,10000);c.equal(unsigned(order.size()),control>=0?2:1,"setup leaf order");c.equal(m.read32(0x0c9015e4),control==-2,"secondary publication mask");c.equal(m.read32(0x0c9015f8),7,"profile opponent geometry");c.equal(m.read32(0x0c901654),car,"selected player car");c.equal(m.read32(0x0c9015f4),car==0,"AE86 override");c.equal(m.read32(0x0c901644),11,"profile-specific progress");c.equal(m.read32(0x0c9015d8),variant==0?0:variant==1?5:10,"clamped difficulty");c.equal(m.read32(0x0c901640),0x0cb01000,"shared collision pointer");c.equal(m.read32(0x0c9015c0),condition>15,"snow flag");
        for(unsigned a=0;a<8;++a)for(unsigned w=0;w<179;++w)c.equal(m.read32(actorBase+a*716+w*4),w?0xa0000000+a*256+w:unsigned(a==1&&control>=0),"preserved rival bank");
        for(unsigned w=0;w<42;++w){c.equal(m.read32(0x0c8ff388+w*4),0x10000000+w,"player publication");c.equal(m.read32(0x0c8ff430+w*4),(control==-2?0x30000000:0x20000000)+w,"secondary publication");}
        for(unsigned w=0;w<272;++w)c.equal(m.read32(0x0c9009f0+w*4),w>=89&&w<=92?0:0x40000000+w,"backup initialization");++c.cases;
    }
}
void frameOrder(RefMemory& m,Checks& c){
    for(unsigned special=0;special<2;++special)for(unsigned missing=0;missing<=4;++missing){reset(m);std::vector<unsigned> order;m.write32(0x0c9015e4,special);m.write32(0x0c4004d8,2);m.write32(0x0c900954,0x0c9008a4);
        for(unsigned w=0;w<42;++w){m.write32(0x0c8ff388+w*4,0x10000000+w);m.write32(0x0c8ff430+w*4,0x20000000+w);m.write32(0x0c8ff580+w*4,0x50000000+w);}
        for(unsigned w=0;w<272;++w)m.write32(0x0c9009f0+w*4,0x60000000+w);for(unsigned i=0;i<4;++i)m.write32(0x0c9009f0+356+i*4,0x10);
        RefCpu x(m);x.r[15]=stack;x.pr=stop;x.setFloat(4,1.f);
        x.callHooks[0x0c157880]=[&](RefCpu&){++c.hooks;order.push_back(0x157880);c.equal(m.read32(0x0c8ff388),0x10000000,"pair uses previous player");c.equal(m.read32(0x0c8ff430),0x20000000,"pair uses previous rival");};
        x.callHooks[0x0c157ae0]=[&](RefCpu&){++c.hooks;order.push_back(0x157ae0);for(unsigned w=0;w<42;++w)m.write32(0x0c9008a4+w*4,0x30000000+w);for(unsigned w=0;w<272;++w)m.write32(0x0c900f00+w*4,0x70000000+w);for(unsigned i=0;i<4;++i)m.write32(0x0c900f00+356+i*4,i<missing?0:0x10);m.write32(0x0c99aa94,0x1111);};
        x.callHooks[0x0c15b0a0]=[&](RefCpu& q){++c.hooks;order.push_back(0x15b0a0);c.equal(q.r[4],1,"rival actor slot");c.equal(q.r[5],1,"rival public slot");c.equal(m.read32(0x0c99aa94),0x1111,"shared scratch follows player");for(unsigned w=0;w<42;++w)m.write32(0x0c9017d4+w*4,0x40000000+w);m.write32(0x0c99aa94,0x2222);};
        c.steps+=x.run(0x0c159920,stop,10000);if(order!=std::vector<unsigned>{0x157880,0x157ae0,0x15b0a0})throw std::runtime_error("Original frame call order");c.equal(m.read32(0x0c901650),0x3f800000,"frame parameter");c.equal(m.read32(0x0c99aa94),0x2222,"rival owns final shared scratch");
        for(unsigned w=0;w<42;++w){c.equal(m.read32(0x0c8ff388+w*4),(missing>2?0x50000000:0x30000000)+w,"player recovery after publish");c.equal(m.read32(0x0c8ff430+w*4),(special?0x20000000:0x40000000)+w,"rival never recovered");}++c.cases;
    }
}
void pathsMatchSource(RefMemory& m,Checks& c,const std::filesystem::path& root,const std::filesystem::path& hostfs){
    auto data=OriginalRivalData::load(root/"original_rival");unsigned alternatives=0;
    for(unsigned condition=0;condition<18;++condition)for(bool alternate:{false,true}){auto stem=std::filesystem::path(stringAt(m,0x0c2eff40+condition*1024+448)).filename().string();auto source=hostfs/"binary"/(stem+(condition&1?"o":"i")+(alternate?"_1.bin":"_0.bin"));std::ifstream f(source,std::ios::binary);if(!f)throw std::runtime_error("Missing source PATH fixture");std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(f),{});
        if(bytes.empty()){bool rejected=false;try{data.loadPath(root/"original_rival",condition,alternate);}catch(const std::runtime_error&){rejected=true;}c.equal(rejected,true,"empty original alternate rejected");continue;}
        auto path=data.loadPath(root/"original_rival",condition,alternate);c.equal(unsigned(path.points.size()),unsigned(bytes.size()/12),"full PATH capacity");for(unsigned i=0;i<path.points.size();++i)for(unsigned k=0;k<3;++k){unsigned word;std::memcpy(&word,bytes.data()+i*12+k*4,4);c.equal(std::bit_cast<unsigned>(path.points[i][k]),word,"source PATH bits");}if(alternate)++alternatives;++c.cases;
    }c.equal(alternatives,2,"two authored alternate paths");
}
}
int main(int argc,char** argv){try{if(argc!=4)throw std::runtime_error("canonical-image data-directory HOSTFS required");RefMemory m(argv[1]);Checks checks;std::mt19937 random(0x159720);disable(m,checks,random);loaderAndDirection(m,checks);callerPolicy(m,checks);setup(m,checks);frameOrder(m,checks);m.clear();pathsMatchSource(m,checks,argv[2],argv[3]);std::cout<<"PASS rival setup contract: "<<checks.cases<<" cases, "<<checks.count<<" exact comparisons, "<<checks.steps<<" original instructions; "<<checks.hooks<<" explicitly controlled loader/setup/update leaf calls (no numerical solver claims).\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
