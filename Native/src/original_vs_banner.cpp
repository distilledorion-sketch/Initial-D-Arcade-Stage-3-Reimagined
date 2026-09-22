#include "imported_course_catalog.h"
#include "original_vs_banner.h"
#include "original_gasstand_data.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace idas3 {
namespace {
constexpr const char* directory = "data/original_assets/hud/start2d";
constexpr const char* nameDirectory = "data/original_assets/hud/start_names";
constexpr const char* recordFontPath = "data/original_assets/attract/alphabet/textures/textures.idastex";
namespace recordAlphabet=original::gasstand_data;
float recordTextWidth(const std::string& text){
    float width=0;
    for(char c:text){const auto at=std::find(recordAlphabet::fontCharacters.begin(),recordAlphabet::fontCharacters.end(),c);
        if(at==recordAlphabet::fontCharacters.end())throw std::logic_error("Battle record character outside original alphabet");
        width+=recordAlphabet::word(recordAlphabet::fontWidthWords[std::size_t(at-recordAlphabet::fontCharacters.begin())]);
    }
    return width;
}
std::string battleRecord(std::uint32_t battles,std::uint32_t wins){
    wins=std::min(wins,battles);
    // Integer tenths truncate, including54.25 ->54.2, without floating point
    // rounding or uint32 multiplication overflow for long-lived profiles.
    const auto tenths=battles?std::uint64_t(wins)*1000/battles:0;
    return std::to_string(battles)+" BATTLE(S)  "+std::to_string(wins)+" WIN(S)  "+
        std::to_string(tenths/10)+"."+std::to_string(tenths%10)+"%";
}
// Exact visible English labels in the original localized namekana textures.
// The corresponding source string bytes are Japanese texture lookup keys;
// decoding those keys as Unicode would report a different, unpainted name.
constexpr const char* localizedRivalNames[]{"IGGY","KENJI","SHINGO","TORU","KAWAI",
    "MAYA & SIMONE","TWO GUYS FROM TOKYO","DANNY","K.T.","COLE","ZACK","KYLE",
    "RY","TAK","HAWK","KYLIE","CAINE","MIKI","DICE","SMILEY","TOUCH","NOBU",
    "SID","AKI","KYLIE","RY","MAN IN EVO.V","MAN IN EVO.VI","K.T.","TAK","BUNTA","BUNTA"};
template<class T> void readValue(std::ifstream& stream,T& value) {
    stream.read(reinterpret_cast<char*>(&value),sizeof(value));
    if(!stream)throw std::runtime_error("Truncated original start-name data");
}
void appendUtf8(std::string& out,std::uint32_t code) {
    if(code<0x80)out.push_back(char(code));
    else if(code<0x800){out.push_back(char(0xc0|(code>>6)));out.push_back(char(0x80|(code&63)));}
    else if(code<0x10000){out.push_back(char(0xe0|(code>>12)));out.push_back(char(0x80|((code>>6)&63)));out.push_back(char(0x80|(code&63)));}
    else {out.push_back(char(0xf0|(code>>18)));out.push_back(char(0x80|((code>>12)&63)));out.push_back(char(0x80|((code>>6)&63)));out.push_back(char(0x80|(code&63)));}
}

// Chunk numbers, read off the authored textures rather than inferred. The bank
// draws each word from its own chunk. Texture sizes remain source-authored;
// metadataPlacement() separates their overlapping authored rectangles.
//
// Courses, indexed the way the rest of the game indexes them: Myogi, Usui,
// Akagi, Akina, Happogahara, Irohazaka, Shomaru, Tsuchisaka, Akina Snow. The
// bank does not store them in that order -- its fifth name is Irohazaka and its
// sixth Happogahara -- so four and five cross over. Each chunk was rendered on
// its own and read; guessing from the texture order gets this pair wrong.
// 98..103 carry Japanese names and are not drawn here.
constexpr int courseChunk[]{72, 73, 74, 75, 77, 76, 78, 79, 80};
// Uphill, downhill, outbound, inbound, clockwise, counter-clockwise, reverse.
// Each of these carries its bracket glyph in the same chunk.
constexpr int directionChunk[]{113, 109, 114, 107, 115, 111, 108};
// RACE plus a digit, or FINAL, or EXTRA. The digits share one rectangle, so
// they are a single-digit counter beside RACE.
constexpr int raceChunk = 104, finalChunk = 110, extraChunk = 116;
constexpr int digitChunk[]{94, 95, 96, 97, 71};   // 1..5
// Day/night and the weather word each come from a texture holding several
// words, so the two chunks of a pair share a rectangle and differ only in the
// sub-rectangle their own UVs select. The manifest records vertex counts, not
// UVs, so which is which was settled by rendering both: 105 draws DAY and 112
// NIGHT, 106 draws DRY, 118 WET and0 SNOW.
constexpr int dayNightChunk[]{105, 112};
constexpr int weatherChunk[]{106, 118, 0};
// 0C0DA700 / 2FCA48, 2FCA80 and 2FCA40. Source arrays reordered into
// this component's semantic direction order. Quads contain transparent
// padding: their full widths are not the source's word advances.
constexpr std::uint32_t directionAdvanceWords[]{0x3f63d70a,0x3fa00000,0x3fa3d70a,0x3f8e147b,0x3f9eb852,0x40051eb8,0x3f8a3d71};
constexpr std::uint32_t directionOffsetWords[]{0,0,0xbf6b851f,0xbf6b851f,0,0,0xbd23d70a};
constexpr std::uint32_t dayAdvanceWords[]{0x3f147ae1,0x3f4a3d71};
constexpr std::uint32_t courseAdvanceWords[]{0x3ed70a3c,0,0x3eb851e8,0x3ea8f5c0,0x3f9851eb,0x3f63d70a,0x3f6e147a,0x3f91eb85,0x3f5c28f6}; //2FCA18
float sourceWord(std::uint32_t value){return std::bit_cast<float>(value);}
constexpr int restingVsChunk = 119;
constexpr int backdropChunk = 120;
}

