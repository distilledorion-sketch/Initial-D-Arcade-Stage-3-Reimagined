#pragma once
#include <chrono>

namespace idas3 {
// The registered callback belongs to the calling Unity thread. Asset work on
// another thread never invokes it. It may only service transport IO: native
// scene/physics/UI APIs can hold locks here and must not be re-entered.
#if defined(_WIN32)
using LoadKeepAliveCallback = void(__cdecl*)();
#else
using LoadKeepAliveCallback = void(*)();
#endif
struct LoadKeepAliveState {
    using Clock = std::chrono::steady_clock;
    LoadKeepAliveCallback callback=nullptr;
    Clock::time_point next{};
    bool pumping=false;
    void service(Clock::time_point now) noexcept {
        if(!callback||pumping||now<next)return;
        next=now+std::chrono::milliseconds(100);pumping=true;
        // Managed callers also catch exceptions before this ABI boundary.
        try{callback();}catch(...){}
        pumping=false;
    }
};
inline thread_local LoadKeepAliveState loadKeepAliveState;
inline void setLoadKeepAlive(LoadKeepAliveCallback callback) noexcept {
    loadKeepAliveState.callback=callback;loadKeepAliveState.next={};
}
inline void serviceLoadKeepAlive() noexcept {
    auto& state=loadKeepAliveState;
    if(state.callback&&!state.pumping)state.service(LoadKeepAliveState::Clock::now());
}
}
