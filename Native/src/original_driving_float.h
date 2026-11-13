#pragma once
#include <cstdint>
#if defined(__clang__)
// Keep arithmetic inside the control-register scope. Without FENV_ACCESS,
// Clang may move a floating-point operation before the FPCR/MXCSR write.
#pragma STDC FENV_ACCESS ON
#endif
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#include <xmmintrin.h>
#elif defined(_M_ARM64)
#include <intrin.h>
#endif

namespace idas3::original {
// The captured AS3/Flycast driving execution uses nearest rounding and flushes
// subnormal values. Do not inherit a caller's floating-point mode: a native
// test, Unity main thread and online worker can have different defaults.
// Scope the control register to the solver and restore it on every exit,
// including exceptions. No process-wide setting or handling coefficient.
class OriginalDrivingFloatScope {
public:
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    OriginalDrivingFloatScope() : saved_(_mm_getcsr()) {
        _mm_setcsr((saved_ & ~0x6000u) | 0x8040u); // nearest, FTZ and DAZ
    }
    ~OriginalDrivingFloatScope() { _mm_setcsr(saved_); }
private:
    unsigned saved_;
#elif defined(__aarch64__) || defined(_M_ARM64)
    OriginalDrivingFloatScope() : saved_(read()) {
        write((saved_ & ~(std::uint64_t(3)<<22)) | (std::uint64_t(1)<<24));
    }
    ~OriginalDrivingFloatScope() { write(saved_); }
private:
    std::uint64_t saved_;
    static std::uint64_t read() {
#if defined(_M_ARM64)
        return _ReadStatusReg(ARM64_FPCR);
#else
        std::uint64_t value;asm volatile("mrs %0, fpcr" : "=r"(value) : : "memory");return value;
#endif
    }
    static void write(std::uint64_t value) {
#if defined(_M_ARM64)
        _WriteStatusReg(ARM64_FPCR,value);
#else
        asm volatile("msr fpcr, %0" : : "r"(value) : "memory");
#endif
    }
#else
#error Original driving requires an explicit floating-point control implementation
#endif
    OriginalDrivingFloatScope(const OriginalDrivingFloatScope&)=delete;
    OriginalDrivingFloatScope& operator=(const OriginalDrivingFloatScope&)=delete;
};
}
