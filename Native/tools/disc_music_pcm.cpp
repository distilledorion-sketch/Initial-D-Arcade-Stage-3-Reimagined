#include "original_audio.h"
#include <fstream>
#include <iostream>

int main(int argc,char** argv){
    if(argc!=2 && argc!=3)return 2;
    try{
        const auto clip=idas3::loadOriginalSpsd(argv[1]);
        if(argc==3){
            std::ofstream pcm(argv[2],std::ios::binary);
            if(!pcm.write(reinterpret_cast<const char*>(clip.samples.data()),clip.samples.size()*sizeof(std::int16_t)))
                throw std::runtime_error("Cannot write decoded PCM");
        }
        std::cout<<clip.sampleRate<<" "<<clip.channels<<" "<<clip.frames()<<" "<<clip.looping<<" "<<clip.loopStart<<" "<<clip.loopEnd<<"\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<error.what()<<"\n";
        return 1;
    }
}
