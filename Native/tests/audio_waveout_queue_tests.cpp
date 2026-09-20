// Actual EngineAudio output loop, with every WinMM call replaced in this TU.
// This fixture never opens a host audio device or changes user audio settings.
#include "audio.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

MMRESULT WINAPI testWaveOutOpen(LPHWAVEOUT,UINT,LPCWAVEFORMATEX,DWORD_PTR,DWORD_PTR,DWORD);
MMRESULT WINAPI testWaveOutPrepareHeader(HWAVEOUT,LPWAVEHDR,UINT);
MMRESULT WINAPI testWaveOutUnprepareHeader(HWAVEOUT,LPWAVEHDR,UINT);
MMRESULT WINAPI testWaveOutWrite(HWAVEOUT,LPWAVEHDR,UINT);
MMRESULT WINAPI testWaveOutReset(HWAVEOUT);
MMRESULT WINAPI testWaveOutClose(HWAVEOUT);
#define waveOutOpen testWaveOutOpen
#define waveOutPrepareHeader testWaveOutPrepareHeader
#define waveOutUnprepareHeader testWaveOutUnprepareHeader
#define waveOutWrite testWaveOutWrite
#define waveOutReset testWaveOutReset
#define waveOutClose testWaveOutClose
#include "../src/audio.cpp"
#undef waveOutOpen
#undef waveOutPrepareHeader
#undef waveOutUnprepareHeader
#undef waveOutWrite
#undef waveOutReset
#undef waveOutClose

using namespace idas3;
using namespace idas3::original;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Packet {
    WAVEHDR* header{};
    std::vector<short> original;
    std::uint64_t firstFrame=0;
    unsigned consumed=0;
};
struct Device {
    EngineAudio* reference=nullptr;
    bool opened=false,failNextWrite=false;
    std::deque<Packet> queue;
    std::vector<WAVEHDR*> prepared;
    std::vector<std::uint64_t> acceptedFirstFrames;
    std::uint64_t attempts=0,accepted=0,compared=0,heard=0,starved=0,discarded=0,resets=0;
    std::uint64_t failedFrames=0,generated=0,heardNonzero=0;
    std::uint64_t lastHeard=0;
    bool hasHeard=false;
    unsigned queuedFrames()const{
        unsigned n=0;for(const auto&p:queue)n+=unsigned(p.original.size()/2)-p.consumed;return n;
    }
    void elapse(unsigned frames){
        for(unsigned i=0;i<frames;++i){
            if(queue.empty()){++starved;continue;}
            auto&p=queue.front();const auto*live=reinterpret_cast<const short*>(p.header->lpData);
            const auto offset=p.consumed*2;
            check(live[offset]==p.original[offset]&&live[offset+1]==p.original[offset+1],"Queued header overwritten before the device finished it");
            const auto frame=p.firstFrame+p.consumed;
            check(!hasHeard||frame>lastHeard,"Actual submitted PCM played out of chronological generation order");
            lastHeard=frame;hasHeard=true;++heard;if(live[offset]||live[offset+1])++heardNonzero;
            if(++p.consumed==p.original.size()/2){
                p.header->dwFlags&=~WHDR_INQUEUE;p.header->dwFlags|=WHDR_DONE;queue.pop_front();
            }
        }
    }
};
Device* device=nullptr;
HWAVEOUT handle(){return reinterpret_cast<HWAVEOUT>(device);}
void valid(HWAVEOUT h){check(device&&device->opened&&h==handle(),"Invalid mocked output handle");}