bool OriginalVsBanner::available(const std::filesystem::path& root) {
    std::error_code ec;
    return std::filesystem::exists(root / directory / "start2d.idasmesh", ec) &&
           std::filesystem::exists(root / directory / "textures" / "textures.idastex", ec) &&
           std::filesystem::exists(root / nameDirectory / "start_names.bin", ec) &&
           std::filesystem::exists(root / nameDirectory / "textures" / "textures.idastex", ec) &&
           std::filesystem::exists(root / recordFontPath, ec);
}

void OriginalVsBanner::load(const std::filesystem::path& root) {
    if (loaded_) return;
    const auto folder = root / directory;
    model_ = NativeModel::load(folder / "start2d.idasmesh");
    textures_ = NativeTextureBank::load(folder / "textures" / "textures.idastex");
    if (model_.chunks.size() <= std::size_t(backdropChunk))
        throw std::runtime_error("Original start banner bank is missing its authored chunks");
    chunkBounds_.resize(model_.chunks.size());
    chunkDepths_.resize(model_.chunks.size());
    for(std::size_t i=0;i<model_.chunks.size();++i){
        const float infinity=std::numeric_limits<float>::infinity();
        std::array<float,4> bounds{infinity,infinity,-infinity,-infinity};
        for(const auto& batch:model_.chunks[i].batches)for(const auto& vertex:batch.vertices){
            chunkDepths_[i]=vertex.position.z;
            const float x=vertex.position.x*unitsPerPixel,y=-vertex.position.y*unitsPerPixel;
            bounds[0]=std::min(bounds[0],x);bounds[1]=std::min(bounds[1],y);
            bounds[2]=std::max(bounds[2],x);bounds[3]=std::max(bounds[3],y);
        }
        chunkBounds_[i]=std::isfinite(bounds[0])?bounds:std::array<float,4>{};
    }
    // Measure the actual source title ink once; authored rectangles include
    // transparent padding that must not become the height of imported lettering.
    for(std::size_t course=0;course<std::size(courseChunk);++course){
        const int chunk=courseChunk[course];const auto& b=chunkBounds_[chunk];
        const int width=int(std::ceil(b[2]-b[0])),height=int(std::ceil(b[3]-b[1]));
        std::vector<std::uint32_t> pixels(std::size_t(width)*height);
        SpritePlacement p;p.scale=unitsPerPixel;p.invertY=true;p.authoredHeight=0;
        p.offsetX=-b[0];p.offsetY=-b[1];p.straightAlphaOverlay=true;p.softwareOnly=true;
        compositeOriginalMenuChunk(pixels,width,height,textures_,model_.chunks[chunk],p);
        int left=width,top=height,right=0,bottom=0;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(pixels[y*width+x]>>24){
            left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);
        }
        if(right<=left||bottom<=top)throw std::runtime_error("Original start title has no visible ink");
        titleInkBounds_[course]={float(left),float(top),float(right-left),float(bottom-top)};
    }
    nameFont_=NativeTextureBank::load(root/nameDirectory/"textures/textures.idastex");
    recordFont_=NativeTextureBank::load(root/recordFontPath);
    importedTitles_=NativeTextureBank::load(root/"data/original_assets/hud/imported_start_names/titles.idastex");
    if(recordFont_.size()!=recordAlphabet::fontCharacters.size())
        throw std::runtime_error("Original battle-record alphabet bank has the wrong glyph count");
    std::ifstream names(root/nameDirectory/"start_names.bin",std::ios::binary);
    std::array<char,8> magic{}; std::array<std::uint32_t,5> header{};
    readValue(names,magic);readValue(names,header);
    if(magic!=std::array<char,8>{'I','D','A','S','3','S','N','1'} ||
       header!=std::array<std::uint32_t,5>{1,341,9216,221,80} || nameFont_.size()!=341)
        throw std::runtime_error("Invalid original start-name export");
    readValue(names,nameIndices_);readValue(names,nameUnicode_);readValue(names,profileGlyphs_);
    readValue(names,rivalNames_);readValue(names,defaultName_);readValue(names,nameMotion_);
    if(names.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected start-name trailing data");
    for(auto glyph:nameIndices_)if(glyph!=0xffff&&glyph>=nameFont_.size())
        throw std::runtime_error("Original start-name glyph exceeds bank");
    for(const auto& sample:nameMotion_)for(float value:sample)if(!std::isfinite(value))
        throw std::runtime_error("Invalid original name motion sample");
    loaded_ = true;
}

