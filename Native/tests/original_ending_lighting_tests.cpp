#include "original_car_lighting.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Native root and canonical image required");
    const auto fscaPath=std::filesystem::path(argv[1])/"data/original_physics/fsca_table.bin";
    const auto trig=OriginalFscaTable::load(fscaPath);
    std::ifstream f(fscaPath,std::ios::binary);std::vector<unsigned>wave(32768);f.seekg(16);f.read(reinterpret_cast<char*>(wave.data()),131072);
    if(!f)throw std::runtime_error("FSCA input");
    RefMemory m(argv[2]);
    constexpr unsigned owner=0xd000000,car=0xd010000,rival=0xd020000,course=0xd030000,
        frame=0xd040000,stack=0xd050000,tls=0xd060000,spot=0xd070000,
        courseArray=0xd080000,playerArray=0xd081000,rivalArray=0xd082000,matrixStack=0xce00000,stop=0xf000000;
    unsigned checks=0;std::size_t instructions=0;
    const auto equal=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b){std::cerr<<why<<" actual="<<std::hex<<a<<" expected="<<b<<std::dec<<'\n';throw std::runtime_error(why);}};
    const auto bits=[](float x){return std::bit_cast<unsigned>(x);};
    const auto reset=[&]{m.clear();m.zeroRegion(owner,0x100000);m.zeroRegion(matrixStack,0x10000);m.zeroRegion(0xc92f000,0x10000);
        m.write32(tls+4,tls+256);m.write32(tls+256,tls+256);m.write32(tls+260,tls+256);
        m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matrixStack);m.write32(0xc98ad14,matrixStack);
    };
    const auto run=[&](RefCpu&c,unsigned begin,unsigned end=0xf000000){c.r[15]=stack+0xf000;c.pr=stop;
        try{instructions+=c.run(begin,end,20000);}catch(const std::exception&e){throw std::runtime_error("entry "+hex(begin)+" pc "+hex(c.pc)+": "+e.what());}};
    // Execute the replay parent's ambient setter block, including the real
    // embedded ACar virtual and0354E0. This deliberately differs from0638C0.
    for(unsigned courseId=0;courseId<9;++courseId)for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet){
        reset();OriginalCarLightingSetup setup{courseId,0,bool(night),bool(wet)};
        const auto ambient=originalCourseCarAmbient(setup);
        m.write32(frame+4,owner+0x27c);m.write32(frame+8,owner+0xbc);m.write32(owner+0xbc+40,course);
        for(unsigned i=0;i<3;++i)m.writeFloat(course+12+4*i,ambient[i]);
        for(unsigned slot=0;slot<2;++slot){const auto actor=slot?rival:car,array=slot?rivalArray:playerArray;
            m.write32(owner+0x27c+12+4*slot,actor);m.write32(actor+0x958,0xc381184);m.write32(actor+0x9ec,array);
            RefCpu c(m);c.r[14]=frame;c.r[13]=slot*4;run(c,0xc0e2008,0xc0e2032);
            const auto native=originalReplayCarLighting(setup,originalLightIdentityMatrix,.75f,false);
            for(unsigned i=0;i<3;++i)equal(bits(native.ambient[i]),m.read32(array+72+4*i),"Replay ambient follows course header");
            equal(bits(native.gain),bits(.75f),"Replay retains explicit road gain at night too");
        }
    }
    // Execute night registration, observing its external light-list boundary.
    // The exact source routes the opposite car's spot and then the camera light.
    for(unsigned twoCars=0;twoCars<2;++twoCars){
        reset();m.write32(owner+0xe4,course);m.write32(owner+0x27c+36,car);m.write32(owner+0x27c+40,twoCars?rival:0);
        m.write32(course,course+0x100);m.write16(course+0x110,0);m.write32(course+0x114,0xf001000);
        for(auto actor:{car,rival}){m.write32(actor+0x958,0xc381184);m.write32(actor+0x9ec,actor==car?playerArray:rivalArray);}
        std::vector<std::pair<unsigned,unsigned>> registrations;
        RefCpu c(m);c.r[4]=owner;c.callHooks[0xc221fc0]=[](auto&q){q.r[0]=tls;};
        c.callHooks[0xc035120]=[](auto&){}; // Headlight motor request, not routing.
        c.callHooks[0xf001000]=[&](auto&q){registrations.emplace_back(courseArray,q.r[5]);};
        c.callHooks[0xc0539e0]=[&](auto&q){registrations.emplace_back(q.r[4],q.r[5]);};
        run(c,0xc0e4fc0,0xc0e507c);
        std::vector<std::pair<unsigned,unsigned>> expected{{courseArray,car+0x9f0}};
        if(twoCars){expected.emplace_back(rivalArray,car+0x9f0);expected.emplace_back(courseArray,rival+0x9f0);expected.emplace_back(playerArray,rival+0x9f0);}
        equal(registrations==expected,true,"Original replay cross-car spotlight routing");
        m.write32(frame+48,owner);m.write32(frame+60,owner+0x3bc);m.write32(owner+0x3f8,spot);
        m.write32(frame+72,owner+0x3bc);c.r[3]=owner+0x3bc;c.r[14]=frame;run(c,0xc0e5180,twoCars?0xc0e51e2:0xc0e5280);
        expected.emplace_back(courseArray,spot);expected.emplace_back(playerArray,spot);if(twoCars)expected.emplace_back(rivalArray,spot);
        equal(registrations==expected,true,"Camera spotlight appended to course and each car");
    }
    std::mt19937 random(195303);std::uniform_real_distribution<float> value(-1,1);
    for(unsigned wet=0;wet<2;++wet)for(unsigned sample=0;sample<128;++sample){
        reset();RefCpu c(m);c.fscaHalfWave=wave;c.callHooks[0xc221fc0]=[](auto&q){q.r[0]=tls;};
        c.r[4]=spot;run(c,0xc0549e0); // Real spotlight defaults.
        m.write32(frame+48,owner);m.write32(frame+60,owner+0x3bc);m.write32(owner+0x3f8,spot);m.write32(owner+0xf4,wet);
        c.r[14]=frame;run(c,0xc0e50e4,0xc0e5180);
        const auto view=originalActorMatrix({value(random)*1000,value(random)*100,value(random)*1000},
            {value(random)*.7f,value(random)*3.1f,value(random)*.2f},trig).elements;
        for(unsigned i=0;i<16;++i)c.xf[i]=bits(view[i]);
        c.r[12]=owner;c.r[14]=frame;run(c,0xc0e33d4,0xc0e3440);
        const auto light=originalReplayCameraLight(view,wet,trig);
        equal(light.enabled,m.read8(spot+24)!=0,"Camera light enabled");
        equal(bits(light.distance0),m.read32(spot+60),"Camera light near distance");equal(bits(light.distance1),m.read32(spot+64),"Camera light far distance");
        equal(light.angle0,m.read32(spot+84),"Camera light angle0");equal(light.angle1,m.read32(spot+88),"Camera light angle1");
        for(unsigned i=0;i<3;++i){equal(bits(light.color[i]),m.read32(spot+32+4*i),"Camera light color");
            equal(bits(light.position[i]),m.read32(spot+44+4*i),"Camera light source world position");
            equal(bits(light.incomingDirection[i]),m.read32(spot+68+4*i),"Camera light source direction");}
        for(unsigned i=0;i<16;++i)equal(c.xf[i],bits(view[i]),"Camera light update restores view");
        // Independently pack that source light, including its distance curve.
        // The scene resets the raw light bank before publishing a light list.
        // Actual1CF180 initializes raw+46 (the packed diffuse flag); a merely
        // zero-filled bank is not a valid source rendering state.
        run(c,0xc1cefe0);
        m.write32(courseArray+68,1);m.write32(courseArray+4,spot);m.writeFloat(courseArray+88,1);
        c.r[4]=courseArray;c.callHooks[0xc1cf0a0]=[](auto&){};run(c,0xc0538a0);
        c.callHooks.erase(0xc1cf0a0);c.callHooks[0xc1d0fc0]=[](auto&){};c.callHooks[0xc1d1000]=[](auto&){};c.callHooks[0xc1d3d00]=[](auto&){};
        OriginalCourseLighting native;native.count=1;native.lights[0]=light;
        const auto packet=originalCourseLightingPacket(native,view);
        unsigned packets=0;
        c.callHooks[0xc1d3a60]=[&](auto&q){if(m.read8(q.r[4]))return;
            RefCpu d(m);d.fscaHalfWave=wave;d.r[4]=q.r[4];d.r[5]=frame+0x8000;
            d.callHooks[0xc2223e0]=[](auto&x){x.fpul=x.r[4]/x.r[5];};m.write32(0xc99a16c,32);run(d,0xc1d3860);
            for(unsigned i=0;i<8;++i)equal(packet.lights[0][i],m.read32(frame+0x8000+4*(7-i)),"Camera light original packed packet");
            ++packets;q.pr=stop; // One registered light; end at its GPU submission boundary.
        };
        run(c,0xc1cf0a0);equal(packets,1,"One camera light packet");
        for(unsigned courseId=0;courseId<9;++courseId)for(unsigned night=0;night<2;++night){
            OriginalCarLightingSetup setup{courseId,0,bool(night),bool(wet)};
            const auto base=originalCourseLighting(courseId,night,wet);
            const auto p=originalReplayCarLighting(setup,view,.75f,true),r=originalReplayCarLighting(setup,view,.25f,false);
            const auto race=composeOriginalRaceLighting(base,p,r,setup),replay=composeOriginalReplayLighting(base,p,r,setup,light);
            equal(replay.course.count,race.course.count+night,"Replay course scope size");equal(replay.player.count,race.player.count+night,"Replay player scope size");equal(replay.rival.count,race.rival.count+night,"Replay rival scope size");
            if(night){equal(replay.course.lights[replay.course.count-1].sourceSlot,0x3f8,"Replay course camera slot");equal(replay.player.lights[replay.player.count-1].sourceSlot,0x3f8,"Replay player camera slot");equal(replay.rival.lights[replay.rival.count-1].sourceSlot,0x3f8,"Replay rival camera slot");}
        }
    }
    std::cout<<"PASS "<<checks<<" ending lighting checks / "<<instructions<<" original instructions; real replay ambient setter, cross-car/camera registration,256 camera light transforms and packed GPU records. TLS, lamp requests, registration boundaries and FF000000/256 division are explicit hooks; not full Flycast.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
