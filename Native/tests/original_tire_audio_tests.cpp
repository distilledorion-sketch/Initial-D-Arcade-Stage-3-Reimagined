#include "original_tire_audio.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <set>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned stack=0xd000000,stop=0xf000000,manager=0xd020000;
std::size_t comparisons=0,instructions=0;
void equal(unsigned a,unsigned b,const std::string& what){++comparisons;if(a!=b)throw std::runtime_error(what+" native="+hex(a)+" original="+hex(b));}
void bind(RefMemory& m,const OriginalTireAudioState& s){
    m.writeFloat(0xc8ff1c0,s.strength);m.writeFloat(0xc8ff1c4,s.volume);m.write8(0xc8ff1c8,s.requested);
    m.write32(0xc31de5c,s.kind);m.write32(0xc31de60,s.surface);m.write32(0xc8ff1e4,s.phase);m.write32(0xc8ff1e8,s.frames);
}
void compare(RefMemory& m,const OriginalTireAudioState& s){
    equal(std::bit_cast<unsigned>(s.strength),m.read32(0xc8ff1c0),"strength");
    equal(std::bit_cast<unsigned>(s.volume),m.read32(0xc8ff1c4),"volume");
    equal(s.requested,m.read8(0xc8ff1c8),"request");equal(s.kind,m.read32(0xc31de5c),"kind");
    equal(s.surface,m.read32(0xc31de60),"surface");equal(s.phase,m.read32(0xc8ff1e4),"phase");equal(s.frames,m.read32(0xc8ff1e8),"frames");
}
RefCpu cpu(RefMemory& m){RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;return c;}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory m(argv[1]);std::size_t frames=0,events=0;std::set<unsigned> played,types;
    const std::array<float,17> strengths{0,.001f,.0087119f,.008712f,.01f,.1f,.29999998f,.3f,.30000004f,
        .32999998f,.33f,.33000004f,.4f,.49999997f,.5f,.7f,1.f};
    for(unsigned variant=0;variant<48;++variant){
        m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(0xc8ff100,0x100);m.zeroRegion(0xc900f00,0x500);
        m.write32(0xc8ff1d0,manager);
        OriginalTireAudioState s;s.strength=.7f;s.phase=variant%6;s.frames=variant%4==0?198:variant%4==1?199:variant%4==2?0:1;
        s.volume=98;s.surface=(variant%3)*2;s.kind=variant;s.requested=variant%3;bind(m,s);
        unsigned seed=1357911+variant;m.write32(0xc37c778,seed);
        for(unsigned frame=0;frame<1000;++frame){
            const unsigned phase=frame%400;
            const std::uint8_t disabled=frame%97==0?1:frame%31==0?2:0;m.write8(0xc31de55,disabled);
            const float speed=frame%37==0?9.f:frame%53==0?10.f:90.f;
            // This is the exact pre-transmission drive field read by1596E0.
            const float driveSpeed=speed/3.6f;m.writeFloat(0xc901138,driveSpeed);
            if(phase<260||phase>=300){
                const float strength=phase<220?.7f:strengths[(phase+variant)%strengths.size()];
                const unsigned mask=variant%8,kind=variant%4;
                auto c=cpu(m);c.r[4]=kind;c.r[5]=mask;c.setFloat(4,strength);
                instructions+=c.run(0xc142520,stop,200);
                requestOriginalTireAudio(s,disabled,driveSpeed*3.6f,kind,mask,strength);compare(m,s);
            }
            auto c=cpu(m);std::vector<OriginalTireCommand> original;
            c.callHooks[0xc142040]=[&](RefCpu& r){equal(r.r[5],1,"forced skid start");original.push_back({OriginalTireCommandType::Play,signed32(r.r[4])});};
            c.callHooks[0xc1433c0]=[&](RefCpu& r){equal(r.r[4],manager,"stop owner");original.push_back({OriginalTireCommandType::Stop,0});};
            c.callHooks[0xc143400]=[&](RefCpu& r){equal(r.r[4],manager,"volume owner");original.push_back({OriginalTireCommandType::Volume,signed32(r.r[5])});};
            instructions+=c.run(0xc142120,stop,500);
            const auto native=stepOriginalTireAudio(s,disabled,seed);
            try{
                compare(m,s);equal(seed,m.read32(0xc37c778),"shared RNG");
                if(original!=native)throw std::runtime_error("ordered commands differ");
            }catch(const std::exception& e){throw std::runtime_error("variant"+std::to_string(variant)+" frame"+std::to_string(frame)+": "+e.what());}
            ++frames;events+=native.size();for(const auto& command:native){types.insert(unsigned(command.type));if(command.type==OriginalTireCommandType::Play)played.insert(command.value);}
        }
    }
    if(played!=std::set<unsigned>{0,1,2,3,4,5}||types.size()!=3)throw std::runtime_error("Incomplete skid command coverage");
    std::cout<<"PASS "<<frames<<" sequential skid frames, "<<events<<" ordered commands, "<<comparisons<<" state/RNG comparisons, "<<instructions<<" original instructions; all six skid cues. Original142520/1596E0/142400/142120 and RNG execute original bytes. Hooks:142040 forced-cue boundary,1433C0 stop,143400 volume. Does not prove scene scheduling or waveform playback.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
