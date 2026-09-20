#include "unity_audio_output.h"
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace idas3;
namespace {
std::uint64_t checks=0;
void require(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
std::vector<float> sequence(unsigned first,unsigned frames){
    std::vector<float> out(frames*2);for(unsigned i=0;i<frames;++i){out[i*2]=float(first+i)/1048576.f;out[i*2+1]=-out[i*2];}return out;
}
void ringTests(){
    UnityStereoPcmRing ring;ring.setRunning(true);ring.setAudible(true);
    std::vector<float> out(1024*2,5);
    require(ring.read(out.data(),512)==0,"Empty ring returned audio");
    for(auto value:out)require(value==0||value==5,"Reader overwrote beyond request");
    auto first=sequence(1,1024);require(ring.write(first)==1024,"Initial write failed");
    require(ring.read(out.data(),512)==512,"Prebuffer did not start");
    for(unsigned i=0;i<1024;++i)require(out[i]==first[i],"Stereo order changed");
    require(ring.read(out.data(),1024)==512,"Partial underrun not reported");
    for(unsigned i=1024;i<2048;++i)require(out[i]==0,"Underrun not zero filled");
    require(ring.statistics().underrunFrames==512,"Underrun counter wrong");
    auto second=sequence(2049,1024);ring.write(second);ring.clear();
    auto third=sequence(4097,1024);ring.write(third);
    require(ring.read(out.data(),1024)==1024,"Clear discarded new PCM");
    require(out==third,"Clear leaked pre-transition PCM");
    ring.write(first);ring.setAudible(false);
    require(ring.read(out.data(),1024)==0,"Mute leaked old PCM");
    for(auto value:out)require(value==0,"Mute was not silent");
    ring.setAudible(true);ring.write(third);ring.setRunning(false);
    require(ring.read(out.data(),1024)==0,"Stopped AudioSource leaked PCM");
    ring.setRunning(true);
    auto large=sequence(1,UnityStereoPcmRing::capacityFrames+17);
    require(ring.write(large)==UnityStereoPcmRing::capacityFrames,"Capacity is unbounded");
    require(ring.statistics().droppedFrames==17,"Overflow count wrong");
    require(ring.read(out.data(),1024)==1024,"Backlog trim stopped audio");
    require(out[0]==large[(UnityStereoPcmRing::capacityFrames-UnityStereoPcmRing::maximumLatencyFrames)*2],"Backlog latency not bounded");
    ring.clear();ring.read(out.data(),1024);
    first.assign(2048,std::numeric_limits<float>::quiet_NaN());first[0]=2;first[1]=-2;
    ring.write(first);ring.read(out.data(),1024);
    require(out[0]==1&&out[1]==-1,"Float output not clamped");
    for(unsigned i=2;i<out.size();++i)require(out[i]==0,"Nonfinite PCM escaped");
    UnityAudioFrameClock clock;std::uint64_t total=0;
    for(unsigned i=0;i<6000;++i)total+=clock.advance(1./60);
    require(total==4410000,"60Hz clock drifts");
    clock.reset();total=0;for(unsigned i=0;i<14400;++i)total+=clock.advance(1./144);
    require(total==4410000,"144Hz clock drifts");
    require(clock.advance(0)==0&&clock.advance(-1)==0&&clock.advance(INFINITY)==0,"Invalid dt advanced audio");
}
void deviceRateTests(){
    for(unsigned rate:{8000u,22050u,32000u,44100u,48000u,96000u,192000u}){
        UnityStereoPcmRing ring;ring.setRunning(true);ring.setAudible(true);
        unsigned source=1,outputFrame=0;const unsigned total=rate*2;std::array<float,1024> out{};
        while(outputFrame<total){
            const unsigned frames=std::min(512u,total-outputFrame);
            const auto required=(std::uint64_t(frames)*44100+rate-1)/rate+1;
            while(ring.statistics().queuedFrames<std::max<std::uint64_t>(2048,required+512)){
                const auto input=sequence(source,512);require(ring.write(input)==512,"Device reader source overflow");source+=512;
            }
            const auto count=ring.readDevice(out.data(),frames,2,rate);require(count==frames,"Device reader inserted silence into available PCM");
            for(unsigned i=0;i<count;++i){
                const auto phase=std::uint64_t(outputFrame+i)*44100;const auto index=phase/rate,remainder=phase%rate;
                const auto a=float(index+1)/1048576.f,b=float(index+2)/1048576.f;
                const float expected=a+(b-a)*(float(remainder)/rate);
                require(out[i*2]==expected&&out[i*2+1]==-expected,"Device rate changed source phase/pitch/stereo");
            }
            outputFrame+=count;
        }
        require(ring.statistics().consumedFrames==88200,"Device sample count drifted from44100Hz");
        require(ring.statistics().underrunFrames==0&&ring.statistics().droppedFrames==0,"Device reader lost buffered audio");
        ring.setAudible(false);ring.readDevice(out.data(),512,2,rate);
        for(auto value:out)require(value==0,"Device reader leaked muted audio");
    }
    UnityStereoPcmRing ring;ring.setRunning(true);ring.setAudible(true);
    std::vector<float> input(8192);for(unsigned i=0;i<4096;++i){input[i*2]=.5f;input[i*2+1]=-.25f;}ring.write(input);
    std::array<float,1024> mono{};require(ring.readDevice(mono.data(),1024,1,44100)==1024,"Mono device reader failed");
    for(auto value:mono)require(value==.125f,"Mono output did not average original stereo");
    std::array<float,512*6> surround{};require(ring.readDevice(surround.data(),512,6,44100)==512,"Surround device reader failed");
    for(unsigned i=0;i<512;++i)for(unsigned c=0;c<6;++c)require(surround[i*6+c]==(c==0?.5f:c==1?-.25f:0.f),"Surround output changed front stereo routing");
}
void concurrentTest(){
    UnityStereoPcmRing ring;ring.setRunning(true);ring.setAudible(true);
    constexpr unsigned total=262144;
    std::thread writer([&]{for(unsigned first=1;first<=total+1024;first+=256){
        while(ring.statistics().queuedFrames>2048)std::this_thread::yield();
        const auto data=sequence(first,256);ring.write(data);
    }});
    std::array<float,1024> out{};unsigned expected=1;
    while(expected<=total){
        const auto got=ring.read(out.data(),512);
        for(unsigned i=0;i<got;++i){require(out[i*2]==float(expected)/1048576.f&&out[i*2+1]==-out[i*2],"Concurrent stereo corruption/order loss");++expected;}
        if(!got)std::this_thread::yield();
    }
    writer.join();require(ring.statistics().droppedFrames==0,"Concurrent ring dropped frames");
}
void mixerTest(const std::filesystem::path& root,bool selection){
    EngineAudio audio,reference;audio.configure(root);reference.configure(root);
    audio.scene(true,false,false);reference.scene(true,false,false);
    if(selection){
        using namespace idas3::original;
        OriginalSelectionMusicState manager;
        for(const auto& c:requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Type)){audio.selection(c);reference.selection(c);}
        for(unsigned i=0;i<6;++i)for(const auto& c:tickOriginalSelectionMusic(manager)){audio.selection(c);reference.selection(c);}
    }else{audio.attract(7,0);reference.attract(7,0);}
    resetUnityAudioOutput();Idas3UnitySetAudioRunning(1);
    const UnityAudioFrameState state{800,0,0,0,false,false,true};
    std::vector<float> expected;expected.reserve(44100*4);std::size_t consumed=0;std::array<float,1470> out{};unsigned nonzero=0;
    for(unsigned tick=0;tick<120;++tick){
        submitUnityAudioOutput(audio,1./60,state);
        for(unsigned i=0;i<735;++i){auto pcm=reference.renderStereo(800,0,0,0,false);expected.push_back(pcm[0]/32768.f);expected.push_back(pcm[1]/32768.f);}
        const auto got=Idas3UnityReadAudio(out.data(),735);
        for(unsigned i=0;i<unsigned(got)*2;++i){require(out[i]==expected[consumed++],"Unity PCM differs from unchanged native mixer");if(out[i])++nonzero;}
    }
    require(nonzero>100,"Actual source music silent");
    const auto before=selection?double(audio.selectionSamplePosition()):audio.attractStatistics().frame;
    audio.scene(true,false,true);auto paused=state;paused.paused=true;
    submitUnityAudioOutput(audio,.1,paused);Idas3UnityReadAudio(out.data(),735);
    for(auto v:out)require(v==0,"Pause leaked buffered music");
    require((selection?double(audio.selectionSamplePosition()):audio.attractStatistics().frame)==before,"Pause changed source music clock");
    audio.scene(true,false,false);audio.enabled=false;
    submitUnityAudioOutput(audio,.1,state);Idas3UnityReadAudio(out.data(),735);
    for(auto v:out)require(v==0,"Mute not silent");
    require((selection?double(audio.selectionSamplePosition()):audio.attractStatistics().frame)>before,"Mute froze source clock");
    audio.enabled=true;submitUnityAudioOutput(audio,1./30,state);
    const auto serial=audio.outputResetSerial();audio.scene(false,false,false);
    require(audio.outputResetSerial()>serial,"Original stream transition lost output reset signal");
    // Zero elapsed still publishes source reset, without creating a PCM tick.
    submitUnityAudioOutput(audio,0,state);require(Idas3UnityReadAudio(out.data(),735)==0,"Old scene PCM survived exact source reset");
    resetUnityAudioOutput();Idas3UnitySetAudioRunning(0);
    require(Idas3UnityReadAudio(out.data(),735)==0,"Shutdown accessed stale PCM");
    Idas3UnityAudioStatistics exported;
    require(Idas3UnityGetAudioStatistics(nullptr)==0,"Statistics accepted null output");
    exported.size=1;require(Idas3UnityGetAudioStatistics(&exported)==0,"Statistics accepted wrong ABI size");
    exported.size=sizeof(exported);require(Idas3UnityGetAudioStatistics(&exported)==1&&exported.version==1,"Statistics ABI unavailable");
    const auto internal=unityAudioOutputStatistics();
    require(exported.producedFrames==internal.producedFrames&&exported.consumedFrames==internal.consumedFrames&&
        exported.droppedFrames==internal.droppedFrames&&exported.underrunFrames==internal.underrunFrames&&
        exported.primingFrames==internal.primingFrames&&exported.discardedFrames==internal.discardedFrames&&
        exported.queuedFrames==internal.queuedFrames,"Statistics export does not reflect PCM ring");
}
}
int main(int argc,char** argv){try{
    require(argc==2,"Native asset root required");ringTests();deviceRateTests();concurrentTest();mixerTest(argv[1],false);mixerTest(argv[1],true);
    std::cout<<"Unity audio PASS: "<<checks<<" checks; stereo SPSC wrap/overflow/trim/underrun/concurrency, exact44100 clock, original attract+TYPE PCM, pause/mute/reset; no audio device opened\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
