#include "original_audio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace idas3;
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(int argc,char**argv){try{
    std::vector<std::uint8_t> header(68);header[0]='S';header[1]='P';header[2]='S';header[3]='D';header[4]=1;header[5]=1;header[7]=4;header[8]=3;header[9]=1;header[10]=255;header[12]=4;header[42]=0x22;header[43]=0x56;
    header[64]=0x70;header[65]=0xF8;header[66]=0x01;header[67]=0x23;
    const auto clip=decodeOriginalSpsd(header);
    require(clip.channels==2&&clip.sampleRate==22050&&clip.frames()==4,"SPSD metadata mismatch");
    const std::array<std::int16_t,8> expected{15,47,252,61,212,171,-301,248};
    require(std::equal(clip.samples.begin(),clip.samples.end(),expected.begin()),"AICA nibble order, channel split or history arithmetic mismatch");
    bool rejected=false;try{decodeOriginalSpsd(std::span(header).first(66));}catch(const std::runtime_error&){rejected=true;}require(rejected,"Truncated SPSD accepted");
    if(argc==2){unsigned count=0;double seconds=0;
        for(const auto& item:std::filesystem::directory_iterator(std::filesystem::path(argv[1])/"data/original_audio/streams"))if(item.path().extension()==".bin"){
            const auto source=loadOriginalSpsd(item.path());require(source.frames()>0,"Empty original audio stream");
            const auto [lo,hi]=std::minmax_element(source.samples.begin(),source.samples.end());require(*hi>*lo,"Silent original audio stream");
            seconds+=double(source.frames())/source.sampleRate;++count;
        }
        require(count==18,"Original soundtrack/effect roster incomplete");std::cout<<count<<" original streams decoded; "<<seconds<<" seconds\n";
    }
    std::cout<<"Original audio decode checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