struct Fixture {
    Device d;
    // Destroy audio while the Device still exists. Offline never calls open.
    EngineAudio offline,audio;
    OriginalSelectionMusicState manager;
    explicit Fixture(const std::filesystem::path&root){
        check(!device,"Only one mocked device fixture may exist");device=&d;d.reference=&offline;
        audio.configure(root);offline.configure(root);audio.scene(true,false,false);offline.scene(true,false,false);
        check(audio.open(),"Mocked waveOutOpen failed");
    }
    ~Fixture(){d.reference=nullptr; /* audio destructor uses d before d is destroyed */}
    void feed(const OriginalSelectionMusicCommands&commands){for(const auto&c:commands){audio.selection(c);offline.selection(c);}}
    void start(OriginalSelectionMusicCue cue){feed(requestOriginalSelectionMusic(manager,cue));}
    void tick(unsigned elapsed){
        d.elapse(elapsed);feed(tickOriginalSelectionMusic(manager));audio.update(800,0,0,0,false);
        check(audio.selectionSamplePosition()==offline.selectionSamplePosition(),"Device submission advanced selection clock differently from offline PCM");
        check(audio.dspRuntime()->diagnostics().frames==offline.dspRuntime()->diagnostics().frames,"Device submission advanced DSP differently from offline PCM");
    }
    void paused(bool value){audio.scene(true,false,value);offline.scene(true,false,value);}
};
struct Stats{std::uint64_t frames=0,starved=0,compared=0;unsigned minimumQueued=99999;};
Stats timeline(const std::filesystem::path&root,OriginalSelectionMusicCue cue,unsigned interval,unsigned iterations,bool jitter=false){
    Stats out;
    {Fixture f(root);f.start(cue);f.tick(0);
        constexpr unsigned intervals[]{400,1050,513,997,735,800,670,715};
        for(unsigned i=0;i<iterations;++i){const auto step=jitter?intervals[i%8]:interval;f.tick(step);out.frames+=step;out.minimumQueued=std::min(out.minimumQueued,f.d.queuedFrames());}
        out.starved=f.d.starved;out.compared=f.d.compared;
        check(f.d.heardNonzero>1000,"Timeline never played source music");
        check(f.d.failedFrames==0,"Unexpected mocked write failure");
        if(interval<=1470)check(!out.starved,"Ordinary 30/60Hz or bounded jitter starved with zero-cost refill");
        check(f.d.acceptedFirstFrames.size()>100,"Fixture did not exercise header-ring wraps");
        std::cout<<(cue==OriginalSelectionMusicCue::Type?"TYPE":"SELECT")<<" interval="<<(jitter?0:interval)<<" frames="<<out.frames<<" starvation="<<out.starved<<" minimum_post_refill="<<out.minimumQueued<<" exact_pcm="<<out.compared<<'\n';
    }
    device=nullptr;return out;
}
void transitions(const std::filesystem::path&root){
    {Fixture f(root);f.start(OriginalSelectionMusicCue::Type);f.tick(0);for(unsigned i=0;i<90;++i)f.tick(735);
        const auto atPause=f.audio.selectionSamplePosition();const auto dspPause=f.audio.dspRuntime()->diagnostics().frames;
        f.paused(true);for(unsigned i=0;i<10;++i)f.tick(735);
        check(f.audio.selectionSamplePosition()==atPause,"Pause advanced score");check(f.audio.dspRuntime()->diagnostics().frames==dspPause,"Pause advanced shared DSP");
        f.paused(false);for(unsigned i=0;i<10;++i)f.tick(735);
        f.audio.enabled=f.offline.enabled=false;const auto muted=f.audio.selectionSamplePosition();for(unsigned i=0;i<5;++i)f.tick(735);
        check(f.audio.selectionSamplePosition()>muted,"Mute froze source clock");f.audio.enabled=f.offline.enabled=true;
        const auto beforeReset=f.d.resets;f.start(OriginalSelectionMusicCue::Select);for(unsigned i=0;i<90;++i)f.tick(735);
        check(f.d.resets>beforeReset,"Cue replacement never reset queued old audio");
        check(f.audio.selectionStatistics().cue==1,"SELECT did not replace TYPE");
        f.audio.attract(7,0);f.offline.attract(7,0);for(unsigned i=0;i<4;++i){f.d.elapse(735);f.audio.update(800,0,0,0,false);}
        f.audio.attract(~0u,0);f.offline.attract(~0u,0);
        f.feed(changeOriginalSelectionMusicScene(f.manager,0));f.start(OriginalSelectionMusicCue::Type);for(unsigned i=0;i<10;++i)f.tick(735);
        check(f.audio.selectionStatistics().cue==0&&f.audio.selectionPlaying(),"Start after attract did not restore TYPE");
        std::cout<<"Transitions exact_pcm="<<f.d.compared<<" resets="<<f.d.resets<<" reset_discarded="<<f.d.discarded<<" (intentional old queued audio)\n";
    }device=nullptr;
}
void writeFailure(const std::filesystem::path&root){
    {Fixture f(root);f.start(OriginalSelectionMusicCue::Type);f.tick(0);for(unsigned i=0;i<10;++i)f.tick(735);
        f.d.elapse(512);const auto before=f.audio.selectionSamplePosition();const auto generated=f.d.generated;
        f.d.failNextWrite=true;f.audio.update(800,0,0,0,false);
        check(f.d.failedFrames==512,"Write failure injection not reached");
        check(f.audio.selectionSamplePosition()==before+f.d.generated-generated,"Failed output did not advance actual mixer as expected");
        for(unsigned i=0;i<10;++i)f.tick(735);
        check(f.audio.enabled,"Write failure incorrectly reported itself by disabling audio");
        std::cout<<"Known robustness defect reproduced: waveOutWrite failure silently discards "<<f.d.failedFrames<<" generated stereo frames (11.610ms).\n";
    }device=nullptr;
}
void benchmark(const std::filesystem::path&root,OriginalSelectionMusicCue cue){
    {Fixture f(root);f.start(cue);f.tick(0);for(unsigned i=0;i<6;++i)f.tick(735);
        // Disable duplicate offline rendering for timing only. All other tests
        // compare actual emitted PCM and retain their strict sample clocks.
        f.d.reference=nullptr;std::vector<double> times;unsigned rendered=0;
        for(unsigned i=0;i<600;++i){
            f.d.elapse(735);const auto previous=f.d.generated;
            const auto begin=std::chrono::steady_clock::now();f.audio.update(800,0,0,0,false);
            const auto end=std::chrono::steady_clock::now();times.push_back(std::chrono::duration<double,std::milli>(end-begin).count());
            rendered+=unsigned(f.d.generated-previous);
        }
        std::sort(times.begin(),times.end());double total=0;for(double value:times)total+=value;
        std::cout<<(cue==OriginalSelectionMusicCue::Type?"TYPE":"SELECT")<<" benchmark_frames="<<rendered<<" refill_ms_mean="<<total/times.size()<<" p95="<<times[times.size()*95/100]<<" maximum="<<times.back()<<" synthesis_realtime_ratio="<<(total/1000)/(rendered/44100.)<<'\n';
    }device=nullptr;
}
}

