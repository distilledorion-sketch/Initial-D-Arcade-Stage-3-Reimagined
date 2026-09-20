#include "original_contact_completion.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass canonical original program image");
    RefMemory memory{std::filesystem::path(argv[1])};std::size_t checks=0,instructions=0,audioCalls=0,cueCalls=0;
    std::uint32_t rng=0x157D6C;auto random=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
    constexpr std::uint32_t driveBase=0x0C900F00,positionBase=0x0CE00000,frameBase=0x0CE01000;
    for(unsigned sample=0;sample<1600;++sample){
        memory.clear();memory.zeroRegion(0x0CFFF000,0x1000);
        OriginalDriveState d;OriginalContactCompletionState state;OriginalContactCompletionInputs inputs;
        idas3::OriginalTransmissionState transmission;
        for(auto& word:d.words)word=random();
        d.setu(0x148,sample%4);d.setu(0x14C,sample%17-3u);d.setu(0x3F8,sample%105);
        d.setf(0x1CC,float(int(sample%3)-1));d.setf(0x1B8,float(sample%101)/100.f);
        state.previousFlag0CAA94C4=sample%2;state.elapsedFrames0C900E84=random();
        state.steeringMask0C900EBC=random();state.randomSeed0C37C778=random();
        for(auto& word:state.cues0C900E5C)word=random();
        for(auto& word:state.snapshot0CAA9718)word=random();
        for(auto& record:state.impactPositions)for(auto& word:record)word=random();
        for(auto& word:state.impactFrames)word=random();
        state.positionCursor=sample%64;state.frameCursor=sample%73;
        inputs.frame0C92DE30=random();inputs.digitalByte0C92ED00=std::uint8_t(sample);
        transmission.gear00=sample%7;transmission.tach1c=float(int(sample%20001)-10000);
        for(std::size_t i=0;i<d.words.size();++i)memory.write32(driveBase+std::uint32_t(i*4),d.words[i]);
        memory.write32(0x0C900E88,transmission.gear00);memory.writeFloat(0x0C900EA4,transmission.tach1c);
        memory.write32(0x0CAA94C4,state.previousFlag0CAA94C4);
        for(std::size_t i=0;i<5;++i)memory.write32(0x0C900E5C+std::uint32_t(i*4),state.cues0C900E5C[i]);
        memory.write32(0x0C900E84,state.elapsedFrames0C900E84);memory.write32(0x0C900EBC,state.steeringMask0C900EBC);
        memory.write32(0x0C37C778,state.randomSeed0C37C778);memory.write32(0x0C92DE30,inputs.frame0C92DE30);
        memory.write8(0x0C92ED00,inputs.digitalByte0C92ED00);
        for(std::size_t i=0;i<8;++i)memory.write32(0x0CAA9718+std::uint32_t(i*4),state.snapshot0CAA9718[i]);
        for(std::size_t i=0;i<state.impactPositions.size();++i)for(std::size_t axis=0;axis<3;++axis)
            memory.write32(positionBase+std::uint32_t(i*12+axis*4),state.impactPositions[i][axis]);
        for(std::size_t i=0;i<state.impactFrames.size();++i)memory.write32(frameBase+std::uint32_t(i*4),state.impactFrames[i]);
        memory.write32(0x0C91FB30,positionBase+state.positionCursor*12);memory.write32(0x0C91FB34,frameBase+state.frameCursor*4);
        RefCpu cpu(memory);cpu.r[9]=driveBase;cpu.r[14]=cpu.r[15]=0x0CFFF800;
        OriginalContactCompletionEffects expected;unsigned localAudio=0,localCues=0;
        cpu.callHooks[0x0C142860]=[&](RefCpu& c){++localAudio;expected.engineChannel=c.r[4];expected.gear=c.r[5];expected.engineValue=c.getFloat(4);expected.engineScale=c.getFloat(5);expected.throttle=c.getFloat(6);};
        cpu.callHooks[0x0C142460]=[&](RefCpu& c){if(c.r[4]!=4)throw std::runtime_error("Unexpected original cue");++localCues;expected.requestCue4=true;};
        instructions+=cpu.run(0x0C157D6C,0x0C157E6E,5000);
        const auto actual=finishOriginalContactFrame(d,transmission,state,inputs);
        const auto equal=[&](std::uint32_t want,std::uint32_t got,const char* label){++checks;if(want!=got)throw std::runtime_error(std::string(label)+" sample="+std::to_string(sample)+" expected="+hex(want)+" actual="+hex(got));};
        for(std::size_t i=0;i<d.words.size();++i)equal(memory.read32(driveBase+std::uint32_t(i*4)),d.words[i],"Drive");
        equal(memory.read32(0x0CAA94C4),state.previousFlag0CAA94C4,"Previous flag");
        for(std::size_t i=0;i<5;++i)equal(memory.read32(0x0C900E5C+std::uint32_t(i*4)),state.cues0C900E5C[i],"Cues");
        equal(memory.read32(0x0C900E84),state.elapsedFrames0C900E84,"Frame counter");
        equal(memory.read32(0x0C900EBC),state.steeringMask0C900EBC,"Steering mask");
        equal(memory.read32(0x0C37C778),state.randomSeed0C37C778,"RNG");
        for(std::size_t i=0;i<8;++i)equal(memory.read32(0x0CAA9718+std::uint32_t(i*4)),state.snapshot0CAA9718[i],"Snapshot");
        for(std::size_t i=0;i<state.impactPositions.size();++i)for(std::size_t axis=0;axis<3;++axis)
            equal(memory.read32(positionBase+std::uint32_t(i*12+axis*4)),state.impactPositions[i][axis],"Impact position");
        for(std::size_t i=0;i<state.impactFrames.size();++i)equal(memory.read32(frameBase+std::uint32_t(i*4)),state.impactFrames[i],"Impact frame");
        equal(memory.read32(0x0C91FB30),positionBase+state.positionCursor*12,"Position cursor");
        equal(memory.read32(0x0C91FB34),frameBase+state.frameCursor*4,"Frame cursor");
        equal(1,localAudio,"Audio request count");equal(expected.engineChannel,actual.engineChannel,"Audio channel");equal(expected.gear,actual.gear,"Audio gear");
        equal(std::bit_cast<std::uint32_t>(expected.engineValue),std::bit_cast<std::uint32_t>(actual.engineValue),"Audio value");
        equal(std::bit_cast<std::uint32_t>(expected.engineScale),std::bit_cast<std::uint32_t>(actual.engineScale),"Audio scale");
        equal(std::bit_cast<std::uint32_t>(expected.throttle),std::bit_cast<std::uint32_t>(actual.throttle),"Audio throttle");
        equal(expected.requestCue4,actual.requestCue4,"Cue request");audioCalls+=localAudio;cueCalls+=localCues;
    }
    std::cout<<"PASS1600 original post-contact cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions; hooks only platform audio="<<audioCalls<<" and cue="<<cueCalls<<". RNG and snapshot execute original code.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
