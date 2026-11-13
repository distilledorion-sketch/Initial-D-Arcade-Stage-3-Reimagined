#include "original_feedback.h"
#include "original_driving_float.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace idas3ffb;
namespace {
std::size_t checks=0,instructions=0,referenceCases=0;
void require(bool condition,const std::string& message){++checks;if(!condition)throw std::runtime_error(message);}
bool equalState(const OriginalProducerState& a,const OriginalProducerState& b){
    return a.ramp==b.ramp&&a.torqueCode==b.torqueCode&&a.damperParameter==b.damperParameter&&a.rumbleLatch==b.rumbleLatch;
}
std::array<OriginalProducer,257> startupFixtures(){
    std::array<OriginalProducer,257> values{};
    for(unsigned i=1;i<values.size();++i){values[i]=values[i-1];require(values[i].update({}).valid,"neutral startup input rejected");}
    return values;
}
void behavior(const std::array<OriginalProducer,257>& startup){
    OriginalProducer p;OriginalInput input;input.steering1CC=.8f;input.acceleration24C=-1;
    auto first=p.update(input);require(first.valid&&first.torqueCode==128&&!first.sendTorque,"first startup window is not zero");
    auto second=p.update(input);require(second.valid&&second.torqueCode==127&&second.sendTorque,"second startup window is not one");
    for(float acceleration:{-1.f,0.f,1.f}){
        auto producer=startup[256];input.acceleration24C=acceleration;const auto out=producer.update(input);
        const int expected=acceleration<0?90:acceleration>0?9:50;
        require(out.valid&&out.limit==expected&&128-out.torqueCode==expected,"speed-change force cap");
    }
    input={};input.drive258=.0125f;
    p=OriginalProducer{};const auto event=p.update(input);const auto repeated=p.update(input);
    require(event.valid&&event.sendRumble&&event.rumbleAmplitude==1&&event.rumbleFrequencyHalfHz==63,"original contact rumble threshold");
    require(repeated.sendRumble&&repeated.rumbleAmplitude==1,"unchanged positive rumble was not refreshed");
    p.reset();require(p.state().ramp==0&&p.state().torqueCode==128&&p.state().damperParameter==0&&p.state().rumbleLatch==1,"original reset/latch semantics");
    const auto stop=p.update({});const auto quiet=p.update({});
    require(stop.sendRumble&&stop.rumbleAmplitude==0&&stop.rumbleFrequencyHalfHz==0&&!quiet.sendRumble,"zero rumble stop transition");
    input={};input.headingFactor27C=1;input.snow438=1;input.oil140=1;input.automaticBrake1A8=1;
    require(p.update(input).damperParameter==90,"automatic braking must override previous damping reductions");
    input={};input.normalizedSpeed248=.5f;input.surface164=4;
    require(p.update(input).rumbleAmplitude==18,"surface4 speed rumble");
    input.snow438=1;require(p.update(input).rumbleAmplitude==0,"snow did not suppress surface4 term");
    input={};input.gear=2;input.gearSnapshot=1;input.transmissionNormalized=.5f;
    input.normalizedSpeed248=std::bit_cast<float>(0x3c23d70au);
    require(p.update(input).rumbleAmplitude==0,"gear event threshold must be strict");
    input.normalizedSpeed248=std::nextafter(input.normalizedSpeed248,1.f);
    require(p.update(input).rumbleAmplitude==30,"gear event above threshold");
    // Validation must not mutate any producer state or silently sanitize bad
    // drive fields into a force. The output owner handles valid=false by stop.
    std::vector<OriginalInput> bad;
    input={};input.carIndex=35;bad.push_back(input);
    input={};input.globalSetting=11;bad.push_back(input);
    input={};input.cardSteeringLevel=11;bad.push_back(input);
    input={};input.condition=18;bad.push_back(input);
    input={};input.steering1CC=std::numeric_limits<float>::quiet_NaN();bad.push_back(input);
    input={};input.filteredRpm=std::numeric_limits<float>::infinity();bad.push_back(input);
    input={};input.steering1CC=std::numeric_limits<float>::max();bad.push_back(input);
    input={};input.normalizedSpeed248=-2.f;bad.push_back(input);
    input={};input.drive258=std::numeric_limits<float>::max();bad.push_back(input);
    input={};input.acceleration24C=std::numeric_limits<float>::max();bad.push_back(input);
    // Conversion is representable, but adding the original center/road bias
    // would overflow the command integer. It must not become a full force.
    input={};input.steering1CC=-10737418.f;
    input.headingFactor27C=1.f-std::nextafter(1.f,0.f);
    input.frontContact3B4=1;input.normalizedFrontContact3C4=1;bad.push_back(input);
    for(const auto& invalid:bad){const auto before=p.state();const auto out=p.update(invalid);
        require(!out.valid&&!out.sendTorque&&!out.sendDamper&&!out.sendRumble&&equalState(before,p.state()),"invalid input advanced/emitted feedback");}
}

void originalOracle(const std::filesystem::path& image,const std::array<OriginalProducer,257>& startup){
    using namespace idas3::reference;
    RefMemory memory(image);
    const idas3::original::OriginalDrivingFloatScope fp;
    auto compare=[&](OriginalProducer producer,const OriginalInput& in,unsigned repeat=1){
        for(unsigned tick=0;tick<repeat;++tick){
            ++referenceCases;const auto previous=producer.state();
            memory.clear();memory.zeroRegion(0x0C800000,0x00800000);
            auto wf=[&](unsigned offset,float value){memory.writeFloat(0x0C900F00+offset,value);};
            auto wu=[&](unsigned offset,unsigned value){memory.write32(0x0C900F00+offset,value);};
            wf(0x1CC,in.steering1CC);wf(0x27C,in.headingFactor27C);wf(0x24C,in.acceleration24C);
            wf(0x3B4,in.frontContact3B4);wf(0x3B8,in.frontContact3B8);wf(0x3C4,in.normalizedFrontContact3C4);wf(0x3C8,in.normalizedFrontContact3C8);
            wu(0x150,in.wallContact150);wu(0x154,in.wallCorner154);wf(0x158,in.wallMagnitude158);
            wu(0x434,in.wet434);wu(0x438,in.snow438);wu(0x140,in.oil140);wu(0x1A8,in.automaticBrake1A8);
            wf(0x258,in.drive258);wf(0x25C,in.drive25C);wf(0x260,in.drive260);wf(0x264,in.drive264);
            wf(0x248,in.normalizedSpeed248);wu(0x164,in.surface164);wu(0x168,in.surface168);wu(0x14C,in.roadFlag14C);
            memory.write32(0x0C901654,in.carIndex);memory.write32(0x0C9015D8,in.globalSetting);memory.write32(0x0C9015EC,in.cardSteeringLevel);memory.write32(0x0C9015CC,in.condition);
            memory.writeFloat(0x0C900EA0,in.filteredRpm);memory.writeFloat(0x0C900EA8,in.transmissionNormalized);
            memory.write32(0x0C900E88,in.gear);memory.write32(0x0C900E8C,in.gearSnapshot);
            memory.write32(0x0CAA974C,previous.ramp);memory.write32(0x0CAA9754,unsigned(previous.torqueCode));memory.write32(0x0CAA9758,unsigned(previous.damperParameter));memory.write32(0x0CAA975C,previous.rumbleLatch);
            std::vector<std::array<int,3>> actualCommands;
            RefCpu cpu(memory);cpu.r[15]=0x0CFFF000;cpu.pr=0x00FF0000;
            cpu.callHooks[0x0C159D40]=[&](RefCpu& c){actualCommands.push_back({4,int(c.r[4]),0});};
            cpu.callHooks[0x0C159DC0]=[&](RefCpu& c){actualCommands.push_back({6,int(c.r[4]),int(c.r[5])});};
            cpu.callHooks[0x0C159D80]=[&](RefCpu& c){actualCommands.push_back({5,int(c.r[4]),int(c.r[5])});};
            instructions+=cpu.run(0x0C15A5E0,0x0C15A840,10000);
            const int originalLimit=int(cpu.r[3]);
            instructions+=cpu.run(cpu.pc,0x00FF0000,10000);
            const auto out=producer.update(in);std::vector<std::array<int,3>> expectedCommands;
            require(out.valid,"valid original fixture rejected case"+std::to_string(referenceCases));
            if(out.sendTorque)expectedCommands.push_back({4,out.torqueCode,0});
            if(out.sendDamper)expectedCommands.push_back({6,2,out.damperParameter});
            if(out.sendRumble)expectedCommands.push_back({5,int(out.rumbleFrequencyHalfHz),out.rumbleAmplitude});
            require(actualCommands==expectedCommands,"original command order/payload mismatch case"+std::to_string(referenceCases));
            const auto& after=producer.state();
            require(after.ramp==memory.read32(0x0CAA974C),"original ramp mismatch");
            require(after.torqueCode==signed32(memory.read32(0x0CAA9754)),"original torque cache mismatch");
            require(after.damperParameter==signed32(memory.read32(0x0CAA9758)),"original damper cache mismatch");
            require(after.rumbleLatch==memory.read32(0x0CAA975C),"original rumble latch mismatch");
            require(out.limit==originalLimit,"original acceleration limit mismatch");
        }
    };
    // Every actual per-car denominator, including car30=12000, at a rumble
    // integer boundary. Source engine tuning/override is intentionally absent.
    for(unsigned car=0;car<35;++car){
        const float denominator=memory.readFloat(0x0C270EB0+car*24);
        for(float rpm:{std::nextafter(denominator,0.f),denominator,std::nextafter(denominator,INFINITY)}){
            OriginalInput in;in.carIndex=car;in.filteredRpm=rpm;compare(startup[256],in,2);
        }
    }
    for(unsigned car=0;car<35;++car)for(unsigned setting=0;setting<11;++setting)for(unsigned card=0;card<11;++card)for(unsigned flags=0;flags<8;++flags){
        const unsigned seed=((car*11+setting)*11+card)*8+flags;OriginalInput in;
        in.carIndex=car;in.globalSetting=setting;in.cardSteeringLevel=card;in.condition=seed%18;
        in.wet434=flags&1;in.snow438=(flags>>1)&1;in.oil140=(flags>>2)&1;in.automaticBrake1A8=seed%23==0;
        in.steering1CC=(int(seed%201)-100)/100.f;in.headingFactor27C=(seed%11)/10.f;in.acceleration24C=(int(seed%21)-10)/10.f;
        in.frontContact3B4=(int(seed%31)-15)*.007f;in.frontContact3B8=(int(seed%37)-18)*.006f;
        in.normalizedFrontContact3C4=in.frontContact3B4/.07f;in.normalizedFrontContact3C8=in.frontContact3B8/.07f;
        in.wallContact150=seed%3==0;in.wallCorner154=seed%5;in.wallMagnitude158=(seed%23)*.003f;
        in.drive258=(int(seed%13)-6)*.001f;in.drive25C=(int(seed%17)-8)*.002f;in.drive260=(int(seed%7)-3)*.03f;in.drive264=(int(seed%11)-5)*.02f;
        in.normalizedSpeed248=(seed%101)/100.f;in.filteredRpm=float(seed%12001);
        in.surface164=seed&255;in.surface168=(seed>>1)&255;in.roadFlag14C=(seed>>2)&1;
        in.gear=seed%7;in.gearSnapshot=(seed>>1)%7;in.transmissionNormalized=(seed%131)/100.f;
        compare(startup[seed%257],in,2);
    }
    for(unsigned flags=0;flags<8;++flags)for(float acceleration:{-1.f,-.5f,0.f,.5f,1.f})for(float factor:{0.f,.1f,.5f,.9f,1.f}){
        OriginalInput in;in.steering1CC=.8f;in.acceleration24C=acceleration;in.headingFactor27C=factor;
        in.wet434=flags&1;in.snow438=(flags>>1)&1;in.oil140=(flags>>2)&1;compare(startup[256],in);
    }
    const float threshold=std::bit_cast<float>(0x3c23d70au);
    for(float speed:{std::nextafter(threshold,0.f),threshold,std::nextafter(threshold,1.f)})for(unsigned surface:{0u,1u,4u,0x20u,0x24u})for(unsigned condition=0;condition<18;++condition){
        OriginalInput in;in.normalizedSpeed248=speed;in.surface164=surface;in.condition=condition;in.roadFlag14C=1;
        in.gear=2;in.gearSnapshot=1;in.transmissionNormalized=.5f;compare(startup[256],in,2);
    }
    // Original reset leaves its rumble latch set. Confirm the next zero frame
    // emits a stop and the following zero frame is silent against the ROM.
    OriginalProducer latched;OriginalInput event;event.drive258=.05f;latched.update(event);latched.reset();compare(latched,{},2);
}
}
int main(int argc,char**argv)try{
    const auto startup=startupFixtures();behavior(startup);
    if(argc==2)originalOracle(std::filesystem::path(argv[1]),startup);
    else if(argc!=1)throw std::runtime_error("usage: original_feedback_tests [original-image]");
    std::cout<<"PASS "<<checks<<" feedback checks; "<<referenceCases<<" original producer executions; "<<instructions<<" original instructions. No hardware output.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
