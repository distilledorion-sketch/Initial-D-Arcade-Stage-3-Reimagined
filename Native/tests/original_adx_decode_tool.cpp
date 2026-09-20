// Private decoder comparison helper. Produces PCM only in an explicit output
// path; the game continues reading unchanged ADX source files.
#include "original_audio.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("usage: original_adx_decode_tool source-stream output.wav");
    const auto clip=idas3::loadOriginalSpsd(argv[1]);std::ofstream out(argv[2],std::ios::binary);
    if(!out)throw std::runtime_error("Cannot open diagnostic PCM output");
    const auto u16=[&](unsigned v){out.put(char(v));out.put(char(v>>8));};
    const auto u32=[&](unsigned v){u16(v);u16(v>>16);};
    const unsigned bytes=unsigned(clip.samples.size()*2);
    out.write("RIFF",4);u32(36+bytes);out.write("WAVEfmt ",8);u32(16);u16(1);u16(clip.channels);
    u32(clip.sampleRate);u32(clip.sampleRate*clip.channels*2);u16(clip.channels*2);u16(16);out.write("data",4);u32(bytes);
    out.write(reinterpret_cast<const char*>(clip.samples.data()),bytes);
    if(!out)throw std::runtime_error("Incomplete diagnostic PCM output");
    std::cout<<"{\"sampleRate\":"<<clip.sampleRate<<",\"channels\":"<<clip.channels<<",\"frames\":"<<clip.frames()
        <<",\"looping\":"<<(clip.looping?"true":"false")<<",\"loopStart\":"<<clip.loopStart<<",\"loopEnd\":"<<clip.loopEnd<<"}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
