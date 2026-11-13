#include "original_rain.h"
#include "original_rain_owner.h"
#include "original_rain_glare.h"
#include "original_roadside_dust_owner.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0xc900000,records=owner+0x1000,splashes=owner+0x3000;
constexpr unsigned matrix=owner+0x4000,motionAt=owner+0x5000,stack=owner+0x1f000;
constexpr unsigned matrixStack=owner+0x10000,stop=0xcfffffc;
constexpr unsigned mesh=owner+0x6000,scratch=owner+0x8000,material=owner+0xa000;
constexpr unsigned mistAt=owner+0xc000,wheelsAt=owner+0xd000;
std::uint64_t checks=0,cases=0,instructions=0;
void equal(unsigned source,unsigned host,const char* label){++checks;if(source!=host){
    std::cerr<<label<<" case="<<cases<<" source=0x"<<std::hex<<source<<" host=0x"<<host<<std::dec<<'\n';
    throw std::runtime_error("Original rain reference mismatch");}}
void equal(float source,float host,const char* label){equal(std::bit_cast<unsigned>(source),std::bit_cast<unsigned>(host),label);}
struct Random {
    unsigned state=0x19a7ee3,calls=0;
    float operator()(){state=state*1103515245u+12345u;++calls;return float((state>>16)&32767u)/32768.f;}
};
void put(RefMemory& m,unsigned at,const OriginalMatrix& value){for(unsigned i=0;i<16;++i)m.writeFloat(at+i*4,value.elements[i]);}
void put(RefMemory& m,unsigned at,const std::array<float,3>& value){for(unsigned i=0;i<3;++i)m.writeFloat(at+i*4,value[i]);}
void eqPoint(const RefMemory& m,unsigned at,const std::array<float,3>& value,const char* label){for(unsigned i=0;i<3;++i)equal(m.readFloat(at+i*4),value[i],label);}
void eqStreak(const RefMemory& m,unsigned at,const OriginalRainStreak& value){
    eqPoint(m,at,value.position,"Streak world position");eqPoint(m,at+12,value.direction,"Streak displacement");
    eqPoint(m,at+24,value.normal,"Streak normal");
    for(unsigned i=0;i<4;++i)eqPoint(m,at+36+i*12,value.offsets[i],"Streak birth quad");
    equal(m.read32(at+84),unsigned(value.life),"Streak lifetime");equal(m.read32(at+88),value.active,"Streak activity");
}
void eqSplash(const RefMemory& m,unsigned at,const OriginalRainSplash& value){
    eqPoint(m,at,value.position,"Splash world position");equal(m.read32(at+12),unsigned(value.life),"Splash lifetime");
    equal(m.read32(at+16),value.active,"Splash activity");
}
void eqMist(const RefMemory& m,unsigned at,const OriginalRainWheelMist& value){
    eqPoint(m,at,value.position,"Wheel mist position");equal(m.readFloat(at+12),value.velocityX,"Wheel mist X velocity");
    equal(m.readFloat(at+16),value.drag,"Wheel mist damping");equal(m.readFloat(at+20),value.velocityZ,"Wheel mist Z velocity");
    equal(m.read32(at+24),unsigned(value.life),"Wheel mist lifetime");equal(m.read32(at+28),value.active,"Wheel mist activity");
}
void setup(RefMemory& m,unsigned seed){
    m.clear();m.zeroRegion(owner,0x20000);m.write32(0xc37c778,seed);
    m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matrixStack);m.write32(0xc98ad14,matrixStack);
}
void makeRainGlareChunk(RefMemory& m,unsigned chunk,unsigned variant){
    m.write32(chunk,0x100);m.write32(chunk+64,256-64);m.write32(chunk+68,96);
    m.write32(chunk+256,0x500);m.write32(chunk+256+12,0x12345678);m.write32(chunk+256+20,0x87654321);
    m.write32(chunk+320,0x8a00072e);m.write32(chunk+324,0x83800000);
    m.write32(chunk+328,0x840020c0+variant);m.write32(chunk+336,0x940004c0+variant);
    m.write32(chunk+344,0x0100004a);m.write32(chunk+348,0);
}
void execute(RefMemory& m,unsigned entry,unsigned a,unsigned b=0,const OriginalMatrix& view=originalIdentityMatrix()){
    RefCpu cpu(m);cpu.r[4]=a;cpu.r[5]=b;cpu.r[15]=stack;cpu.pr=stop;
    for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
    instructions+=cpu.run(entry,stop,30000);
    if(entry!=0xc1fc0a0)for(unsigned i=0;i<16;++i)equal(cpu.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Caller matrix preserved");
}
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("Canonical image and FSCA table required");RefMemory m(argv[1]);
    const auto fsca=OriginalFscaTable::load(argv[2]);
    std::ifstream fscaFile(argv[2],std::ios::binary);fscaFile.seekg(16);std::vector<unsigned> fscaWords(32768);
    fscaFile.read(reinterpret_cast<char*>(fscaWords.data()),std::streamsize(fscaWords.size()*4));
    if(!fscaFile)throw std::runtime_error("Reference FSCA table read failed");
    unsigned fixtureSeed=0x180000;const auto next=[&](){fixtureSeed^=fixtureSeed<<13;fixtureSeed^=fixtureSeed>>17;fixtureSeed^=fixtureSeed<<5;return fixtureSeed;};
    // Real1FC0A0, including its LU factorization, solve and copy. No math hooks.
    for(unsigned trial=0;trial<1024;++trial){
        setup(m,next());OriginalMatrix native;
        for(auto& v:native.elements)v=float(int(next()%2049)-1024)/128.f;
        for(unsigned i=0;i<4;++i)native.elements[i*5]+=20.f;
        if(trial%8==0)for(unsigned i=0;i<4;++i)native.elements[(trial%4)*4+i]=0;
        if(trial%8==1)for(unsigned i=0;i<4;++i)native.elements[4+i]=native.elements[i];
        put(m,matrix,native);RefCpu cpu(m);cpu.r[4]=matrix;cpu.r[15]=stack;cpu.pr=stop;
        instructions+=cpu.run(0xc1fc0a0,stop,10000);
        equal(cpu.getFloat(0),invertOriginalMatrix(native),"Inverse determinant");
        for(unsigned i=0;i<16;++i)equal(m.readFloat(matrix+i*4),native.elements[i],"Inverse matrix");++cases;
    }
    // Actual RNG, matrix-stack push/pop, inversion, FTRV and birth/update code.
    // Translated, non-axis-aligned camera matrices catch camera/world confusion.
    for(unsigned trial=0;trial<256;++trial){
        Random random{next()};setup(m,random.state);
        auto view=originalIdentityMatrix();
        for(unsigned i=0;i<3;++i){for(unsigned j=0;j<3;++j)view.elements[i*4+j]+=float(int(next()%65)-32)/64.f;view.elements[12+i]=float(int(next()%8193)-4096)/8.f;}
        const std::array<float,3> motion{float(int(next()%1025)-512)/128.f,float(int(next()%1025)-512)/128.f,float(int(next()%1025)-512)/128.f};
        put(m,motionAt,motion);OriginalRainStreak streak;streak.setMotion(motion);
        execute(m,0xc180160,records,motionAt,view);execute(m,0xc17ffc0,records,0,view);
        streak.spawn(view,random);eqStreak(m,records,streak);equal(m.read32(0xc37c778),random.state,"Streak RNG");
        for(unsigned frame=0;frame<28;++frame){execute(m,0xc180180,records,0,view);streak.advance();eqStreak(m,records,streak);}
        auto world=view;put(m,matrix,world);execute(m,0xc180280,splashes,matrix,view);
        execute(m,0xc1801e0,splashes,0,view);OriginalRainSplash splash;splash.spawn(world,random);
        eqSplash(m,splashes,splash);equal(m.read32(0xc37c778),random.state,"Splash RNG");
        for(unsigned frame=0;frame<6;++frame){execute(m,0xc1802a0,splashes,0,view);splash.advance();eqSplash(m,splashes,splash);}++cases;
    }
    // Source first-free admission/cursor behavior with full, sparse, unusual
    // non-one flags and recycled pools. Both owners retain the same shared RNG.
    for(unsigned scenario=0;scenario<8;++scenario){
        Random random{next()};setup(m,random.state);OriginalRainFields fields;
        m.write32(owner+24,records);m.write32(owner+40,splashes);m.write32(owner+60,matrix);m.write32(owner+28,mesh);
        // Build the dynamic strip with the real constructor/binding code; the
        // texture-publication leaf is the only skipped dependency here.
        RefCpu init(m);init.r[4]=material;init.r[5]=0x0100004a;init.r[6]=100;init.r[15]=stack;init.pr=stop;
        instructions+=init.run(0xc1d0260,stop,2000);
        RefCpu bind(m);bind.r[4]=material;bind.r[5]=6;bind.r[15]=stack;bind.pr=stop;
        bind.callHooks[0xc1d8a60]=[](auto&){};instructions+=bind.run(0xc1d0720,stop,2000);
        m.write32(scratch+16,material);RefCpu edit(m);edit.r[14]=scratch;edit.r[15]=stack;
        instructions+=edit.run(0xc17ea68,0xc17eab8,1000);m.write32(owner+32,material);
        const auto ich=m.read32(material+16);equal(m.read32(ich),rainPcw,"Rain material PCW");
        equal(m.read32(ich+4),rainIsp,"Rain material ISP");equal(m.read32(ich+8)&~63u,rainTspWithoutSize,"Rain material TSP without texture dimensions");
        m.write32(owner+48,mistAt);
        for(unsigned i=0;i<72;++i)m.write32(mistAt+i*36+24,100);
        for(unsigned i=0;i<25;++i){fields.streaks[i].life=int(next()%28);fields.streaks[i].active=(i+scenario)%3;
            m.write32(records+i*92+84,fields.streaks[i].life);m.write32(records+i*92+88,fields.streaks[i].active);}
        for(unsigned i=0;i<9;++i){fields.splashes[i].life=int(next()%5);fields.splashes[i].active=(i+scenario)%3;
            m.write32(splashes+i*24+12,fields.splashes[i].life);m.write32(splashes+i*24+16,fields.splashes[i].active);}
        fields.streakCursor=scenario*3;fields.splashCursor=scenario;
        m.write32(owner+36,fields.streakCursor);m.write32(owner+44,fields.splashCursor);
        for(unsigned frame=0;frame<160;++frame){
            auto view=originalIdentityMatrix();view.elements[12]=float(frame)*.21f;view.elements[13]=float(scenario);view.elements[14]=-float(frame)*.43f;
            rotateOriginalMatrixPhase(view,1,std::uint16_t(frame*101+scenario*8192),fsca);
            rotateOriginalMatrixPhase(view,0,std::uint16_t(scenario*155),fsca);
            auto world=originalIdentityMatrix();world.elements[12]=float(frame)*.25f;world.elements[13]=float(scenario)*2;world.elements[14]=float(frame)*.4f;
            rotateOriginalMatrixPhase(world,1,std::uint16_t(frame*73+scenario*231),fsca);put(m,matrix,world);
            const std::array<float,3> motion{frame%19==0?3.f:.25f,0,.4f};put(m,owner+260,motion);
            execute(m,0xc17ee20,owner,0,view);fields.admitStreak(view,motion,random);
            equal(m.read32(owner+36),fields.streakCursor,"Streak admission cursor");
            // Complete25-record source update and mesh construction, including
            // inactive zeroing and the last visible frame after expiry.
            m.write32(scratch+0x128,owner);m.write32(scratch+0x140,scratch+88);
            RefCpu update(m);update.r[14]=scratch;update.r[15]=stack;update.r[9]=0;
            instructions+=update.run(0xc17fa38,0xc17fbfc,15000);
            fields.advanceStreaks();
            for(unsigned birth=0;birth<3;++birth){execute(m,0xc17eec0,owner,0,view);fields.admitSplash(world,random);}
            equal(m.read32(owner+44),fields.splashCursor,"Splash admission cursor");
            // Shared72-record pool, up to six births per moving car, and
            // capacity/recycling under two-car traffic. Admission gates are
            // supplied here; the outer17F7E0 car/surface owner remains separate.
            const std::array<std::array<float,3>,2> wheels{{{-.75f,.3f,-1.1f},{.75f,.3f,-1.2f}}};
            put(m,wheelsAt,wheels[0]);put(m,wheelsAt+12,wheels[1]);put(m,motionAt,motion);
            for(unsigned car=0;car<2;++car)if((frame+car*7)%19<16){
                RefCpu emit(m);emit.r[4]=owner;emit.r[5]=motionAt;emit.r[6]=matrix;emit.r[7]=wheelsAt;emit.r[15]=stack;emit.pr=stop;
                m.write32(stack,wheelsAt+12);for(unsigned i=0;i<16;++i)emit.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
                instructions+=emit.run(0xc17f040,stop,20000);fields.admitWheelMist(world,motion,wheels,random);
            }
            equal(m.read32(owner+52),fields.wheelMistCursor,"Wheel mist admission cursor");
            const bool drawEnabled=frame%31<25;
            std::vector<OriginalMatrix> splashMatrices;
            if(drawEnabled){
                RefCpu splashDraw(m);splashDraw.r[4]=owner;splashDraw.r[15]=stack;splashDraw.pr=stop;splashDraw.fscaHalfWave=fscaWords;
                for(unsigned i=0;i<16;++i)splashDraw.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
                splashDraw.callHooks[0xc05a8e0]=[&](auto& c){equal(c.r[5],4u,"Splash source chunk");c.r[0]=material;};
                splashDraw.callHooks[0xc1d7120]=[&](auto& c){OriginalMatrix drawn;for(unsigned i=0;i<16;++i)drawn.elements[i]=std::bit_cast<float>(c.xf[i]);splashMatrices.push_back(drawn);};
                instructions+=splashDraw.run(0xc17ef60,stop,15000);
                for(unsigned i=0;i<16;++i)equal(splashDraw.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Splash draw restores caller matrix");
            }
            fields.advanceSplashes(drawEnabled);
            unsigned splashIndex=0;
            for(unsigned i=0;i<9;++i)if(fields.visibleSplashes[i]){
                const auto expected=originalRainSplashViewMatrix(view,fields.splashes[i].position,fsca);
                for(unsigned j=0;j<16;++j)equal(splashMatrices.at(splashIndex).elements[j],expected.elements[j],"Source splash draw matrix");++splashIndex;
            }
            equal(unsigned(splashMatrices.size()),splashIndex,"Splash draw count");
            std::vector<OriginalMatrix> mistMatrices;
            RefCpu mistDraw(m);mistDraw.r[4]=owner;mistDraw.r[15]=stack;mistDraw.pr=stop;
            for(unsigned i=0;i<16;++i)mistDraw.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
            mistDraw.callHooks[0xc05a8e0]=[&](auto& c){equal(c.r[5],6u,"Wheel mist source chunk");c.r[0]=material;};
            mistDraw.callHooks[0xc1d7120]=[&](auto& c){OriginalMatrix drawn;for(unsigned i=0;i<16;++i)drawn.elements[i]=std::bit_cast<float>(c.xf[i]);mistMatrices.push_back(drawn);};
            instructions+=mistDraw.run(0xc17f120,stop,50000);fields.advanceWheelMist();
            for(unsigned i=0;i<16;++i)equal(mistDraw.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Wheel mist draw restores matrix");
            unsigned mistIndex=0;
            for(unsigned i=0;i<72;++i){eqMist(m,mistAt+i*36,fields.wheelMist[i]);if(fields.drawnWheelMist[i].active==1){
                const auto expected=originalRainWheelMistViewMatrix(view,fields.drawnWheelMist[i]);
                for(unsigned j=0;j<16;++j)equal(mistMatrices.at(mistIndex).elements[j],expected.elements[j],"Wheel mist draw matrix");++mistIndex;
            }}
            equal(unsigned(mistMatrices.size()),mistIndex,"Wheel mist draw count");
            const unsigned night=frame%3;m.write32(owner+56,night);
            RefCpu publish(m);publish.r[4]=owner;publish.r[15]=stack;publish.pr=stop;
            unsigned draws=0;publish.callHooks[0xc1d5400]=[&](auto& c){equal(c.r[4],material+32,"Rain strip publication");c.r[0]=material;};
            publish.callHooks[0xc1d7120]=[&](auto&){++draws;};instructions+=publish.run(0xc17ebe0,stop,20000);
            equal(draws,1u,"Rain strip draw count");
            const unsigned vertices=m.read32(material+16)+32;
            for(unsigned i=0;i<25;++i){const auto quad=fields.streakQuad(i,night);for(unsigned j=0;j<4;++j){
                const unsigned at=vertices+(i*4+j)*32;eqPoint(m,at+4,quad[j].position,"Rain published position");
                equal(m.readFloat(at+16),quad[j].u,"Rain U");equal(m.readFloat(at+20),quad[j].v,"Rain V");
                equal(m.read32(at+24),quad[j].argb,"Rain alpha");equal(m.read32(at)&0xe0000000,quad[j].stripControl,"Rain strip control");
            }}
            for(unsigned i=0;i<25;++i)eqStreak(m,records+i*92,fields.streaks[i]);
            for(unsigned i=0;i<9;++i)eqSplash(m,splashes+i*24,fields.splashes[i]);
            equal(m.read32(0xc37c778),random.state,"Shared field RNG");++cases;
        }
    }
    // Execute the complete17F7E0 owner: real virtual matrix access/copies,
    // actor-pointer fallback, contact gates, source wheel origins, magnitude,
    // all three particle pools, draw matrices, and final matrix-history copy.
    //17F500/17F260 execute as part of the same owner, including their actual
    // conditional RNG draws. Collision results and final publication are hooks.
    constexpr unsigned refs=owner+0x21000,carsAt=owner+0x22000,actorsAt=owner+0x28000,partsAt=owner+0x2a000;
    for(unsigned scenario=0;scenario<12;++scenario){
        Random random{next()};setup(m,random.state);m.zeroRegion(owner+0x20000,0x10000);
        OriginalRainOwner native;std::array<OriginalRainCar,2> cars;
        std::array<OriginalRainGlareCar,2> glareCars;
        constexpr unsigned cameraAt=owner+0x2e000,collision=0xcaae2a0;
        m.zeroRegion(collision,256);m.write32(owner+272,cameraAt);
        const std::array<float,3> plane{0,-.70710677f,-.70710677f};put(m,cameraAt+192,plane);
        m.write32(owner+24,records);m.write32(owner+28,mesh);m.write32(owner+40,splashes);m.write32(owner+48,mistAt);
        m.write32(owner+64,refs);m.write32(owner+68,refs+4);
        for(unsigned i=0;i<25;++i)m.write32(records+i*92+84,100);
        for(unsigned i=0;i<9;++i)m.write32(splashes+i*24+12,100);
        for(unsigned i=0;i<72;++i)m.write32(mistAt+i*36+24,100);
        m.write32(material+16,material+256);m.write32(owner+32,material);
        for(unsigned car=0;car<2;++car){
            const unsigned at=carsAt+car*0x2000,actor=actorsAt+car*0x800,parts=partsAt+car*0x1000;
            m.write32(at+0x95c,actor);m.write32(at+0x960,scenario%2?actor+256:0);
            m.write32(at+0x964+64,0xc380b8c);m.write32(at+0x2f0,parts);
            const unsigned id=(scenario*3+car*17)%35;m.write32(at+0x34c,id);
            const unsigned record=0xc32437c+id*16,anchorAt=m.read32(record);
            glareCars[car].rearCount=m.read32(record+8);
            for(unsigned lamp=0;lamp<glareCars[car].rearCount;++lamp)for(unsigned axis=0;axis<3;++axis)
                glareCars[car].rearAnchors[lamp][axis]=m.readFloat(anchorAt+lamp*12+axis*4);
            makeRainGlareChunk(m,material+0x1000+car*0x400,car);
            OriginalCarParts anchors;
            anchors.transforms[14].translation={-.7f,.25f,1.15f};anchors.transforms[15].translation={-.8f,.32f,-1.22f};
            cars[car].effectOrigins=originalCarEffectOrigins(anchors);
            unsigned p=parts;for(const auto& transform:anchors.transforms)for(const auto* values:{&transform.scale,&transform.rotationDegrees,&transform.translation})
                for(float value:*values){m.writeFloat(p,value);p+=4;}
        }
        native.reset(cars);
        for(unsigned frame=0;frame<180;++frame){
            for(unsigned car=0;car<2;++car){
                const unsigned at=carsAt+car*0x2000,actor=actorsAt+car*0x800;
                auto& input=cars[car];input.available=car==0||(scenario%3!=0&&frame%61<52);
                m.write32(refs+car*4,input.available?at:0);
                input.actorPosition={float(frame)*(.03f+.015f*scenario),float(car)+float(frame%7)*.006f,-15.f-float(frame)*.012f+float(car)*5};
                if(frame%41<4)input.actorPosition[0]=float(frame/41)*3.f;
                input.world=originalIdentityMatrix();translateOriginalMatrix(input.world,input.actorPosition);
                translateOriginalMatrix(input.world,{0,-.02f,0});rotateOriginalMatrixPhase(input.world,1,std::uint16_t(frame*131+car*16384),fsca);
                put(m,at+0x964,input.world);put(m,at+0x9a8,input.actorPosition);
                auto& glare=glareCars[car];glare.available=input.available;glare.world=input.world;
                glare.blindAttack=(frame/13+car)%2;glare.braking=(frame/7+car)%2;
                m.write8(at+80,glare.braking?1:0);m.write32(actor+80,scenario%2?0x10000:glare.blindAttack?0x10000:0);
                m.write32(actor+256+80,glare.blindAttack?0x10000:0);
                input.wheelSurfaces.fill(0x30);
                if(frame%17<3)input.wheelSurfaces[(frame+car)%4]=0x26;
                if(frame%23<4)input.wheelSurfaces[(frame+car+2)%4]=0x47;
                for(unsigned wheel=0;wheel<4;++wheel){
                    m.write8(actor+116+wheel,scenario%2?0x57:input.wheelSurfaces[wheel]);
                    m.write8(actor+256+116+wheel,input.wheelSurfaces[wheel]);
                }
            }
            auto view=originalIdentityMatrix();translateOriginalMatrix(view,{float(frame)*-.013f,-2.f,5.f});
            rotateOriginalMatrixPhase(view,1,std::uint16_t(frame*79+scenario*2107),fsca);
            const unsigned night=frame%3;m.write32(owner+56,night);
            native.preserveStreakMotion=frame%29<5?2:0;m.write32(owner+276,native.preserveStreakMotion);
            std::array<bool,2> expectedGlare{true,true};for(unsigned car=0;car<2;++car)if(cars[car].available)
                for(auto surface:cars[car].wheelSurfaces)if((surface&15)==6||(surface&15)==7)expectedGlare[car]=false;
            RefCpu cpu(m);cpu.r[4]=owner;cpu.r[15]=stack;cpu.pr=stop;cpu.fscaHalfWave=fscaWords;
            for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
            unsigned rainDraws=0,splashDraws=0,mistDraws=0,currentChunk=0;
            std::vector<OriginalMatrix> splashMatrices,mistMatrices;
            struct GlareDraw {unsigned chunk,alpha;OriginalMatrix matrix;};std::vector<GlareDraw> glareDraws;
            std::vector<std::array<float,3>> queryStarts,queryEnds;
            cpu.callHooks[0xc05a8e0]=[&](auto& c){currentChunk=c.r[5];c.r[0]=currentChunk>=7?material+0x1000+(currentChunk-7)*0x400:material;};
            cpu.callHooks[0xc1d5400]=[&](auto& c){currentChunk=99;c.r[0]=material;};
            cpu.callHooks[0xc1d7120]=[&](auto& c){
                OriginalMatrix drawn;for(unsigned i=0;i<16;++i)drawn.elements[i]=std::bit_cast<float>(c.xf[i]);
                if(currentChunk==99)++rainDraws;
                else if(currentChunk==4){++splashDraws;splashMatrices.push_back(drawn);}
                else if(currentChunk==6){++mistDraws;mistMatrices.push_back(drawn);}
                else if(currentChunk==7||currentChunk==8)glareDraws.push_back({currentChunk,m.read32(c.r[4]+268)>>24,drawn});
                else throw std::runtime_error("Unexpected rain-owner draw chunk");
            };
            const auto queryResult=[&](unsigned index){return OriginalRainGlareHit{(frame+index)%13!=0,(frame+index)%11==0?0x8000u:0u};};
            cpu.callHooks[0xc022d20]=[&](auto& c){
                equal(c.r[4],collision,"Combined owner glare query");const auto hit=queryResult(unsigned(queryStarts.size()));
                std::array<float,3> a{},b{};for(unsigned axis=0;axis<3;++axis){a[axis]=m.readFloat(collision+32+axis*4);b[axis]=m.readFloat(collision+44+axis*4);}
                queryStarts.push_back(a);queryEnds.push_back(b);m.write32(collision+28,hit.flags);c.r[0]=hit.hit?1:0;
            };
            instructions+=cpu.run(0xc17f7e0,stop,100000);
            unsigned queryIndex=0,glareIndex=0;native.advance(view,cars,random,[&](const auto& gates,const auto&,auto& rng){
                equal(unsigned(gates[0]),unsigned(expectedGlare[0]),"Native primary optical gate");
                equal(unsigned(gates[1]),unsigned(expectedGlare[1]),"Native rival optical gate");
                originalRainRearGlare(view,plane,night,glareCars,gates,[&](const auto& a,const auto& b){
                    for(unsigned axis=0;axis<3;++axis){equal(queryStarts.at(queryIndex)[axis],a[axis],"Combined glare query start");equal(queryEnds.at(queryIndex)[axis],b[axis],"Combined glare query end");}
                    return queryResult(queryIndex++);
                },rng,[&](unsigned,unsigned,unsigned chunk,const OriginalRainGlareDraw& draw){
                    const auto& expected=glareDraws.at(glareIndex++);equal(expected.chunk,chunk,"Combined glare draw order");
                    equal(expected.alpha,draw.alpha,"Combined glare alpha");
                    for(unsigned i=0;i<16;++i)equal(expected.matrix.elements[i],draw.viewMatrix.elements[i],"Combined glare draw matrix");
                });
            });
            equal(unsigned(queryStarts.size()),queryIndex,"Combined glare query count");equal(unsigned(glareDraws.size()),glareIndex,"Combined glare draw count");
            equal(rainDraws,unsigned(native.weatherAllowed[0]),"Rain field surface gate");
            eqPoint(m,owner+248,native.previousVisualPosition,"Prior visual origin");eqPoint(m,owner+260,native.streakMotion,"Preserved rain motion");
            for(unsigned car=0;car<2;++car){eqPoint(m,owner+72+car*12,native.previousActorPosition[car],"Prior raw actor origin");
                eqPoint(m,owner+96+car*12,native.actorMotion[car],"Raw actor motion");
                if(cars[car].available)for(unsigned i=0;i<16;++i)equal(m.readFloat(owner+120+car*64+i*4),native.previousWorld[car].elements[i],"Prior visual matrix");}
            equal(m.read32(owner+36),native.fields.streakCursor,"Owner streak cursor");equal(m.read32(owner+44),native.fields.splashCursor,"Owner splash cursor");
            equal(m.read32(owner+52),native.fields.wheelMistCursor,"Owner wheel-mist cursor");
            for(unsigned i=0;i<25;++i)eqStreak(m,records+i*92,native.fields.streaks[i]);
            unsigned si=0,mi=0;
            for(unsigned i=0;i<9;++i){eqSplash(m,splashes+i*24,native.fields.splashes[i]);if(native.fields.visibleSplashes[i]){
                const auto expected=originalRainSplashViewMatrix(view,native.fields.splashes[i].position,fsca);
                for(unsigned j=0;j<16;++j)equal(splashMatrices.at(si).elements[j],expected.elements[j],"Owner splash matrix");++si;
            }}
            for(unsigned i=0;i<72;++i){eqMist(m,mistAt+i*36,native.fields.wheelMist[i]);if(native.fields.drawnWheelMist[i].active==1){
                const auto expected=originalRainWheelMistViewMatrix(view,native.fields.drawnWheelMist[i]);
                for(unsigned j=0;j<16;++j)equal(mistMatrices.at(mi).elements[j],expected.elements[j],"Owner wheel mist matrix");++mi;
            }}
            equal(splashDraws,si,"Owner splash draw count");equal(mistDraws,mi,"Owner wheel-mist draw count");
            equal(m.read32(0xc37c778),random.state,"Full owner shared RNG order");
            for(unsigned i=0;i<16;++i)equal(cpu.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Owner caller matrix preserved");++cases;
        }
    }
    #include "original_rain_glare_checks.inl"
    #include "original_rain_init_checks.inl"
    std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<instructions<<" original instructions. Complete17F7E0 rain and17F500/17F260 glare match together;17E0E0 constructor, zero allocation payload, initial histories and material edits match. Collision/resource/publication services are boundaries. Integrated ending remains separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
