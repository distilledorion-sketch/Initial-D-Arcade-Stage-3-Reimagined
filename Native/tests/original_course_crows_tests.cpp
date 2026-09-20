#include "original_course_crows.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Canonical image and project root required");
    const std::filesystem::path root=argv[2];RefMemory m(argv[1]);
    constexpr unsigned owner=0x0d000000,path=0x0d010000,group=0x0d020000,chunks=0x0d030000;
    constexpr unsigned model=0x0d040000,stack=0x0d100000,tls=0x0d200000,stop=0x0f000000;
    m.zeroRegion(owner,0x50000);m.zeroRegion(stack,0x10000);m.zeroRegion(tls,0x1000);
    m.zeroRegion(0x0e000000,0x100000);m.zeroRegion(0x0c92ece0,0x100);m.zeroRegion(0x0c98ad0c,12);
    m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0e080000);m.write32(0x0c98ad14,0x0e080000);
    unsigned checks=0;std::uint64_t instructions=0;
    const auto check=[&](bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);};
    const auto run=[&](RefCpu& c,unsigned begin,unsigned end,unsigned limit=100000){try{instructions+=c.run(begin,end,limit);}catch(const std::exception& e){throw std::runtime_error(hex(begin)+": "+e.what());}};
    const auto copy=[&](const std::filesystem::path& p,unsigned addr){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Fixture unavailable");std::vector<unsigned char> b{std::istreambuf_iterator<char>(f),{}};for(unsigned i=0;i<b.size();++i)m.write8(addr+i,b[i]);};
    copy(root/"data/original_assets/crows/flight.bin",path);copy(root/"data/original_assets/crows/group.bin",group);
    const auto text=[&](unsigned a){std::string s;while(m.read8(a))s+=char(m.read8(a++));return s;};
    const auto divide=[](RefCpu& c){if(!c.r[5])throw std::runtime_error("Source divide zero");c.fpul=unsigned(signed32(c.r[4])/signed32(c.r[5]));};
    std::vector<unsigned> fsca(32768);{std::ifstream f(root/"data/original_physics/fsca_table.bin",std::ios::binary);f.seekg(16);f.read(reinterpret_cast<char*>(fsca.data()),131072);if(!f)throw std::runtime_error("FSCA fixture unavailable");}
    auto native=OriginalCourseCrows::load(root);check(native.model.chunks.size()==30&&native.textures.size()==1,"Crow asset counts");
    check(m.read32(path)==900&&m.read32(group)==19,"Crow authored record counts");
    //The actual042700 constructor dispatch selects the crow-bearing Myogi
    //day constructor only for dry/day. Reverse does not enter this decision.
    check(m.read32(0x0c042a2c)==0x0c19ea40&&m.read32(0x0c042a34)==0x0c19f4c0,"Original Myogi constructor identities");
    for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet)for(unsigned reverse=0;reverse<2;++reverse){
        RefCpu c(m);c.r[7]=night;c.r[14]=stack;c.r[15]=stack+0xf000;
        m.write32(stack+2504,night);m.write32(stack+2732,wet);m.write32(stack+2500,reverse);
        run(c,0x0c0427d6,night||wet?0x0c042920:0x0c042880);
        check((c.r[7]==0)==OriginalCourseCrows::availableFor(0,night!=0,wet!=0),"Crow dry/day constructor gate");
    }
    for(unsigned other=1;other<9;++other)check(!OriginalCourseCrows::availableFor(other,false,false),"Crow owner is exclusive to Myogi");
    check(m.read32(0x0c38de80)==0x0c19eec0&&m.read32(0x0c38de78)==0x0c19f3c0,"Myogi source draw/update virtual entries");
    //Full synchronous041D20 construction: original paths/field writes/RNG and
    //nested matrix-free initialization run; only resource/TLS APIs are hooked.
    m.write32(tls+4,tls+256);unsigned heap=0x0e000000,loads=0,modelLoads=0;
    RefCpu ctor(m);ctor.r[4]=owner;ctor.r[5]=0x0c2a3bf4;ctor.r[6]=0x0c2a3c14;ctor.r[7]=0;ctor.r[15]=stack+0xf000;ctor.pr=stop;
    ctor.callHooks[0x0c221fc0]=[&](auto& c){c.r[0]=tls;};
    ctor.callHooks[0x0c0219a0]=[](auto& c){c.r[0]=c.r[4];};
    ctor.callHooks[0x0c021960]=[&](auto& c){check(c.r[5]<=4096,"Bounded source allocation");c.r[0]=heap;heap+=4096;};
    ctor.callHooks[0x0c057920]=[&](auto& c){check(text(c.r[5])=="/driveA/model/crow/crow_pol"&&text(c.r[6])=="/driveA/model/crow/crow_tex","Source crow bank paths");c.r[0]=model;++modelLoads;};
    ctor.callHooks[0x0c04e480]=[&](auto& c){auto name=text(c.r[4]);check(name=="/driveA/path/k_ez1_crow_fly.bin"||name=="/driveA/path/k_ez1_crow_grp.bin","Source crow path load");c.r[0]=name.find("_fly")!=std::string::npos?path:group;++loads;};
    ctor.callHooks[0x0c2223b8]=divide;
    std::uint32_t seed=0x76543210;m.write32(0x0c37c778,seed);run(ctor,0x0c041d20,stop);
    OriginalCourseCrowState state;resetOriginalCourseCrows(state,seed);
    check(ctor.r[0]==owner&&m.read32(owner+8)==model&&m.read32(owner+12)==path&&m.read32(owner+16)==group,"Full source owner resource bindings");
    check(loads==2&&modelLoads==1&&m.read8(owner+28)==1,"Synchronous constructor resource readiness");
    check(m.read32(0x0c37c778)==seed&&m.read32(owner+32)==0,"Constructor RNG consumption and frame0");
    for(unsigned i=0;i<19;++i)check(state.animationFrames[i]==m.read32(m.read32(owner+36)+4*i),"Initial source wing phase");
    m.write32(owner+36,chunks);
    //Numerical RNG/animation lifecycle over varied incoming shared seeds.
    for(unsigned trial=0;trial<257;++trial){
        seed=trial*0x9e3779b9u;auto originalSeed=seed;m.write32(0x0c37c778,seed);
        resetOriginalCourseCrows(state,seed);RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;c.callHooks[0x0c2223b8]=divide;
        run(c,0x0c042440,stop);check(seed==m.read32(0x0c37c778),"Original shared RNG final state");
        for(unsigned i=0;i<19;++i)check(state.animationFrames[i]==m.read32(chunks+4*i),"Original random wing phases");
        check(state.flightFrame==m.read32(owner+32),"Reset source flight cursor");
        if(trial==256){seed=originalSeed;native.reset(seed);}
    }
    //All900 authored flight orientations and all19 world placements for two
    //complete loops. Actual042520 matrix code runs, including look-at, inverse,
    //FSCA half-turn and each placement; only final mesh dispatch is hooked.
    for(unsigned frame=0;frame<1800;++frame){
        const auto& assembly=native.assembly();check(assembly.instances.size()==19,"Nineteen original crow draws");
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;c.fscaHalfWave=fsca;c.callHooks[0x0c2223b8]=divide;
        for(unsigned i=0;i<16;++i)c.xf[i]=(i%5==0)?0x3f800000u:0;
        unsigned draw=0,selected=~0u;
        c.callHooks[0x0c05a8e0]=[&](auto& q){check(q.r[4]==model&&q.r[5]<30,"Source crow model/chunk bounds");selected=q.r[5];q.r[0]=0x0d04f000;};
        c.callHooks[0x0c1d7120]=[&](auto& q){
            check(q.r[4]==0x0d04f000&&draw<19,"Source crow draw order bound");const auto& actual=assembly.instances[draw];check(selected==actual.chunk,"Source animated mesh index");
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
                const float a=std::bit_cast<float>(q.xf[col*4+row]),b=actual.transform[row*4+col];
                if(a!=b)throw std::runtime_error("Crow matrix frame"+std::to_string(frame)+" bird"+std::to_string(draw)+" cell"+std::to_string(row*4+col)+" original"+hex(q.xf[col*4+row])+" native"+hex(std::bit_cast<unsigned>(b)));
                ++checks;
            }++draw;
        };
        run(c,0x0c042520,stop);check(draw==19,"Complete source crow draw count");
        check(native.state().flightFrame==frame%900,"Read-only rendering preserved flight cursor");
        native.advance();RefCpu update(m);update.r[4]=owner;update.r[15]=stack+0xf000;update.pr=stop;run(update,0x0c0424c0,stop);
        check(native.state().flightFrame==m.read32(owner+32),"Original flight loop phase");
        for(unsigned i=0;i<19;++i)check(native.state().animationFrames[i]==m.read32(chunks+4*i),"Original wing animation update");
    }
    constexpr unsigned parent=0x0d050000;m.zeroRegion(parent,0x2000);m.write32(parent+1280,owner);
    //One unconditional crow update before the generic parent update. Caller
    //arguments and relative order are checked at the actual virtual method.
    std::vector<unsigned> calls;RefCpu update(m);update.r[4]=parent;update.r[5]=123;update.r[6]=456;update.r[15]=stack+0xf000;update.pr=stop;
    update.callHooks[0x0c0424c0]=[&](auto& c){check(c.r[4]==owner,"Parent crow update owner");calls.push_back(0);};
    update.callHooks[0x0c03a2e0]=[&](auto& c){check(c.r[4]==parent&&c.r[5]==123&&c.r[6]==456,"Parent update arguments preserved");calls.push_back(1);};
    run(update,0x0c19f3c0,stop);check(calls==std::vector<unsigned>{0,1},"Parent crow/base update order");
    RefCpu draw(m);draw.r[12]=parent;draw.r[15]=stack+0xf000;unsigned crowCalls=0;
    draw.callHooks[0x0c042520]=[&](auto& c){check(c.r[4]==owner,"Parent crow draw owner");++crowCalls;};
    run(draw,0x0c19f0da,0x0c19f0e2);check(crowCalls==1,"One source crow draw invocation");
    //Parent readiness includes the crow owner; already-loaded native assets
    //satisfy this boundary. Base readiness still gates the original query.
    for(unsigned ready=0;ready<2;++ready){
        RefCpu c(m);c.r[4]=parent;c.r[15]=stack+0xf000;c.pr=stop;c.callHooks[0x0c19b8c0]=[&](auto& q){q.r[0]=ready;};
        run(c,0x0c19f400,stop);check(c.r[0]==ready,"Parent readiness gate");
    }
    //Startup ordering, kept separate from the numerical crow proof above.
    //The source factory/vehicle initialization are dependency boundaries in
    //this enclosing slice; these checks do not model all global RNG users.
    constexpr unsigned race=0x0d060000;
    m.zeroRegion(race,0x3000);m.zeroRegion(0x0c8ff36c,4);
    m.write32(parent,0x0c38de4c);m.write32(parent+1280,owner);
    m.write32(tls+4,tls+256);m.write32(tls+260,0);
    m.write32(0x0c31c9a0,0);m.write32(0x0c31c9a4,0);
    m.write32(0x0c31c9a8,0);m.write32(0x0c31c9b8,0);m.write32(0x0c31c9bc,0);
    for(unsigned mode:{0u,2u}){
        m.write32(race+1640,mode);std::vector<unsigned> ordering;
        RefCpu c(m);c.r[4]=race;c.r[15]=stack+0xf000;c.pr=stop;
        c.callHooks[0x0c221fc0]=[&](auto& q){q.r[0]=tls;};
        c.callHooks[0x0c055d60]=[](auto&){}; //Source diagnostics output.
        c.callHooks[0x0c142100]=[](auto&){}; //Source sound command boundary.
        c.callHooks[0x0c042700]=[&](auto& q){
            check(q.r[5]==0&&q.r[6]==0&&q.r[7]==0,"Source Myogi setup selectors");
            check(m.read32(q.r[15]+4)==0,"Race requests synchronous course construction");
            ordering.push_back(0x042700);q.r[0]=parent;
        };
        c.callHooks[0x0c19afa0]=[&](auto& q){check(q.r[4]==parent,"Course activation owner");ordering.push_back(0x19afa0);};
        c.callHooks[0x0c0625e0]=[&](auto& q){check(q.r[4]==race,"Player setup owner");ordering.push_back(0x0625e0);};
        run(c,0x0c061b00,0x0c061c08);
        check(ordering==std::vector<unsigned>{0x042700,0x19afa0,0x0625e0},"Course construction precedes player initialization");
    }
    //Actual Myogi factory argument forwarding. Its last stack word is the
    //sync/async byte read by19EA40 and then passed as041D20 r7.
    for(unsigned async:{0u,1u}){
        RefCpu factory(m);factory.r[14]=stack;factory.r[15]=stack+0xf000;
        m.write32(stack+2680,tls);m.write32(stack+2512,0x0c2a0000);
        m.write32(stack+2500,0);m.write32(stack+2508,async);
        factory.callHooks[0x0c021ee0]=[&](auto& q){check(q.r[4]==1284,"Myogi owner allocation size");q.r[0]=parent;};
        factory.callHooks[0x0c19ea40]=[&](auto& q){check(q.r[4]==parent&&m.read32(q.r[15]+16)==async,"Myogi factory forwards source loading flag");};
        run(factory,0x0c042880,0x0c042912);
        RefCpu init(m);init.r[4]=parent;init.r[15]=stack+0xf000;
        m.write32(init.r[15]+16,async);
        init.callHooks[0x0c221fc0]=[&](auto& q){q.r[0]=tls;};
        run(init,0x0c19ea40,0x0c19ea7a);
        check(m.read32(init.r[14]+112)==async,"Myogi constructor reads loading byte");
        m.write32(init.r[14]+136,tls+300);m.write32(init.r[14]+120,owner);
        init.callHooks[0x0c041d20]=[&](auto& q){check(q.r[4]==owner&&q.r[7]==async,"Crow constructor inherits loading flag");q.r[0]=owner;};
        run(init,0x0c19eb64,0x0c19eb7e);
    }
    //No seed1 reset occurs in15EE00: the source reads the existing shared
    //generator. Its one draw is after crow construction in the normal race.
    m.write32(0x0c37c778,0x12345678);std::uint32_t before=0x12345678;
    OriginalCourseCrowState initialCrow;resetOriginalCourseCrows(initialCrow,before);
    RefCpu sourceCrow(m);sourceCrow.r[4]=owner;sourceCrow.r[15]=stack+0xf000;sourceCrow.pr=stop;sourceCrow.callHooks[0x0c2223b8]=divide;
    run(sourceCrow,0x0c042440,stop);check(m.read32(0x0c37c778)==before,"Crow consumes nineteen source values before player boundary");
    RefCpu player(m);player.r[8]=race;player.r[15]=stack+0xf000;
    run(player,0x0c15ef5c,0x0c15ef7a);before=before*0x41c64e6du+12345u;
    check(m.read32(0x0c37c778)==before,"Player initialization consumes subsequent shared value");
    const float initializedNoise=std::fma(float((before>>16)&32767),std::bit_cast<float>(0x3c23d70au),std::bit_cast<float>(0x3a83126fu));
    check(m.readFloat(race+0x21c)==initializedNoise,"Player initialization noise comes from subsequent value");
    //Boot056A20 seeds from2021E0's platform word, not a fixed value. The
    //fixture supplies that RAM boundary; it does not access platform devices.
    m.zeroRegion(0x0cb11af4,4);m.write32(0x0cb11af4,tls+512);
    for(unsigned platformWord:{0u,1u,0x12345678u,0xffffffffu}){
        m.write32(tls+512,platformWord);RefCpu c(m);c.r[14]=stack;c.r[15]=stack+0xf000;
        run(c,0x0c056a44,0x0c056a54);check(m.read32(0x0c37c778)==platformWord,"Source boot forwards variable platform seed");
    }
    //Unique predecessor reconstruction preserves all caller-owned driving
    //state. Actual042440, not a second inverse implementation, checks every
    //wing phase and the final shared seed across257 unrelated boundaries.
    for(unsigned trial=0;trial<257;++trial){
        const std::uint32_t drivingEntry=trial==256?0xffffffffu:trial*0x9e3779b9u;
        auto callerOwnedSeed=drivingEntry;
        OriginalCourseCrowState recovered;
        resetOriginalCourseCrowsBeforeDrivingSeed(recovered,callerOwnedSeed);
        native.resetBeforeDrivingSeed(callerOwnedSeed);
        check(callerOwnedSeed==drivingEntry,"Crow boundary recovery preserves driving seed");
        m.write32(0x0c37c778,originalCourseCrowPrecedingSeed(drivingEntry));
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;c.callHooks[0x0c2223b8]=divide;
        run(c,0x0c042440,stop);
        check(m.read32(0x0c37c778)==drivingEntry,"Original nineteen draws finish at unchanged driving-entry boundary");
        check(recovered.initialized&&recovered.flightFrame==0&&native.state().flightFrame==0,"Reconstructed crow owner starts at source flight0");
        for(unsigned i=0;i<19;++i){
            check(recovered.animationFrames[i]==m.read32(chunks+4*i),"Recovered source wing phase");
            check(native.assembly().instances[i].chunk==recovered.animationFrames[i],"Owner publishes reconstructed wing phases");
        }
    }
    //The sixty solver warmup calls do not invoke the course-owner update.
    //Their solver/presentation bodies are explicit boundaries, not sixty
    //world-display frames or sixty fabricated crow advances.
    m.write32(owner+32,0);m.write32(stack+196,race);unsigned warmSolver=0,warmCar=0;
    RefCpu warm(m);warm.r[14]=stack;warm.r[15]=stack+0xf000;
    warm.callHooks[0x0c062de0]=[&](auto& q){check(q.r[4]==race,"Warmup solver owner");++warmSolver;};
    warm.callHooks[0x0c063ce0]=[&](auto& q){check(q.r[4]==race,"Warmup car presentation owner");++warmCar;};
    run(warm,0x0c0620fa,0x0c06211a);
    check(warmSolver==60&&warmCar==60&&m.read32(owner+32)==0,"Warmup contains no course animation update");
    //The first ordinary ARace Main invokes actual19F3C0 through its real
    //vtable once before the countdown/rules/solver span. Vary the path inputs
    //to verify they are forwarded to the base only, never used by the flock.
    for(unsigned trial=0;trial<4;++trial){
        m.write32(race+1036,parent);m.write32(race+1408,trial&1);
        m.write32(race+1412+12*(trial&1),17+trial);m.write32(race+1460,29+trial);
        RefCpu c(m);c.r[12]=race;c.r[8]=race+1020;c.r[15]=stack+0xf000;unsigned baseCalls=0;
        c.callHooks[0x0c03a2e0]=[&](auto& q){check(q.r[4]==parent&&q.r[5]==17+trial&&q.r[6]==29+trial,"Course update receives source path inputs");++baseCalls;};
        run(c,0x0c05fe48,0x0c05fe6a);
        check(baseCalls==1&&m.read32(owner+32)==trial+1,"Exactly one crow advance before each ordinary owner frame");
    }
    //Parent destruction releases the crow owner and clears its pointer. Only
    //the external delete operation is hooked; its selection/clear are source.
    RefCpu destroy(m);destroy.r[9]=parent;destroy.r[15]=stack+0xf000;unsigned deletes=0;
    destroy.callHooks[0x0c0422e0]=[&](auto& c){check(c.r[4]==owner&&c.r[5]==3,"Parent releases crow owner");++deletes;};
    run(destroy,0x0c19ee38,0x0c19ee6c);check(deletes==1&&m.read32(parent+1280)==0,"Crow owner lifetime ends with course");
    std::cout<<"PASS "<<checks<<" checks / "<<instructions<<" original instructions: full crow construction,257 RNG seeds,two900-frame loops,19 placements and30 wing frames. No devices or user data.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
