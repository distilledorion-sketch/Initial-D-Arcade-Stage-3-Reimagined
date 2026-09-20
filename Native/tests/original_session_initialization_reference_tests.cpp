#include "original_session_initialization.h"
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

static Words serializeSession(const OriginalVehicleState& s,const OriginalVehicleParameters& p,
        const OriginalInitializationSideState& side,const OriginalActorState& actor,
        const OriginalRoadContactState& road,const OriginalSessionInitializationState& session){
    auto out=serialize(s,p,side);
    for(auto a:{0x0CFC0000u,0x0CFC0004u,0x0CFC0008u,0x0CFC0050u})out.erase(a);
    // Original public actor record is168 bytes. The host type's spare capacity
    // is not adjacent original globals90094C.. and must not be serialized there.
    for(std::size_t i=0;i<42;++i)out[0x0C9008A4+std::uint32_t(i*4)]=actor.words[i];
    for(std::size_t i=0;i<4;++i){
        for(std::size_t j=0;j<16;++j){
            out[0x0CAA9518+std::uint32_t(i*64+j*4)]=road.surfaces0CAA9518[i].words[j];
            out[0x0CAA9618+std::uint32_t(i*64+j*4)]=road.sweeps0CAA9618[i].words[j];
        }
        for(std::size_t j=0;j<3;++j)out[0x0CAA94C8+std::uint32_t(i*12+j*4)]=bits(road.normals0CAA94C8[i][j]);
        out[0x0CAA94F8+std::uint32_t(i*4)]=road.flags0CAA94F8[i];
        out[0x0CAA9508+std::uint32_t(i*4)]=bits(road.impacts0CAA9508[i]);
    }
    out[0x0C900E5C]=bits(road.impact0C900E5C);out[0x0C900E60]=bits(road.impact0C900E60);
    out[0x0C92DE30]=road.tick0C92DE30;
    out[0x0C900954]=session.activeActorIdentity0C900954;
    out[0x0C900E84]=std::bit_cast<std::uint32_t>(session.elapsedFrames0C900E84);
    out[0x0C900EBC]=session.steeringMask0C900EBC;
    out[0x0CAA94C4]=session.previousFlag0CAA94C4;
    out[0x0C31FD44]=session.state0C31FD44;
    for(std::size_t i=0;i<20;++i)out[0x0C91FB50+std::uint32_t(i*4)]=session.statisticsRemainder0C91FB50[i];
    out[0x0C91FB08]=0x13579BDF;out[0x0C91FBA0]=0x2468ACE0;
    return out;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass the canonical original program image");
    RefMemory memory{std::filesystem::path(argv[1])};
    std::size_t comparisons=0,instructions=0;
    const auto defaults=verifiedOriginalSessionDefaults();
    for(std::size_t i=0;i<37;++i){
        ++comparisons;
        if(defaults.words0C270B08[i]!=memory.read32(0x0C270B08+std::uint32_t(i*4)))
            throw std::runtime_error("Canonical session defaults mismatch");
    }
    // Audit the original calibration producers too: two complete leaf
    // functions plus the three-byte configuration publication block.
    for(std::uint32_t value=0;value<256;++value){
        memory.clear();memory.zeroRegion(0x0C800000,0x800000);
        const std::array high{value,255u-value,(value*17u)&255u};
        constexpr std::array addresses{0x0C9015E8u,0x0C9015C8u,0x0C90164Cu};
        for(std::size_t i=0;i<3;++i)memory.write16(0x0C92F0E0+std::uint32_t(i*2),std::uint16_t((high[i]<<8)|((value*13+i*41)&255)));
        RefCpu cpu(memory);cpu.r[15]=0x0CFFF100;cpu.pr=0x0F000000;
        instructions+=cpu.run(0x0C159AA0,0x0F000000,100);
        for(std::size_t i=0;i<3;++i){++comparisons;if(memory.read32(addresses[i])!=high[i])throw std::runtime_error("Original calibration capture differs from upper byte");}
        instructions+=cpu.run(0x0C159AE0,0x0F000000,100);
        constexpr std::array defaultsCalibration{128u,32u,32u};
        for(std::size_t i=0;i<3;++i){++comparisons;if(memory.read32(addresses[i])!=defaultsCalibration[i])throw std::runtime_error("Original default calibration differs");}
        for(std::size_t i=0;i<3;++i)memory.write8(0x0CFD0009+std::uint32_t(i),std::uint8_t(high[i]));
        memory.write32(0x0CFD000C,0xABCDEF78);
        cpu.r[0]=0x0CFD0000;
        instructions+=cpu.run(0x0C056A72,0x0C056A96,100);
        for(std::size_t i=0;i<3;++i){
            const auto expected=i==0?high[i]:std::uint32_t(std::int32_t(std::int8_t(high[i])));
            ++comparisons;if(memory.read32(addresses[i])!=expected)throw std::runtime_error("Original configuration calibration extension differs");
        }
        ++comparisons;if(memory.read32(0x0C4004DC)!=8)throw std::runtime_error("Original calibration neighboring config mask differs");
    }
    for(std::uint32_t vehicle=0;vehicle<35;++vehicle)for(std::uint32_t mode=0;mode<32;++mode){
        memory.clear();memory.zeroRegion(0x0C800000,0x800000);
        OriginalVehicleState s;OriginalVehicleParameters p;OriginalInitializationSideState side;
        OriginalActorState actor;OriginalRoadContactState road;OriginalSessionInitializationState session;
        for(std::size_t i=0;i<actor.words.size();++i)actor.words[i]=0xFACA0000u+std::uint32_t(i*13+mode+vehicle);
        const auto actorBefore=actor.words;
        for(std::size_t i=0;i<4;++i){
            for(std::size_t j=0;j<16;++j){road.surfaces0CAA9518[i].words[j]=0xF00F0000u+std::uint32_t(i*16+j);road.sweeps0CAA9618[i].words[j]=0xBABA0000u+std::uint32_t(i*16+j);}
            road.normals0CAA94C8[i]={1.25f,2.5f,3.75f};road.flags0CAA94F8[i]=0xFFFFAAAA;
            road.impacts0CAA9508[i]=float(i)+.625f;
        }
        road.impact0C900E5C=4.125f;road.impact0C900E60=5.875f;road.tick0C92DE30=0x728493;
        road.impactRecords.push_back({{1,2,3},18,1.25f});road.feedback142460.push_back(142);
        road.invalidScalarDiagnostics=9;
        session.activeActorIdentity0C900954=0x0CFC0000;session.elapsedFrames0C900E84=13279;
        session.steeringMask0C900EBC=0xFFEF0102;session.previousFlag0CAA94C4=71;session.state0C31FD44=89;
        session.statisticsRemainder0C91FB50.fill(0xFEFECACA);
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
        auto before=serializeSession(s,p,side,actor,road,session);
        for(const auto [address,value]:before)memory.write32(address,value);
        memory.write32(0x0C901654,vehicle);
        memory.write32(0x0C9015E0,mode);memory.write32(0x0C9015FC,in.mode0C9015FC);memory.write32(0x0C9015C0,in.mode0C9015C0);
        for(std::size_t i=0;i<3;++i){memory.writeFloat(0x0CFD0000+std::uint32_t(i*4),in.position[i]);memory.writeFloat(0x0CFD0010+std::uint32_t(i*4),in.angles[i]);}
        RefCpu cpu(memory);cpu.r[4]=0x0CFD0000;cpu.r[5]=0x0CFD0010;cpu.r[15]=0x0CFFF100;cpu.pr=0x0F000000;
        cpu.callHooks[0x0C15A380]=[](RefCpu&){};
        instructions+=cpu.run(0x0C1595C0,0x0F000000,20000);
        const auto result=initializeOriginalSession(s,p,side,actor,road,session,in,defaults);
        if(!result.resetPlatformDigitalInput)throw std::runtime_error("Missing platform-reset effect");
        for(std::size_t i=42;i<actor.words.size();++i){++comparisons;if(actor.words[i]!=actorBefore[i])throw std::runtime_error("Actor spare storage modified");}
        if(road.impactRecords.size()!=1||road.impactRecords[0].tick!=18||road.feedback142460!=std::vector<std::uint32_t>{142}||road.invalidScalarDiagnostics!=9)
            throw std::runtime_error("Original initializer altered host output history");
        for(std::size_t i=0;i<3;++i){++comparisons;if(bits(side.actorPosition[i])!=actor.u(i*4))throw std::runtime_error("Actor pose alias mismatch");}
        ++comparisons;if(side.actorFlags50!=actor.u(0x50))throw std::runtime_error("Actor flag alias mismatch");
        const auto after=serializeSession(s,p,side,actor,road,session);
        for(const auto [address,actual]:after){
            ++comparisons;const auto expected=memory.read32(address);
            if(expected!=actual){std::cerr<<"Session initializer mismatch vehicle="<<vehicle<<" mode="<<mode<<" address="<<std::hex<<address<<" expected="<<expected<<" actual="<<actual<<std::dec<<'\n';return 1;}
        }
    }
    std::cout<<"PASS 1,120 full1595C0 session initializer cases (35 cars x 32 modes) plus768 calibration provenance cases, "<<comparisons<<" bit-exact field comparisons, "<<instructions<<" decoded instructions; only platform digital reset15A380 hooked. Original15EE00, RNG, all8query resets and37-word copy execute.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
