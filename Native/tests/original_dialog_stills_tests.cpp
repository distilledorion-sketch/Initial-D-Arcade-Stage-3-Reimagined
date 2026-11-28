#include "original_rival_dialog.h"
#include "original_rival_dialog_scene.h"
#include <bit>
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
    const auto descriptor=[&](unsigned enemy){
        if(data.japanese())return word(enemy==0?0x0c104534:0x0c104538+enemy*4);
        return 0x0c31a008+enemy*8;
    };
    const auto verify=[&](const OriginalRivalDialogState& s,unsigned enemy,unsigned picture){
        const auto table=word(descriptor(enemy)),count=word(descriptor(enemy)+4);
        const auto record=table+44*(picture%count),raw=word(record);
        const auto expected=word(word((data.japanese()?0x0c326454:0x0c31a6a8)+enemy*4)+raw*4);
        ++checks;
        if(s.portraitChunk!=expected||s.backgroundChunk!=word(record+4)||s.spectators!=word(record+8)){
            if(failures<16)std::cout<<"Mismatch enemy="<<enemy<<" picture="<<picture<<" portrait="<<s.portraitChunk<<" expected="<<expected<<'\n';
            ++failures;
        }
    };
    bool comparedItsuki=false;
    const auto verifySpeech=[&](OriginalRivalDialogState& s){
        unsigned previous=~0u;
        for(unsigned frame=0;frame<10000&&!s.completed;++frame){
            stepOriginalRivalDialog(s,data,profile);
            if(s.text.glyphs.empty()||s.cursor==previous||s.backgroundWait)continue;
            previous=s.cursor;
            // A P command changes the portrait. Already placed glyphs
            // retain the old picture's coordinates until the next page.
            const auto& tokens=(s.buntaChallenge?data.buntaRecord(s.course,s.kind):data.record(s.character,s.kind)).tokens;
            if(s.cursor&&tokens.at(s.cursor-1).bytes.starts_with("P"))continue;
            const auto row=word(descriptor(s.character))+44*(s.picture%word(descriptor(s.character)+4));
            const auto anchor=word(row+36)+12*(s.speaker%word(row+40));
            const float x=std::bit_cast<float>(word(anchor)),y=std::bit_cast<float>(word(anchor+4));
            ++checks;
            const auto& first=s.text.glyphs.front();
            // The Flycast capture of "イツキは / すぐ…" starts the second
            // character below the first and the next column to its left.
            if(s.character==0&&s.kind==0&&s.picture==0&&s.speaker==1&&s.cursor==32){
                const auto& gs=s.text.glyphs;comparedItsuki=true;++checks;
                if(gs.size()<5||gs[0].x!=255||gs[0].y!=140||gs[1].x!=255||gs[1].y!=156||
                    gs[4].x!=237||gs[4].y!=140||s.text.color!=0xffffffffu)++failures;
            }
            if(!s.text.vertical||s.text.cell!=16||s.text.linePitch!=18||first.x!=x||first.y!=y){
                if(failures<16)std::cout<<"Speech mismatch enemy="<<s.character<<" kind="<<s.kind<<" picture="<<s.picture
                    <<" speaker="<<s.speaker<<" cursor="<<s.cursor<<" expected="<<x<<","<<y
                    <<" actual="<<first.x<<","<<first.y<<'\n';
                ++failures;
            }
        }
    };
    for(unsigned enemy=0;enemy<31;++enemy){
        selectOriginalRival(profile,enemy);
        for(unsigned picture=0;picture<word(descriptor(enemy)+4);++picture){
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
                if(data.japanese()&&!beaten)verifySpeech(s);
            }
        }
    }
    if(data.japanese()&&data.hasBunta()){
        for(unsigned course=0;course<9;++course)for(unsigned kind=0;kind<51;++kind){
            OriginalRivalDialogSetup setup;setup.buntaChallenge=true;setup.buntaCourse=course;setup.kind=kind;
            OriginalRivalDialogState state;resetOriginalRivalDialog(state,data,profile,setup);
            verifySpeech(state);
        }
    }
    if(data.japanese()&&!comparedItsuki)throw std::runtime_error("Flycast dialogue fixture was not reached");
    std::cout<<(failures?"FAIL ":"PASS ")<<checks<<" original dialogue still checks, "<<failures<<" mismatches; all31 rivals and24 scene kinds\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
