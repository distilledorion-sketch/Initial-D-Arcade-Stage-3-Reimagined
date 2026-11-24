#include "flycast_keyboard_steering.h"
#include "original_host_input.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::uint64_t checks=0,instructions=0;
void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
using idas3::FlycastKeyboardSteering;
using namespace idas3::original;
void originalAdc(const char* image){
    // Independent JVS wire compensation followed by the signed-low-byte
    // guest reconstruction; retain the full word, including saturation.
    for(int raw=-32768;raw<=32767;++raw){
        auto wire=std::uint16_t(raw+0x8000);
        if(wire>0xff7f)wire=0xff7f;
        if(wire&0x80)wire=std::uint16_t(wire+0x100);
        const auto guest=std::uint16_t(int(wire&0xff00)+int(std::int8_t(wire&255)));
        require(FlycastKeyboardSteering::analogFromRaw(std::int16_t(raw))==guest,"Full signed axis JVS conversion differs");
        OriginalHostInputState state;OriginalHostControls host;host.steeringAnalog=guest;
        require(adaptOriginalHostInput(state,host,false,true,0).analog.steering==guest,"Host adapter truncated keyboard raw ADC");
    }
    if(!image)return;
    // Actual original 15CF64..15D124, all high-byte values and both steering
    // modes. No arithmetic leaf hooks: check every published output word.
    idas3::reference::RefMemory memory{std::filesystem::path(image)};
    for(unsigned high=0;high<256;++high)for(unsigned scale:{0u,1u}){
        memory.clear();constexpr std::uint32_t raw=0x0c92f0e0,drive=0x0c900f00;
        OriginalHostInputState state;OriginalHostControls host;host.steeringAnalog=int(high*256+(high^0x5a));
        const auto packet=adaptOriginalHostInput(state,host,false,true,0);
        memory.write16(raw,packet.analog.steering);memory.write16(raw+2,packet.analog.throttle);memory.write16(raw+4,packet.analog.brake);
        memory.write32(0x0c9015e8,128);memory.write32(0x0c9015c8,32);memory.write32(0x0c90164c,32);
        memory.write32(0x0c2f4bc8,0);memory.write32(drive+0x13c,scale);memory.write32(0x0c900ea0,0);
        for(auto address:{0x0caa9894u,0x0caa9898u,0x0caa989cu,0x0caa98a0u,0x0caa98acu,drive+0x1c8})memory.write32(address,0xdeadbeef);
        idas3::reference::RefCpu cpu(memory);cpu.r[2]=cpu.r[3]=raw;
        for(unsigned fr:{2u,4u,12u,13u,15u})cpu.setFloat(fr,0);
        instructions+=cpu.run(0x0c15cf64,0x0c15d124,512);
        const auto native=idas3::conditionOriginalInputs(packet.analog,packet.calibration,{0,scale},idas3::verifiedGds0033InputConstants(),0);
        require(memory.read32(0x0caa9894)==bits(native.steering),"Original steering result differs");
        require(memory.read32(drive+0x1c8)==bits(native.steeringAlias),"Original steering alias differs");
        require(memory.read32(0x0caa9898)==bits(native.throttle)&&memory.read32(0x0caa989c)==bits(native.throttleAlias),"Keyboard raw channel altered throttle");
        require(memory.read32(0x0caa98a0)==bits(native.brake)&&memory.read32(0x0caa98ac)==0,"Keyboard raw channel altered brake/passthrough");
    }
}
}
int main(int argc,char** argv)try{
    FlycastKeyboardSteering key;
    key.sample(1000,false,true);require(key.raw()==0,"New press moved before its source ramp sample");
    key.sample(1016,false,true);require(key.raw()==5243,"16ms source float rounding differs");
    key.sample(1033,false,true);require(key.raw()==10813,"17ms incremental rounding differs");
    key.sample(1050,false,true);require(key.raw()==16383,"Raw ramp lost retained signed position");
    key.sample(1066,false,true);require(key.raw()==21626&&key.steering()==1,"Calibrated full lock should precede raw saturation");
    key.sample(1101,false,true);require(key.raw()==32767,"Raw right endpoint differs");
    key.sample(1102,false,false);require(key.raw()==32767,"Release event was applied before the preceding ramp");
    key.sample(1118,false,false);require(key.raw()==27524&&key.steering()==1,"First release plateau lost");
    key.sample(1134,false,false);require(key.raw()==22281&&key.steering()==1,"Second release plateau lost");
    key.sample(1150,false,false);require(key.raw()==17038&&key.steering()==.825f,"Release ADC quantization differs");
    key.sample(1250,false,false);require(key.raw()==0,"Release failed to center");
    key.sample(1250,true,false);require(key.raw()==0,"Same-timestamp event advanced axis");
    key.sample(1451,true,false);require(key.raw()==-32768,"Raw left endpoint differs");
    key.sample(1451,false,true);key.sample(1551,false,true);require(key.raw()==-1,"Reversal lost signed endpoint asymmetry");
    key.sample(1552,true,true);require(key.raw()==327,"New opposite-key event changed preceding interval");
    key.sample(1553,true,true);require(key.raw()==0,"Opposite held keys must decay to center");
    key.sample(1554,false,true);key.sample(std::numeric_limits<std::uint64_t>::max(),false,true);
    require(key.raw()==32767,"Long host sleep overflowed signed axis");
    key.sample(10,true,false);require(key.raw()==0,"Reset host clock retained stale steering");
    key.sample(211,true,false);require(key.raw()==-32768,"Clock restart lost next held direction");
    key.reset();require(key.raw()==0&&key.analog()==0x8000,"Lifecycle reset did not neutralize full raw ADC");

    OriginalHostInputState state;OriginalHostControls invalid;invalid.steeringAnalog=65536;
    bool rejected=false;try{adaptOriginalHostInput(state,invalid,false,true,0);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid raw ADC was silently wrapped");
    originalAdc(argc>1?argv[1]:nullptr);
    std::cout<<"PASS "<<checks<<" keyboard owner/JVS checks, "<<instructions<<" original instructions; host event order, signed ramp, full raw ADC, clock reset and lifecycle.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
