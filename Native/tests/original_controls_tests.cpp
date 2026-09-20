#include "original_controls.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace idas3;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t rawBase=0x0c92f0e0,driveBase=0x0c900f00;
constexpr std::uint32_t steeringDestination=0x0caa9894,throttleDestination=0x0caa9898;
constexpr std::uint32_t throttleAliasDestination=0x0caa989c,brakeDestination=0x0caa98a0;
constexpr std::uint32_t passthroughDestination=0x0caa98ac,steeringAliasDestination=driveBase+0x1c8;
void require(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
std::uint32_t bits(float value) {return std::bit_cast<std::uint32_t>(value);}
std::uint16_t rawByte(std::uint32_t high,std::uint32_t low=0) {return std::uint16_t((high<<8)|(low&255));}

struct Differential {
    RefMemory memory;
    OriginalInputConstants constants=verifiedGds0033InputConstants();
    std::size_t cases=0,instructions=0;
    explicit Differential(const char* image):memory(image) {
        require(memory.read16(0x0c15d0a6)==128,"Steering-addend literal identity");
        require(memory.read32(0x0c15d0f0)==constants.negativeSignBits,"Steering-sign literal identity");
        require(memory.read32(0x0c15d0f4)==constants.steeringDivisorBits,"Steering-divisor literal identity");
        require(memory.read32(0x0c15d0f8)==constants.pedalDivisorBits,"Pedal-divisor literal identity");
        require(memory.read32(0x0c15d0fc)==constants.steeringScaleBits,"Steering-mode literal identity");
        require(memory.read16(0x0c15cf64)==0x6135,"Raw-input entry opcode identity");
    }
    OriginalControls check(RawAnalog16 raw,OriginalInputCalibration calibration,OriginalInputMode mode,std::uint32_t passthrough) {
        memory.clear();
        // Every external read is initialized explicitly. An accidentally added
        // dependency therefore fails closed instead of reading fabricated RAM.
        memory.write16(rawBase,raw.steering);memory.write16(rawBase+2,raw.throttle);memory.write16(rawBase+4,raw.brake);
        memory.write32(0x0c9015e8,calibration.steeringWord);
        memory.write32(0x0c9015c8,calibration.throttleWord);
        memory.write32(0x0c90164c,calibration.brakeWord);
        memory.write32(0x0c2f4bc8,mode.suppressRawThrottleWord);
        memory.write32(driveBase+0x13c,mode.scaleSteeringWord);
        memory.write32(0x0c900ea0,passthrough);
        for(auto address:{steeringDestination,throttleDestination,throttleAliasDestination,brakeDestination,passthroughDestination,steeringAliasDestination})
            memory.write32(address,0xdeadbeefu);
        RefCpu cpu(memory);
        cpu.r[2]=rawBase;cpu.r[3]=rawBase;
        cpu.setFloat(4,0.0f);
        // Only the omitted preamble's register-only parameter adds use these.
        cpu.setFloat(15,0.0f);cpu.setFloat(13,0.0f);cpu.setFloat(12,0.0f);cpu.setFloat(2,0.0f);
        instructions+=cpu.run(0x0c15cf64,0x0c15d124,512);
        const auto native=conditionOriginalInputs(raw,calibration,mode,constants,passthrough);
        const std::array<std::pair<std::uint32_t,std::uint32_t>,6> expected={{{steeringDestination,bits(native.steering)},
            {steeringAliasDestination,bits(native.steeringAlias)},{throttleDestination,bits(native.throttle)},
            {throttleAliasDestination,bits(native.throttleAlias)},{brakeDestination,bits(native.brake)},
            {passthroughDestination,native.passthroughBits}}};
        for(auto [address,value]:expected) if(memory.read32(address)!=value) {
            std::ostringstream text;text<<"Original controls mismatch case="<<cases<<" address=0x"<<std::hex<<address
                <<" opcode_bits=0x"<<memory.read32(address)<<" native_bits=0x"<<value
                <<" raw="<<raw.steering<<','<<raw.throttle<<','<<raw.brake
                <<" calibration="<<calibration.steeringWord<<','<<calibration.throttleWord<<','<<calibration.brakeWord
                <<" mode="<<mode.suppressRawThrottleWord<<','<<mode.scaleSteeringWord;
            throw std::runtime_error(text.str());
        }
        require(memory.read32(0x0c900ea0)==passthrough,"Passthrough source remains untouched");
        ++cases;return native;
    }
};
}