void OriginalVsBanner::begin(const OriginalVsBannerSetup& setup) {
    if (setup.course >= std::size(courseChunk))
        throw std::invalid_argument("Original start banner course is outside the authored set");
    if (setup.direction >= std::size(directionChunk))
        throw std::invalid_argument("Original start banner direction is outside the authored set");
    if (setup.race > std::size(digitChunk))
        throw std::invalid_argument("Original start banner race number is outside the authored set");
    if(setup.enemy>=rivalNames_.size())throw std::invalid_argument("Invalid start-banner rival");
    setup_ = setup;
    sourceTick_=phaseTick_=vsCounter_=playerMotion_=opponentMotion_=0;
    phase_=0;vsChunk_=-1;nameAlpha_={0,0};
    nameX_={nameMotion_[0][0]*1.2f,-nameMotion_[0][0]*1.2f};
    nameY_={nameMotion_[0][1],-nameMotion_[0][1]};
    for(auto& name:names_)name.clear();for(auto& text:displayNames_)text.clear();
    for(unsigned side=0;side<2;++side)displayBattleRecords_[side]=setup.showBattleRecords?
        battleRecord(setup.battles[side],setup.wins[side]):std::string{};
    if(!setup.showVersus)return;
    if(!setup.localNameUtf8.empty())names_[0]=encodeUtf8(setup.localNameUtf8);
    else {
        const auto length=std::min(setup.profile.words[76/4],5u);
        if(!length)names_[0]=encodeSource(defaultName_);
        else for(unsigned i=0;i<length;++i){
            const auto id=setup.profile.words[44/4+i];
            const auto encoded=encodeSource(profileGlyphs_[id<=220?id:0]);
            names_[0].insert(names_[0].end(),encoded.begin(),encoded.end());
        }
    }
    if(!setup.opponentNameUtf8.empty())names_[1]=encodeUtf8(setup.opponentNameUtf8);
    else {
        const auto enemy=setup.profile.words[0]==2?30u:setup.enemy;
        names_[1]=encodeSource(rivalNames_[enemy][0]);
        names_[2]=encodeSource(rivalNames_[enemy][1]);
    }
    displayNames_[0]=decodeName(names_[0]);
    displayNames_[1]=setup.opponentNameUtf8.empty()?
        localizedRivalNames[setup.profile.words[0]==2?30u:setup.enemy]:decodeName(names_[1]);
}

