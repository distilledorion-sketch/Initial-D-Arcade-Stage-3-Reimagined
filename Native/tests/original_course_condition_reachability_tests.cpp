#include "original_battle_profile.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <set>
#include <tuple>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned profile=0x0c31c99c,owner=0x0d000000,frame=0x0d008000,
    stack=0x0d020000,tls=0x0d030000,race=0x0d040000,courseObject=0x0d050000,stop=0x00ff0000;
using Condition=std::tuple<unsigned,unsigned,unsigned,unsigned>;
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("canonical image required");
    RefMemory m(argv[1]);std::uint64_t checks=0,instructions=0;
    auto check=[&](bool b,const char* text){++checks;if(!b)throw std::runtime_error(text);};
    auto run=[&](RefCpu& c,unsigned start,unsigned end=stop){try{instructions+=c.run(start,end,10000);}catch(...){std::cerr<<"source entry "<<std::hex<<start<<" at "<<c.pc<<std::dec<<'\n';throw;}};
    auto initialize=[&](){m.clear();m.zeroRegion(owner,0x60000);};
    auto put=[&](const OriginalBattleProfile&p){for(unsigned i=0;i<p.words.size();++i)m.write32(profile+4*i,p.words[i]);};
    auto get=[&](){OriginalBattleProfile p;for(unsigned i=0;i<p.words.size();++i)p.words[i]=m.read32(profile+4*i);return p;};
    auto hooks=[&](RefCpu& c){
        c.callHooks[0x0c221fc0]=[](auto&q){q.r[0]=tls;};
        for(unsigned entry:{0x0c141f80u,0x0c055d60u,0x0c1140a0u,0x0c16db80u})c.callHooks[entry]=[](auto&){};
        // Screen timer-accounting boundaries do not choose a condition.
        for(unsigned entry:{0x0c192960u,0x0c192980u,0x0c1929a0u,0x0c1346a0u})c.callHooks[entry]=[](auto&q){q.r[0]=0;};
    };
    auto call=[&](unsigned entry,unsigned a,unsigned b){RefCpu c(m);c.r[4]=a;c.r[5]=b;c.r[15]=stack;c.pr=stop;hooks(c);run(c,entry);return c.r[0];};
    initialize();check(call(0x0c134760,0,0)==9,"Time Attack has nine normal course choices");
    std::array<unsigned,9> order{};std::set<unsigned> unique;
    for(unsigned i=0;i<9;++i){order[i]=call(0x0c134780,i,0);unique.insert(order[i]);}
    check(unique==std::set<unsigned>{0,1,2,3,4,5,6,7,8},"Normal TA course map covers exactly nine authored courses");

    // Each generic route/weather/time selector has two entries regardless of
    // course. Execute its actual constructor-call preparation, stopping at the
    // explicit UI allocation boundary (no invented selector policy).
    for(unsigned course=0;course<9;++course)for(unsigned entry:{0x0c137e64u,0x0c138104u,0x0c138364u}){
        initialize();m.write32(profile+4,course);m.write32(frame+48,owner);m.write32(frame+64,owner+1024);
        m.write32(owner+464,1);RefCpu c(m);c.r[14]=frame;c.r[15]=stack;c.r[0]=frame+128;unsigned calls=0;
        c.callHooks[0x0c1b3800]=[&](auto&q){check(q.r[5]==2&&q.r[6]==1,"Original condition selector count and selected entry");++calls;};
        run(c,entry,entry==0x0c137e64?0x0c137e7e:entry==0x0c138104?0x0c13811e:0x0c13837e);
        check(calls==1,"Condition selector construction boundary reached once");
    }
    // Full Weather/Time Init bodies, including the actual24-byte selector
    // constructor and actual steering update, close the disabled-option
    // question independently from later profile commits. Only TLS and the
    // preallocated storage service are substituted.
    for(unsigned course=0;course<9;++course)for(unsigned entry:{0x0c1382a0u,0x0c138040u}){
        initialize();auto p=makeOriginalFreshBattleProfile();p.setu(4,course);put(p);
        m.write8(owner+476,1);m.write32(tls+4,tls+256);m.write32(tls+260,0);
        RefCpu c(m);c.r[4]=owner;c.r[5]=0;c.r[15]=stack;c.pr=stop;hooks(c);
        c.callHooks[0x0c021960]=[&](auto&q){check(q.r[5]==24,"Original binary selector storage size");q.r[0]=owner+1024;};
        c.callHooks[0x0c0219a0]=[](auto&q){q.r[0]=q.r[4];};run(c,entry);
        const auto selector=m.read32(owner+488);
        check(selector==owner+1024&&m.read32(selector+8)==2&&m.read32(selector+16)==0,"Full original Init creates binary clamped selector");
        bool sawZero=false,sawOne=false;
        // Find both actual steering-table branches within the normalized
        // input interval; no selected-index override or selector hook.
        for(float input:{-1.f,0.f,1.f}){
            m.write32(selector+12,0);RefCpu steer(m);steer.r[4]=selector;steer.r[15]=stack;steer.pr=stop;steer.setFloat(4,input);run(steer,0x0c1b39c0);
            check(steer.r[0]<2,"Original steering keeps binary choice in bounds");sawZero|=steer.r[0]==0;sawOne|=steer.r[0]==1;
        }
        check(sawZero&&sawOne,"Both weather/time choices reachable through actual selector input");
    }

    auto confirm=[&](unsigned entry,unsigned selection){
        m.write8(owner+476,0);m.write32(owner+464,selection);m.write32(owner+452,1);
        call(entry,owner,1);check(m.read32(owner+452)==2,"Source confirm enters wait phase");
    };
    auto next=[&](unsigned stage){
        m.write32(owner+436,31);m.write32(owner+460,30);m.write32(owner+468,stage);
        RefCpu c(m);c.r[11]=owner;c.r[15]=stack;hooks(c);run(c,0x0c138740,0x0c138912);
        check(m.read32(owner+468)==stage+1,"Source accepted stage advances once");
        return m.read32(owner+452)==3;
    };
    std::set<Condition> ta,legend,bunta;
    for(unsigned menu=0;menu<9;++menu)for(unsigned direction=0;direction<2;++direction)
    for(unsigned requestedNight=0;requestedNight<2;++requestedNight)for(unsigned requestedWet=0;requestedWet<2;++requestedWet){
        initialize();auto p=makeOriginalFreshBattleProfile();p.setu(0,1);p.setu(4,9);put(p);
        confirm(0x0c137c40,menu);check(m.read32(profile+4)==order[menu],"Source course confirmation commits original map");
        check(!next(0),"No normal course skips its route selection");
        confirm(0x0c137da0,direction);check(m.read32(profile+12)==direction,"Source route commit");
        bool finished=next(1);
        if(!finished){confirm(0x0c1382a0,requestedWet);check(m.read32(profile+32)==requestedWet,"Source weather commit");finished=next(2);}
        if(!finished){confirm(0x0c138040,requestedNight);check(m.read32(profile+8)==requestedNight,"Source time commit");finished=next(3);}
        check(finished,"TA condition sequence reaches source exit");
        const unsigned course=order[menu],night=course==4||course==8?1:requestedNight,wet=course==8?1:requestedWet;
        check(m.read32(profile+8)==night&&m.read32(profile+32)==wet,"Only Happo/Snow force conditions");
        // Final exit computes the scene with original133DC0; no remap on wet.
        m.write32(owner+440,15);RefCpu exit(m);exit.r[11]=owner;exit.r[15]=stack;hooks(exit);run(exit,0x0c138840,0x0c138912);
        check(m.read32(profile+28)==course*2+night,"TA scene selector matches course/time independently of weather");
        ta.emplace(course,direction,night,wet);
    }
    check(ta.size()==62,"TA has62 normal directed conditions,31 environment rows");
    check(ta.contains({5,0,1,1})&&ta.contains({5,1,1,1}),"Iro night/wet is reachable in both TA directions");

    // All31 authored opponents and all256 result-nibble states. This includes
    // unlocked opponents/rematches; it is not a claim all are fresh-profile choices.
    for(unsigned enemy=0;enemy<31;++enemy)for(unsigned progress=0;progress<256;++progress){
        initialize();auto p=makeOriginalFreshBattleProfile();p.setu(0,0);p.setByte(116+enemy,std::uint8_t(progress));put(p);
        call(0x0c133ca0,enemy,0);selectOriginalRival(p,enemy);const auto actual=get();
        check(actual.words==p.words,"Source Legend complete profile selection");
        legend.emplace(actual.u(4),actual.u(12),actual.u(8),actual.u(32));
    }
    check(legend.contains({5,0,1,1}),"Iro night/wet reachable in Legend rematches");
    for(unsigned enemy:{15u,16u}){
        initialize();put(makeOriginalFreshBattleProfile());call(0x0c133ca0,enemy,0);
        check(m.read32(profile+4)==5&&m.read32(profile+8)==1&&m.read32(profile+32)==0,"Fresh Iro night opponent starts dry");
        call(0x0c1343a0,enemy,0);check(m.read8(profile+116+enemy)==0x10,"Actual first Legend win produces rematch state");
        call(0x0c133ca0,enemy,0);
        check(m.read32(profile+4)==5&&m.read32(profile+8)==1&&m.read32(profile+32)==1,"Actual first win then reselect reaches Iro night/wet");
    }
    check(call(0x0c1347a0,0,0)==8,"Bunta has eight course slots");
    for(unsigned menu=0;menu<8;++menu)for(unsigned level=0;level<=31;++level){
        initialize();auto p=makeOriginalFreshBattleProfile();p.setu(0,2);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,level);put(p);
        const auto course=call(0x0c1347c0,menu,level);
        RefCpu c(m);c.r[13]=course;c.r[9]=owner+444;c.r[10]=1;c.r[14]=frame;c.r[15]=frame;hooks(c);
        run(c,0x0c184374,0x0c1845e6);selectOriginalBuntaCourse(p,menu);const auto actual=get();
        check(actual.words==p.words,"Source Bunta complete profile selection");
        check(actual.u(8)==1&&actual.u(32)==unsigned(course==8),"Bunta fixed night, wet only Snow");
        bunta.emplace(actual.u(4),actual.u(12),actual.u(8),actual.u(32));
    }
    check(bunta.size()==9&&!bunta.contains({5,0,1,1}),"Bunta nine fixed directed conditions including Snow substitution");

    // ARace062480 propagates normal live profile inputs unchanged. The special
    // replay/resource mode1 is deliberately outside these normal mode0/2 cases.
    for(unsigned course=0;course<9;++course)for(unsigned direction=0;direction<2;++direction)
    for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet)for(unsigned mode:{0u,2u}){
        initialize();auto p=makeOriginalFreshBattleProfile();p.setu(4,course);p.setu(28,course*2+night);
        p.setu(12,direction);p.setu(8,night);p.setu(32,wet);put(p);
        m.write32(tls+4,tls+256);m.write32(tls+260,0);m.write32(race+1640,mode);
        m.write32(courseObject,0x0c38de4c);unsigned factoryCalls=0;
        RefCpu c(m);c.r[4]=race;c.r[15]=stack;c.pr=stop;hooks(c);
        c.callHooks[0x0c042700]=[&](auto&q){
            check(q.r[5]==course*2+night&&q.r[6]==direction&&q.r[7]==night&&m.read32(q.r[15])==wet,"ARace forwards scene/direction/time/weather");
            check(m.read32(q.r[15]+4)==0,"ARace synchronous loading flag");q.r[0]=courseObject;++factoryCalls;
        };
        c.callHooks[0x0c19afa0]=[](auto&){};run(c,0x0c062480);
        check(factoryCalls==1&&m.read32(race+1652)==course,"ARace course identity and one construction");
        m.write32(frame+2496,course*2+night);m.write32(frame+2500,direction);
        m.write32(frame+2504,night);m.write32(frame+2732,wet);m.write32(frame+2516,courseObject);
        RefCpu producer(m);producer.r[14]=frame;run(producer,0x0c0427a4,0x0c0427c6);
        check(m.read32(0x0c92ea7c)==course&&m.read32(0x0c92ea78)==night&&m.read32(0x0c92ea74)==wet,"Factory publishes unchanged lighting condition globals");
        RefCpu branch(m);branch.r[14]=frame;branch.r[15]=stack;run(branch,0x0c0427c6,0x0c0427f4);
        check(branch.r[7]==course*2+night+unsigned(!night&&wet&&course!=4),"Day rain remaps only model constructor selector");
        RefCpu tail(m);tail.r[14]=frame;run(tail,0x0c0437bc,0x0c0437cc);
        check(m.read32(courseObject+52)==night&&m.read32(courseObject+56)==wet,"Course owner retains original time/weather after constructor remap");
    }
    auto print=[&](const char* name,const std::set<Condition>&s){
        std::cout<<name<<" directed_conditions="<<s.size()<<'\n';
        for(unsigned course=0;course<9;++course){std::cout<<" course"<<course<<":";for(auto[c,d,n,w]:s)if(c==course)std::cout<<" d"<<d<<"t"<<n<<"w"<<w;std::cout<<'\n';}
    };
    print("TimeAttack",ta);print("Legend (all authored rivals/rematches)",legend);print("Bunta",bunta);
    std::cout<<"PASS "<<checks<<" checks / "<<instructions<<" original instructions. Input selector, graphics/resource/TLS, sound and timer-accounting boundaries explicit; no runtime/device/userdata.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
