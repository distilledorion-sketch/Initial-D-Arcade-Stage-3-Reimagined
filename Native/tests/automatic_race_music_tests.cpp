#include "race_music_selection.h"
#include <iostream>
#include <set>
int main()try{
    idas3::AutomaticRaceMusic automatic(1234);std::set<int> heard;int previous=0;
    for(unsigned i=0;i<500;++i){
        int track=automatic.select(-1);
        if(track<1||track>12||track==previous)throw std::runtime_error("Invalid or repeated AUTO track");
        heard.insert(track);previous=track;
        if(automatic.select(26)!=26||automatic.select(-2)!=-2)
            throw std::runtime_error("AUTO replaced an explicit built-in/custom selection");
    }
    if(heard.size()!=12)throw std::runtime_error("AUTO never selected every Stage 3 race song");
    std::cout<<"PASS 500 automatic race selections; all 12 songs, no consecutive repeats, explicit choices preserved\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