void OriginalVsBanner::tick() {
    // Exact 14B700 counter ordering: phase0 compares the old count with20;
    // transition tick resets to0. The source leaves phase1 running forever.
    if(phase_==0) {
        if(phaseTick_>20){phase_=1;phaseTick_=0;}
        else ++phaseTick_;
    } else {
        auto sample=[&](unsigned side,unsigned index){
            const auto& value=nameMotion_[index];const float sign=side?-1.f:1.f;
            nameX_[side]=sign*value[0]*1.2f;nameY_[side]=sign*value[1];
            nameAlpha_[side]=float(std::clamp(int(value[2]*255.f),0,255))/255.f;
        };
        if(playerMotion_<=79)sample(0,playerMotion_++);
        if(opponentMotion_<=79)sample(1,opponentMotion_);
        if(phaseTick_>30&&opponentMotion_<80)++opponentMotion_;
        ++vsCounter_;
        vsChunk_=vsCounter_<=100?-1:vsCounter_<=130?int(vsCounter_)-60:70;
        ++phaseTick_;
    }
    ++sourceTick_;
}

const std::string& OriginalVsBanner::displayName(unsigned side) const {
    return displayNames_.at(side);
}

const std::string& OriginalVsBanner::displayBattleRecord(unsigned side) const {
    return displayBattleRecords_.at(side);
}

OriginalVsBattleRecordPlacement OriginalVsBanner::battleRecordPlacement(unsigned side) const {
    const auto& text=displayBattleRecords_.at(side);
    if(!setup_.showVersus||text.empty())return {};
    // Source name placements/motion are retained. The new network row uses
    // the original alphabet; its fitting and outline are host presentation,
    // not a claim about an unrecovered original statistics draw routine.
    const float naturalWidth=recordTextWidth(text)+4.f;
    const float scale=std::min(1.f,412.f/naturalWidth);
    const float rowWidth=naturalWidth*scale;
    const auto& name=names_[side];
    const bool onlineName=side?!setup_.opponentNameUtf8.empty():!setup_.localNameUtf8.empty();
    const float nameSize=onlineName&&!name.empty()?std::min(64.f,416.f/float(name.size())):64.f;
    const float nameTop=side?(!names_[2].empty()?344.f:312.f):104.f;
    return {(side?416.f:224.f)-rowWidth*.5f+nameX_[side],
        nameTop+nameSize+5.f+nameY_[side],rowWidth,18.f*scale,nameAlpha_[side]};
}

std::vector<std::uint16_t> OriginalVsBanner::encodeSource(std::span<const std::uint8_t> source) const {
    std::vector<std::uint16_t> out;
    for(std::size_t i=0;i+1<source.size()&&source[i];i+=2){
        const auto index=std::uint8_t(source[i]-160)*96u+std::uint8_t(source[i+1]-160);
        if(index>=nameIndices_.size())throw std::runtime_error("Original start name outside font mapping");
        out.push_back(std::uint16_t(index));
    }
    return out;
}

