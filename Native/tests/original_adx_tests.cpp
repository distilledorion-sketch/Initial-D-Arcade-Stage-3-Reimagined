#include "original_audio.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace idas3;
namespace {
unsigned checks=0;
void require(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
void be16(std::vector<std::uint8_t>& data,unsigned at,unsigned value){data.at(at)=std::uint8_t(value>>8);data.at(at+1)=std::uint8_t(value);}
void be32(std::vector<std::uint8_t>& data,unsigned at,unsigned value){be16(data,at,value>>16);be16(data,at+2,value);}
std::vector<std::uint8_t> fixture(unsigned channels=2,unsigned frames=49){
    const unsigned start=64;std::vector<std::uint8_t> out(start+((frames+31)/32)*18*channels);
    be16(out,0,0x8000);be16(out,2,start-4);out[4]=3;out[5]=18;out[6]=4;out[7]=std::uint8_t(channels);
    be32(out,8,44100);be32(out,12,frames);be16(out,16,500);out[18]=4;
    be16(out,24,1000);be16(out,26,900);if(channels==2){be16(out,28,65536-700);be16(out,30,65536-650);}
    const std::string signature="(c)CRI";std::copy(signature.begin(),signature.end(),out.begin()+start-6);
    for(unsigned block=0;block<(frames+31)/32;++block)for(unsigned c=0;c<channels;++c){
        const auto at=start+(block*channels+c)*18;be16(out,at,block?4:1);
        for(unsigned j=0;j<16;++j)out[at+2+j]=std::uint8_t((j+3*c+block*5)*19);
    }
    return out;
}
void reject(const std::vector<std::uint8_t>& bytes){bool rejected=false;try{decodeOriginalSpsd(bytes);}catch(const std::runtime_error&){rejected=true;}require(rejected,"Malformed ADX accepted");}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("usage: original_adx_tests private-proof-directory");
    const std::filesystem::path proof=argv[1];std::filesystem::create_directories(proof);
    for(unsigned channels:{1u,2u})for(unsigned frames:{1u,31u,32u,33u,49u,64u}){
        auto data=fixture(channels,frames);const auto clip=decodeOriginalAdx(data),dispatched=decodeOriginalSpsd(data);
        require(clip.channels==channels&&clip.sampleRate==44100&&clip.frames()==frames,"ADX exact source sample count/rate/channels");
        require(clip.loopEnd==frames&&!clip.looping&&clip.loopStart==0,"ADX no-loop metadata");
        require(clip.samples==dispatched.samples,"ADX magic dispatch changed PCM");
        require(clip.samples[0]!=0,"ADX v4 initial history discarded");
        const auto path=proof/("histories-"+std::to_string(channels)+"-"+std::to_string(frames)+".adx");
        std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());
    }
    auto valid=fixture();be32(valid,36,1);be32(valid,40,7);be32(valid,48,42);
    auto looped=decodeOriginalAdx(valid);require(looped.looping&&looped.loopStart==7&&looped.loopEnd==42&&looped.frames()==49,"ADX preserves exact nonblock-aligned loop and full PCM");
    {std::ofstream f(proof/"nonaligned-loop.adx",std::ios::binary);f.write(reinterpret_cast<const char*>(valid.data()),valid.size());}
    for(std::size_t size=0;size<valid.size();++size){auto cut=valid;cut.resize(size);reject(cut);}
    for(auto [at,value]:std::array<std::pair<unsigned,unsigned>,7>{{{4,2},{5,17},{6,8},{7,0},{7,3},{18,3},{19,8}}}){auto bad=valid;bad[at]=std::uint8_t(value);reject(bad);}
    for(unsigned rate:{0u,7999u,48001u,0xffffffffu}){auto bad=valid;be32(bad,8,rate);reject(bad);}
    for(unsigned count:{0u,0xffffffffu}){auto bad=valid;be32(bad,12,count);reject(bad);}
    for(unsigned cutoff:{0u,22050u,65535u}){auto bad=valid;be16(bad,16,cutoff);reject(bad);}
    for(unsigned offset:{0u,28u,65535u}){auto bad=valid;be16(bad,2,offset);reject(bad);}
    for(auto [start,end]:std::array<std::pair<unsigned,unsigned>,4>{{{7,7},{8,7},{0,50},{0xffffffffu,42}}}){auto bad=valid;be32(bad,40,start);be32(bad,48,end);reject(bad);}
    {auto bad=valid;bad[58]='?';reject(bad);}
    {auto bad=valid;be16(bad,64,0x8001);reject(bad);}
    {auto bad=valid;be32(bad,36,0x41494e46);be32(bad,40,0xffffffff);reject(bad);}
    std::cout<<"PASS "<<checks<<" ADX metadata/dispatch/history/loop/truncation/unsupported-codec/bounds checks; source PCM comparisons are separate.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
