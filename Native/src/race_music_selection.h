#pragma once
#include "music_catalog.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace idas3 {
// This host preference never touches the original driver/card profile. IDs,
// rather than menu order or corrected display titles, are persisted.
// Index0 remains catalogued for original attract audio, but is not a race option.
inline bool validRaceMusicMetadata(int index){return index>=-1&&index<int(raceMusicCatalog.size());}
inline bool validRaceMusicSelection(int index){return validRaceMusicMetadata(index)&&index!=0;}
inline int effectiveRaceMusicSelection(int index){return index==-2?-2:index<=0?1:index;}
inline int loadRaceMusicSelection(const std::filesystem::path& directory,int legacyIndex){
    const int legacy=clampMusicTrack(legacyIndex);
    const int fallback=legacy==0?-1:legacy;
    const auto path=directory/"race_music.txt";
    std::error_code error;const auto bytes=std::filesystem::file_size(path,error);
    if(error||bytes>256)return fallback;
    std::ifstream input(path);std::string header,id,extra;
    if(!std::getline(input,header)||!std::getline(input,id))return fallback;
    if(!header.empty()&&header.back()=='\r')header.pop_back();
    if(!id.empty()&&id.back()=='\r')id.pop_back();
    if(header!="IDAS3_RACE_MUSIC_V1"||std::getline(input,extra))return fallback;
    if(id=="default")return -1;
    const int index=findMusicTrack(id);return index==0?-1:index>0?index:fallback;
}
inline void saveRaceMusicSelection(const std::filesystem::path& directory,int index){
    if(!validRaceMusicSelection(index))throw std::invalid_argument("Unknown race music track");
    static std::atomic<unsigned long long> sequence{0};
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path=directory/"race_music.txt";
    const auto temporary=directory/("race_music.txt.tmp-"+std::to_string(stamp)+"-"+std::to_string(++sequence));
    try{
        std::filesystem::create_directories(directory);
        {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
            if(!output)throw std::runtime_error("Could not save the race music choice");
            output<<"IDAS3_RACE_MUSIC_V1\n"<<(index<0?"default":raceMusicCatalog[index].id)<<'\n';
            output.flush();if(!output)throw std::runtime_error("Could not write the race music choice");
            output.close();if(!output)throw std::runtime_error("Could not close the race music choice");}
#if defined(_WIN32)
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace the saved race music choice");
#else
        std::filesystem::rename(temporary,path);
#endif
    }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
}