std::vector<std::uint16_t> OriginalVsBanner::encodeUtf8(const std::string& source) const {
    std::vector<std::uint16_t> out;
    auto lookup=[&](std::uint32_t code){
        if((code>='a'&&code<='z')||(code>=0xff41&&code<=0xff5a))code-=32;
        if(code==32)code=0x3000;
        else if(code>=33&&code<=126)code+=0xfee0;
        const auto it=std::find(nameUnicode_.begin(),nameUnicode_.end(),code);
        if(it==nameUnicode_.end())return nameUnicode_.size();
        const auto index=std::size_t(it-nameUnicode_.begin());
        // Rival-only Japanese keys are localized word fragments, not user
        // characters. Accept only the source name-entry character repertoire.
        const std::array<std::uint8_t,2> encoded{std::uint8_t(index/96+160),std::uint8_t(index%96+160)};
        return std::find(profileGlyphs_.begin(),profileGlyphs_.end(),encoded)!=profileGlyphs_.end()?
            index:nameUnicode_.size();
    };
    const auto question=lookup(0xff1f);
    for(std::size_t i=0;i<source.size()&&out.size()<32;){
        auto c=std::uint8_t(source[i++]);std::uint32_t code=c;unsigned continuation=0;
        if(c>=0xc2&&c<=0xdf){code=c&31;continuation=1;}
        else if(c>=0xe0&&c<=0xef){code=c&15;continuation=2;}
        else if(c>=0xf0&&c<=0xf4){code=c&7;continuation=3;}
        else if(c>=128)code=0xfffd;
        const unsigned length=continuation;
        for(unsigned n=0;n<continuation;++n){
            if(i>=source.size()||(std::uint8_t(source[i])&0xc0)!=0x80){code=0xfffd;break;}
            code=(code<<6)|(std::uint8_t(source[i++])&63);
        }
        if((length==1&&code<0x80)||(length==2&&code<0x800)||(length==3&&code<0x10000)||
           (code>=0xd800&&code<=0xdfff)||code>0x10ffff)code=0xfffd;
        if(code<32||code==127)continue;
        auto index=lookup(code);
        if(index>=nameIndices_.size()||(nameIndices_[index]==0xffff&&code!=32&&code!=0x3000))index=question;
        if(index<nameIndices_.size())out.push_back(std::uint16_t(index));
    }
    return out;
}

std::string OriginalVsBanner::decodeName(const std::vector<std::uint16_t>& name) const {
    std::string out;
    for(auto index:name){
        auto code=nameUnicode_[index];
        if(code>=0xff01&&code<=0xff5e)code-=0xfee0;
        if(code==0x3000)code=32;
        appendUtf8(out,code?code:'?');
    }
    return out;
}

std::vector<int> OriginalVsBanner::chunks() const {
    std::vector<int> out;
    if (setup_.drawBackdrop) out.push_back(backdropChunk);
    if(setup_.showBattleRecords&&!setup_.compactHeader){if(setup_.showVersus)out.push_back(restingVsChunk);return out;}
    if(setup_.customCourseName.empty())out.push_back(courseChunk[setup_.course]);
    if(setup_.compactHeader){}
    else if (setup_.extra) out.push_back(extraChunk);
    else if (setup_.race == 0) out.push_back(finalChunk);
    else { out.push_back(raceChunk); out.push_back(digitChunk[setup_.race - 1]); }
    if (setup_.drawDirection) out.push_back(directionChunk[setup_.direction]);
    out.push_back(dayNightChunk[setup_.night ? 1 : 0]);
    out.push_back(weatherChunk[setup_.snow || setup_.course==8 ? 2 : setup_.wet ? 1 : 0]);
    if(setup_.showVersus)out.push_back(restingVsChunk);
    return out;
}

std::vector<int> OriginalVsBanner::unsequencedZoomChunks() const {
    std::vector<int> out;
    for (int chunk = 1; chunk <= 70; ++chunk) out.push_back(chunk);
    return out;
}

unsigned OriginalVsBanner::headerCourse() const {
    // Imported tracks share one lettering baseline, independent of the course
    // whose handling they borrow. Irohazaka is the supplied D3 intro reference.
    return !setup_.customCourseName.empty()?5u:setup_.course;
}

std::string OriginalVsBanner::profileDisplayName(const original::OriginalBattleProfile& profile) const {
    if(!loaded_)return {};
    const auto length=std::min(profile.u(76),5u);
    if(!length)return decodeName(encodeSource(defaultName_));
    std::vector<std::uint16_t> glyphs;
    for(unsigned i=0;i<length;++i){const auto id=profile.u(44+4*i);const auto encoded=encodeSource(profileGlyphs_[id<=220?id:0]);glyphs.insert(glyphs.end(),encoded.begin(),encoded.end());}
    return decodeName(glyphs);
}
const char* OriginalVsBanner::rivalDisplayName(unsigned enemy){
    return enemy<std::size(localizedRivalNames)?localizedRivalNames[enemy]:"OPPONENT";
}

