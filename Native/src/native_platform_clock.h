#pragma once
#include "original_platform_clock.h"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

namespace idas3 {
//Host persistence/RTC adapter, separate from source arithmetic. One owner per
//userdata root represents one cabinet; numerical/online sessions do not use it.
class NativePlatformClock {
public:
    NativePlatformClock();
    ~NativePlatformClock();
    NativePlatformClock(const NativePlatformClock&)=delete;
    NativePlatformClock& operator=(const NativePlatformClock&)=delete;
    static std::uint64_t hostMilliseconds();
    std::uint32_t activate(const std::filesystem::path& userdata,std::uint64_t milliseconds);
    //Explicit deterministic injection; optional private-test persistence sink.
    //With no sink, this never creates a worker or accesses the filesystem.
    std::uint32_t activateInjected(std::uint32_t retained,std::uint64_t milliseconds,
        std::function<bool(std::uint32_t)> persist={});
    //Only queues changed snapshots; never performs or waits for disk IO.
    bool advanceTo(std::uint64_t milliseconds);
    //Shutdown/testing boundary only: drains the latest snapshot, retrying a
    //previous completed failure once. Gameplay must not call this.
    bool flush()noexcept;
    bool active()const{return active_;}
    std::uint32_t current()const{return clock_.current();}
    std::uint64_t ticks()const{return ticks_;}
    const original::OriginalPlatformClock& source()const{return clock_;}
private:
    original::OriginalPlatformClock clock_;
    std::filesystem::path root_,file_;
    std::uint64_t bootMilliseconds_=0,lastMilliseconds_=0,ticks_=0;
    std::intptr_t lock_=-1;
    struct Writer;
    std::unique_ptr<Writer> writer_;
    bool active_=false,persistent_=false,degraded_=false;
    void begin(std::uint32_t retained,std::uint64_t milliseconds);
    void releaseLock()noexcept;
};
}
