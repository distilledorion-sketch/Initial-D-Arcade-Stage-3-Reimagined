#include "original_snow.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0xc900000,records=owner+0x1000,stack=owner+0x1f000,matrixStack=owner+0x10000,stop=0xcfffffc;
constexpr unsigned material=owner+0x20000,vertices=owner+0x30000,bank=owner+0x40000;
unsigned cases=0;std::uint64_t checks=0,instructions=0;
void equal(unsigned a,unsigned b,const char* label){++checks;if(a!=b){
    std::cerr<<label<<" case="<<cases<<" source="<<std::hex<<a<<" host="<<b<<std::dec<<'\n';
    throw std::runtime_error("Snow source mismatch");}}
void equal(float a,float b,const char* label){equal(std::bit_cast<unsigned>(a),std::bit_cast<unsigned>(b),label);}
struct Random{unsigned state=0;float operator()(){state=state*1103515245u+12345u;return float((state>>16)&32767u)/32768.f;}};
void setup(RefMemory& memory,unsigned seed){
    memory.clear();memory.zeroRegion(owner,0x20000);memory.zeroRegion(0xc92ea94,48);memory.write32(0xc37c778,seed);
    memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
}
void execute(RefMemory& memory,unsigned address,unsigned r4,const OriginalMatrix& view,unsigned index=0,unsigned mode=0){
    RefCpu cpu(memory);cpu.r[4]=r4;cpu.r[5]=0xc33fc24;cpu.r[6]=index;cpu.r[7]=mode;cpu.r[15]=stack;cpu.pr=stop;
    //The compiler's double-precision signed divide helper saves FPSCR, which
    //this scalar interpreter does not model. All callers here divide a
    //nonnegative slot0..319 by3 or5; preserve its exact FPUL quotient contract.
    cpu.callHooks[0xc2223b8]=[](RefCpu& c){
        if(c.r[4]>=320||(c.r[5]!=3&&c.r[5]!=5))throw std::runtime_error("Unexpected snow division boundary");
        c.fpul=c.r[4]/c.r[5];
    };
    if(address==0xc1adfe0){
        //Run all320 updates and the real vertex copy. Only GPU-list publication
        //and submission are leaves; neither may modify the simulation.
        cpu.callHooks[0xc1d5400]=[&](RefCpu& c){equal(c.r[4],material+32,"Published snow model");c.r[0]=material;};
        cpu.callHooks[0xc1d7120]=[&](RefCpu& c){equal(c.r[4],material,"Submitted snow model");};
    }
    for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
    instructions+=cpu.run(address,stop,300000);
    for(unsigned i=0;i<16;++i)equal(cpu.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Caller matrix");
}
void particle(const RefMemory& memory,unsigned address,const OriginalSnowParticle& value){
    for(unsigned i=0;i<3;++i){equal(memory.readFloat(address+4*i),value.position[i],"Position");
        equal(memory.readFloat(address+12+4*i),value.velocity[i],"Velocity");equal(memory.readFloat(address+24+4*i),value.normal[i],"Normal");}
    equal(memory.read32(address+36),unsigned(value.life),"Life");equal(memory.read32(address+40),value.active,"Active");
}
void basis(const RefMemory& memory,const std::array<std::array<float,3>,4>& corners){
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<3;++j)equal(memory.readFloat(0xc92ea94+i*12+j*4),corners[i][j],"Shared quad basis");
}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required");RefMemory memory(argv[1]);
    unsigned fixture=0x1ad820;const auto next=[&]{fixture^=fixture<<13;fixture^=fixture>>17;fixture^=fixture<<5;return fixture;};
    const auto initDraw=[&]{
        memory.zeroRegion(material,0x30000);memory.write32(owner+16,bank);memory.write32(bank+44,17);
        memory.write32(owner+24,vertices);memory.write32(0xc2f4b94,100);
        RefCpu init(memory);init.r[4]=owner;init.r[15]=stack;init.pr=stop;
        init.callHooks[0xc021e80]=[&](RefCpu& c){equal(c.r[4],1280u*32+224,"Snow draw allocation size");c.r[0]=material;};
        init.callHooks[0xc1d8a60]=[](RefCpu&){}; //Final texture publication only.
        instructions+=init.run(0xc1adda0,stop,150000);
        equal(memory.read32(owner+28),material,"Snow model binding");
        const auto ich=memory.read32(material+16),gmp=memory.read32(material+12);
        equal(memory.read32(ich),snowPcw,"Snow PCW");equal(memory.read32(ich+4),snowIsp,"Snow ISP");
        equal(memory.read32(ich+8)&~63u,snowTspWithoutSize,"Snow TSP without texture sizes");
        equal(memory.read32(gmp+12),snowDiffuse,"Snow diffuse");equal(memory.read32(gmp+16),snowDiffuse,"Snow second diffuse");
        equal(memory.read32(gmp+8),snowMaterialFlags,"Snow material flags");
        for(unsigned i=0;i<1280;++i){const unsigned at=ich+32+i*32,corner=i%4;
            equal(memory.read32(at+24),0xffffffffu,"Snow vertex colour");
            equal(memory.readFloat(at+16),corner%2?1.f:0.f,"Snow U");equal(memory.readFloat(at+20),corner<2?1.f:0.f,"Snow V");}
    };
    setup(memory,1);initDraw();++cases;
    for(unsigned mode:{0u,1u,2u}){
        setup(memory,123);memory.zeroRegion(material,0x40000);
        constexpr unsigned context=bank+0x1000,refs=bank+0x2000,view=bank+0x3000;
        RefCpu init(memory);init.r[4]=owner;init.r[5]=refs;init.r[6]=refs+4;init.r[7]=view;
        init.r[15]=stack;init.pr=stop;memory.write32(stack,mode);unsigned allocations=0,draws=0;
        init.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=context;};
        init.callHooks[0xc021960]=[&](RefCpu& c){c.r[0]=bank;};
        init.callHooks[0xc057920]=[&](RefCpu& c){
            equal(c.r[5],0xc2a6940u,"Snow shares rain model path");equal(c.r[6],0xc2a6964u,"Snow shares rain textures");c.r[0]=bank;};
        init.callHooks[0xc021f00]=[&](RefCpu& c){
            equal(c.r[4],allocations?0x3c00u:0x3700u,"Snow record/vertex allocation");
            c.r[0]=allocations?vertices:records;++allocations;};
        init.callHooks[0xc1adda0]=[&](RefCpu& c){equal(c.r[4],owner,"Snow constructor initializes draw model");++draws;};
        instructions+=init.run(0xc1ad820,stop,20000);
        equal(allocations,2u,"Snow allocations");equal(draws,1u,"Snow draw initialization");
        equal(memory.read32(owner+36),refs,"Constructed primary reference");equal(memory.read32(owner+40),refs+4,"Constructed rival reference");
        equal(memory.read32(owner+68),view,"Constructed snow view");equal(memory.read32(owner+72),mode,"Constructed replay mode");
        equal(memory.read32(owner+32),0u,"Initial snow cursor");equal(memory.read32(0xc37c778),123u,"Snow constructor RNG");
        OriginalSnowFields fields;for(unsigned i=0;i<320;++i)particle(memory,records+i*44,fields.particles[i]);++cases;
    }
    //Manager weather==1 and the original snow-course flag select this owner.
    //The ending passes replay mode1 through manager+92 to the constructor.
    for(unsigned weather:{0u,1u,2u})for(bool snow:{false,true}){
        setup(memory,123);constexpr unsigned frame=owner+0x6000,manager=owner+0x7000;
        memory.write32(manager+88,weather);memory.write32(0xc31ce38,snow?0x04000000:0);
        RefCpu gate(memory);gate.r[1]=manager+64;gate.r[0]=0x168;gate.r[14]=frame;gate.r[15]=stack;
        instructions+=gate.run(0xc17d76c,weather!=1?0xc17d92e:snow?0xc17d796:0xc17d840,100);
        equal(memory.read32(frame+0x12c),weather,"Manager weather admission");++cases;
    }
    for(unsigned mode:{0u,1u,2u}){
        setup(memory,123);constexpr unsigned frame=owner+0x6000,manager=owner+0x7000;
        memory.write32(frame+0x170,manager+64);memory.write32(frame+0x120,manager);memory.write32(frame+0x134,owner);
        memory.write32(manager+84,owner+0x8000);memory.write32(manager+92,mode);
        RefCpu bind(memory);bind.r[14]=frame;bind.r[0]=0x16c;bind.r[15]=stack;unsigned calls=0;
        bind.callHooks[0xc1ad820]=[&](RefCpu& c){
            equal(c.r[4],owner,"Snow constructor object");equal(c.r[5],manager+76,"Snow primary reference");
            equal(c.r[6],manager+80,"Snow rival reference");equal(c.r[7],owner+0x8000,"Snow embedded view");
            equal(memory.read32(c.r[15]),mode,"Snow replay mode");++calls;};
        instructions+=bind.run(0xc17d7ee,0xc17d80e,100);equal(calls,1u,"Single snow constructor");++cases;
    }
    for(unsigned trial=0;trial<4;++trial)for(unsigned mode:{0u,1u,2u})for(unsigned index=0;index<320;++index){
        Random random{next()};setup(memory,random.state);auto view=originalIdentityMatrix();
        if(trial)for(unsigned i=0;i<3;++i){for(unsigned j=0;j<3;++j)view.elements[i*4+j]+=float(int(next()%65)-32)/64.f;view.elements[12+i]=float(int(next()%8193)-4096)/8.f;}
        std::array<std::array<float,3>,4> corners{};
        for(unsigned i=0;i<4;++i)for(unsigned j=0;j<3;++j){corners[i][j]=float(int(next()%1025)-512)/32.f;memory.writeFloat(0xc92ea94+i*12+j*4,corners[i][j]);}
        OriginalSnowParticle value;execute(memory,0xc1ae260,records,view,index,mode);value.spawn(view,index,mode,corners,random);
        particle(memory,records,value);basis(memory,corners);equal(memory.read32(0xc37c778),random.state,"Birth RNG");
        for(unsigned frame=0;frame<83;++frame){execute(memory,0xc1ae480,records,view);value.advance();particle(memory,records,value);}
        ++cases;
    }
    for(unsigned scenario=0;scenario<8;++scenario){
        Random random{next()};setup(memory,random.state);OriginalSnowFields fields;auto view=originalIdentityMatrix();
        view.elements[12]=-.37f;view.elements[13]=13.f;view.elements[14]=284.f;
        memory.write32(owner+20,records);memory.write32(owner+72,scenario&1);fields.cursor=scenario*43%320;memory.write32(owner+32,fields.cursor);
        for(unsigned i=0;i<fields.particles.size();++i){auto& p=fields.particles[i];p.active=scenario<2?0:scenario<4?1:i%3==0?2:1;
            memory.write32(records+44*i+36,100);memory.write32(records+44*i+40,p.active);}
        for(unsigned tick=0;tick<360;++tick){
            if(tick%7==0&&scenario>=2){const auto index=next()%320;fields.particles[index].active=0;memory.write32(records+44*index+40,0);}
            execute(memory,0xc1ae1e0,owner,view);fields.admit(view,scenario&1,random);
            equal(memory.read32(owner+32),fields.cursor,"Admission cursor");equal(memory.read32(0xc37c778),random.state,"Admission RNG");
            for(unsigned i=0;i<320;++i)particle(memory,records+44*i,fields.particles[i]);basis(memory,fields.quadOffsets);
        }
        ++cases;
    }
    for(unsigned scenario=0;scenario<6;++scenario){
        Random random{next()};setup(memory,random.state);initDraw();OriginalSnowFields fields;
        memory.write32(owner+20,records);memory.write32(owner+72,scenario&1);
        for(unsigned i=0;i<320;++i){auto& p=fields.particles[i];p.life=scenario>=4?int(i%5)-1:100;p.active=scenario>=2?(i%3==0?2:1):0;
            memory.write32(records+44*i+36,unsigned(p.life));memory.write32(records+44*i+40,p.active);}
        for(unsigned tick=0;tick<360;++tick){
            const auto world=originalLookAtCameraWorld({float(tick)*.13f,7.f,float(tick)*.4f},{float(tick)*.09f,2.f,float(tick)*.4f-17.f});
            const auto view=originalCameraWorldToView(world);
            execute(memory,0xc1adfe0,owner,view);fields.advance(view,scenario&1,random);
            equal(memory.read32(0xc37c778),random.state,"Owner RNG");equal(memory.read32(owner+32),fields.cursor,"Owner cursor");basis(memory,fields.quadOffsets);
            const auto ich=memory.read32(material+16);
            for(unsigned i=0;i<320;++i){particle(memory,records+i*44,fields.particles[i]);const auto quad=fields.quad(i);
                for(unsigned corner=0;corner<4;++corner)for(unsigned axis=0;axis<3;++axis){
                    equal(memory.readFloat(vertices+i*48+corner*12+axis*4),quad[corner][axis],"Snow prepared vertex");
                    equal(memory.readFloat(ich+32+(i*4+corner)*32+4+axis*4),quad[corner][axis],"Snow published vertex");}}
        }
        ++cases;
    }
    std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<instructions<<" original instructions. Snow birth, update, shared quad basis, admission, material initialization and complete320-record draw updates execute with original RNG and matrix routines. Allocation, bounded integer division and final texture/GPU publication are explicit hook boundaries.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