OriginalVsMetadataPlacement OriginalVsBanner::projectedMetadataPlacement(int chunk) const {
    const auto& bounds=chunkBounds_.at(std::size_t(chunk));
    OriginalVsMetadataPlacement out;out.chunk=chunk;
    // 14C380 -> 0DA440/0DA700 ->145A00/145C00. The bank scale
    // includes the original rational tangent and double arithmetic.
    // ARaceStandBy copies only the world pose into its default camera;
    // the identity pushes in14C380 remove that pose for this header.
    const float scale=sourceWord(0x3cd413ce),bankX=sourceWord(0xc04ccccc),bankY=sourceWord(0x40199999);
    float x,y;
    if(chunk==courseChunk[headerCourse()]){
        x=(bankX-.15f)*scale;y=(bankY+.06f+.65f)*scale;
    }else{
        x=bankX*scale;x+=sourceWord(courseAdvanceWords[headerCourse()])*scale;
        x+=-.8f*scale;x+=-.35f*scale;
        y=bankY*scale;y+=.06f*scale;y+=.67f*scale;
        const bool direction=std::find(std::begin(directionChunk),std::end(directionChunk),chunk)!=std::end(directionChunk);
        if(direction)x+=sourceWord(directionOffsetWords[setup_.direction])*scale;
        else{
            x+=sourceWord(directionAdvanceWords[setup_.direction])*scale;x+=-.85f*scale;
            if(std::find(std::begin(weatherChunk),std::end(weatherChunk),chunk)!=std::end(weatherChunk)){
                x+=sourceWord(dayAdvanceWords[setup_.night?1:0])*scale;x+=-.48f*scale;
            }
        }
    }
    const float depth=.15f-chunkDepths_.at(std::size_t(chunk));
    const float px=320.f*sourceWord(0x3fe7c3b5)/depth,py=240.f*sourceWord(0x401a8279)/depth;
    out.left=320.f+(bounds[0]*.01f*scale+x)*px;
    out.top=240.f-(-bounds[1]*.01f*scale+y)*py;
    out.width=(bounds[2]-bounds[0])*.01f*scale*px;
    out.height=(bounds[3]-bounds[1])*.01f*scale*py;
    return out;
}

OriginalVsMetadataPlacement OriginalVsBanner::sourceTitleInkPlacement() const {
    auto out=projectedMetadataPlacement(courseChunk[headerCourse()]);
    const auto& bounds=chunkBounds_[courseChunk[headerCourse()]];
    const auto& ink=titleInkBounds_[headerCourse()];
    const float scale=out.width/(bounds[2]-bounds[0]);
    out.left+=ink[0]*scale;out.top+=ink[1]*scale;
    out.width=ink[2]*scale;out.height=ink[3]*scale;
    return out;
}

OriginalVsMetadataPlacement OriginalVsBanner::importedTitlePlacement() const {
    if(setup_.customCourseName.empty())return {};
    auto out=sourceTitleInkPlacement();out.chunk=-1;
    const auto& image=importedTitles_.at(unsigned(std::find_if(importedCourseDefinitions.begin(),importedCourseDefinitions.end(),[&](const auto& course){return course.name==setup_.customCourseName;})-importedCourseDefinitions.begin()));
    out.width=out.height*float(image.width)/float(image.height);
    return out;
}

OriginalVsMetadataPlacement OriginalVsBanner::metadataPlacement(int chunk) const {
    const auto& bounds=chunkBounds_.at(std::size_t(chunk));
    OriginalVsMetadataPlacement out{chunk,20.f,8.f,bounds[2]-bounds[0],bounds[3]-bounds[1]};
    if(setup_.compactHeader){
        out=projectedMetadataPlacement(chunk);
        if(!setup_.customCourseName.empty()){
            const auto title=importedTitlePlacement();
            if(title.width>0)out.left+=title.width-sourceTitleInkPlacement().width;
        }
        return out;
    }
    // This is a host layout, not a claim about the original parent transforms.
    // Original word quads overlap if all receive one common translation.
    // Keep their complete source extents and leave the name row at y104 clear.
    if(chunk==raceChunk||chunk==finalChunk||chunk==extraChunk)out.left=310.f;
    else if(std::find(std::begin(digitChunk),std::end(digitChunk),chunk)!=std::end(digitChunk)){
        const auto& raceBounds=chunkBounds_.at(raceChunk);
        out.left=310.f+(raceBounds[2]-raceBounds[0])+8.f;
    }else if(chunk==dayNightChunk[0]||chunk==dayNightChunk[1]){
        out.left=310.f;out.top=44.f;
    }else if(std::find(std::begin(weatherChunk),std::end(weatherChunk),chunk)!=std::end(weatherChunk)){
        const auto& dayBounds=chunkBounds_.at(dayNightChunk[setup_.night?1:0]);
        out.left=310.f+(dayBounds[2]-dayBounds[0])+8.f;out.top=44.f;
    }else if(std::find(std::begin(directionChunk),std::end(directionChunk),chunk)!=std::end(directionChunk))
        out.top=74.f;
    return out;
}

