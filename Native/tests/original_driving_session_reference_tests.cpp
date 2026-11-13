#include "original_driving_session.h"
#include "original_driving_float.h"
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
    // The scalar opcode interpreter uses host FP instructions. Explicitly
    // execute it in the captured original game's nearest/flush mode too.
    const OriginalDrivingFloatScope originalExecutionMode;
    if(argc!=4&&argc!=5)throw std::invalid_argument("Usage: original_driving_session_reference_tests native_root canonical_image primary_fsca_header [--handling-matrix]");
    const bool matrix=argc==5&&std::string(argv[4])=="--handling-matrix";
    if(argc==5&&!matrix)throw std::invalid_argument("Unknown session reference option");
    const std::filesystem::path root=argv[1];RefMemory memory{std::filesystem::path(argv[2])};
    const auto data=OriginalPhysicsData::load(root/"data/original_physics/tables.bin");
    std::ifstream primary(argv[3]);const std::string source{std::istreambuf_iterator<char>(primary),{}};
    const std::regex pattern("0x([0-9A-Fa-f]{8})");std::vector<std::uint32_t> halfWave;
    for(std::sregex_iterator it(source.begin(),source.end(),pattern),end;it!=end;++it)halfWave.push_back(std::stoul((*it)[1].str(),nullptr,16));
    if(halfWave.size()!=32768)throw std::runtime_error("Independent FSCA source count mismatch");
    std::size_t comparisons=0,instructions=0,impactRecords=0,acceptedContacts=0,suppressedContacts=0;
    std::size_t wallFrames=0,cleanSteeringFrames=0,movingBrakeFrames=0,gearChanges=0;
    float peakSpeed=0;
    // Isolate159720's final backup write set before the sequential tests.
    // Random suffixes/actor records ensure the native helper does not clear
    // data which the original copy/invalidation deliberately leaves intact.
    std::uint32_t rng=0x492A57C1;
    const auto random=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
    for(std::uint32_t sample=0;sample<512;++sample){
        memory.clear();memory.zeroRegion(0x0CFFF000,0x1000);
        OriginalDriveState drive;OriginalRecoveryState recovery;
        for(std::size_t i=0;i<drive.words.size();++i){
            drive.words[i]=random();recovery.drive0C9009F0.words[i]=random();
            memory.write32(0x0C900F00+std::uint32_t(i*4),drive.words[i]);
            memory.write32(0x0C9009F0+std::uint32_t(i*4),recovery.drive0C9009F0.words[i]);
        }
        for(auto& value:recovery.actor0C8FF580)value=random();
        const auto savedActor=recovery.actor0C8FF580;
        RefCpu cpu(memory);cpu.r[15]=cpu.r[14]=0x0CFFF800;
        instructions+=cpu.run(0x0C159870,0x0C15988A,3000);
        initializeOriginalRecoveryBackup(drive,recovery);
        for(std::size_t i=0;i<drive.words.size();++i){
            ++comparisons;if(memory.read32(0x0C9009F0+std::uint32_t(i*4))!=recovery.drive0C9009F0.words[i])throw std::runtime_error("Outer recovery backup mismatch");
        }
        comparisons+=42;if(recovery.actor0C8FF580!=savedActor)throw std::runtime_error("Outer backup altered actor");
    }
    const unsigned scenarios=matrix?70:4,frames=matrix?900:600;
    for(unsigned scenario=0;scenario<scenarios;++scenario){
        const bool contactAudio=matrix?true:scenario>=2;
        const unsigned variant=scenario%2,car=matrix?scenario/2:variant?7:0;
        const unsigned condition=matrix?(car*7+variant*9)%18:6+variant;
        OriginalDrivingSelection selection;selection.physics.conditionCode=condition;
        selection.physics.vehicleIndex=car;selection.collisionVariant=condition&1;
        if(matrix){
            selection.bodyContactEnabled=false;
            selection.physics.mode0C9015FC=variant;
            selection.physics.mode0C9015C0=condition>15?1:0;
            selection.physics.upgradeIndex0C9015F0=variant?63:0;
            // The tuned Trueno's original engine override is selected by its
            // profile package; other cars retain their own authored gearbox.
            selection.physics.overrideMode0C9015F4=variant&&car==0?1:0;
        }
        const auto path=data.loadPath(root/"data/original_physics",condition);
        auto position=path.points[0];position[1]+=1.0f;
        const auto& next=path.points[1];const std::array<float,3> angles{0,std::atan2(position[0]-next[0],position[2]-next[2]),0};
        OriginalDrivingSession session;session.reset(root,selection,position,angles);
        OriginalDrivingSession observedSession;std::vector<unsigned> callbackEvents;
        if(contactAudio){
            observedSession.reset(root,selection,position,angles);
            observedSession.setEngineOutput([&](const auto&,auto&){callbackEvents.push_back(8);},
                [&](unsigned cue){callbackEvents.push_back(cue);});
        }
        memory.clear();memory.zeroRegion(0x0C800000,0x800000);
        const auto raw=read(root/"data/original_physics"/originalCollisionFile(condition,selection.collisionVariant));
        constexpr std::uint32_t collisionBase=0x0CB00000,pathBase=0x0CFE0000,stack=0x0CFFF800,stop=0x0F000000;
        for(std::size_t i=0;i<raw.size();i+=4)memory.write32(collisionBase+std::uint32_t(i),word(raw,i));
        for(const auto offset:{12u,20u,28u,36u,44u})memory.write32(collisionBase+offset,collisionBase+word(raw,offset));
        memory.write32(0x0C2EEF6C,collisionBase);
        for(std::size_t i=0;i<path.points.size();++i)for(std::size_t j=0;j<3;++j)memory.writeFloat(pathBase+std::uint32_t(i*12+j*4),path.points[i][j]);
        memory.write32(0x0C901728,pathBase);memory.write32(0x0C901654,selection.physics.vehicleIndex);
        memory.write32(0x0C9015CC,selection.physics.conditionCode);
        memory.write32(0x0C9015FC,selection.physics.mode0C9015FC);
        memory.write32(0x0C9015C0,selection.physics.mode0C9015C0);
        memory.write32(0x0C9015F0,selection.physics.upgradeIndex0C9015F0);
        memory.write32(0x0C9015F4,selection.physics.overrideMode0C9015F4);
        memory.write32(0x0C2F4BC8,0);memory.write32(0x0C31FD44,0);
        for(std::size_t i=0;i<3;++i){memory.writeFloat(0x0CFD0000+std::uint32_t(i*4),position[i]);memory.writeFloat(0x0CFD0010+std::uint32_t(i*4),angles[i]);}
        memory.write32(0x0C98AD0C,0x00200000);memory.write32(0x0C98AD10,0x0CFD1000);memory.write32(0x0C98AD14,0x0CFD1000);
        //142520 uses its original disabled platform-service policy, as in the
        // lower-level CEC0 oracle. This does not bypass its numerical caller.
        memory.write8(memory.read32(0x0C142564),1);
        RefCpu boot(memory);boot.r[4]=0x0CFD0000;boot.r[5]=0x0CFD0010;boot.r[15]=stack;boot.pr=stop;
        std::size_t resets=0;boot.callHooks[0x0C15A380]=[&](RefCpu&){++resets;};
        instructions+=boot.run(0x0C1595C0,stop,20000);
        if(resets!=1)throw std::runtime_error("Missing original platform input reset");
        boot.r[15]=stack;boot.pr=stop;instructions+=boot.run(0x0C157A80,stop,3000);
        boot.r[15]=boot.r[14]=stack;instructions+=boot.run(0x0C159870,0x0C15988A,3000);
        constexpr std::uint32_t contactQueue=0x0D030000;
        if(contactAudio){
            // Run the actual PACK22 wrapper and 1435C0 queue, rather than
            // substituting the native gate into the reference executable.
            memory.zeroRegion(contactQueue,0x1000);memory.write32(contactQueue+40,0x0C31EC1C);
            memory.write32(0x0C8FF1CC,contactQueue);memory.write8(0x0C31DE55,0);
            session.setContactSoundQueueActive(true);
        }
        std::uint32_t previousGear=0;
        for(std::uint32_t tick=0;tick<frames;++tick){
            OriginalVehicleInputs in;in.calibration={128,32,32};in.automaticMode=!matrix||!variant;in.gearEnabled=true;
            const int steering=tick>=120&&tick<220?6:tick>=220&&tick<320?-6:0;
            in.analog={std::uint16_t((128+steering)<<8),std::uint16_t((tick>=40&&tick<420?171:64)<<8),std::uint16_t((tick>=420&&tick<520?171:64)<<8)};
            in.pressedByte=tick==200?0x10:tick==300?0x20:0;
            if(matrix){
                // Real ADC values, shared by the two independent executions:
                // accelerate, corner both ways, lift, brake while steering,
                // opposite lock, partial pedals, then recover under throttle.
                const int lock=tick<240?0:tick<330?20:tick<420?-20:tick<510?80:tick<600?-80:tick<720?8:0;
                const int throttle=tick<30||tick>=420&&tick<510?64:tick>=600&&tick<720?118:171;
                const int brake=tick>=420&&tick<510?171:tick>=510&&tick<600?118:64;
                in.analog={std::uint16_t((128+lock)<<8),std::uint16_t(throttle<<8),std::uint16_t(brake<<8)};
                in.pressedByte=tick==30||tick==120||tick==210||tick==300||tick==720?0x20:tick==450||tick==540?0x10:0;
            }
            memory.write16(0x0C92F0E0,in.analog.steering);memory.write16(0x0C92F0E2,in.analog.throttle);memory.write16(0x0C92F0E4,in.analog.brake);
            memory.write32(memory.read32(0x0C15D0E4),in.calibration.steeringWord);
            memory.write32(memory.read32(0x0C15D0E8),in.calibration.throttleWord);
            memory.write32(memory.read32(0x0C15D0EC),in.calibration.brakeWord);
            memory.write8(0x0C92ED40,in.pressedByte);memory.write32(0x0C9015C4,in.automaticMode?1:0);
            memory.write32(0x0C9008F4,memory.read32(0x0C9008F4)|0x8000u);
            memory.write32(0x0C92DE30,10000+tick);memory.write8(0x0C92ED00,tick%120==0?0x80:0);
            const auto impactPositionPointer=memory.read32(0x0C91FB20),impactMagnitudePointer=memory.read32(0x0C91FB24),impactTickPointer=memory.read32(0x0C91FB28);
            RefCpu cpu(memory);cpu.fscaHalfWave=halfWave;cpu.r[15]=cpu.r[14]=stack;
            std::vector<std::uint32_t> cues,sourceEvents;std::size_t diagnostics=0,audio=0;
            OriginalContactCompletionEffects expectedAudio;
            if(contactAudio){
                cpu.callHooks[0x0C1ED9C0]=[&](RefCpu& c){
                    if((c.r[4]&0xffff)!=0x02a9)throw std::runtime_error("Unexpected source contact command");
                    cues.push_back(c.r[4]>>16);
                    sourceEvents.push_back(c.r[4]>>16);
                };
                cpu.callHooks[0x0C2223B8]=[](RefCpu& c){c.fpul=c.r[4]/c.r[5];};
            }else cpu.callHooks[0x0C142460]=[&](RefCpu& c){cues.push_back(c.r[4]);};
            cpu.callHooks[0x0C055D60]=[&](RefCpu&){++diagnostics;};
            cpu.callHooks[0x0C142860]=[&](RefCpu& c){++audio;sourceEvents.push_back(8);expectedAudio.engineChannel=c.r[4];expectedAudio.gear=c.r[5];expectedAudio.engineValue=c.getFloat(4);expectedAudio.engineScale=c.getFloat(5);expectedAudio.throttle=c.getFloat(6);};
            // Explicit single-player boundary: inactive original pair result.
            // The actual following response code still executes unchanged.
            memory.write32(0x0C401B2C,0);cpu.r[10]=0x0C900F00;cpu.r[11]=0x0C401B04;
            instructions+=cpu.run(0x0C1578C4,0x0C157A06,3000);
            cpu.r[15]=stack;cpu.pr=stop;
            instructions+=cpu.run(0x0C157AE0,stop,3000000);
            // Original outer wrapper, with the auxiliary actor disabled.
            cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0x0C157A80,stop,3000);
            cpu.r[14]=cpu.r[15]=stack;instructions+=cpu.run(0x0C159952,0x0C1599CA,6000);
            session.setPlatformFrame(10000+tick,tick%120==0?0x80:0);
            const auto before=session.checkpoint();
            const auto effects=session.tick(in);
            if(contactAudio){
                const auto after=session.rollbackDigest();session.restore(before);
                const auto repeated=session.tick(in);
                if(after!=session.rollbackDigest()||effects.acceptedContactCues!=repeated.acceptedContactCues)
                    throw std::runtime_error("Rollback changed contact queue/RNG or accepted cues");
                callbackEvents.clear();observedSession.setPlatformFrame(10000+tick,tick%120==0?0x80:0);
                const auto observed=observedSession.tick(in);
                if(callbackEvents!=sourceEvents||observed.acceptedContactCues!=effects.acceptedContactCues||
                    observedSession.rollbackDigest()!=session.rollbackDigest())
                    throw std::runtime_error("Bound audio callbacks changed source ordering, accepted cues or physics");
            }
            const auto equal=[&](std::uint32_t expected,std::uint32_t actual,const char* label){
                ++comparisons;if(expected!=actual){std::cerr<<"car="<<car<<" condition="<<condition<<" variant="<<variant<<" tick="<<tick<<" "<<label<<" expected="<<hex(expected)<<" actual="<<hex(actual)<<'\n';throw std::runtime_error("Integrated original frame differential mismatch");}
            };
            const auto at=[&](std::uint32_t address,std::uint32_t actual){
                ++comparisons;const auto expected=memory.read32(address);if(expected!=actual){std::cerr<<"car="<<car<<" condition="<<condition<<" variant="<<variant<<" tick="<<tick<<" address="<<hex(address)<<" expected="<<hex(expected)<<" actual="<<hex(actual)<<'\n';throw std::runtime_error("Integrated original state differential mismatch");}
            };
            const auto& v=session.vehicle();const auto& g=v.transmissionGlobals;const auto& h=v.tail;const auto& c=v.controls;
            peakSpeed=std::max(peakSpeed,v.drive.f(0x238));
            wallFrames+=v.drive.u(0x150)!=0;
            cleanSteeringFrames+=v.drive.u(0x150)==0&&v.drive.f(0x238)>20.f&&std::abs(v.drive.f(0x1CC))>.1f;
            movingBrakeFrames+=v.drive.f(0x238)>5.f&&v.drive.f(0x1C4)>.1f;
            gearChanges+=previousGear!=v.transmission.gear00;previousGear=v.transmission.gear00;
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
            auto nativeCues=effects.feedback142460;if(effects.completion.requestCue4)nativeCues.push_back(4);
            if(contactAudio){
                suppressedContacts+=nativeCues.size()-effects.acceptedContactCues.size();
                nativeCues=effects.acceptedContactCues;acceptedContacts+=nativeCues.size();
                equal(memory.read32(contactQueue+48),std::uint32_t(session.contactSoundCooldown()),"contact queue cooldown");
            }
            equal(std::uint32_t(cues.size()),std::uint32_t(nativeCues.size()),"cue count");for(std::size_t i=0;i<cues.size();++i)equal(cues[i],nativeCues[i],"cue id");
            equal(std::uint32_t(diagnostics),effects.invalidScalarDiagnostics,"diagnostics");equal(1,std::uint32_t(audio),"audio call count");
            equal(expectedAudio.engineChannel,effects.completion.engineChannel,"audio channel");equal(expectedAudio.gear,effects.completion.gear,"audio gear");
            equal(bits(expectedAudio.engineValue),bits(effects.completion.engineValue),"audio RPM");equal(bits(expectedAudio.throttle),bits(effects.completion.throttle),"audio throttle");
            for(std::size_t i=0;i<effects.newImpactRecords.size();++i){
                const auto& record=effects.newImpactRecords[i];for(std::size_t j=0;j<3;++j)at(impactPositionPointer+std::uint32_t(i*12+j*4),bits(record.position[j]));
                at(impactMagnitudePointer+std::uint32_t(i*4),bits(record.magnitude));at(impactTickPointer+std::uint32_t(i*4),record.tick);++impactRecords;
            }
            if(contactAudio){
                RefCpu outer(memory);outer.r[4]=contactQueue;outer.r[15]=stack;outer.pr=stop;
                instructions+=outer.run(0x0C143680,stop,100);session.finishSoundFrame({});
                observedSession.finishSoundFrame({});
                equal(memory.read32(contactQueue+48),std::uint32_t(session.contactSoundCooldown()),"outer contact queue tick");
            }
        }
        if(matrix)std::cout<<"MATCH car="<<car<<" condition="<<condition<<" wet="<<variant<<" upgrade="<<selection.physics.upgradeIndex0C9015F0<<" automatic="<<(!variant)<<" frames="<<frames<<std::endl;
    }
    if(!acceptedContacts||!suppressedContacts)throw std::runtime_error("Session fixture missed contact acceptance/suppression");
    if(matrix&&(!wallFrames||!cleanSteeringFrames||!movingBrakeFrames||!gearChanges))
        throw std::runtime_error("Handling matrix missed a driving/contact/braking/shifting condition");
    if(matrix)std::cout<<"COVERAGE wall_frames="<<wallFrames<<" clean_steering_above72kmh_frames="<<cleanSteeringFrames
        <<" moving_brake_frames="<<movingBrakeFrames<<" gear_changes="<<gearChanges<<" peak_speed_kmh="<<peakSpeed*3.6f<<'\n';
    std::cout<<"PASS 512 outer backup cases and "<<scenarios*frames<<" sequential complete original player session frames after actual1595C0 plus159720 publication/backup startup, "<<comparisons<<" bit comparisons, "<<instructions
        <<" original instructions, "<<impactRecords<<" impact records. "<<(matrix?"70 paired scenarios across35 cars/all18 course-directions, dry AT stock/wet MT tuned (not their full Cartesian product); ":"Two actual Akina datasets/cars; ")<<"source contact queue, "<<acceptedContacts<<" accepted and "<<suppressedContacts<<" suppressed contact cues, exact RNG/steering state and rollback. Engine/diagnostic/digital-reset hooks and inactive rival-pair boundary remain; enabled contact path executes actual queue/RNG instructions with final sound-command sink and integer quotient hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
