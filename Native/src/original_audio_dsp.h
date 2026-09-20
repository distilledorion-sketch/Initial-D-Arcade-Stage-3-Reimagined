#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
namespace idas3 {
struct OriginalAudioDspProgram {
    std::array<std::array<std::uint16_t,4>,128> instructions{};
    std::array<std::int16_t,128> coefficients{};
    std::array<std::uint16_t,64> memoryAddresses{};
    std::array<std::uint16_t,16> effectRoutes{};
    std::uint32_t ringBaseBytes=0,ringLengthWords=32768;
    // Owned logical window starting at ringBaseBytes, up to 64K words. Empty
    // means the original 0x6000 floating-point silence fill.
    std::vector<std::uint16_t> initialMemoryWords;
};
struct OriginalAudioDspFrame {
    std::array<std::int16_t,16> effects{}; // raw EFREG16-bit outputs
    std::array<std::int32_t,2> wet{}; // original effectRoutes level/pan, no host gain
};
std::uint16_t packOriginalDspFloat(std::int32_t value);
std::int32_t unpackOriginalDspFloat(std::uint16_t value);
class OriginalAudioDsp {
public:
    void configure(const OriginalAudioDspProgram& program,bool preserveState=false);
    void reset();
    // ARM612C clears registers and TEMP/MEMS, preserves ring RAM and phase,
    // and installs muted 0x10 effect routes. Voice sends are managed separately.
    void clearProgram();
    // Writes one current EFREG return route without touching DSP state.
    void setEffectRoute(unsigned index,std::uint16_t route);
    OriginalAudioDspFrame render(std::span<const std::int32_t,16> input,
                                std::array<std::int16_t,2> external={});
    const std::array<std::int32_t,128>& temporary()const{return temporary_;}
    const std::array<std::int32_t,32>& memoryRegisters()const{return memoryRegisters_;}
    const std::vector<std::uint16_t>& memory()const{return memory_;}
    unsigned decrementCounter()const{return decrement_;}
    std::uint64_t frames()const{return frames_;}
    const OriginalAudioDspProgram& program()const{return program_;}
private:
    struct Instruction {
        unsigned tra=0,twa=0,ira=0,iwa=0,ysel=0,shift=0,ewa=0,masa=0;
        bool twt=false,xsel=false,iwt=false,table=false,mwt=false,mrd=false,
            ewt=false,adrl=false,frcl=false,yrl=false,negb=false,zero=false,
            bsel=false,nofl=false,adreb=false,nxadr=false;
    };
    OriginalAudioDspProgram program_;
    std::array<Instruction,128> instructions_{};
    std::array<std::int32_t,128> temporary_{};
    std::array<std::int32_t,32> memoryRegisters_{};
    std::array<std::int16_t,16> effects_{};
    std::vector<std::uint16_t> memory_;
    unsigned decrement_=1;
    std::uint64_t frames_=0;
    bool configured_=false;
};
}