MMRESULT WINAPI testWaveOutOpen(LPHWAVEOUT h,UINT id,LPCWAVEFORMATEX f,DWORD_PTR callback,DWORD_PTR instance,DWORD flags){
    check(device&&!device->opened,"Mock device double-open");
    check(id==WAVE_MAPPER&&callback==0&&instance==0&&flags==CALLBACK_NULL,"Unexpected WinMM callback contract");
    check(f&&f->wFormatTag==WAVE_FORMAT_PCM&&f->nChannels==2&&f->nSamplesPerSec==44100&&f->wBitsPerSample==16&&f->nBlockAlign==4&&f->nAvgBytesPerSec==176400&&f->cbSize==0,"Incorrect PCM format");
    device->opened=true;*h=handle();return MMSYSERR_NOERROR;
}
MMRESULT WINAPI testWaveOutPrepareHeader(HWAVEOUT h,LPWAVEHDR p,UINT size){
    valid(h);check(size==sizeof(WAVEHDR)&&p&&p->lpData&&p->dwBufferLength==2048,"Incorrect wave header length/layout");
    p->dwFlags|=WHDR_PREPARED;device->prepared.push_back(p);return MMSYSERR_NOERROR;
}
MMRESULT WINAPI testWaveOutUnprepareHeader(HWAVEOUT h,LPWAVEHDR p,UINT size){
    valid(h);check(size==sizeof(WAVEHDR)&&!(p->dwFlags&WHDR_INQUEUE),"Unprepare before reset/completion");p->dwFlags&=~WHDR_PREPARED;return MMSYSERR_NOERROR;
}
MMRESULT WINAPI testWaveOutWrite(HWAVEOUT h,LPWAVEHDR p,UINT size){
    valid(h);check(size==sizeof(WAVEHDR)&&(p->dwFlags&WHDR_PREPARED)&&!(p->dwFlags&WHDR_INQUEUE),"Write reused queued/unprepared header");
    auto&d=*device;++d.attempts;const auto frames=p->dwBufferLength/4;const auto*pcm=reinterpret_cast<const short*>(p->lpData);
    if(d.reference)for(unsigned i=0;i<frames;++i){const auto expected=d.reference->renderStereo(800,0,0,0,false);check(pcm[i*2]==expected[0]&&pcm[i*2+1]==expected[1],"WaveOut PCM differs from identical offline mixer/source commands");++d.compared;}
    const auto first=d.generated;d.generated+=frames;
    if(d.failNextWrite){d.failNextWrite=false;d.failedFrames+=frames;return MMSYSERR_ERROR;}
    d.acceptedFirstFrames.push_back(first);++d.accepted;d.queue.push_back({p,{pcm,pcm+frames*2},first,0});
    p->dwFlags|=WHDR_INQUEUE;p->dwFlags&=~WHDR_DONE;return MMSYSERR_NOERROR;
}
MMRESULT WINAPI testWaveOutReset(HWAVEOUT h){
    valid(h);auto&d=*device;++d.resets;d.discarded+=d.queuedFrames();for(auto&p:d.queue){p.header->dwFlags&=~WHDR_INQUEUE;p.header->dwFlags|=WHDR_DONE;}d.queue.clear();return MMSYSERR_NOERROR;
}
MMRESULT WINAPI testWaveOutClose(HWAVEOUT h){valid(h);check(device->queue.empty(),"Close retained queued output");device->opened=false;return MMSYSERR_NOERROR;}

int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("project root required");const std::filesystem::path root=argv[1];
    for(auto cue:{OriginalSelectionMusicCue::Type,OriginalSelectionMusicCue::Select}){
        timeline(root,cue,735,240);timeline(root,cue,1470,120);timeline(root,cue,735,120,true);
        const auto slow=timeline(root,cue,2205,120);check(slow.starved==120*(2205-2048),"20FPS starvation amount differs from four-buffer capacity");
    }
    transitions(root);writeFailure(root);
    benchmark(root,OriginalSelectionMusicCue::Type);benchmark(root,OriginalSelectionMusicCue::Select);
    std::cout<<"WaveOut mock PASS. Real WinMM calls are compile-time replaced; no audio device opened.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
