#include "original_engine_playback.h"
#include "original_contact_completion.h"
#include "original_menu_audio.h"
#include "original_tire_audio.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <set>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::size_t checks=0,instructions=0;
void equal(unsigned a,unsigned b,const std::string& message){++checks;if(a!=b)throw std::runtime_error(message+" native="+hex(a)+" source="+hex(b));}
void compareState(const OriginalEngineControlState& s,const RefMemory& m){
    const std::array<unsigned,17> words{std::bit_cast<unsigned>(s.previousThrottle),unsigned(s.fallingRpmFrames),unsigned(s.shiftFrames),unsigned(s.previousGear),
        std::bit_cast<unsigned>(s.shiftOffset),std::bit_cast<unsigned>(s.previousRpm),unsigned(s.auxiliaryLevel),unsigned(s.decayFrames),unsigned(s.heldVolume),
        unsigned(s.recoveryFrames),unsigned(s.decayLatched),unsigned(s.roadFrames),unsigned(s.backfireFrames),unsigned(s.previousRoadFrames),s.backfirePattern,0,0};
    for(unsigned i=0;i<10;++i)equal(words[i],m.read32(0xc2fb3a4+i*4),"engine state");
    equal(words[10],m.read8(0xc2fb3cc),"decay latch");equal(words[11],m.read32(0xc2fb3d0),"road frames");
    equal(words[12],m.read32(0xc2fbb1c),"backfire frames");equal(words[13],m.read32(0xc2fbb20),"previous road");equal(words[14],m.read16(0xca9b4e8),"backfire pattern");
    for(unsigned i=0;i<4;++i){equal(unsigned(s.lastVolume[i]),m.read32(0xca9b4ec+8+i*16),"volume cache");equal(unsigned(s.lastPitch[i]),m.read32(0xca9b4ec+12+i*16),"pitch cache");}
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("project-root canonical-image required");
    RefMemory m(argv[2]);constexpr unsigned stack=0xd020000,stop=0xf000000,owner=0xd000000,actor=0xd010000,queue=0xd030000,drive=0xc900f00;
    std::size_t profiles=0,frames=0,cues=0;std::set<unsigned> heard,effectBuses,sourceEffectBuses;
    for(unsigned car=0;car<35;++car)for(unsigned upgrade=0;upgrade<8;++upgrade)for(unsigned option:{0u,1u,2u,3u,127u,128u,255u})for(unsigned engine:{0u,1u}){
        m.clear();m.zeroRegion(stack,0x10000);OriginalBattleProfile p;p.setu(16,car);p.setByte(164,std::uint8_t(upgrade));p.setByte(152,std::uint8_t(option));p.setByte(162,std::uint8_t(engine));
        for(unsigned i=0;i<p.words.size();++i)m.write32(0xc31c99c+i*4,p.words[i]);
        const auto selected=selectOriginalEngineSound(p);RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;unsigned calls=0;
        c.callHooks[0xc1416a0]=[](auto& r){if(r.r[4]!=4)throw std::runtime_error("Not race sound scene");};
        c.callHooks[0xc142580]=[&](auto& r){equal(selected.family,r.r[4],"profile sound family");equal(unsigned(selected.level),r.r[5],"profile sound level");++calls;};
        instructions+=c.run(0xc067a00,stop,200);equal(calls,1,"profile selection call");++profiles;
    }
    m.clear();const auto selections=OriginalEngineBankSelections::load(argv[1]);
    for(unsigned f=0;f<36;++f)for(unsigned g=0;g<2;++g){const auto entry=m.read32(0xc2fb3d4+f*4)+g*24,params=m.read32(entry+4);
        equal(selections.families[f][g].instrument,m.read32(params),"source instrument");equal(selections.families[f][g].effect,m.read32(params+4),"source effect");
        auto at=m.read32(entry);std::string name;for(;m.read8(at);++at)name+=char(m.read8(at));
        if(name!="PACK"+std::to_string(selections.families[f][g].bank)+".bin")throw std::runtime_error("Source bank filename differs");
    }
    equal(selections.auxiliary.instrument,m.read32(0xc25d37c),"auxiliary instrument");
    //0C3E20 has three separate effect setup sites; execute their actual
    //parameter reloads and packers before comparing native voice routing.
    //Only the final command FIFO and its flush are host boundaries.
    OriginalEnginePlayback initialized(argv[1]);
    for(unsigned family=0;family<36;++family){
        OriginalBattleProfile profile;profile.setu(16,family==35?0:family);
        if(family==35)profile.setByte(164,5);
        initialized.select(profile);const auto& setup=initialized.initialization();
        m.clear();m.zeroRegion(stack,0x10000);m.write32(0xca9b530,family);
        const auto entry=m.read32(0xc2fb3d4+family*4),parameters=m.read32(entry+4);
        equal(setup.effectSelector,m.read32(parameters+4),"group0 DSP selector");
        equal(setup.loadedBankIndices[0],selections.families[family][0].bank,"first loaded engine bank");
        equal(setup.loadedBankIndices[1],10,"second loaded auxiliary bank");
        auto at=m.read32(entry);std::string name;for(;m.read8(at);++at)name+=char(m.read8(at));
        if(setup.loadedBankNames()!=std::array<std::string,2>{name,"PACK10.bin"})throw std::runtime_error("Native loaded bank names differ from source");
        for(unsigned channel:{0u,1u,3u}){
            m.write32(0xca9b4ec+52,channel);RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;
            c.r[9]=channel;c.r[8]=0xc1ed6a0;c.r[13]=0xc1ed840;std::vector<unsigned> words;
            c.callHooks[0xc1ed9c0]=[&](auto& r){words.push_back(r.r[4]);r.r[0]=0;};c.callHooks[0xc1db8c0]=[](auto&){};
            instructions+=c.run(channel==3?0xc0c40d2:0xc0c3f82,channel==3?0xc0c4114:0xc0c3fb8,1000);
            equal(unsigned(words.size()),5,"source engine effect command count");
            equal(setup.effectSelector,((words[0]>>8)&0x7f00)|((words[1]>>16)&127),"packed DSP selector");
            const auto& controls=initialized.channelControls(channel);
            //10A4/20A4 write GLOBAL effect-return level/pan. They do not
            //replace the ICS layer's input bus or its authored voice pan.
            equal((words[2]>>16)&15,channel==3?5:10,"global effect return level");
            equal((words[3]>>16)&127,64,"packed global effect return pan");
            equal(controls.effectBus,255,"native retains layer-derived input bus");
            equal(controls.pan,64,"native default voice pan remains unchanged");
            equal(controls.effectSend,(words[4]>>16)&127,"native initial voice effect send");
            const auto& voice=channel==3?selections.auxiliary:selections.families[family][channel];
            const auto bank=loadOriginalIcsBank(std::filesystem::path(argv[1])/"data/original_audio/continuous"/("PACK"+std::to_string(voice.bank)+".dtpk"));
            for(const auto& layer:bank.programs.at(voice.instrument&255).layers)sourceEffectBuses.insert(layer.header[1]&15);
        }
    }
    {
        OriginalBattleProfile profile;OriginalEnginePlayback reference(argv[1]),cleared(argv[1]);reference.select(profile);cleared.select(profile);
        OriginalEngineControlInput in;in.rpm=5000;in.throttle=1;in.gear=2;unsigned aSeed=991,bSeed=991;
        auto tick=[&](){reference.step(in,aSeed,{});cleared.step(in,bSeed,{});};
        auto samples=[&](bool expectCleared){bool audible=false;for(unsigned i=0;i<735;++i){auto a=reference.renderFrame(),b=cleared.renderFrame();if(a.dry!=b.dry)throw std::runtime_error("Engine DSP clear changed dry PCM");
            if(expectCleared&&b.effects!=std::array<std::int32_t,16>{})throw std::runtime_error("Source ordinary step restored cleared engine send");
            // A channel command restores its current layers. Cleared released
            // physical tails stay cleared until they expire (<160 samples).
            if(!expectCleared&&i>=200&&a.effects!=b.effects)throw std::runtime_error("Source effect command failed to restore engine routing");
            for(auto value:b.effects)audible|=value!=0;}
            if(!expectCleared&&!audible)throw std::runtime_error("Engine send restoration remained silent");};
        tick();samples(false);cleared.clearDspSends();tick();samples(true);
        // Four surface6 frames arm the original road effect; the fifth emits
        // explicit40A5(30), restoring the cleared hardware send.
        in.wheelSurface[0]=6;for(unsigned i=0;i<4;++i){tick();samples(true);}tick();samples(false);
        // Source repeats that same value while the effect remains active.
        cleared.clearDspSends();tick();samples(false);equal(aSeed,bSeed,"DSP clear left source RNG unchanged");
    }
    // Execute the original contact->142860->142800->0C4360 path, plus real
    // release/backfire wrappers,1435C0 queue and143680 cooldown. Only the final
    // sound command sinks and integer quotient are substituted. No hardware.
    for(unsigned car:{0u,1u,8u,19u,34u})for(unsigned upgrade:{0u,2u,6u}){
        m.clear();for(auto [at,size]:{std::pair{stack,0x10000u},{owner,0x10000u},{actor,0x10000u},{queue,0x1000u},{0xc900000u,0x2000u},{0xca9b000u,0x1000u},{0xcaa9400u,0x400u}})m.zeroRegion(at,size);
        OriginalBattleProfile p;p.setu(16,car);p.setByte(164,std::uint8_t(upgrade));p.setByte(162,1);p.setByte(166,1);
        for(unsigned i=0;i<p.words.size();++i)m.write32(0xc31c99c+i*4,p.words[i]);
        OriginalEnginePlayback native(argv[1]);native.select(p);const auto selected=selectOriginalEngineSound(p);
        m.write32(0xca9b52c,selected.family);m.write32(0xca9b530,selected.family);m.write32(0xca9b534,unsigned(selected.level));
        RefCpu cfg(m);cfg.r[15]=stack+0xf000;cfg.pr=stop;instructions+=cfg.run(0xc0c3c60,stop,500);
        // Game-profile correction: Levin A Step 2 has no turbo installed.
        // Raw 0C3C60 parity remains covered by original_engine_control_tests.
        if(car==1&&upgrade==2){m.write8(0xca9b538,0);m.write8(0xca9b539,0);}
        RefCpu reset(m);instructions+=reset.run(0xc0c4114,0xc0c4160,200);
        m.write32(0xc31de44,owner);m.write8(0xc31de55,0);m.write8(0xc31de54,0);m.write32(0xc900954,actor);
        m.write32(owner+8,owner+20);m.write32(owner+12,2);
        const auto tables=OriginalEngineTables::load(argv[1]);
        for(unsigned i=0;i<2;++i)m.write32(owner+20+i*28+24,unsigned(tables.families[selected.family].pitchLimits[i]));
        for(unsigned i=0;i<4;++i)m.write32(0xca9b4ec+4+i*16,i);
        m.write32(0xc8ff1cc,queue);m.write32(queue+40,0xc31ec1c);
        m.write32(0xc8ff1dc,queue+128);m.write32(queue+168,0xc31ed0c);
        m.write32(0xc8ff1d0,queue+256);m.write32(queue+296,0xc31ec7c);
        OriginalTireAudioState tire;m.writeFloat(0xc8ff1c0,0);m.writeFloat(0xc8ff1c4,127);m.write8(0xc8ff1c8,0);
        m.write32(0xc31de5c,0);m.write32(0xc31de60,0);m.write32(0xc8ff1e4,0);m.write32(0xc8ff1e8,0);
        OriginalContactCompletionState completion;completion.randomSeed0C37C778=991+car*7+upgrade;m.write32(0xc37c778,completion.randomSeed0C37C778);
        OriginalDriveState d;OriginalTransmissionState transmission;double energy=0;
        for(unsigned frame=0;frame<720;++frame){
            const unsigned phase=frame%120;OriginalEngineControlInput in;
            in.rpm=phase<12?float(phase)*70.f:phase<80?1000.f+float(phase-12)*110.f:8500.f-float(phase-80)*170.f;
            in.throttle=phase<10?0.f:phase<70?1.f:phase<108?0.f:.5f;in.gear=phase<40?1:phase<80?2:phase<100?1:3;in.suppressShiftRelease=frame/120==4;
            if(frame%36<16)in.wheelSurface[frame/36%4]=6;
            for(unsigned i=0;i<4;++i)m.write8(actor+116+i,in.wheelSurface[i]);m.write8(0xc2f4de0,in.suppressShiftRelease);
            d.setf(0x1b8,in.throttle);transmission.gear00=in.gear;transmission.tach1c=in.rpm;
            d.setf(0x238,phase<20?1.f:25.f);const float tireStrength=phase<90?.7f:0.f;const unsigned tireMask=(car+upgrade+frame/120)%3;
            for(unsigned i=0;i<d.words.size();++i)m.write32(drive+i*4,d.words[i]);
            RefCpu request(m);request.r[4]=0;request.r[5]=tireMask;request.setFloat(4,tireStrength);request.r[15]=stack+0xf000;request.pr=stop;
            instructions+=request.run(0xc142520,stop,200);requestOriginalTireAudio(tire,0,d.f(0x238)*3.6f,0,tireMask,tireStrength);
            m.write32(0xc900e88,unsigned(in.gear));m.writeFloat(0xc900ea4,in.rpm);m.write32(0xc92de30,frame);m.write8(0xc92ed00,0);
            std::vector<unsigned> expected,actual;std::vector<OriginalTireCommand> tireExpected;RefCpu c(m);c.r[9]=drive;c.r[14]=c.r[15]=stack+0xf000;
            c.callHooks[0xc1ed6a0]=[](auto&){};
            c.callHooks[0xc1ed9c0]=[&](auto& r){expected.push_back(r.r[4]);if((r.r[4]&65535)==0x3a9){for(unsigned i=0;i<6;++i)if(m.read32(0xc31ec7c+i*12)==r.r[4])tireExpected.push_back({OriginalTireCommandType::Play,int(i)});}};
            c.callHooks[0xc1433c0]=[&](auto& r){equal(r.r[4],queue+256,"skid stop owner");tireExpected.push_back({OriginalTireCommandType::Stop,0});};
            c.callHooks[0xc143400]=[&](auto& r){equal(r.r[4],queue+256,"skid volume owner");tireExpected.push_back({OriginalTireCommandType::Volume,std::bit_cast<int>(r.r[5])});};
            c.callHooks[0xc2223b8]=[](auto& r){r.fpul=r.r[4]/r.r[5];};
            instructions+=c.run(0xc157d6c,0xc157e6e,5000);
            finishOriginalContactFrame(d,transmission,completion,{frame,0},[&](const auto& effect,auto& seed){
                in.rpm=effect.engineValue;in.gear=std::bit_cast<int>(effect.gear);in.throttle=effect.throttle;
                native.step(in,seed,[&](unsigned bank,unsigned cue){actual.push_back(0xa9u|(bank<<8)|(cue<<16));});
            });
            //1415E0 runs tire control before decrementing the scene queues.
            c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0xc142120,stop,1000);
            const auto tireActual=stepOriginalTireAudio(tire,0,completion.randomSeed0C37C778);
            for(const auto& command:tireActual)if(command.type==OriginalTireCommandType::Play)actual.push_back(m.read32(0xc31ec7c+unsigned(command.value)*12));
            if(tireActual!=tireExpected)throw std::runtime_error("Integrated tire command order/values differ");
            equal(std::bit_cast<unsigned>(tire.strength),m.read32(0xc8ff1c0),"skid strength");equal(std::bit_cast<unsigned>(tire.volume),m.read32(0xc8ff1c4),"skid volume");
            equal(tire.requested,m.read8(0xc8ff1c8),"skid requested");equal(tire.phase,m.read32(0xc8ff1e4),"skid phase");equal(tire.frames,m.read32(0xc8ff1e8),"skid frames");
            if(actual!=expected)throw std::runtime_error("Integrated release/backfire sequence differs");
            compareState(native.state(),m);equal(completion.randomSeed0C37C778,m.read32(0xc37c778),"contact/engine/queue shared RNG");
            equal(completion.steeringMask0C900EBC,m.read32(0xc900ebc),"contact steering mask");equal(completion.elapsedFrames0C900E84,m.read32(0xc900e84),"contact frame");
            equal(unsigned(native.releaseQueue().cooldown),m.read32(queue+176),"release queue cooldown");
            RefCpu tick(m);tick.r[4]=queue+128;tick.r[15]=stack+0xf000;tick.pr=stop;instructions+=tick.run(0xc143680,stop,100);native.finishSoundFrame();
            equal(unsigned(native.releaseQueue().cooldown),m.read32(queue+176),"outer cooldown tick");
            for(unsigned i=0;i<735;++i){const auto sample=native.renderFrame();for(auto value:sample.dry)energy+=double(value)*value;
                if(i==0)for(unsigned bus=0;bus<sample.effects.size();++bus)if(sample.effects[bus])effectBuses.insert(bus);
            }
            ++frames;cues+=actual.size();heard.insert(actual.begin(),actual.end());
        }
        if(energy<1000000)throw std::runtime_error("Selected original engine silent");
    }
    if(!heard.contains(0x000005a9)||!heard.contains(0x000702a9))throw std::runtime_error("Release/backfire source path not covered");
    if(effectBuses.empty())throw std::runtime_error("Engine/auxiliary PCM had no authored effect sends");
    for(auto bus:effectBuses)if(!sourceEffectBuses.contains(bus))throw std::runtime_error("Engine PCM used an effect input bus absent from original layers");
    for(unsigned cue=0;cue<3;++cue){const auto s=loadOriginalRaceSound(argv[1],5,cue);equal(s.command,0x5a9u|(cue<<16),"PACK25 command");if(s.clip.sampleRate!=22050||s.clip.frames()<16000)throw std::runtime_error("Release sample invalid");}
    std::cout<<"PASS "<<profiles<<" original profile sound selections,72 source bank descriptors,"<<frames<<" integrated contact/engine/tire/queue frames,"<<cues<<" release/backfire/skid cues,"<<checks<<" comparisons,"<<instructions<<" original instructions. Hooks: final continuous/sequence outputs, tire stop/volume, integer quotient;142860/142800,1424A0/1424E0,142520/1596E0/142120,1435C0,143680 and shared RNG execute original bytes. PCM rendered for all live engine selections; cabinet DSP remains separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