std::vector<OriginalVsMetadataPlacement> OriginalVsBanner::metadataPlacements() const {
    std::vector<OriginalVsMetadataPlacement> out;
    for(int chunk:chunks())if(chunk!=backdropChunk&&chunk!=restingVsChunk)
        out.push_back(metadataPlacement(chunk));
    return out;
}

void OriginalVsBanner::paintChunk(std::span<std::uint32_t> target, int width, int height,
                                  int chunk) const {
    // Authored at a hundred units per pixel with y upward, fitted to whatever
    // canvas the caller paints on, exactly as the loading screen is.
    const float fit = std::min(float(width) / 640.f, float(height) / 480.f);
    SpritePlacement placement;
    placement.invertY = true;
    placement.authoredHeight = 0;
    placement.scale = unitsPerPixel * fit;
    placement.offsetX = (float(width) - 640.f * fit) * .5f;
    placement.offsetY = (float(height) - 480.f * fit) * .5f;
    // 14C380 translates the selected animated mark before drawing it.
    if(chunk>=41&&chunk<=70){placement.offsetX+=289.f*fit;placement.offsetY+=214.f*fit;}
    else if(chunk!=backdropChunk){
        const auto layout=metadataPlacement(chunk);
        const auto& bounds=chunkBounds_.at(std::size_t(chunk));
        const float scale=layout.width/(bounds[2]-bounds[0]);
        placement.scale*=scale;
        placement.offsetX+=(layout.left-bounds[0]*scale)*fit;
        placement.offsetY+=(layout.top-bounds[1]*scale)*fit;
    }
    placement.straightAlphaOverlay=!setup_.drawBackdrop;
    compositeOriginalMenuChunk(target, width, height, textures_,
                               model_.chunks.at(std::size_t(chunk)), placement);
}

void OriginalVsBanner::paintNames(std::span<std::uint32_t> target,int width,int height) const {
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    for(unsigned line=0;line<3;++line){
        const auto& name=names_[line];if(name.empty())continue;
        const unsigned side=line?1:0;
        const float center=side?416.f:224.f;
        const float y=line==0?104.f:!names_[2].empty()?(line==1?280.f:344.f):312.f;
        // Only online names can exceed the source's short name fields. Fit
        // their complete glyph sequence into this side's visible source area.
        const bool onlineName=side?!setup_.opponentNameUtf8.empty():!setup_.localNameUtf8.empty();
        const float size=onlineName?std::min(64.f,416.f/float(name.size())):64.f;
        float x=center-float(name.size())*size*.5f+nameX_[side];
        for(auto index:name){
            const auto glyph=nameIndices_[index];
            if(glyph!=0xffff&&nameAlpha_[side]>0){
                const float top=y+nameY_[side];
                OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{x,top,0,0,1,0xffffffff,0},
                    {x,top+size,0,0,0,0xffffffff,0},{x+size,top,0,1,1,0xffffffff,0},
                    {x+size,top+size,0,1,0,0xffffffff,0}};
                SpritePlacement placement;placement.scale=fit;
                placement.offsetX=(float(width)-640.f*fit)*.5f;placement.offsetY=(float(height)-480.f*fit)*.5f;
                placement.opacity=nameAlpha_[side];
                compositeOriginalSprite(target,width,height,nameFont_.at(glyph),sprite,placement);
            }
            x+=size;
        }
    }
}

