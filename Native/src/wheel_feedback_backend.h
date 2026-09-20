#pragma once
#include <cstdint>

// Separate Windows DirectInput DLL. Discovery never acquires a device or
// sends motor commands; output always requires an explicitly selected GUID.
#if defined(_WIN32)
#define IDAS3_WHEEL_EXPORT extern "C" __declspec(dllexport)
#define IDAS3_WHEEL_CALL __cdecl
#else
#define IDAS3_WHEEL_EXPORT extern "C"
#define IDAS3_WHEEL_CALL
#endif

struct Idas3WheelDevice {
    std::uint32_t size;
    char id[40];                 // Canonical {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
    char name[128];              // Null-terminated UTF-8, truncated at a code point.
    std::uint32_t vendorId;      // Zero when the driver does not expose VID/PID.
    std::uint32_t productId;
};
static_assert(sizeof(Idas3WheelDevice)==180);

// Returns the number of attached force-feedback devices, or -1 on failure.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelRefreshDevices() noexcept;
// Caller must set out->size=sizeof(Idas3WheelDevice). Returns 1 on success.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelGetDevice(int index,Idas3WheelDevice* out) noexcept;
// One coherent view of the arcade board, mirroring idas3ffb::Snapshot. The
// cabinet plays all of these at once; a wheel that lacks an effect simply does
// not receive that part.
struct Idas3WheelCabinetState {
    std::uint32_t size;              // caller sets sizeof(Idas3WheelCabinetState)
    std::uint32_t version;           // 1
    float power;                     // [0,1] board drive power, scales everything
    float torque;                    // [-1,1] signed constant force
    float spring;                    // [0,1] centring magnitude
    float damperStrength;            // [0,1] viscous damping
    float damperParameter;           // [0,1] the board's transfer-function pole
    float rumbleIntensity;           // [0,1] sine magnitude
    float rumbleFrequencyHz;         // sine rate; <=0 silences the rumble
    std::uint32_t active;            // 0 releases the wheel, as command 0 does
};
static_assert(sizeof(Idas3WheelCabinetState)==40);

// Apply the whole board state at once. Same ownership, foreground and 100 ms
// lease rules as Idas3WheelSetForce. Effects the wheel cannot play are skipped
// rather than failing the call. Returns 1 on success.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelSetCabinetState(const char* guidUtf8,
    const Idas3WheelCabinetState* state) noexcept;

// The driving state the cabinet's force owner reads, in this remake's units.
// Idas3WheelUpdateCabinet runs the ported board and its owner over this and
// applies the result, so the game sends telemetry rather than forces and the
// whole force model stays on the native side where it is testable.
struct Idas3WheelRaceState {
    std::uint32_t size;              // caller sets sizeof(Idas3WheelRaceState)
    std::uint32_t version;           // 1
    std::uint32_t driving;           // 0 releases the wheel between races
    std::uint32_t carIndex;          // 0..34; picks the cabinet's per-car force limit
    float speedKmh;
    float steering;                  // [-1,1] as the driver is asking
    float headingError;              // radians of slip
    float wallLateral;               // [-1,1] signed push out of a barrier
    float impact;                    // [0,1] one-shot collision severity
    std::uint32_t wallContact;
    float strength;                  // [0,1] the player's feedback setting
    std::uint32_t invert;
    float deltaSeconds;              // frame time, used only to age the rumble
};
static_assert(sizeof(Idas3WheelRaceState)==52);

// Run one frame of the cabinet board and apply it to the wheel. Same ownership,
// foreground and lease rules as the calls below. Returns 1 on success.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelUpdateCabinet(const char* guidUtf8,
    const Idas3WheelRaceState* state) noexcept;

// Read back what the board is playing, so the game can show it and tests can
// assert on it without a wheel attached. Returns 1 on success.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelGetCabinetState(Idas3WheelCabinetState* out) noexcept;

// Signed constant force in [-1,1], at most 100 ms per successful call. Invalid
// input stops/releases any owned device. Zero stops a cached effect; a first
// zero does not acquire anything. The managed owner should refresh at 60 Hz.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelSetForce(const char* guidUtf8,float force) noexcept;
// Release effect/device and restore the saved autocenter setting. No reacquire.
IDAS3_WHEEL_EXPORT void IDAS3_WHEEL_CALL Idas3WheelStop() noexcept;
IDAS3_WHEEL_EXPORT void IDAS3_WHEEL_CALL Idas3WheelShutdown() noexcept;
// Returns UTF-8 bytes copied, excluding the terminator. Null/zero capacity
// returns zero. A positive capacity always receives a null-terminated string.
IDAS3_WHEEL_EXPORT int IDAS3_WHEEL_CALL Idas3WheelCopyStatus(char* dest,int capacity) noexcept;
