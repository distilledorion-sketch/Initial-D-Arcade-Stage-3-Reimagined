#include "original_sfx_sequence.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iterator>
#include <sstream>
#include <iostream>
#include <algorithm>
using namespace idas3;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("project-root ARM-reference.csv canonical-image required");
    const auto root=std::filesystem::path(argv[1]);std::ifstream f(root/"data/original_audio/race/PACK23.dtpk",std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
    std::ifstream reference(argv[2]);require(bool(reference),"Missing original ARM sequence reference");
    std::string line;std::getline(reference,line);std::array<std::vector<std::pair<unsigned,unsigned>>,6> expected;
    while(std::getline(reference,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned track,tick,command;require(bool(row>>track>>tick>>command),"Malformed ARM sequence reference");expected.at(track).push_back({tick,command});}
    unsigned events=0;std::uint64_t sampleFrames=0;
    for(unsigned track=0;track<6;++track){
        const auto sequence=decodeOriginalSfxSequence(bytes,0x3a9u|(track<<16));OriginalSfxSequencer clock;
        std::vector<std::pair<unsigned,unsigned>> actual;const auto output=[&](const auto& event){actual.push_back({unsigned(clock.tick()),0x9f000000u|(unsigned(event.playback)<<16)|(unsigned(event.volume)<<8)});};
        clock.start(sequence,output);unsigned guard=0;
        while(clock.active()){clock.advanceSample(output);require(++guard<44100*12,"Sequence did not finish in bounded interval");}
        require(actual==expected[track],"Native event order/time differs from original716C/7234");events+=unsigned(actual.size());sampleFrames+=guard;
        const auto count=actual.size();for(unsigned i=0;i<100;++i)clock.advanceSample(output);require(actual.size()==count,"Finished sequence emitted stale command");
        clock.start(sequence,output);clock.stop();const auto stopped=actual.size();for(unsigned i=0;i<44100;++i)clock.advanceSample(output);require(actual.size()==stopped,"Stopped sequence emitted command");
    }
    require(events==23,"Reference did not cover all23 authored playback events");
    reference::RefMemory original(argv[3]);OriginalTirePlayback player(root);double energy=0;
    for(unsigned cue=0;cue<6;++cue){
        require(player.commands()[cue]==original.read32(0xc31ec7c+cue*12),"Skid cue mapping differs from original");
        player.reset();player.apply({original::OriginalTireCommandType::Play,int(cue)});player.apply({original::OriginalTireCommandType::Volume,127});
        for(unsigned i=0;i<44100*8;++i){const auto value=player.renderFrame();energy+=double(value)*value;}
        const auto sequence=decodeOriginalSfxSequence(bytes,player.commands()[cue]);require(player.startedSamples()==sequence.events.size(),"Tire playback omitted a sequence sample");
        player.apply({original::OriginalTireCommandType::Stop,0});for(unsigned i=0;i<1000;++i)require(player.renderFrame()==0,"Stopped skid leaked PCM");
        player.reset();player.apply({original::OriginalTireCommandType::Play,int(cue)});player.apply({original::OriginalTireCommandType::Volume,0});for(unsigned i=0;i<1000;++i)require(player.renderFrame()==0,"Muted skid leaked PCM");
    }
    require(energy>1e9,"Skid bank playback silent");
    std::cout<<"PASS all23 original ARM-timed sequence events over"<<sampleFrames<<" sample clocks, six source cue mappings, complete dry/wet/snow playback, stop/reset and mute. TimerB nominal44-sample interval; original IRQ/main-loop latency and cabinet one-shot envelopes/DSP are outside this check.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
