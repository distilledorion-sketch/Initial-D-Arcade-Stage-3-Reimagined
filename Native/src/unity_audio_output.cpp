#include "unity_audio_output.h"
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace idas3 {
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

void UnityStereoPcmRing::clear() noexcept {
    discardThrough_.store(written_.load(std::memory_order_acquire),std::memory_order_release);
    epoch_.fetch_add(1,std::memory_order_release);
}
void UnityStereoPcmRing::setRunning(bool value) noexcept {
    if(running_.exchange(value,std::memory_order_acq_rel)!=value)clear();
}
void UnityStereoPcmRing::setAudible(bool value) noexcept {
    if(audible_.exchange(value,std::memory_order_acq_rel)!=value)clear();
}
unsigned UnityStereoPcmRing::write(std::span<const float> data) noexcept {
    const auto frames=data.size()/2;
    const auto w=written_.load(std::memory_order_relaxed);
    const auto r=read_.load(std::memory_order_acquire);
    const auto accepted=std::min<std::size_t>(frames,capacityFrames-std::min<std::uint64_t>(capacityFrames,w-r));
    for(std::size_t i=0;i<accepted;++i)for(unsigned c=0;c<2;++c){
        const auto v=data[i*2+c];pcm_[(w+i)%capacityFrames][c]=std::isfinite(v)?std::clamp(v,-1.f,1.f):0.f;
    }
    written_.store(w+accepted,std::memory_order_release);
    produced_.fetch_add(frames,std::memory_order_relaxed);
    dropped_.fetch_add(frames-accepted,std::memory_order_relaxed);
    return unsigned(accepted);
}
unsigned UnityStereoPcmRing::read(float* out,unsigned frames) noexcept {
    if(!out||!frames)return 0;
    std::fill_n(out,std::size_t(frames)*2,0.f);
    auto r=read_.load(std::memory_order_relaxed);
    const auto w=written_.load(std::memory_order_acquire);
    const auto epoch=epoch_.load(std::memory_order_acquire);
    if(epoch!=observedEpoch_){observedEpoch_=epoch;primed_=false;}
    const auto discard=std::min(w,discardThrough_.load(std::memory_order_acquire));
    if(discard>r){discarded_.fetch_add(discard-r,std::memory_order_relaxed);r=discard;}
    if(!running_.load(std::memory_order_acquire)||!audible_.load(std::memory_order_acquire)){
        discarded_.fetch_add(w-r,std::memory_order_relaxed);
        read_.store(w,std::memory_order_release);primed_=false;return 0;
    }
    // After a host stall, discard old frames on the consumer side only. The
    // producer never races the reader by forcibly advancing its read cursor.
    if(w-r>maximumLatencyFrames){
        const auto trim=w-r-maximumLatencyFrames;r+=trim;
        discarded_.fetch_add(trim,std::memory_order_relaxed);
    }
    const auto prebuffer=std::max(prebufferFrames,std::min(frames,maximumLatencyFrames));
    if(!primed_&&w-r<prebuffer){
        priming_.fetch_add(frames,std::memory_order_relaxed);
        read_.store(r,std::memory_order_release);return 0;
    }
    primed_=true;
    const auto count=unsigned(std::min<std::uint64_t>(frames,w-r));
    for(unsigned i=0;i<count;++i){const auto& v=pcm_[(r+i)%capacityFrames];out[i*2]=v[0];out[i*2+1]=v[1];}
    read_.store(r+count,std::memory_order_release);
    consumed_.fetch_add(count,std::memory_order_relaxed);
    if(count<frames){underrun_.fetch_add(frames-count,std::memory_order_relaxed);primed_=false;}
    return count;
}
UnityAudioOutputStatistics UnityStereoPcmRing::statistics()const noexcept {
    const auto r=read_.load(std::memory_order_acquire),w=written_.load(std::memory_order_acquire);
    return {produced_.load(),consumed_.load(),dropped_.load(),underrun_.load(),priming_.load(),discarded_.load(),w>=r?w-r:0};
}
unsigned UnityStereoPcmRing::readDevice(float* out,unsigned frames,unsigned channels,unsigned rate) noexcept {
    if(!out||!frames||channels<1||channels>8||rate<8000||rate>192000)return 0;
    std::fill_n(out,std::size_t(frames)*channels,0.f);
    auto r=read_.load(std::memory_order_relaxed);
    const auto w=written_.load(std::memory_order_acquire),epoch=epoch_.load(std::memory_order_acquire);
    if(epoch!=observedEpoch_||deviceRate_!=rate){observedEpoch_=epoch;deviceRate_=rate;devicePhase_=0;primed_=false;}
    const auto discard=std::min(w,discardThrough_.load(std::memory_order_acquire));
    if(discard>r){discarded_.fetch_add(discard-r,std::memory_order_relaxed);r=discard;devicePhase_=0;}
    if(!running_.load(std::memory_order_acquire)||!audible_.load(std::memory_order_acquire)){
        discarded_.fetch_add(w-r,std::memory_order_relaxed);read_.store(w,std::memory_order_release);
        primed_=false;devicePhase_=0;return 0;
    }
    const auto required=(devicePhase_+std::uint64_t(frames)*sampleRate+rate-1)/rate+1;
    const auto prebuffer=std::max<std::uint64_t>(prebufferFrames,std::min<std::uint64_t>(required,capacityFrames));
    const auto latency=std::max<std::uint64_t>(maximumLatencyFrames,prebuffer);
    if(w-r>latency){const auto trim=w-r-latency;r+=trim;devicePhase_=0;discarded_.fetch_add(trim,std::memory_order_relaxed);}
    if(!primed_&&w-r<prebuffer){priming_.fetch_add(frames,std::memory_order_relaxed);read_.store(r,std::memory_order_release);return 0;}
    primed_=true;const auto available=w-r;auto phase=devicePhase_;unsigned count=0;
    for(;count<frames;++count){
        const auto index=phase/rate,remainder=phase%rate,nextPhase=phase+sampleRate;
        if(index>=available||(remainder&&index+1>=available)||nextPhase/rate>available)break;
        const auto& a=pcm_[(r+index)%capacityFrames];float left=a[0],right=a[1];
        if(remainder){const auto& b=pcm_[(r+index+1)%capacityFrames];const float fraction=float(remainder)/rate;
            left+=(b[0]-left)*fraction;right+=(b[1]-right)*fraction;}
        if(channels==1)out[count]=(left+right)*.5f;
        else{out[std::size_t(count)*channels]=left;out[std::size_t(count)*channels+1]=right;}
        phase=nextPhase;
    }
    const auto consumed=phase/rate;devicePhase_=phase%rate;read_.store(r+consumed,std::memory_order_release);
    consumed_.fetch_add(consumed,std::memory_order_relaxed);
    if(count<frames){underrun_.fetch_add(frames-count,std::memory_order_relaxed);primed_=false;}
    return count;
}
unsigned UnityAudioFrameClock::advance(double dt) noexcept {
    if(!std::isfinite(dt)||dt<=0)return 0;
    const auto amount=fractionalFrames_+std::min(dt,.25)*UnityStereoPcmRing::sampleRate;
    const auto whole=unsigned(std::floor(amount+1e-9));
    fractionalFrames_=amount-whole;
    return whole;
}
namespace {
struct AudioRuntime {
    UnityStereoPcmRing ring;
    UnityAudioFrameClock clock;
    std::uint64_t resetSerial=std::numeric_limits<std::uint64_t>::max();
};
// Constructed on the main thread before AudioSource.Play. Keep storage alive
// across App teardown and Editor play sessions; audio callbacks never see App.
AudioRuntime& audioRuntime(){static auto* runtime=new AudioRuntime;return *runtime;}
}
void resetUnityAudioOutput() noexcept {
    auto& r=audioRuntime();r.ring.setAudible(false);r.ring.clear();r.clock.reset();
    r.resetSerial=std::numeric_limits<std::uint64_t>::max();
}
void submitUnityAudioOutput(EngineAudio& audio,double dt,const UnityAudioFrameState& state){
    auto& r=audioRuntime();
    if(r.resetSerial!=audio.outputResetSerial()){r.resetSerial=audio.outputResetSerial();r.ring.clear();}
    const bool audible=state.audible&&audio.enabled&&!state.paused;
    r.ring.setAudible(audible);
    auto remaining=r.clock.advance(dt);
    std::array<float,2048> block{};
    while(remaining){
        const auto frames=std::min(remaining,1024u);
        for(unsigned i=0;i<frames;++i){
            const auto pcm=audio.renderStereo(state.rpm,state.throttle,state.speed,state.slip,state.active);
            block[i*2]=pcm[0]/32768.f;block[i*2+1]=pcm[1]/32768.f;
        }
        // Muting still advances source music/voices/DSP; pausing was already
        // applied by EngineAudio::scene and freezes those clocks in its mixer.
        if(audible)r.ring.write(std::span(block.data(),std::size_t(frames)*2));
        remaining-=frames;
    }
}
UnityAudioOutputStatistics unityAudioOutputStatistics() noexcept{return audioRuntime().ring.statistics();}
}
extern "C" int IDAS3_AUDIO_CALL Idas3UnityReadAudio(float* stereo,int frames){
    if(!stereo||frames<=0||frames>65536)return 0;
    return int(idas3::audioRuntime().ring.read(stereo,unsigned(frames)));
}
extern "C" void IDAS3_AUDIO_CALL Idas3UnitySetAudioRunning(int running){idas3::audioRuntime().ring.setRunning(running!=0);}
extern "C" int IDAS3_AUDIO_CALL Idas3UnityReadAudioDevice(float* output,int frames,int channels,int rate){
    if(!output||frames<=0||frames>65536||channels<1||channels>8||rate<8000||rate>192000)return 0;
    return int(idas3::audioRuntime().ring.readDevice(output,unsigned(frames),unsigned(channels),unsigned(rate)));
}
extern "C" int IDAS3_AUDIO_CALL Idas3UnityGetAudioStatistics(Idas3UnityAudioStatistics* destination){
    if(!destination||destination->size!=sizeof(Idas3UnityAudioStatistics))return 0;
    try{
        const auto s=idas3::unityAudioOutputStatistics();
        *destination={sizeof(Idas3UnityAudioStatistics),1,s.producedFrames,s.consumedFrames,s.droppedFrames,
            s.underrunFrames,s.primingFrames,s.discardedFrames,s.queuedFrames};
        return 1;
    }catch(...){return 0;}
}