void OriginalVsBanner::paintBattleRecords(std::span<std::uint32_t> target,int width,int height) const {
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const unsigned first=setup_.showBattleRecords&&setup_.showVersus?0:2,end=setup_.customCourseName.empty()?2:3;
    for(unsigned side=first;side<end;++side){
        const auto bounds=side==2?OriginalVsBattleRecordPlacement{20,8,180,32,1}:battleRecordPlacement(side);if(bounds.opacity<=0||bounds.width<=0)continue;
        if(side==2&&setup_.compactHeader&&(!setup_.customCourseName.empty())){
            const auto& title=importedTitles_.at(unsigned(std::find_if(importedCourseDefinitions.begin(),importedCourseDefinitions.end(),[&](const auto& course){return course.name==setup_.customCourseName;})-importedCourseDefinitions.begin()));
            const auto layout=importedTitlePlacement();
            compositeImage(target,width,height,title,(float(width)-640.f*fit)*.5f+layout.left*fit,
                (float(height)-480.f*fit)*.5f+layout.top*fit,layout.width*fit,layout.height*fit);
            continue;
        }
        const auto& text=side==2?setup_.customCourseName:displayBattleRecords_[side];
        const float scale=bounds.width/(recordTextWidth(text)+4.f);
        SpritePlacement p;p.scale=fit*scale;
        p.offsetX=(float(width)-640.f*fit)*.5f+bounds.left*fit;
        p.offsetY=(float(height)-480.f*fit)*.5f+bounds.top*fit;
        p.opacity=bounds.opacity;p.straightAlphaOverlay=!setup_.drawBackdrop;
        float advance=0;
        for(char c:text){
            const auto at=std::find(recordAlphabet::fontCharacters.begin(),recordAlphabet::fontCharacters.end(),c);
            const auto index=std::size_t(at-recordAlphabet::fontCharacters.begin());
            if(c==' '){advance+=recordAlphabet::word(recordAlphabet::fontWidthWords[index]);continue;}
            // The source atlas is vertically inverted; white ink occupies
            // rows13..29 after projection. Offset it onto the visible row.
            const auto glyph=[&](float dx,float dy,std::uint32_t ink){
                constexpr float shear=5.f;OriginalSprite sprite;
                const float x=advance+dx,y=-13.f+dy;
                sprite.vertices={OriginalSpriteVertex{x+shear,y,0,0,1,ink,0},
                    {x,y+32,0,0,0,ink,0},{x+32+shear,y,0,1,1,ink,0},
                    {x+32,y+32,0,1,0,ink,0}};
                compositeOriginalSprite(target,width,height,recordFont_.at(unsigned(index)),sprite,p);
            };
            // Dark outline stays readable against headlights and car paint;
            // all passes share the corresponding source name alpha/motion.
            for(const auto offset:std::array<std::array<float,2>,8>{{{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}})
                glyph(offset[0],offset[1],0xff101020);
            glyph(0,0,0xffffffff);
            advance+=recordAlphabet::word(recordAlphabet::fontWidthWords[index]);
        }
    }
}

void OriginalVsBanner::paint(std::span<std::uint32_t> target, int width, int height) const {
    if (!loaded_) throw std::logic_error("Start banner painted before it was loaded");
    if (width <= 0 || height <= 0 || target.size() != std::size_t(width) * height)
        throw std::runtime_error("Invalid start banner destination");
    if(setup_.compactHeader){
        static const NativeImage black{1,1,{0xff000000u}};
        const float fit=std::min(float(width)/640.f,float(height)/480.f);
        compositeImage(target,width,height,black,0,(height-480.f*fit)*.5f,float(width),55.f*fit);
        compositeImage(target,width,height,black,0,(height+480.f*fit)*.5f-55.f*fit,float(width),55.f*fit);
    }
    for (const auto chunk : chunks()) {
        if(chunk==restingVsChunk){if(activeVsChunk()>=0)paintChunk(target,width,height,activeVsChunk());}
        else paintChunk(target, width, height, chunk);
    }
    if(setup_.showVersus)paintNames(target,width,height);
    if((setup_.showBattleRecords&&setup_.showVersus)||!setup_.customCourseName.empty())paintBattleRecords(target,width,height);
}
}
