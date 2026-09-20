#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace idas3 {
class EngineAudio;
struct UnityAudioFrameState {
    float rpm=0,throttle=0,speed=0,slip=0;
    bool active=false,paused=false,audible=true;
};
struct UnityAudioOutputStatistics {
    std::uint64_t producedFrames=0,consumedFrames=0,droppedFrames=0;
    std::uint64_t underrunFrames=0,primingFrames=0,discardedFrames=0,queuedFrames=0;
};

// Exactly one producer (Unity main thread) and one consumer (Unity PCM reader).
// clear() never rewinds either cursor or overwrites memory owned by the reader.
class UnityStereoPcmRing {
public:
    static constexpr unsigned sampleRate=44100,capacityFrames=8192;
    static constexpr unsigned prebufferFrames=1024,maximumLatencyFrames=4096;
    void setRunning(bool value) noexcept;
    void setAudible(bool value) noexcept;
    void clear() noexcept;
    unsigned write(std::span<const float> interleavedStereo) noexcept;
    unsigned read(float* interleavedStereo,unsigned frameCount) noexcept;
    // Alternative consumer for Unity's actual DSP blocks. The original mixer
    // still produces44100Hz. Exact integer phase preserves pitch at device rate.
    // Use one reader API exclusively on the single audio thread.
    unsigned readDevice(float* interleaved,unsigned frameCount,unsigned channels,unsigned sampleRate) noexcept;
    UnityAudioOutputStatistics statistics()const noexcept;
private:
    std::array<std::array<float,2>,capacityFrames> pcm_{};
    alignas(64) std::atomic<std::uint64_t> written_{0};
    alignas(64) std::atomic<std::uint64_t> read_{0};
    std::atomic<std::uint64_t> discardThrough_{0},epoch_{0};
    std::atomic<bool> running_{false},audible_{false};
    std::atomic<std::uint64_t> produced_{0},consumed_{0},dropped_{0},underrun_{0},priming_{0},discarded_{0};
    // Consumer-owned. No main-thread code mutates these fields.
    std::uint64_t observedEpoch_=0;
    std::uint64_t devicePhase_=0;
    unsigned deviceRate_=0;
    bool primed_=false;
};

class UnityAudioFrameClock {
public:
    unsigned advance(double deltaSeconds) noexcept;
    void reset() noexcept {fractionalFrames_=0;}
private:
    double fractionalFrames_=0;
};

// Main thread only. Native scene owners and EngineAudio::scene run before
// submit; renderStereo then advances unchanged source sound/DSP on that thread.
void resetUnityAudioOutput() noexcept;
void submitUnityAudioOutput(EngineAudio&,double deltaSeconds,const UnityAudioFrameState&);
UnityAudioOutputStatistics unityAudioOutputStatistics() noexcept;
}

#if defined(_WIN32)
#define IDAS3_AUDIO_EXPORT __declspec(dllexport)
#define IDAS3_AUDIO_CALL __cdecl
#else
#define IDAS3_AUDIO_EXPORT __attribute__((visibility("default")))
#define IDAS3_AUDIO_CALL
#endif
struct Idas3UnityAudioStatistics {
    std::uint32_t size=sizeof(Idas3UnityAudioStatistics),version=1;
    std::uint64_t producedFrames=0,consumedFrames=0,droppedFrames=0;
    std::uint64_t underrunFrames=0,primingFrames=0,discardedFrames=0,queuedFrames=0;
};
static_assert(sizeof(Idas3UnityAudioStatistics)==64);
extern "C" {
// Audio callback: always fills the provided stereo buffer, returns real frames.
// Does not acquire locks, allocate, access App/EngineAudio, or use the filesystem.
IDAS3_AUDIO_EXPORT int IDAS3_AUDIO_CALL Idas3UnityReadAudio(float* stereo,int frameCount);
IDAS3_AUDIO_EXPORT int IDAS3_AUDIO_CALL Idas3UnityReadAudioDevice(float* output,int frameCount,int channels,int sampleRate);
// Unity main-thread lifecycle gate; keep DLL loaded until AudioSource is stopped.
IDAS3_AUDIO_EXPORT void IDAS3_AUDIO_CALL Idas3UnitySetAudioRunning(int running);
// Read-only cumulative counters; fields are independent atomic snapshots.
IDAS3_AUDIO_EXPORT int IDAS3_AUDIO_CALL Idas3UnityGetAudioStatistics(Idas3UnityAudioStatistics* destination);
}
