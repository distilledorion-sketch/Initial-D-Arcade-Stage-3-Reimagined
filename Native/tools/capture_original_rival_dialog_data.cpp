// Bounded original startup/data exporter. No graphics, OS or device execution.
#include "sh4_scalar_reference.h"
#include "original_rival_dialog_data.h"
#include <iostream>
#include <set>
using namespace idas3::reference;
namespace {
void word(std::ostream&o,unsigned v){for(unsigned i=0;i<4;++i)o.put(char(v>>(8*i)));}
std::string sourceString(const RefMemory&m,unsigned p){std::string s;for(unsigned i=0;i<4096;++i){const auto c=m.read8(p+i);if(!c)return s;s+=char(c);}throw std::runtime_error("Unterminated source dialog token");}
void text(std::ostream&o,const std::string&s){word(o,unsigned(s.size()));o.write(s.data(),s.size());}
void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("usage: capture_original_rival_dialog_data canonical-image project-root");
    RefMemory m(argv[1],RefMemory::SupportedImages::EnglishAndJapanRevC);m.zeroRegion(0x0d000000,0x20000);RefCpu c(m);c.r[15]=0x0d010000;
    constexpr unsigned stop=0x0f000000;
    const std::array<unsigned,10> expected{0x0c1ce6a0,0x0c1cada0,0x0c1c9a40,0x0c1c6e00,0x0c1c5a60,0x0c1c4720,0x0c1c33e0,0x0c1c2040,0x0c1c1980,0x0c1bf9c0};
    std::size_t instructions=0,checks=0,totalTokens=0;
    //221E20 traverses the constructor table backwards. Unrelated constructors
    // are omitted explicitly; these ten contain only bounded data writes.
    if(m.japanRevC){
        // The Rev C constructor initializes the entire contiguous 31 x 24 array.
        require(m.read32(0x0c104718)==0x0c0f8220,"Japanese dialogue constructor identity");
        c.pr=stop;instructions+=c.run(0x0c104700,stop,1000000);
    }else for(unsigned i=0;i<10;++i){const auto entry=m.read32(0x0c2eef10-4*i);require(entry==expected[i],"Original constructor table changed");c.pr=stop;instructions+=c.run(entry,stop,1000000);}
    const unsigned characters=m.japanRevC?0x0c27307c:0x0c26b81c;
    const unsigned courses=m.japanRevC?0x0c26c150:0x0c2637f8;
    const unsigned nights=m.japanRevC?0x0c26c1cc:0x0c263874;
    const unsigned portraits=m.japanRevC?0x0c3269b8:0x0c31abe4;
    const auto recordAddress=[&](unsigned i,unsigned k){
        return m.japanRevC?0x0c318bb0+(i*24+k)*32:m.read32(0x0c31bb18+4*i)+32*k;
    };
    auto outDir=std::filesystem::path(argv[2])/"data/original_assets/rival_dialog";std::filesystem::create_directories(outDir);
    {
        std::ofstream o(outDir/"dialog.idasdialog",std::ios::binary);o.write("IDAS3RD1",8);word(o,m.japanRevC?2:1);word(o,31);word(o,24);
        for(unsigned i=0;i<31;++i){word(o,m.read32(characters+4*i));word(o,m.read32(courses+4*i));word(o,m.read32(nights+4*i));text(o,sourceString(m,m.read32(portraits+4*i)));}
        for(unsigned i=0;i<31;++i)for(unsigned k=0;k<24;++k){const auto p=recordAddress(i,k),s=m.read32(p);word(o,p);word(o,s);for(unsigned j=1;j<8;++j)word(o,m.read32(p+4*j));
            std::vector<std::pair<unsigned,std::string>> tokens;
            if(s)for(unsigned j=0;j<4096;++j){const auto a=m.read32(s+4*j);auto t=sourceString(m,a);tokens.emplace_back(a,t);if(t=="E")break;}
            require(!s||(!tokens.empty()&&tokens.back().second=="E"),"Original script has no bounded end");word(o,unsigned(tokens.size()));for(const auto&[a,t]:tokens){word(o,a);text(o,t);}totalTokens+=tokens.size();
        }
        require(bool(o),"Cannot write original dialogue data");
    }
    const auto native=idas3::original::OriginalRivalDialogData::load(argv[2]);
    for(unsigned i=0;i<31;++i){const auto&id=native.enemy(i);require(id.character==m.read32(characters+4*i)&&id.backgroundCourse==m.read32(courses+4*i)&&id.backgroundNight==m.read32(nights+4*i)&&id.portrait==sourceString(m,m.read32(portraits+4*i)),"Native identity differs");checks+=4;
        for(unsigned k=0;k<24;++k){const auto&r=native.record(i,k);auto p=recordAddress(i,k);require(r.sourceRecord==p&&r.sourceScript==m.read32(p),"Native scene row differs");checks+=2;
            for(unsigned j=0;j<6;++j){require(std::bit_cast<unsigned>(r.portraitCoordinates[j])==m.read32(p+4+4*j),"Native layout differs");++checks;}
            require(r.portraitKinds==m.read32(p+28),"Native portrait type differs");++checks;
            for(unsigned j=0;j<r.tokens.size();++j){const auto&t=r.tokens[j];require(t.sourceAddress==m.read32(r.sourceScript+4*j)&&t.bytes==sourceString(m,t.sourceAddress),"Native script token differs");checks+=2;}
        }
    }
    // Complete tiny source eligibility helper, all authored/nearby values.
    for(unsigned kind=0;kind<64;++kind){c.r[4]=0;c.r[5]=kind;c.pr=stop;instructions+=c.run(m.japanRevC?0x0c1069a0:0x0c0fa5c0,stop,1000);require(c.r[0]==idas3::original::originalRivalDialogKindAvailable(kind),"Native page availability differs");++checks;}
    std::cout<<"PASS original dialogue initialization,744 authored rows,"<<totalTokens<<" original tokens; "<<checks<<" checks,"<<instructions<<" original instructions.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
