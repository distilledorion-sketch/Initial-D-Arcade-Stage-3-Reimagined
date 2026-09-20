#pragma once
#include "math_types.h"
#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace idas3 {
enum class RacePhase { Ready, Countdown, Running, Finished };
class RaceClock {
public:
    static constexpr int hz=60;
    RacePhase phase=RacePhase::Ready;
    std::uint64_t ticks=0;
    int countdown=180, sector=0;
    int originalStartDigit=-1;
    unsigned originalStartElapsed=0;
    std::array<std::uint64_t,4> splits{};
    bool originalTiming=false,timeUp=false;
    std::uint32_t elapsed6000=0;
    std::int32_t remaining6000=0;
    std::array<std::uint32_t,4> sectionTimes6000{};
    unsigned sectionCapacity=4;
    float progress=0, furthest=0, courseLength=1;
    void start(float totalLength);
    void tick(float courseProgress);
    double seconds() const { return originalTiming?double(elapsed6000)/6000:double(ticks)/hz; }
    int countdownDigit() const { return (countdown+hz-1)/hz; }
};
// IDR2 preserves every 60 Hz simulation frame without float quantization.
struct ReplayDetail {
    float rpm=0;Vec3 bodyPosition{};float pitch=0,roll=0,steering=0;
    std::array<float,4> suspension{},rotation{};
    float throttle=0,brake=0;
    std::uint32_t elapsed=0;std::int32_t remaining=0;
    std::array<std::uint32_t,4> sections{};
    std::uint32_t sector=0,capacity=4;
    std::int32_t lightCounter=0;std::uint32_t lightMaximum=0,lightPhase=0,lightVisible=0;
    float lightFraction=0;std::uint32_t lights=0;float progress=0;std::uint32_t extension=0;
};
static_assert(sizeof(ReplayDetail)==132);
inline constexpr std::array<unsigned,12> replayProfileOffsets{32,64,44,48,52,56,60,76,152,156,160,164};
struct ReplayFrame { std::uint64_t tick; Vec3 position; float yaw,speed; int gear; ReplayDetail detail{}; };
class Replay {
public:
    static constexpr std::size_t maxFrames=60*60*30; // Thirty minutes at 60 Hz.
    std::vector<ReplayFrame> frames;
    bool truncated=false;
    bool detailed=false;
    std::array<std::uint32_t,12> profile{};
    std::array<std::uint32_t,3> priorRecords{};
    // Exact original finish timestamp; zero means telemetry without a finish.
    std::uint32_t finishTicks6000=0;
    void beginCapture(bool details);
    void record(std::uint64_t tick,Vec3 position,float yaw,float speed,int gear,const ReplayDetail* detail=nullptr);
    bool save(const std::string& filename) const;
    bool load(const std::string& filename);
    // Portable little-endian pose replay: 20 Hz plus the exact final frame.
    std::vector<std::uint8_t> sharedBytes(std::uint32_t finish) const;
    ReplayFrame sample(double tick) const;
};
// Wall-clock accumulation is separate from simulation time. Focus/pause clears it.
class FixedClock {
public:
    static constexpr double step=1.0/60.0;
    double accumulator=0;
    template<class F> int advance(double elapsed,F&& f) {
        accumulator+=std::clamp(elapsed,0.0,0.1);
        int steps=0;
        while(accumulator+1e-12>=step && steps<6) {
            f(); accumulator=std::max(0.0,accumulator-step);++steps;
        }
        return steps;
    }
    float alpha() const { return float(std::clamp(accumulator/step,0.0,1.0)); }
    void reset() { accumulator=0; }
};
std::string formatTime(double seconds);
}
