#include "original_initialization.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <unordered_map>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
using Words=std::unordered_map<std::uint32_t,std::uint32_t>;
static std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
static Words serialize(const OriginalVehicleState& s,const OriginalVehicleParameters& p,
        const OriginalInitializationSideState& side){
    Words out;
    for(std::size_t i=0;i<s.drive.words.size();++i)out[0x0C900F00+std::uint32_t(i*4)]=s.drive.words[i];
    auto transmission=std::bit_cast<std::array<std::uint32_t,10>>(s.transmission);
    for(std::size_t i=0;i<10;++i)out[0x0C900E88+std::uint32_t(i*4)]=transmission[i];
    for(std::size_t i=0;i<64;++i){out[0x0CAA98E0+std::uint32_t(i*4)]=bits(s.tail.history0CAA98E0[i]);out[0x0CAA9BE0+std::uint32_t(i*4)]=bits(s.tail.steeringHistory0CAA9BE0[i]);}
    for(std::size_t i=0;i<128;++i)out[0x0CAA99E0+std::uint32_t(i*4)]=bits(s.tail.throttleHistory0CAA99E0[i]);
    for(std::size_t i=0;i<16;++i)out[0x0C91FB0C+std::uint32_t(i*4)]=s.tail.statistics0C91FB0C[i];
    const auto& g=s.transmissionGlobals;const auto& h=s.tail;const auto& c=s.controls;
    out[0x0CAA9870]=g.phase9870;out[0x0CAA9874]=bits(h.previousSpeed0CAA9874);out[0x0CAA9878]=bits(h.speedDelta0CAA9878);
    out[0x0CAA9880]=bits(s.loss.speedLoss0CAA9880);out[0x0CAA9884]=bits(s.loss.persistentPenalty0CAA9884);
    out[0x0CAA9888]=h.lastNonzeroGear0CAA9888;out[0x0CAA988C]=h.previousGear0CAA988C;
    out[0x0CAA9894]=bits(c.steering);out[0x0CAA9898]=bits(c.throttle);out[0x0CAA989C]=bits(c.throttleAlias);out[0x0CAA98A0]=bits(c.brake);
    out[0x0CAA98A8]=bits(g.shiftDifference98a8);out[0x0CAA98AC]=bits(g.coupledSnapshot98ac);out[0x0CAA98B0]=bits(h.filteredDelta0CAA98B0);out[0x0CAA98D0]=bits(g.coupling98d0);
    out[0x0CAA9CE0]=bits(h.mean0CAA9CE0);out[0x0CAA9CE4]=h.counter0CAA9CE4;out[0x0CAA9CE8]=h.counter0CAA9CE8;out[0x0CAA9CEC]=h.counter0CAA9CEC;
    out[0x0CAA9CFC]=g.downCounter9cfc;out[0x0C91FB4C]=g.flag91fb4c;
    out[0x0C900E40]=bits(p.angular.global0C900E40);out[0x0C8FF380]=bits(p.angular.global0C8FF380);
    out[0x0C900EC0]=bits(p.angular.global0C900EC0);out[0x0C900E54]=bits(p.angular.global0C900E54);
    out[0x0C900E4C]=bits(p.steeringMemory.global0C900E4C);out[0x0C90094C]=bits(p.steeringMemory.global0C90094C);
    out[0x0C900EB0]=bits(p.steeringMemory.global0C900EB0);out[0x0C900E30]=bits(p.steeringMemory.global0C900E30);
    out[0x0C900EF8]=bits(p.steeringMemory.global0C900EF8);out[0x0CAA9CF0]=p.steeringMemory.mask0CAA9CF0;
    for(std::size_t i=0;i<12;++i)out[initializationContactMultiplierAddresses[i]]=bits(side.contactMultipliers[i]);
    for(std::size_t i=0;i<18;++i)out[initializationOtherGlobalAddresses[i]]=side.otherGlobals[i];
    // These two are shared aliases, not independent fields.
    out[0x0CAA987C]=bits(p.frame.global0CAA987C);out[0x0C91FB40]=s.tail.statistics0C91FB0C[13];
    for(std::size_t i=0;i<3;++i)out[0x0CFC0000+std::uint32_t(i*4)]=bits(side.actorPosition[i]);
    out[0x0CFC0050]=side.actorFlags50;out[0x0C37C778]=side.randomSeed0C37C778;
    return out;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass the canonical original program image");
    RefMemory memory{std::filesystem::path(argv[1])};
    std::size_t comparisons=0,instructions=0;
    for(std::uint32_t vehicle=0;vehicle<35;++vehicle)for(std::uint32_t mode=0;mode<32;++mode){
        memory.clear();memory.zeroRegion(0x0C800000,0x800000);
        OriginalVehicleState s;OriginalVehicleParameters p;OriginalInitializationSideState side;
        for(std::size_t i=0;i<s.drive.words.size();++i)s.drive.words[i]=0x3F000000u+std::uint32_t(i*13);
        std::array<std::uint32_t,10> transmission{};
        for(std::size_t i=0;i<10;++i)transmission[i]=0x3F800000u+std::uint32_t(i*23);
        s.transmission=std::bit_cast<OriginalTransmissionState>(transmission);
        s.tail.history0CAA98E0.fill(3.25f);s.tail.throttleHistory0CAA99E0.fill(-.375f);s.tail.steeringHistory0CAA9BE0.fill(.75f);
        s.tail.statistics0C91FB0C.fill(123);s.tail.counter0CAA9CEC=943;
        s.tail.previousSpeed0CAA9874=57;s.tail.speedDelta0CAA9878=19;
        s.tail.counter0CAA9CE4=414;s.tail.counter0CAA9CE8=51;
        s.transmissionGlobals.phase9870=73;s.transmissionGlobals.shiftDifference98a8=52;
        s.transmissionGlobals.coupling98d0=9;s.transmissionGlobals.downCounter9cfc=72;
        s.transmissionGlobals.flag91fb4c=1;s.controls.throttleAlias=.5f;
        s.loss.speedLoss0CAA9880=2;s.loss.persistentPenalty0CAA9884=3;
        side.contactMultipliers.fill(.625f);side.otherGlobals.fill(42);
        side.actorPosition={10,20,30};side.actorFlags50=0xACEF00FFu;
        side.randomSeed0C37C778=0xFA731942u+mode+vehicle*0x10101u;
        OriginalInitializationInputs in;
        in.position={float(vehicle)*7.125f,1130-float(mode)*17.25f,float(mode)*-2.75f};
        in.angles={.01f*float(mode),-.1f*float(vehicle),.002f*float(vehicle)};
        in.vehicleIndex0C901654=vehicle;in.throttleHistoryCount0C285098=memory.read32(0x0C285098+vehicle*4);
        in.vehicleType0C284EF4=memory.read32(0x0C284EF4+vehicle*4);in.modeMask0C283E08=memory.read32(0x0C283E08+mode*4);
        in.mode0C9015FC=mode&1;in.mode0C9015C0=mode&2;
        auto before=serialize(s,p,side);
        for(const auto [address,value]:before)memory.write32(address,value);
        memory.write32(0x0C900954,0x0CFC0000);memory.write32(0x0C901654,vehicle);
        memory.write32(0x0C9015E0,mode);memory.write32(0x0C9015FC,in.mode0C9015FC);memory.write32(0x0C9015C0,in.mode0C9015C0);
        for(std::size_t i=0;i<3;++i){memory.writeFloat(0x0CFD0000+std::uint32_t(i*4),in.position[i]);memory.writeFloat(0x0CFD0010+std::uint32_t(i*4),in.angles[i]);}
        RefCpu cpu(memory);cpu.r[4]=0x0CFD0000;cpu.r[5]=0x0CFD0010;cpu.r[15]=0x0CFFF100;cpu.pr=0x0F000000;
        cpu.callHooks[0x0C15A380]=[](RefCpu&){};
        instructions+=cpu.run(0x0C15EE00,0x0F000000,20000);
        const auto result=initializeOriginalVehicle(s,p,side,in);
        if(!result.resetPlatformDigitalInput)throw std::runtime_error("Missing platform-reset effect");
        const auto after=serialize(s,p,side);
        for(const auto [address,actual]:after){
            ++comparisons;const auto expected=memory.read32(address);
            if(expected!=actual){std::cerr<<"Initializer mismatch vehicle="<<vehicle<<" mode="<<mode<<" address="<<std::hex<<address<<" expected="<<expected<<" actual="<<actual<<std::dec<<'\n';return 1;}
        }
    }
    std::cout<<"PASS 1,120 original initializer cases (35 cars x 32 modes), "<<comparisons<<" bit-exact field comparisons, "<<instructions<<" decoded instructions; only platform digital reset15A380 hooked. Original RNG executes.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
