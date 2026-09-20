#include "original_car_color_selection.h"
#include "original_car_color_catalog.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,child=0x0cd01000,timer=0x0cd02000,
    selector=0x0cd03000,widget=0x0cd04000,vtable=0x0cd05000,showroom=0x0cd06000,
    remembered=0x0cd08000,counts=0x0cd09000,recolor=0x0cd0a000,
    stack=0x0cfff000,stop=0x00ff0000,profile=0x0c31c99c;
std::uint64_t cases=0,checks=0,steps=0,hooks=0;
void equal(unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<why<<": "<<hex(a)<<" != "<<hex(b)<<'\n';throw std::runtime_error(why);}}
void run(RefCpu& c,unsigned start,unsigned end,unsigned budget=20000){try{steps+=c.run(start,end,budget);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m){
    m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+432,vtable);m.write16(vtable+48,0);m.write32(vtable+52,0x00ed0000);
    m.write32(owner+428,showroom);m.write32(owner+696,child);m.write32(child+412,timer);
    m.write32(child+416,selector);m.write32(child+420,widget);m.write32(owner+500,selector);
    m.write32(owner+676,remembered);m.write32(owner+680,counts);m.write32(profile+1176,2479);
    m.write8(0x0c92ed00,0);m.write8(0x0c92ed40,0);
}
std::vector<unsigned> carIds(RefMemory& m,unsigned raw){
    constexpr unsigned n[7]={7,9,4,5,6,3,1};std::vector<unsigned> ids;
    const auto p=m.read32(0x0c31d88c+4*raw);for(unsigned i=0;i<n[raw];++i)ids.push_back(m.read32(p+4*i));return ids;
}
std::vector<unsigned> colorCounts(RefMemory& m,unsigned raw){std::vector<unsigned> out;for(auto id:carIds(m,raw))out.push_back(m.read32(0x0c30ecd4+32*id));return out;}
void put(RefMemory& m,const OriginalCarColorSelection& s,unsigned raw){
    m.write32(owner+480,s.selected480);m.write32(owner+484,s.previous484);m.write32(owner+684,s.currentColor684);
    m.write32(profile+64,s.profileColor64);m.write32(owner+488,unsigned(s.colorCounts680.size()));m.write32(owner+492,raw);
    for(unsigned i=0;i<s.colorCounts680.size();++i){m.write32(counts+i*4,s.colorCounts680[i]);m.write32(remembered+i*4,s.rememberedColors676[i]);}
}
void compare(RefMemory& m,const OriginalCarColorSelection& s){
    equal(m.read32(owner+480),s.selected480,"selected index");equal(m.read32(owner+484),s.previous484,"previous index");
    equal(m.read32(owner+684),s.currentColor684,"current color");equal(m.read32(profile+64),s.profileColor64,"profile color");
    for(unsigned i=0;i<s.colorCounts680.size();++i){equal(m.read32(counts+i*4),s.colorCounts680[i],"color count");equal(m.read32(remembered+i*4),s.rememberedColors676[i],"remembered color");}
}
void initTests(RefMemory& m){
    for(unsigned raw=0;raw<7;++raw){const auto n=colorCounts(m,raw);
        for(unsigned selected=0;selected<n.size();++selected)for(unsigned color:{0u,1u,n[selected]-1,0xffffffffu}){
            seed(m);OriginalCarColorSelection s;initializeOriginalCarColorSelection(s,n,selected,color);put(m,s,raw);
            for(unsigned i=0;i<n.size();++i)m.write32(remembered+4*i,0x87654321);
            m.write32(owner+684,0x12345678);RefCpu c(m);c.r[14]=stack-512;c.r[15]=stack-512;c.r[0]=counts;
            m.write32(c.r[14]+220,owner+636);m.write32(c.r[14]+200,owner+444);m.write32(c.r[14]+144,owner);
            unsigned applied=0;
            c.callHooks[0x0c12e4a0]=[&](auto& x){++hooks;++applied;equal(x.r[4],owner,"init apply owner");equal(x.r[5],selected,"init apply index");equal(x.r[6],color,"init preserves profilecolor");};
            run(c,0x0c12e142,0x0c12e1ce);compare(m,s);equal(applied,1,"initial applies selected color");++cases;
        }
    }
}
void tick(RefMemory& m,unsigned raw,OriginalCarColorSelection& s,const OriginalCarColorSelectionInput& input,bool timeout=false){
    seed(m);put(m,s,raw);m.write32(owner+456,1);m.write8(owner+672,timeout?1:0);m.write8(0x0c92ed40,input.gearButtons92ED40);
    RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned applied=0,changed=0,committed=0;
    c.callHooks[0x0c0d43a0]=[](auto& x){++hooks;x.setFloat(0,0.f);};
    c.callHooks[0x0c0d4300]=[&](auto& x){++hooks;x.r[0]=x.r[5]==1&&input.confirmOrTimeout&&!timeout;};
    c.callHooks[0x0c1b39c0]=[&](auto& x){++hooks;x.r[0]=input.selectedLocalIndex;};
    c.callHooks[0x0c141f80]=[&](auto& x){++hooks;if(x.r[4]==2)++changed;if(x.r[4]==3)++committed;};
    unsigned appliedIndex=0,appliedColor=0;
    c.callHooks[0x0c12e4a0]=[&](auto& x){++hooks;++applied;equal(x.r[4],owner,"cycle apply owner");appliedIndex=x.r[5];appliedColor=x.r[6];};
    c.callHooks[0x0c0a94c0]=[](auto& x){++hooks;x.r[0]=showroom;};
    for(unsigned fn:{0x0c0a9da0u,0x0c0a9e40u,0x0c16db80u,0x0c1ba260u,0x0c1b5880u,0x0c1bbc60u,0x0c10f420u,
        0x0c1fcc60u,0x0c1fd060u,0x0c1f69d0u,0x0c1fbd60u,0x0c1f65c0u,0x00ed0000u})c.callHooks[fn]=[](auto&){++hooks;};
    c.callHooks[0x0c2223b8]=[](auto& x){++hooks;x.r[0]=std::uint32_t(signed32(x.r[4])/signed32(x.r[5]));x.fpul=x.r[0];};
    run(c,0x0c12e520,stop);auto effective=input;effective.confirmOrTimeout|=timeout;
    const auto out=stepOriginalCarColorSelection(s,effective);compare(m,s);equal(changed,out.selectionChanged,"car change event");
    equal(applied,out.showroomColorApplyRequested,"cycle applies model color");equal(committed,out.profileColorWritten,"confirm color event");
    if(applied){equal(appliedIndex,s.selected480,"apply selected index");equal(appliedColor,s.currentColor684,"apply current color");}
    equal(m.read32(widget+24),s.currentColor684,"real tail publishes color indicator");++cases;
}
void selectionTests(RefMemory& m){
    for(unsigned raw=0;raw<7;++raw){const auto n=colorCounts(m,raw);
        for(unsigned selected=0;selected<n.size();++selected)for(unsigned previous=0;previous<n.size();++previous)
        for(unsigned buttons:{0u,0x10u,0x20u,0x30u})for(unsigned color:{0u,n[selected]-1,n[selected],0xffffffffu,0x80000000u}){
            OriginalCarColorSelection s;initializeOriginalCarColorSelection(s,n,previous,color);
            for(unsigned j=0;j<n.size();++j)s.rememberedColors676[j]=j%n[j];
            tick(m,raw,s,{selected,std::uint8_t(buttons),bool(buttons&0x10)},bool(buttons==0x30));
        }
    }
    OriginalCarColorSelection s;const auto n=colorCounts(m,0);initializeOriginalCarColorSelection(s,n,0,2);
    tick(m,0,s,{1,0,false});tick(m,0,s,{0,0,false});equal(s.currentColor684,0,"source saved-color revisit resets tozero");
    tick(m,0,s,{0,0x20,false});tick(m,0,s,{1,0,false});tick(m,0,s,{0,0,true});equal(s.profileColor64,1,"cycled color remembered on revisit");
}
void indicatorTests(RefMemory& m){
    for(unsigned car=0;car<35;++car){
        const unsigned n=m.read32(0x0c30ecd4+32*car),p=m.read32(0x0c33b1a0+4*car);std::vector<unsigned> rgb;
        equal(n,originalCarColorCounts[car],"exported color count");
        for(unsigned i=0;i<n;++i){rgb.push_back((m.read8(p+36*i)<<16)|(m.read8(p+36*i+1)<<8)|m.read8(p+36*i+2));equal(rgb.back(),originalCarPaintRgb[car][i],"exported palette RGB");}
        for(unsigned selected=0;selected<=n;++selected){
            seed(m);m.write32(widget+20,car);m.write32(widget+24,selected);m.write32(widget+28,recolor);
            RefCpu c(m);c.r[4]=widget;c.r[15]=stack;c.pr=stop;std::vector<OriginalCarColorIndicatorDraw> draws;
            unsigned selectedSelector=0,recoloredObject=0,recoloredArgb=0;float x=0,y=0,z=0;bool translated=false;
            c.callHooks[0x0c1baf40]=[](auto& x){++hooks;x.r[0]=child;};
            c.callHooks[0x0c1bb6e0]=[](auto& x){++hooks;equal(x.r[5],13,"color bank13");x.r[0]=showroom;};
            c.callHooks[0x0c05a8e0]=[&](auto& cpu){++hooks;equal(cpu.r[4],showroom,"color bank");selectedSelector=cpu.r[5];cpu.r[0]=selectedSelector;};
            c.callHooks[0x0c1d5400]=[](auto&){++hooks;};
            c.callHooks[0x0c1b8540]=[&](auto& cpu){++hooks;equal(cpu.r[6],2,"swatch replacement mode2");recoloredObject=cpu.r[5];recoloredArgb=m.read32(recolor+4);};
            c.callHooks[0x0c1f6610]=[&](auto& cpu){++hooks;equal(cpu.r[4],0,"palette matrix push mode");x=y=z=0;translated=false;};
            c.callHooks[0x0c1f6ac0]=[&](auto& cpu){++hooks;x+=std::bit_cast<float>(cpu.fr[4]);y+=std::bit_cast<float>(cpu.fr[5]);z+=std::bit_cast<float>(cpu.fr[6]);translated=true;};
            c.callHooks[0x0c1f65c0]=[&](auto&){++hooks;x=y=z=0;translated=false;};
            c.callHooks[0x0c1d7120]=[&](auto& cpu){++hooks;const auto id=cpu.r[4];const bool replace=id==recoloredObject&&translated;
                draws.push_back({id,x,y,z,replace,replace?recoloredArgb:0});};
            run(c,0x0c1b4660,stop);const auto native=originalCarColorIndicatorDraws(car,selected,rgb);equal(unsigned(draws.size()),unsigned(native.size()),"palette draw count");
            for(unsigned i=0;i<draws.size();++i){const auto& a=draws[i];const auto& b=native[i];equal(a.selector,b.selector,"palette selector");
                equal(std::bit_cast<unsigned>(a.x),std::bit_cast<unsigned>(b.x),"palette x");equal(std::bit_cast<unsigned>(a.y),std::bit_cast<unsigned>(b.y),"palette y");
                equal(std::bit_cast<unsigned>(a.z),std::bit_cast<unsigned>(b.z),"palette z");equal(a.replaceColors,b.replaceColors,"palette material override");equal(a.argb,b.argb,"palette exact RGB");}
            ++cases;
        }
    }
}
void materialTests(RefMemory& m){
    // Actual1B8540 mode2, with only its geometry iterator as a boundary.
    // Type1 is GMP andtype2 ICH. Neither params nor vertex data is rewritten.
    constexpr unsigned gmp=owner+0xb000,ich=owner+0xc000;
    for(unsigned argb:{0u,0xffffffffu,0xff112233u,0xffe6e6e6u,0xff000000u}){
        seed(m);m.write32(recolor+4,argb);
        std::array<unsigned,16> originalMaterial{};std::array<unsigned,8> originalIch{};
        for(unsigned i=0;i<16;++i){originalMaterial[i]=0x12340000u+i;m.write32(gmp+4*i,originalMaterial[i]);}
        for(unsigned i=0;i<8;++i){originalIch[i]=0x98760000u+i;m.write32(ich+4*i,originalIch[i]);}
        RefCpu c(m);c.r[4]=recolor;c.r[5]=showroom;c.r[6]=2;c.r[15]=stack;c.pr=stop;unsigned cursor=0;
        c.callHooks[0x0c1d4be0]=[](auto&){++hooks;};
        c.callHooks[0x0c1d4c40]=[&](auto& x){++hooks;m.write32(x.r[5],cursor==0?1:cursor==1?2:3);x.r[0]=cursor==0?gmp:ich;++cursor;};
        run(c,0x0c1b8540,stop);equal(cursor,3,"material iterator visits");
        for(unsigned i=0;i<16;++i)equal(m.read32(gmp+4*i),i==3?argb:originalMaterial[i],"mode2 GMP diffuse-only write");
        for(unsigned i=0;i<8;++i)equal(m.read32(ich+4*i),originalIch[i],"mode2 preserves ICH");++cases;
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical original image required");RefMemory m(argv[1]);initTests(m);selectionTests(m);indicatorTests(m);materialTests(m);
    std::cout<<"PASS original Car color selection/indicator: "<<cases<<" cases, "<<checks<<" comparisons, "<<steps<<" actual instructions, "<<hooks
        <<" explicit input/showroom/matrix/draw hooks; color arithmetic/profile/RGB tables execute original bytes.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

