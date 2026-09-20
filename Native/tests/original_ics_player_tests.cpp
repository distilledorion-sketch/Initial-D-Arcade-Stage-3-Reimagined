#include "original_ics_player.h"
#include "original_engine_control.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace idas3;
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void wav(const std::filesystem::path& path,const std::vector<std::int16_t>& samples){
    std::ofstream f(path,std::ios::binary);const auto u16=[&](std::uint16_t n){f.put(char(n));f.put(char(n>>8));};
    const auto u32=[&](std::uint32_t n){u16(std::uint16_t(n));u16(std::uint16_t(n>>16));};
    f.write("RIFF",4);u32(36+std::uint32_t(samples.size()*2));f.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(44100);u32(176400);u16(4);u16(16);
    f.write("data",4);u32(std::uint32_t(samples.size()*2));f.write(reinterpret_cast<const char*>(samples.data()),samples.size()*2);
    if(!f)throw std::runtime_error("Could not write original engine preview");
}
// Tiny authored fixture: alternating sample values, loop [1,4), exact unit
// pitch and maximum dry level. Driver maximum gain gives TL1 after its two
// rounded127-volume stages. Hardcoded expected PCM catches sample-phase and
// loop-end mistakes independently of the production sampler's formulas.
std::shared_ptr<OriginalIcsBank> fixture(){
    auto b=std::make_shared<OriginalIcsBank>();b->volumeTable.fill(255);
    OriginalIcsSample sample;sample.pcm={0,12000,-12000,24000,32123};sample.loopStart=1;sample.loopEnd=4;sample.looping=true;b->samples.push_back(sample);
    OriginalIcsLayer layer;layer.sample=256;layer.first=1;layer.last=255;layer.header[6]=31;layer.header[7]=31;layer.header[8]=15;layer.header[9]=1;
    for(unsigned i=0;i<6;++i)layer.controls[i].push_back({255,std::uint16_t(i==4?3072:i==1?64:i==3?15:127),0});
    OriginalIcsProgram program;program.header[2]=127;program.layers.push_back(layer);b->programs.push_back(program);return b;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("project-root preview-wav required");
    const auto tables=OriginalIcsVoiceTables::load(argv[1]);OriginalIcsPlayer player(tables);
    auto b=fixture();player.select(0,b,0);player.setValue(0,100);
    constexpr std::array<int,10> expected{0,11490,-11491,22981,11490,-11491,22981,11490,-11491,22981};
    for(const auto value:expected){const auto s=player.renderFrame();require(s.dry[0]==value&&s.dry[1]==value,"PCM unit pitch/loop endpoint mismatch");}
    const auto starts=player.startedVoices();player.setValue(0,110);require(player.startedVoices()==starts,"Pitch command restarted existing layer");
    player.setMasterVolume(0);for(unsigned i=0;i<17;++i)require(player.renderFrame().dry==std::array<std::int32_t,2>{},"Master mute did not silence voices");
    player.setMasterVolume(127);require(player.renderFrame().dry[0]!=0,"Mute stopped sample phase");
    player.stop(0);unsigned releaseFrames=0;
    while(player.activeVoices()&&releaseFrames<200){player.renderFrame();++releaseFrames;}
    require(releaseFrames>=136&&releaseFrames<=159,"Source release rate duration differs");
    require(player.activeVoices()==0&&player.renderFrame().dry==std::array<std::int32_t,2>{},"Released PCM voice did not terminate");
    // Half-rate is an authored pitch of2688: literal halfway samples and a
    // loop-boundary blend to sample1, never the sentinel after loopEnd.
    player.reset();b=fixture();b->programs[0].layers[0].controls[4][0].base=2688;
    player.select(0,b,0);player.setValue(0,100);
    constexpr std::array<int,12> half{0,5745,11490,0,-11491,5745,22981,17236,11490,0,-11491,5745};
    for(const auto value:half){const auto s=player.renderFrame();require(s.dry[0]==value&&s.dry[1]==value,"PCM half-rate interpolation mismatch");}
    OriginalIcsVoiceControls pan;pan.pan=0;player.setControls(0,pan);auto left=player.renderFrame();require(left.dry[1]==0&&left.dry[0]!=0,"Left pan direction/mute mismatch");
    pan.pan=127;player.setControls(0,pan);auto right=player.renderFrame();require(right.dry[0]==0&&right.dry[1]!=0,"Right pan direction/mute mismatch");
    player.reset();require(player.activeVoices()==0,"Race reset retained voices");
    // A new DSP program clears physical send bytes (ARM2E94) without changing
    // voice phase or channel parameters. Ordinary reconfiguration must not
    // undo this write; explicit send writes and new notes can restore sends.
    {
        OriginalIcsPlayer reference(tables),cleared(tables);auto source=fixture();
        OriginalIcsVoiceControls controls;controls.effectBus=10;
        for(auto* p:{&reference,&cleared}){p->select(0,source,0);p->setControls(0,controls);p->setValue(0,100);}
        for(unsigned i=0;i<10;++i){reference.renderFrame();cleared.renderFrame();}
        cleared.clearDspSends();controls.volume=91;
        for(auto* p:{&reference,&cleared}){p->setControls(0,controls);p->setValue(0,110);p->setMasterVolume(117);}
        for(unsigned i=0;i<16;++i){auto a=reference.renderFrame(),b=cleared.renderFrame();require(a.dry==b.dry,"DSP clear changed dry PCM/phase");require(b.effects==std::array<std::int32_t,16>{},"Pitch/volume/master restored cleared send");require(a.effects[10]!=0,"Clear-send reference fixture has no send");}
        cleared.setControls(0,controls,true); // Explicit unchanged register write.
        for(unsigned i=0;i<8;++i){auto a=reference.renderFrame(),b=cleared.renderFrame();require(a.dry==b.dry&&a.effects==b.effects,"Explicit unchanged send did not restore physical routing");}
        cleared.clearDspSends();controls.effectBus=5;
        for(auto* p:{&reference,&cleared})p->setControls(0,controls);
        for(unsigned i=0;i<8;++i){auto a=reference.renderFrame(),b=cleared.renderFrame();require(a.dry==b.dry&&a.effects==b.effects&&b.effects[5]!=0,"Changed bus did not restore send");}
        reference.stop(0);cleared.stop(0);cleared.clearDspSends();
        for(unsigned i=0;i<16;++i){auto a=reference.renderFrame(),b=cleared.renderFrame();require(a.dry==b.dry&&b.effects==std::array<std::int32_t,16>{},"DSP clear missed released physical voice");}
        // New physical voice initialization still writes the current controls.
        cleared.select(0,source,0);cleared.setControls(0,controls);cleared.setValue(0,100);
        cleared.renderFrame();require(cleared.renderFrame().effects[5]!=0,"New voice inherited stale cleared-send state");
    }
    // Stress every actual program through pitch/range transitions, stops,
    // master changes and restart. A released tail must not exhaust64 voices.
    std::size_t programs=0,frames=0;
    const auto directory=std::filesystem::path(argv[1])/"data/original_audio/continuous";
    for(const auto& f:std::filesystem::directory_iterator(directory))if(f.path().extension()==".dtpk"){
        const auto bank=std::make_shared<OriginalIcsBank>(loadOriginalIcsBank(f.path()));
        for(unsigned program=0;program<bank->programs.size();++program){
            player.reset();player.setMasterVolume(127);player.select(0,bank,program);++programs;
            for(unsigned value=1;value<=255;++value){
                player.setValue(0,value);
                for(unsigned i=0;i<200;++i){const auto s=player.renderFrame();for(auto channel:s.dry)require(std::abs(channel)<131072,"Engine PCM out of mix bounds");++frames;}
            }
            player.stop(0);for(unsigned i=0;i<200;++i)player.renderFrame();require(player.activeVoices()==0,"Original bank tail survived release");
        }
    }
    // Isolated controller-to-PCM preview of the AE86 family, two authored
    // programs from PACK4. Game RNG integration and effects are separate.
    player.reset();const auto bank=std::make_shared<OriginalIcsBank>(loadOriginalIcsBank(directory/"PACK4.dtpk"));
    player.select(0,bank,0);player.select(1,bank,1);std::array<OriginalIcsVoiceControls,2> controls{};
    for(unsigned i=0;i<2;++i){controls[i].cutoff=63;player.setControls(i,controls[i]);}
    const auto engineTables=original::OriginalEngineTables::load(argv[1]);const auto config=original::configureOriginalEngine(0,0,0,0,0);
    original::OriginalEngineControlState state;unsigned seed=1;std::vector<std::int16_t> recorded;recorded.reserve(44100*12*2);
    double energy=0;unsigned clipped=0;
    for(unsigned tick=0;tick<720;++tick){
        original::OriginalEngineControlInput in;
        const unsigned phase=tick%240;in.rpm=phase<180?1000.f+phase*38.f:7840.f-(phase-180)*105.f;in.throttle=phase<180?1.f:0.f;in.gear=int(tick/240)+1;
        for(const auto& command:original::stepOriginalEngineControl(engineTables,config,state,in,seed)){
            if(command.target!=original::OriginalEngineCommandTarget::Continuous||command.handle>=2)continue;
            const auto channel=command.handle;
            if(command.command==0xa6)player.setValue(channel,unsigned(command.value)&255);
            else if(command.command==0x10a5){controls[channel].volume=std::uint8_t(command.value&127);player.setControls(channel,controls[channel]);}
            else if(command.command==0x40a5){controls[channel].effectSend=std::uint8_t((command.value+64)&127);player.setControls(channel,controls[channel]);}
        }
        for(unsigned i=0;i<735;++i){const auto s=player.renderFrame();for(auto value:s.dry){energy+=double(value)*value;if(value>32767||value<-32768)++clipped;recorded.push_back(std::int16_t(std::clamp(value,-32768,32767)));}}
    }
    const auto rms=std::sqrt(energy/recorded.size());require(rms>100&&rms<20000,"Engine preview silent or excessive");require(clipped==0,"Dry engine preview clipped");wav(argv[2],recorded);
    std::cout<<"PASS exact unit/half-rate PCM and exclusive loop boundary, pan, mute/phase, release and reset; "<<programs<<" original programs, "<<frames<<" stress PCM frames. Twelve-second native controller-to-PCM AE86 dry preview RMS "<<rms<<", clipped samples "<<clipped<<". Does not establish cabinet DSP or live session timing.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
