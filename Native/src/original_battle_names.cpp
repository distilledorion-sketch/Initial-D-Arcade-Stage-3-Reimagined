#include "original_battle_names.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
float lit(std::uint32_t v){return std::bit_cast<float>(v);}
template<class T>void readExact(const std::filesystem::path& path,T& out,std::size_t offset=0){
    std::ifstream f(path,std::ios::binary);f.seekg(std::streamoff(offset));f.read(reinterpret_cast<char*>(&out),sizeof(out));
    if(!f||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected original name data: "+path.string());
}
void appendUtf8(std::string& out,std::uint32_t code){
    if(code<0x80)out.push_back(char(code));
    else if(code<0x800){out.push_back(char(0xc0|(code>>6)));out.push_back(char(0x80|(code&63)));}
    else {out.push_back(char(0xe0|(code>>12)));out.push_back(char(0x80|((code>>6)&63)));out.push_back(char(0x80|(code&63)));}
}
// Diagnostic transcriptions of the actual game2d147..181 label artwork.
// Rendering always uses that artwork, including the source29/30 icon swap.
constexpr const char* carCodes[]{"AE86 TRUENO","AE86 LEVIN","AE85","SW20","ZZW30","SXE 10","ST205",
    "BNR32","BNR34","S13","S14 1st","S14 2nd","S15","RPS13","RPS13 KAI","EK9","EG6","DC2","AP1",
    "CE9A","CN9A","CT9A","FD3S VI","FD3S I","FC3S","NA6CE","NB8C","GC8","GDB","GC8V","EA11R",
    "ER34","CP9A Evo.V","CP9A Evo.VI","SE3P"};
}
OriginalBattleNames OriginalBattleNames::load(const std::filesystem::path& root){
    OriginalBattleNames out;const auto folder=root/"data/original_assets/hud/names";
    const auto path=folder/"names.idasname";std::ifstream f(path,std::ios::binary);std::array<char,8> magic{};std::uint32_t version{},count{};
    f.read(magic.data(),8);f.read(reinterpret_cast<char*>(&version),4);f.read(reinterpret_cast<char*>(&count),4);
    if(magic!=std::array<char,8>{'I','D','A','S','3','N','1','\0'}||version!=1||count!=32)throw std::runtime_error("Unknown original name stream");
    readExact(path,out.names_,16);readExact(folder/"font_indices.bin",out.indices_);
    // Shared lossless name-entry export: source268D54 initializes the table
    // used by191640. Only its fixed glyph prefix is needed by this HUD.
    std::ifstream glyphFile(root/"data/original_frontend/name_entry.bin",std::ios::binary);
    std::array<std::uint32_t,5> glyphHeader{};
    glyphFile.read(reinterpret_cast<char*>(glyphHeader.data()),sizeof(glyphHeader));
    glyphFile.read(reinterpret_cast<char*>(out.profileGlyphs_.data()),sizeof(out.profileGlyphs_));
    if(!glyphFile||glyphHeader[0]!=0x454e4449u||glyphHeader[1]!=1||glyphHeader[2]!=221||glyphHeader[3]!=173)
        throw std::runtime_error("Invalid original profile-name glyph table");
    // This existing lossless export maps the same EUC lookup keys to Unicode.
    // Read only its Unicode table; the race font keeps its own336 textures.
    std::ifstream unicodeFile(root/"data/original_assets/hud/start_names/start_names.bin",std::ios::binary);
    std::array<char,8> unicodeMagic{};std::array<std::uint32_t,5> unicodeHeader{};
    unicodeFile.read(unicodeMagic.data(),8);unicodeFile.read(reinterpret_cast<char*>(unicodeHeader.data()),sizeof(unicodeHeader));
    if(unicodeMagic!=std::array<char,8>{'I','D','A','S','3','S','N','1'}||unicodeHeader!=std::array<std::uint32_t,5>{1,341,9216,221,80})
        throw std::runtime_error("Invalid original Unicode name map");
    unicodeFile.seekg(std::streamoff(28+9216*2));unicodeFile.read(reinterpret_cast<char*>(out.unicode_.data()),sizeof(out.unicode_));
    if(!unicodeFile)throw std::runtime_error("Truncated original Unicode name map");
    out.font_=NativeTextureBank::load(folder/"textures/textures.idastex");out.labels_=OriginalBattleHudAssets::load(root);
    if(out.font_.size()!=336)throw std::runtime_error("Original name font bank mismatch");
    for(unsigned i=0;i<32;++i)(void)out.glyphs(i==31?0:i,i==31);
    return out;
}
std::span<const std::uint8_t> OriginalBattleNames::sourceName(std::uint32_t enemy,bool player)const{
    if(enemy>30)throw std::invalid_argument("Invalid original rival name");
    const auto& name=names_[player?31:enemy];const auto end=std::find(name.begin(),name.end(),0);
    if(end==name.end()||(end-name.begin())%2)throw std::runtime_error("Invalid original double-byte name");
    return {name.data(),std::size_t(end-name.begin())};
}
std::vector<std::uint8_t> OriginalBattleNames::sourcePlayerName(const original::OriginalBattleProfile& profile)const{
    const auto length=profile.u(76);
    if(length>5)throw std::invalid_argument("Original profile name exceeds five glyphs");
    if(!length){const auto name=sourceName(0,true);return {name.begin(),name.end()};}
    std::vector<std::uint8_t> out;out.reserve(length*2);
    for(unsigned i=0;i<length;++i){
        const auto id=profile.u(44+i*4);
        //191640's unsigned bound replaces invalid IDs with glyph0.
        const auto& bytes=profileGlyphs_[id<=220?id:0];
        out.insert(out.end(),bytes.begin(),bytes.end());
    }
    return out;
}
std::vector<OriginalNameGlyph> OriginalBattleNames::glyphs(std::uint32_t enemy,bool player,
        const original::OriginalBattleProfile* profile)const{
    //0C9F20 normal-layout0 branch; 0C5420 dimensions and advancement feed
    //0C68E0. Missing0xffff glyphs consume spacing exactly as in that loop.
    const auto custom=player&&profile?sourcePlayerName(*profile):std::vector<std::uint8_t>{};
    const auto name=player&&profile?std::span<const std::uint8_t>(custom):sourceName(enemy,player);std::vector<OriginalNameGlyph> out;
    float x=510.f;const float y=player?182.f:146.f,size=player?12.f:16.f,advance=player?10.5f:16.f;
    for(std::size_t i=0;i<name.size();i+=2){
        const unsigned index=std::uint8_t(name[i]-160)*96u+std::uint8_t(name[i+1]-160);
        if(index>=indices_.size())throw std::runtime_error("Original name index outside exported font table");
        const auto glyph=indices_[index];
        if(glyph!=0xffff){if(glyph>=font_.size())throw std::runtime_error("Original name glyph outside texture bank");out.push_back({glyph,x,y,size,size});}
        x=std::fma(advance,1.f,x);
    }
    return out;
}
void OriginalBattleNames::paint(std::span<std::uint32_t> argb,int width,int height,
        std::uint32_t enemy,std::uint32_t profileMode,std::uint32_t playerCar,std::uint32_t rivalCar,
        const original::OriginalBattleProfile* profile)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height||playerCar>=35||rivalCar>=35)throw std::invalid_argument("Invalid original battle name destination or car");
    if(profileMode==2)enemy=30;
    for(bool player:{true,false})paintGlyphs(argb,width,height,glyphs(enemy,player,profile));
    paintLabels(argb,width,height,playerCar,rivalCar,182.f,146.f);
}
void OriginalBattleNames::paintTimeAttack(std::span<std::uint32_t> argb,int width,int height,std::uint32_t car,const original::OriginalBattleProfile* profile)const{
    paintGlyphs(argb,width,height,glyphs(0,true,profile));
    paintLabels(argb,width,height,car,0,182.f,146.f,true);
}
void OriginalBattleNames::paintGlyphs(std::span<std::uint32_t> argb,int width,int height,std::span<const OriginalNameGlyph> glyphs)const{
    const float fit=std::min(float(width)/640.f,float(height)/480.f),offsetX=float(width)-640.f*fit;
    for(const auto& g:glyphs){
        // Original0C5420 UV0..-1 uses wrapping; native image vertices retain
        // that vertical orientation. Coordinates are the original640×480
        // font canvas, mapped uniformly into the native edge-anchored HUD.
        OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{g.x,g.y,0,0,1,0xffffffff,0},{g.x,g.y+g.height,0,0,0,0xffffffff,0},
            {g.x+g.width,g.y,0,1,1,0xffffffff,0},{g.x+g.width,g.y+g.height,0,1,0,0xffffffff,0}};
        SpritePlacement placement;placement.scale=fit;placement.offsetX=offsetX;
        compositeOriginalSprite(argb,width,height,font_.at(g.texture),sprite,placement);
    }
}
void OriginalBattleNames::paintLabels(std::span<std::uint32_t> argb,int width,int height,
        std::uint32_t playerCar,std::uint32_t rivalCar,float playerY,float rivalY,bool playerOnly)const{
    std::vector<OriginalBattleHudDraw> labels;
    for(bool player:{true,false}){
        if(playerOnly&&!player)continue;
        const auto car=player?playerCar:rivalCar;const auto icon=car==29?30:car==30?29:car;
        const float nameY=((player?playerY:rivalY)-71.f)/100.f;
        auto matrix=original::originalIdentityMatrix();original::translateOriginalMatrix(matrix,{lit(0x3dcccccd),-nameY+lit(0x3c23d70a),0});
        OriginalBattleHudDraw iconDraw;iconDraw.kind=OriginalBattleHudDraw::Kind::game2d;iconDraw.draw.index=147+icon;iconDraw.draw.matrix=matrix;labels.push_back(iconDraw);
        matrix=original::originalIdentityMatrix();original::translateOriginalMatrix(matrix,{lit(0x409e6666),-nameY-lit(0x3f3851ec),player?0.f:lit(0x3a83126f)});
        for(unsigned i=0;i<8;++i)matrix.elements[i]*=lit(0x3f4ccccd);
        OriginalBattleHudDraw marker;marker.kind=OriginalBattleHudDraw::Kind::game2d;marker.draw.index=player?93u:94u;marker.draw.matrix=matrix;labels.push_back(marker);
    }
    labels_.paintGame2d(argb,width,height,labels,true);
}
OriginalOnlineBattleName OriginalBattleNames::onlineName(const std::string& utf8,bool player,std::uint32_t car)const{
    if(car>=35)throw std::invalid_argument("Invalid online HUD car");
    OriginalOnlineBattleName out;out.carCode=carCodes[car];
    std::vector<std::uint16_t> keys;
    const auto lookup=[&](std::uint32_t code)->std::size_t{
        if((code>='a'&&code<='z')||(code>=0xff41&&code<=0xff5a))code-=32;
        if(code==32)code=0x3000;else if(code>=33&&code<=126)code+=0xfee0;
        for(const auto& bytes:profileGlyphs_){
            const auto key=unsigned(std::uint8_t(bytes[0]-160))*96+std::uint8_t(bytes[1]-160);
            if(key<unicode_.size()&&unicode_[key]==code)return key;
        }
        return unicode_.size();
    };
    const auto question=lookup(0xff1f);
    const auto& source=utf8.empty()?(player?std::string("PLAYER"):std::string("OPPONENT")):utf8;
    for(std::size_t i=0;i<source.size()&&keys.size()<32;){
        const auto c=std::uint8_t(source[i++]);std::uint32_t code=c;unsigned length=0;
        if(c>=0xc2&&c<=0xdf){code=c&31;length=1;}
        else if(c>=0xe0&&c<=0xef){code=c&15;length=2;}
        else if(c>=0xf0&&c<=0xf4){code=c&7;length=3;}
        else if(c>=128)code=0xfffd;
        for(unsigned n=0;n<length;++n){
            if(i>=source.size()||(std::uint8_t(source[i])&0xc0)!=0x80){code=0xfffd;break;}
            code=(code<<6)|(std::uint8_t(source[i++])&63);
        }
        if((length==1&&code<0x80)||(length==2&&code<0x800)||(length==3&&code<0x10000)||
            (code>=0xd800&&code<=0xdfff)||code>0x10ffff)code=0xfffd;
        if(code<32||code==127)continue;
        auto key=lookup(code);
        if(key>=indices_.size()||(indices_[key]==0xffff&&code!=32&&code!=0x3000))key=question;
        if(key<indices_.size())keys.push_back(std::uint16_t(key));
    }
    //0CA344/0CA460 initialize profile3 names at(510,130)/(510,83).
    //0CA5EE..0CA608 use12px glyphs and10.5px advancement for entered names.
    // Original cabinet names have five characters. Longer native names fit
    // uniformly inside x510..572, before the unmodified car artwork at574.
    const float extent=keys.empty()?0.f:12.f+10.5f*float(keys.size()-1);
    const float scale=extent>62.f?62.f/extent:1.f;
    float x=510.f;const float y=(player?130.f:83.f)+(12.f-12.f*scale)*.5f;
    for(auto key:keys){
        auto code=unicode_[key];if(code>=0xff01&&code<=0xff5e)code-=0xfee0;if(code==0x3000)code=32;
        appendUtf8(out.text,code?code:'?');
        const auto glyph=indices_[key];
        if(glyph!=0xffff){if(glyph>=font_.size())throw std::runtime_error("Online name exceeds source font");out.glyphs.push_back({glyph,x,y,12.f*scale,12.f*scale});}
        x=std::fma(10.5f*scale,1.f,x);
    }
    return out;
}
std::array<OriginalOnlineBattleName,2> OriginalBattleNames::paintOnline(std::span<std::uint32_t> argb,int width,int height,
        const std::string& playerName,const std::string& rivalName,std::uint32_t playerCar,std::uint32_t rivalCar)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid online battle name destination");
    std::array<OriginalOnlineBattleName,2> rows{onlineName(playerName,true,playerCar),onlineName(rivalName,false,rivalCar)};
    for(const auto& row:rows)paintGlyphs(argb,width,height,row.glyphs);
    paintLabels(argb,width,height,playerCar,rivalCar,130.f,83.f);
    return rows;
}
}
