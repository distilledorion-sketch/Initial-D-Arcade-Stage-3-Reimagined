#include "original_rival_dialog.h"
#include "original_rival_dialog_scene.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace idas3::original;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Arguments: canonical image, native root");
    std::ifstream input(argv[1],std::ios::binary);
    const std::vector<unsigned char> image{std::istreambuf_iterator<char>(input),{}};
    const auto word=[&](unsigned address){
        const auto at=std::size_t(address-0x0c020000u);
        if(at+4>image.size())throw std::runtime_error("Source address outside image");
        return unsigned(image[at])|(unsigned(image[at+1])<<8)|(unsigned(image[at+2])<<16)|(unsigned(image[at+3])<<24);
    };
    const auto data=OriginalRivalDialogData::load(argv[2]);
    auto profile=makeOriginalFreshBattleProfile();
    unsigned checks=0,failures=0;
    const auto verify=[&](const OriginalRivalDialogState& s,unsigned enemy,unsigned picture){
        const auto table=word(0x0c31a008+enemy*8),count=word(0x0c31a00c+enemy*8);
        const auto record=table+44*(picture%count),raw=word(record);
        const auto expected=word(word(0x0c31a6a8+enemy*4)+raw*4);
        ++checks;
        if(s.portraitChunk!=expected||s.backgroundChunk!=word(record+4)||s.spectators!=word(record+8)){
            if(failures<16)std::cout<<"Mismatch enemy="<<enemy<<" picture="<<picture<<" portrait="<<s.portraitChunk<<" expected="<<expected<<'\n';
            ++failures;
        }
    };
    for(unsigned enemy=0;enemy<31;++enemy){
        selectOriginalRival(profile,enemy);
        for(unsigned picture=0;picture<word(0x0c31a00c+enemy*8);++picture){
            OriginalRivalDialogState s;s.initialized=true;s.character=enemy;
            s.phase=1;s.scriptPhase=3;s.backgroundWait=1;s.picture=picture;
            stepOriginalRivalDialog(s,data,profile);verify(s,enemy,picture);
        }
        // Initial picture selection also runs through the actual script prescan
        // for every intro, rematch and result page, with fresh/beaten profiles.
        for(unsigned beaten=0;beaten<2;++beaten){
            profile.setByte(116+enemy,beaten?0x11:0);
            for(unsigned kind=0;kind<24;++kind){
                OriginalRivalDialogSetup setup;setup.enemy=enemy;setup.kind=kind;
                OriginalRivalDialogState s;resetOriginalRivalDialog(s,data,profile,setup);
                verify(s,enemy,s.picture);
            }
        }
    }
    std::cout<<(failures?"FAIL ":"PASS ")<<checks<<" original dialogue still checks, "<<failures<<" mismatches; all31 rivals and24 scene kinds\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
