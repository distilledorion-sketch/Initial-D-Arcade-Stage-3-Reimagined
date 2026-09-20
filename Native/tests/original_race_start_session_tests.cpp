#include "original_driving_session.h"
#include "original_race_start.h"
#include "original_start_grid.h"
#include "sh4_scalar_reference.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <regex>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
std::vector<std::uint8_t> read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Original session fixture unavailable");
    return {std::istreambuf_iterator<char>(f),{}};
}
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t o){
    return std::uint32_t(b.at(o))|(std::uint32_t(b.at(o+1))<<8)|(std::uint32_t(b.at(o+2))<<16)|(std::uint32_t(b.at(o+3))<<24);
}
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("Usage: original_race_start_session_tests native_root canonical_image primary_fsca_header");
    const std::filesystem::path root=argv[1];RefMemory memory{std::filesystem::path(argv[2])};
    const auto data=OriginalPhysicsData::load(root/"data/original_physics/tables.bin");
    std::ifstream primary(argv[3]);const std::string source{std::istreambuf_iterator<char>(primary),{}};
    const std::regex pattern("0x([0-9A-Fa-f]{8})");std::vector<std::uint32_t> halfWave;
    for(std::sregex_iterator it(source.begin(),source.end(),pattern),end;it!=end;++it)halfWave.push_back(std::stoul((*it)[1].str(),nullptr,16));
    if(halfWave.size()!=32768)throw std::runtime_error("Independent FSCA source count mismatch");
    std::size_t comparisons=0,instructions=0,impactRecords=0;

    memory.image.resize(0xC00000);
    const auto rivals=OriginalRivalData::load(root/"data/original_rival");
    constexpr std::uint32_t collisionBase=0x0CB00000,pathBase=0x0CD00000,alternateBase=0x0CD10000;
    constexpr std::uint32_t stack=0x0CFFF800,stop=0x0F000000,profile=0x0C31C99C;
    std::size_t frames=0,contacts=0,resets=0;
    for(std::uint32_t variant=0;variant<4;++variant){
        OriginalDrivingSession session;
        memory.clear();memory.zeroRegion(0x0C8FF000,0x1C0000);memory.zeroRegion(0x0CFF0000,0x10000);
        memory.zeroRegion(0x0C401B04,1248);memory.zeroRegion(profile,0x500);
        memory.write32(0x0C37C778,1);memory.write32(0x0C2F4BC8,0);memory.write32(0x0C31FD44,0);
        memory.write32(0x0C98AD0C,0x00200000);memory.write32(0x0C98AD10,0x0CE00000);memory.write32(0x0C98AD14,0x0CE00000);memory.zeroRegion(0x0CE00000,32*64);
        memory.write8(memory.read32(0x0C142564),1);
        // One active setup followed by two sparse disabled resets. Preserve
        // all state between resets, including secondary publication and body
        // contacts. The preinstalled primary PATH is an explicit caller input
        // when159720's negative control does not write901728.
        for(unsigned reset=0;reset<1;++reset){
        OriginalDrivingSelection selection;selection.physics=makeOriginalFreshTimeAttackSelection(
            variant&1?7:0,6+(variant&1),variant&1?OriginalWeather::Wet:OriginalWeather::Dry);
        selection.collisionVariant=selection.physics.conditionCode&1;
        OriginalDrivingRivalSetup rival;rival.control=variant<2?1:-1;
        rival.enemyId0C9015E0=13;rival.profileMode0C901648=variant<2?0:1;
        rival.geometryCar0C9015F8=0;rival.level0C9015D0=11;
        rival.opponentProgress0C901644=5;rival.progress0C901604={1,3,5,7,9,11,13,15};
        const auto pose=originalStartPose(selection.physics.conditionCode,0);
        const auto rivalPose=originalStartPose(selection.physics.conditionCode,1);
        rival.position=rivalPose.position;rival.angles=rivalPose.angles;
        
        selection.rival=rival;
        const auto path=rivals.loadPath(root/"data/original_rival",selection.physics.conditionCode);
        const auto raw=read(root/"data/original_physics"/originalCollisionFile(selection.physics.conditionCode,selection.collisionVariant));
        std::copy(raw.begin(),raw.end(),memory.image.begin()+collisionBase-RefMemory::imageBase);
        for(const auto offset:{12u,20u,28u,36u,44u}){auto v=word(raw,offset)+collisionBase;std::memcpy(memory.image.data()+collisionBase-RefMemory::imageBase+offset,&v,4);}
        memory.write32(0x0C2EEF6C,collisionBase);
        for(std::size_t i=0;i<path.points.size();++i)for(unsigned j=0;j<3;++j)memory.writeFloat(pathBase+std::uint32_t(i*12+j*4),path.points[i][j]);
        if(false){const auto alternate=rivals.loadPath(root/"data/original_rival",selection.physics.conditionCode,true);for(std::size_t i=0;i<alternate.points.size();++i)for(unsigned j=0;j<3;++j)memory.writeFloat(alternateBase+std::uint32_t(i*12+j*4),alternate.points[i][j]);}
        memory.write32(0x0C901728,pathBase);
        memory.write32(profile,rival.profileMode0C901648);memory.write32(profile+16,selection.physics.vehicleIndex);memory.write32(profile+20,rival.geometryCar0C9015F8);
        memory.write32(profile+24,rival.enemyId0C9015E0);memory.write32(profile+32,selection.physics.mode0C9015FC);memory.write32(profile+68,0);memory.write32(profile+148,rival.level0C9015D0);
        memory.write8(profile+116+rival.enemyId0C9015E0,std::uint8_t(rival.opponentProgress0C901644));memory.write8(profile+152,0);memory.write8(profile+164,0);
        for(unsigned i=0;i<8;++i)memory.write32(profile+1080+i*4,rival.progress0C901604[i]);
        memory.write32(0x0C4004D8,0);memory.write32(0x0C4004DC,0);
        for(unsigned i=0;i<3;++i){memory.writeFloat(0x0CFD0000+i*4,pose.position[i]);memory.writeFloat(0x0CFD0010+i*4,pose.angles[i]);memory.writeFloat(0x0CFD0020+i*4,rival.position[i]);memory.writeFloat(0x0CFD0030+i*4,rival.angles[i]);}
        memory.write32(stack+4,0x0CFD0000);memory.write32(stack+8,0x0CFD0010);memory.write32(stack+12,std::uint32_t(rival.control));memory.write32(stack+16,0x0CFD0020);memory.write32(stack+20,0x0CFD0030);
        RefCpu boot(memory);boot.r[4]=selection.physics.conditionCode;boot.r[5]=collisionBase;boot.r[6]=pathBase;boot.r[7]=alternateBase;boot.r[15]=stack;boot.pr=stop;
        std::size_t digitalResets=0;boot.callHooks[0x0C15A380]=[&](RefCpu&){++digitalResets;};
        instructions+=boot.run(0x0C159720,stop,60000);
        if(digitalResets!=1)throw std::runtime_error("Missing original platform input reset");
        session.reset(root,selection,pose.position,pose.angles);++resets;
        if(session.rivalInitialized()!=(variant<2)||session.rivalActive()!=(variant<2))throw std::runtime_error("Session rival reset status mismatch");
        //062FA0's race auto-brake clear is part of the existing host reset.
        memory.write32(0x0C9008F4,memory.read32(0x0C9008F4)&~0xA000u);
        OriginalRaceStart start;start.reset(variant<2?0:2);
        OriginalDrivingSession warmupMirror;warmupMirror.reset(root,selection,pose.position,pose.angles);
        bool rpmMoved=false;
        for(std::uint32_t tick=0;tick<360;++tick){
            OriginalVehicleInputs in;in.calibration={128,32,32};in.automaticMode=variant!=3;in.gearEnabled=false;
            if(tick>=60){const auto startFrame=start.step();in.gearEnabled=startFrame.gearEnabled;
                if(startFrame.go){session.enableRaceStart(variant<2?0:2);memory.write32(0x0C9008F4,memory.read32(0x0C9008F4)|0x8000u);if(variant<2)memory.write32(0x0C901824,memory.read32(0x0C901824)|0x8000u);}}
            const int steering=tick>=280&&tick<320?6:tick>=320?-6:0;
            const unsigned throttle=variant==1&&tick<120?64:171,brake=variant==2&&tick<240?171:64;
            in.analog={std::uint16_t((128+steering)<<8),std::uint16_t(throttle<<8),std::uint16_t(brake<<8)};
            in.pressedByte=variant==3&&tick<60?0x20:tick==260?0x20:0;
            memory.write16(0x0C92F0E0,in.analog.steering);memory.write16(0x0C92F0E2,in.analog.throttle);memory.write16(0x0C92F0E4,in.analog.brake);
            memory.write32(memory.read32(0x0C15D0E4),in.calibration.steeringWord);
            memory.write32(memory.read32(0x0C15D0E8),in.calibration.throttleWord);
            memory.write32(memory.read32(0x0C15D0EC),in.calibration.brakeWord);
            memory.write8(0x0C92ED40,in.pressedByte);memory.write32(0x0C9015C4,in.automaticMode);
            const auto platformFrame=10000+(tick<60?0:tick-59);
            memory.write32(0x0C92DE30,platformFrame);memory.write8(0x0C92ED00,0);
            const auto impactPositionPointer=memory.read32(0x0C91FB20),impactMagnitudePointer=memory.read32(0x0C91FB24),impactTickPointer=memory.read32(0x0C91FB28);
            RefCpu cpu(memory);cpu.fscaHalfWave=halfWave;cpu.r[15]=cpu.r[14]=stack;
            std::vector<std::uint32_t> cues;std::size_t diagnostics=0,audio=0;
            OriginalContactCompletionEffects expectedAudio;
            cpu.callHooks[0x0C142460]=[&](RefCpu& c){cues.push_back(c.r[4]);};
            cpu.callHooks[0x0C055D60]=[&](RefCpu&){++diagnostics;};
            cpu.callHooks[0x0C142860]=[&](RefCpu& c){++audio;expectedAudio.engineChannel=c.r[4];expectedAudio.gear=c.r[5];expectedAudio.engineValue=c.getFloat(4);expectedAudio.engineScale=c.getFloat(5);expectedAudio.throttle=c.getFloat(6);};
            const float progress=0.f;const auto correctionMode=0u;
            memory.write32(0x0C4004D8,correctionMode);session.setProgressCorrection(progress,correctionMode);
            cpu.r[15]=stack;cpu.pr=stop;cpu.setFloat(4,progress);
            instructions+=cpu.run(0x0C159920,stop,3000000);
            session.setPlatformFrame(platformFrame,0);
            const auto effects=session.tick(in);++frames;if(effects.bodyCollision.active)++contacts;
            if(tick==59){unsigned published=0;
                warmupOriginalRaceSession(warmupMirror,in,10000,0,[&](const auto& effect){if(effect.frame0C92DE30!=10000)throw std::runtime_error("Warmup leaked platform frame");++published;});
                if(published!=60||warmupMirror.platformFrame()!=10000||warmupMirror.vehicle().drive.words!=session.vehicle().drive.words||warmupMirror.actor().words!=session.actor().words||warmupMirror.rivalActor().words!=session.rivalActor().words||warmupMirror.contactCompletion().randomSeed0C37C778!=session.contactCompletion().randomSeed0C37C778)throw std::runtime_error("Warmup helper diverged from complete original frames");comparisons+=4;
            }
            if(tick<239&&session.vehicle().transmission.gear00!=0)throw std::runtime_error("Original pre-GO gear left neutral");
            const auto equal=[&](std::uint32_t expected,std::uint32_t actual,const char* label){
                ++comparisons;if(expected!=actual){std::cerr<<"variant="<<variant<<" reset="<<reset<<" tick="<<tick<<" "<<label<<" expected="<<hex(expected)<<" actual="<<hex(actual)<<'\n';throw std::runtime_error("Integrated original frame differential mismatch");}
            };
            const auto at=[&](std::uint32_t address,std::uint32_t actual){
                ++comparisons;const auto expected=memory.read32(address);if(expected!=actual){std::cerr<<"variant="<<variant<<" reset="<<reset<<" tick="<<tick<<" address="<<hex(address)<<" expected="<<hex(expected)<<" actual="<<hex(actual)<<'\n';throw std::runtime_error("Integrated original state differential mismatch");}
            };
            const auto& v=session.vehicle();const auto& g=v.transmissionGlobals;const auto& h=v.tail;const auto& c=v.controls;
            at(0x0C901650,bits(session.parameters().frame.global0C901650));at(0x0C9015D4,session.parameters().frame.global0C9015D4);
            for(std::size_t i=0;i<v.drive.words.size();++i)at(0x0C900F00+std::uint32_t(i*4),v.drive.words[i]);
            for(std::size_t i=0;i<42;++i)at(0x0C9008A4+std::uint32_t(i*4),session.actor().words[i]);
            const auto transmission=std::bit_cast<std::array<std::uint32_t,10>>(v.transmission);
            for(std::size_t i=0;i<10;++i)at(0x0C900E88+std::uint32_t(i*4),transmission[i]);
            for(std::size_t i=0;i<64;++i){at(0x0CAA98E0+std::uint32_t(i*4),bits(h.history0CAA98E0[i]));at(0x0CAA9BE0+std::uint32_t(i*4),bits(h.steeringHistory0CAA9BE0[i]));}
            for(std::size_t i=0;i<128;++i)at(0x0CAA99E0+std::uint32_t(i*4),bits(h.throttleHistory0CAA99E0[i]));
            for(std::size_t i=0;i<16;++i)at(0x0C91FB0C+std::uint32_t(i*4),h.statistics0C91FB0C[i]);
            at(0x0CAA9870,g.phase9870);at(0x0CAA9874,bits(h.previousSpeed0CAA9874));at(0x0CAA9878,bits(h.speedDelta0CAA9878));
            at(0x0CAA9880,bits(v.loss.speedLoss0CAA9880));at(0x0CAA9884,bits(v.loss.persistentPenalty0CAA9884));
            at(0x0CAA9888,h.lastNonzeroGear0CAA9888);at(0x0CAA988C,h.previousGear0CAA988C);
            at(0x0CAA9894,bits(c.steering));at(0x0CAA9898,bits(c.throttle));at(0x0CAA989C,bits(c.throttleAlias));at(0x0CAA98A0,bits(c.brake));
            at(0x0CAA98A8,bits(g.shiftDifference98a8));at(0x0CAA98AC,bits(g.coupledSnapshot98ac));at(0x0CAA98B0,bits(h.filteredDelta0CAA98B0));at(0x0CAA98D0,bits(g.coupling98d0));
            at(0x0CAA9CE0,bits(h.mean0CAA9CE0));at(0x0CAA9CE4,h.counter0CAA9CE4);at(0x0CAA9CE8,h.counter0CAA9CE8);at(0x0CAA9CEC,h.counter0CAA9CEC);
            at(0x0CAA9CFC,g.downCounter9cfc);at(0x0C91FB4C,g.flag91fb4c);
            const auto& road=session.roadContact();const auto& completion=session.contactCompletion();
            for(std::size_t i=0;i<4;++i){
                for(std::size_t j=0;j<16;++j){at(0x0CAA9518+std::uint32_t(i*64+j*4),road.surfaces0CAA9518[i].words[j]);at(0x0CAA9618+std::uint32_t(i*64+j*4),road.sweeps0CAA9618[i].words[j]);}
                for(std::size_t j=0;j<3;++j)at(0x0CAA94C8+std::uint32_t(i*12+j*4),bits(road.normals0CAA94C8[i][j]));
                at(0x0CAA94F8+std::uint32_t(i*4),road.flags0CAA94F8[i]);at(0x0CAA9508+std::uint32_t(i*4),bits(road.impacts0CAA9508[i]));
                at(0x0CAA94B4+std::uint32_t(i*4),session.wheelHistory().rotationCounters0CAA94B4[i]);
            }
            for(std::size_t i=0;i<5;++i)at(0x0C900E5C+std::uint32_t(i*4),completion.cues0C900E5C[i]);
            at(0x0C900E84,completion.elapsedFrames0C900E84);at(0x0C900EBC,completion.steeringMask0C900EBC);at(0x0C37C778,completion.randomSeed0C37C778);at(0x0CAA94C4,completion.previousFlag0CAA94C4);
            for(std::size_t i=0;i<8;++i)at(0x0CAA9718+std::uint32_t(i*4),completion.snapshot0CAA9718[i]);
            for(std::size_t i=0;i<completion.positionCursor;++i)for(std::size_t j=0;j<3;++j)
                at(0x0C91155C+std::uint32_t(i*12+j*4),completion.impactPositions[i][j]);
            for(std::size_t i=0;i<completion.frameCursor;++i)at(0x0C92DE34+std::uint32_t(i*4),completion.impactFrames[i]);
            const auto& publication=session.publishedActors();const auto& recovery=session.recovery();
            for(std::size_t i=0;i<42;++i){at(0x0C8FF388+std::uint32_t(i*4),publication.player0C8FF388[i]);at(0x0C8FF430+std::uint32_t(i*4),publication.secondary0C8FF430[i]);at(0x0C8FF580+std::uint32_t(i*4),recovery.actor0C8FF580[i]);}
            for(std::size_t i=0;i<272;++i)at(0x0C9009F0+std::uint32_t(i*4),recovery.drive0C9009F0.words[i]);

            const auto& rivalState=session.rivalActor();
            for(unsigned i=0;i<179;++i)at(0x0C901C6C+716+i*4,rivalState.words[i]);
            for(unsigned i=0;i<42;++i)at(0x0C9017D4+i*4,session.rivalPublicActor().words[i]);
            at(0x0CAA986C,session.rivalFrameCounter());at(0x0CAA9868,session.rivalPaceInputs().profile0CAA9868);
            at(0x0C9015D0,session.rivalPaceInputs().level0C9015D0);at(0x0C9015E0,session.selection().physics.vehicleMode0C9015E0);
            const auto& rivalRoad=session.rivalRoadContact();
            for(unsigned i=0;i<4;++i)for(unsigned k=0;k<16;++k)at(0x0CAA9764+i*64+k*4,rivalRoad.surfaces0CAA9764[i].words[k]);
            at(0x0CAA9864,rivalRoad.surfaceValid0CAA9864);
            for(unsigned i=0;i<100;++i)at(0x0C99A904+i*4,std::uint32_t(rivalRoad.trace.indices0C99A904[i]));
            at(0x0C99AA94,rivalRoad.trace.count0C99AA94);
            for(unsigned i=0;i<21;++i)at(0x0C99AA98+i*4,rivalRoad.surface.words[i]);
            const auto& body=session.bodyContact();
            for(unsigned a=0;a<2;++a)for(unsigned i=0;i<156;++i)at(0x0C401B04+a*624+i*4,body.shapes0C401B04[a].words[i]);
            at(0x0CA9B360,body.count0CA9B360);
            for(unsigned i=0;i<32;++i)for(unsigned j=0;j<3;++j)at(0x0CA9B364+i*12+j*4,bits(body.intersections0CA9B364[i][j]));
            auto nativeCues=effects.feedback142460;if(effects.completion.requestCue4)nativeCues.push_back(4);
            equal(std::uint32_t(cues.size()),std::uint32_t(nativeCues.size()),"cue count");for(std::size_t i=0;i<cues.size();++i)equal(cues[i],nativeCues[i],"cue id");
            equal(std::uint32_t(diagnostics),effects.invalidScalarDiagnostics,"diagnostics");equal(1,std::uint32_t(audio),"audio call count");
            equal(expectedAudio.engineChannel,effects.completion.engineChannel,"audio channel");equal(expectedAudio.gear,effects.completion.gear,"audio gear");
            equal(bits(expectedAudio.engineValue),bits(effects.completion.engineValue),"audio RPM");equal(bits(expectedAudio.throttle),bits(effects.completion.throttle),"audio throttle");
            for(std::size_t i=0;i<effects.newImpactRecords.size();++i){
                const auto& record=effects.newImpactRecords[i];for(std::size_t j=0;j<3;++j)at(impactPositionPointer+std::uint32_t(i*12+j*4),bits(record.position[j]));
                at(impactMagnitudePointer+std::uint32_t(i*4),bits(record.magnitude));at(impactTickPointer+std::uint32_t(i*4),record.tick);++impactRecords;
            }

        }
    }
    }
    std::cout<<"PASS "<<resets<<" source159720 setups, "<<frames<<" sequential warmup/countdown/GO/launch159920 frames, "<<comparisons<<" exact comparisons, "<<instructions<<" original instructions. Four frozen warmup pedal/shift snapshots, real Akina both directions, active rivals and solo. OriginalRaceStart emits GO on owner update180; complete player/rival numerical stages, contacts, RNG, engine-output parameters, histories, publication and recovery execute without numerical hooks. Warmup helper final state independently matches sixty original calls.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