int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Usage: original_controls_tests <verified idas3_main_0C020000.bin>");
        Differential test(argv[1]);
        // Explicit synthetic calibration cases exercise arithmetic, not a claim
        // that any particular cabinet was captured with these values.
        const OriginalInputCalibration centred{128,0,0};
        auto neutral=test.check({rawByte(128),rawByte(32),rawByte(32)},centred,{0,0},0x7fc12345);
        require(bits(neutral.steering)==0x80000000u,"Original neutral steering preserves negative zero");
        require(bits(neutral.throttle)==0 && bits(neutral.brake)==0,"Original pedal threshold is calibration+32");
        auto half=test.check({rawByte(168),rawByte(139),rawByte(139)},centred,{0,0},0xffc54321);
        require(bits(half.steering)==0xbf000000u,"Original steering sign and /80 division");
        require(bits(half.throttle)==0x3f800000u && bits(half.brake)==0x3f800000u,"Pedals saturate at 107 beyond baseline");
        auto scaled=test.check({rawByte(168),rawByte(255),rawByte(255)},centred,{1,1},0x80000000u);
        require(bits(scaled.steering)==0xbecccccdu,"Original conditional steering multiplier");
        require(bits(scaled.throttle)==0,"Mode zeros raw throttle before conditioning");
        require(bits(scaled.brake)==0x3f800000u,"Throttle mode does not suppress brake");

        const std::array<OriginalInputCalibration,5> calibrations={{{128,0,0},{0,32,64},{255,255,255},
            {0xffffffffu,0xffffffffu,0xffffffffu},{0x80000000u,0x7fffffffu,0x80000000u}}};
        for(auto calibration:calibrations) for(std::uint32_t suppress:{0u,0xffffffffu}) for(std::uint32_t scale:{0u,7u}) {
            for(std::uint32_t high=0;high<256;++high) {
                RawAnalog16 raw{rawByte(128,0xab),rawByte(139,0xcd),rawByte(139,0xef)};
                raw.steering=rawByte(high,high^0x5a);test.check(raw,calibration,{suppress,scale},0x7fc00000u|high);
                raw={rawByte(128,0xab),rawByte(high,high^0x69),rawByte(139,0xef)};
                test.check(raw,calibration,{suppress,scale},0x80000000u|high);
                raw={rawByte(128,0xab),rawByte(139,0xcd),rawByte(high,high^0x96)};
                test.check(raw,calibration,{suppress,scale},0xff800000u|high);
            }
        }
        std::uint32_t random=0x159920u;
        auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
        for(int i=0;i<4096;++i) {
            const RawAnalog16 raw{std::uint16_t(next()),std::uint16_t(next()),std::uint16_t(next())};
            const OriginalInputCalibration calibration{next(),next(),next()};
            const OriginalInputMode mode{next()&1u,next()&1u};
            test.check(raw,calibration,mode,next());
        }
        // All 256 low-byte combinations of each channel must be ignored.
        for(std::uint32_t low=0;low<256;++low) {
            auto result=test.check({rawByte(168,low),rawByte(100,low),rawByte(90,low)},centred,{0,0},low);
            require(bits(result.steering)==0xbf000000u,"Raw low byte must not affect steering");
        }
        std::cout<<"Original analog conditioning: "<<test.cases<<" cases, "<<test.instructions
            <<" original SH-4 instructions, six destination words compared bit-for-bit; PASS\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
