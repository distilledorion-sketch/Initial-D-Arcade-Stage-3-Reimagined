#include "original_rival_dialog_data.h"
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
class Reader {
    std::ifstream in_;
public:
    explicit Reader(const std::filesystem::path&p):in_(p,std::ios::binary){if(!in_)throw std::runtime_error("Original rival dialogue data unavailable");}
    std::string bytes(std::size_t n){std::string s(n,'\0');in_.read(s.data(),std::streamsize(n));if(!in_)throw std::runtime_error("Truncated original rival dialogue data");return s;}
    std::uint32_t word(){auto s=bytes(4);std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(static_cast<unsigned char>(s[i]))<<(8*i);return v;}
    std::string text(std::size_t limit){const auto n=word();if(n>limit)throw std::runtime_error("Original rival dialogue string bound");return bytes(n);}
    void end(){if(in_.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original rival dialogue data");}
};
void require(bool v){if(!v)throw std::runtime_error("Original rival dialogue identity mismatch");}
}
OriginalRivalDialogData OriginalRivalDialogData::load(const std::filesystem::path&root){
    Reader f(root/"data/original_assets/rival_dialog/dialog.idasdialog");
    require(f.bytes(8)==std::string("IDAS3RD1",8));require(f.word()==1&&f.word()==31&&f.word()==24);
    OriginalRivalDialogData out;
    for(auto&id:out.identities_){id.character=f.word();id.backgroundCourse=f.word();id.backgroundNight=f.word();id.portrait=f.text(31);
        require(id.character<31&&id.backgroundCourse<9&&id.backgroundNight<2&&!id.portrait.empty());
        for(char c:id.portrait)require((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_');}
    for(auto&r:out.records_){r.sourceRecord=f.word();r.sourceScript=f.word();require(r.sourceRecord>=0x0c35f168&&r.sourceRecord<0x0c36a600);
        for(auto&v:r.portraitCoordinates){v=std::bit_cast<float>(f.word());require(std::isfinite(v));}
        r.portraitKinds=f.word();const auto count=f.word();require(count<=4096);
        require((r.sourceScript==0)==(count==0));
        if(count)require(r.sourceScript>=0x0c020000&&r.sourceScript<0x0c420000);
        for(unsigned i=0;i<count;++i){OriginalRivalDialogToken t;t.sourceAddress=f.word();require(t.sourceAddress>=0x0c020000&&t.sourceAddress<0x0c420000);t.bytes=f.text(4096);require(!t.bytes.empty()&&t.bytes.find('\0')==std::string::npos);r.tokens.push_back(std::move(t));}
        if(count)require(r.tokens.back().bytes=="E");
    }
    f.end();
    const auto buntaPath=root/"data/original_assets/rival_dialog/bunta.idasdialog";
    if(std::filesystem::exists(buntaPath)){
        Reader b(buntaPath);require(b.bytes(8)=="IDAS3BD1");require(b.word()==1&&b.word()==8&&b.word()==51);
        out.buntaRecords_.resize(8*51);
        for(std::size_t index=0;index<out.buntaRecords_.size();++index){
            auto&r=out.buntaRecords_[index];r.sourceRecord=b.word();r.sourceScript=b.word();
            require(r.sourceRecord==0x0c363100+std::uint32_t(index)*32);
            require(r.sourceScript>=0x0c020000&&r.sourceScript<0x0c420000);
            for(auto&v:r.portraitCoordinates){v=std::bit_cast<float>(b.word());require(std::isfinite(v));}
            r.portraitKinds=b.word();const auto count=b.word();require(count>0&&count<=512);
            for(unsigned i=0;i<count;++i){OriginalRivalDialogToken t;t.sourceAddress=b.word();
                require(t.sourceAddress>=0x0c020000&&t.sourceAddress<0x0c420000);
                t.bytes=b.text(4096);require(!t.bytes.empty()&&t.bytes.find('\0')==std::string::npos);r.tokens.push_back(std::move(t));}
            require(r.tokens.back().bytes=="E");
        }
        b.end();
    }
    return out;
}
const OriginalRivalDialogIdentity&OriginalRivalDialogData::enemy(std::uint32_t index)const{if(index>=identities_.size())throw std::out_of_range("Original rival dialogue enemy");return identities_[index];}
const OriginalRivalDialogRecord&OriginalRivalDialogData::record(std::uint32_t character,std::uint32_t kind)const{if(character>=31||kind>=24)throw std::out_of_range("Original rival dialogue kind");return records_[character*24+kind];}
const OriginalRivalDialogRecord&OriginalRivalDialogData::buntaRecord(std::uint32_t course,std::uint32_t kind)const{
    if(course>=9||kind>=51||buntaRecords_.empty())throw std::out_of_range("Original Bunta dialogue kind");
    //0F611A maps Akina Snow to the same authored script table as Akina.
    return buntaRecords_[(course==8?3:course)*51+kind];
}
bool originalRivalDialogKindAvailable(std::uint32_t kind){return kind!=1&&kind!=8&&kind!=15;}
}
