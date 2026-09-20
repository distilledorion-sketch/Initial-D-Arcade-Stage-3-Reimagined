#include "original_audio.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace idas3;
namespace {
using Bytes=std::vector<std::uint8_t>;
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void u16(Bytes& b,std::size_t at,unsigned value){b.at(at)=std::uint8_t(value);b.at(at+1)=std::uint8_t(value>>8);}
void u32(Bytes& b,std::size_t at,std::uint32_t value){u16(b,at,value);u16(b,at+2,value>>16);}
void extent(Bytes& b){u32(b,4,unsigned(b.size()-8));}
std::size_t chunk(Bytes& b,const char* tag,const Bytes& body){
    const auto at=b.size();b.resize(at+8+body.size()+(body.size()&1));
    std::copy_n(tag,4,b.begin()+at);u32(b,at+4,unsigned(body.size()));
    std::copy(body.begin(),body.end(),b.begin()+at+8);extent(b);return at+8;
}
struct Fixture {Bytes bytes;std::size_t fmt,fact,loop,data;unsigned channels,blockBytes,blockFrames,frames;};
Fixture fixture(unsigned channels=2,unsigned blockBytes=32,unsigned frames=35,unsigned predictor=0,unsigned rate=44099,bool looping=false){
    Fixture f{{'R','I','F','F',0,0,0,0,'W','A','V','E'},0,0,0,0,channels,blockBytes,(blockBytes-7*channels)*2/channels+2,frames};
    Bytes format(50);u16(format,0,2);u16(format,2,channels);u32(format,4,rate);
    u32(format,8,rate*blockBytes/f.blockFrames);u16(format,12,blockBytes);u16(format,14,4);
    u16(format,16,32);u16(format,18,f.blockFrames);u16(format,20,7);
    constexpr int coefs[7][2]={{256,0},{512,-256},{0,0},{192,64},{240,0},{460,-208},{392,-232}};
    for(unsigned i=0;i<7;++i)for(unsigned j=0;j<2;++j)u16(format,22+i*4+j*2,unsigned(coefs[i][j]));
    f.fmt=chunk(f.bytes,"fmt ",format);Bytes fact(4);u32(fact,0,frames);f.fact=chunk(f.bytes,"fact",fact);
    if(looping){Bytes loop(60);u32(loop,8,1000000000u/rate);u32(loop,12,60);u32(loop,28,1);u32(loop,44,3);u32(loop,48,frames-5);f.loop=chunk(f.bytes,"smpl",loop);}
    Bytes payload(((frames+f.blockFrames-1)/f.blockFrames)*blockBytes);
    for(unsigned block=0;block<payload.size()/blockBytes;++block){
        const auto at=block*blockBytes;
        for(unsigned c=0;c<channels;++c){
            payload[at+c]=std::uint8_t(predictor);u16(payload,at+channels+c*2,16+c*15);
            u16(payload,at+3*channels+c*2,unsigned(c?-321:123));
            u16(payload,at+5*channels+c*2,unsigned(c?456:-234));
        }
        for(unsigned i=7*channels;i<blockBytes;++i)payload[at+i]=std::uint8_t(i*37+block*11);
    }
    f.data=chunk(f.bytes,"data",payload);return f;
}
void reject(const Bytes& bytes,const char* message="Malformed MS ADPCM accepted"){
    bool failed=false;try{decodeOriginalSpsd(bytes);}catch(const std::runtime_error&){failed=true;}require(failed,message);
}
void save(const std::filesystem::path& path,const Bytes& bytes){
    std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!out)throw std::runtime_error("Could not write fixture");
}
void samples(const Fixture& f,std::initializer_list<std::int16_t> expected,const char* message){
    require(decodeOriginalMsAdpcmWave(f.bytes).samples==std::vector<std::int16_t>(expected),message);
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("usage: original_msadpcm_tests private-fixture-directory");
    const std::filesystem::path proof=argv[1];std::filesystem::create_directories(proof);
    for(unsigned channels:{1u,2u})for(unsigned predictor=0;predictor<7;++predictor){
        const unsigned perBlock=(32-7*channels)*2/channels+2;
        auto f=fixture(channels,32,perBlock*2-3,predictor,predictor&1?44100:44099,predictor==3);
        auto clip=decodeOriginalMsAdpcmWave(f.bytes),dispatch=decodeOriginalSpsd(f.bytes);
        require(clip.channels==channels&&clip.sampleRate==(predictor&1?44100u:44099u)&&clip.frames()==f.frames,"Source rate/channel/fact count not retained");
        require(clip.samples==dispatch.samples,"RIFF magic dispatch changes PCM");
        require(clip.samples[0]==-234&&clip.samples[channels]==123,"Block header sample order wrong");
        require(clip.samples[perBlock*channels]==-234,"Next block did not reset history");
        require(clip.looping==(predictor==3),"Loop flag incorrect");
        if(clip.looping)require(clip.loopStart==3&&clip.loopEnd==f.frames-4,"Nonaligned exclusive loop endpoint changed");
        save(proof/("predictor-"+std::to_string(predictor)+"-channels-"+std::to_string(channels)+".wav"),f.bytes);
    }
    for(unsigned frames:{1u,2u,3u,4u}){
        auto f=fixture(1,8,frames);require(decodeOriginalMsAdpcmWave(f.bytes).frames()==frames,"Fact partial final block not trimmed");
        save(proof/("partial-"+std::to_string(frames)+".wav"),f.bytes);
    }
    {
        auto f=fixture(1,8,4);u16(f.bytes,f.data+1,16);u16(f.bytes,f.data+3,0);u16(f.bytes,f.data+5,0);f.bytes[f.data+7]=0x18;
        samples(f,{0,0,16,-112},"Mono signed high nibble first or minimum delta incorrect");save(proof/"mono-nibble-order.wav",f.bytes);
    }
    {
        auto f=fixture(2,16,4);u16(f.bytes,f.data+2,16);u16(f.bytes,f.data+4,16);
        u16(f.bytes,f.data+6,100);u16(f.bytes,f.data+8,unsigned(-100));u16(f.bytes,f.data+10,unsigned(-10));u16(f.bytes,f.data+12,20);
        f.bytes[f.data+14]=0x1f;f.bytes[f.data+15]=0x87;
        samples(f,{-10,20,100,-100,116,-116,-12,-4},"Stereo channel/nibble order incorrect");save(proof/"stereo-nibble-order.wav",f.bytes);
    }
    for(unsigned predictor:{3u,4u}){
        auto f=fixture(1,8,4,predictor);u16(f.bytes,f.data+3,unsigned(-1));u16(f.bytes,f.data+5,0);f.bytes[f.data+7]=0;
        samples(f,{0,-1,-1,-1},"Negative predictor must use arithmetic shift, not division");save(proof/("negative-predictor-"+std::to_string(predictor)+".wav"),f.bytes);
    }
    {
        auto f=fixture(1,8,4);u16(f.bytes,f.data+1,32767);u16(f.bytes,f.data+3,32760);u16(f.bytes,f.data+5,0);f.bytes[f.data+7]=0x78;
        samples(f,{0,32760,32767,-32768},"PCM16 positive/negative saturation incorrect");save(proof/"saturation.wav",f.bytes);
    }
    {
        auto f=fixture(1,8,4);u16(f.bytes,f.data+1,0);u16(f.bytes,f.data+3,0);u16(f.bytes,f.data+5,0);f.bytes[f.data+7]=0x11;
        samples(f,{0,0,0,16},"Zero initial delta or subsequent minimum delta incorrect");save(proof/"zero-initial-delta.wav",f.bytes);
    }
    auto valid=fixture(2,32,35,5,44099,true);
    for(std::size_t length=0;length<valid.bytes.size();++length){
        auto bad=valid.bytes;bad.resize(length);reject(bad,"Truncated RIFF extent accepted");
        if(length>=12){extent(bad);reject(bad,"Truncated chunk accepted after adjusted RIFF extent");}
    }
    for(unsigned field:{0u,2u,12u,14u,16u,18u,20u}){auto bad=valid.bytes;u16(bad,valid.fmt+field,0);reject(bad);}
    for(unsigned channels:{3u,65535u}){auto bad=valid.bytes;u16(bad,valid.fmt+2,channels);reject(bad);}
    for(unsigned rate:{0u,7999u,48001u,0xffffffffu}){auto bad=valid.bytes;u32(bad,valid.fmt+4,rate);reject(bad);}
    for(unsigned frames:{0u,20u,41u,0xffffffffu}){auto bad=valid.bytes;u32(bad,valid.fact,frames);reject(bad);}
    for(unsigned predictor:{7u,255u}){auto bad=valid.bytes;bad[valid.data+1]=std::uint8_t(predictor);reject(bad);}
    {auto bad=valid.bytes;u16(bad,valid.data+2,0xffff);reject(bad);}
    {auto bad=valid.bytes;u16(bad,valid.fmt+22,255);reject(bad);}
    {auto bad=valid.bytes;u32(bad,valid.fmt+8,1);reject(bad);}
    for(auto [start,end]:std::array<std::pair<unsigned,unsigned>,4>{{{7,6},{35,34},{0,35},{0,0xffffffffu}}}){
        auto bad=valid.bytes;u32(bad,valid.loop+44,start);u32(bad,valid.loop+48,end);reject(bad);
    }
    for(unsigned at:{28u,32u,40u,52u,56u}){auto bad=valid.bytes;u32(bad,valid.loop+at,2);reject(bad);}
    for(auto [tag,at,length]:std::array<std::tuple<const char*,std::size_t,unsigned>,4>{{{"fmt ",valid.fmt,50},{"fact",valid.fact,4},{"smpl",valid.loop,60},{"data",valid.data,64}}}){
        auto bad=valid.bytes;chunk(bad,tag,Bytes(valid.bytes.begin()+at,valid.bytes.begin()+at+length));reject(bad,"Duplicate required chunk accepted");
    }
    {auto f=valid;chunk(f.bytes,"JUNK",Bytes{1,2,3});require(decodeOriginalMsAdpcmWave(f.bytes).samples==decodeOriginalMsAdpcmWave(valid.bytes).samples,"Padded unknown chunk affected audio");}
    {auto bad=valid.bytes;chunk(bad,"JUNK",Bytes{1,2,3});bad.pop_back();extent(bad);reject(bad,"Missing odd-chunk padding accepted");}
    {auto bad=valid.bytes;bad.push_back(0);extent(bad);reject(bad,"Incomplete final chunk header accepted");}
    {auto bad=valid.bytes;u32(bad,valid.data-4,0xffffffffu);reject(bad,"Oversized data chunk accepted");}
    std::cout<<"PASS "<<checks<<" MS ADPCM metadata, mono/stereo nibble order, negative predictor, delta, saturation, loops, truncation and malformed-header checks.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
