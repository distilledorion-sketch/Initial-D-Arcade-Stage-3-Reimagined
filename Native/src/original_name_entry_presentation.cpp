#include "original_name_entry_presentation.h"
#include "original_math.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float f(std::uint32_t word){return std::bit_cast<float>(word);}
unsigned bankIndex(unsigned bank){if(bank==8)return 0;if(bank==16)return 1;if(bank==32)return 2;if(bank==0)return 3;throw std::out_of_range("Name-entry bank");}
bool additiveTsp(std::uint32_t tsp){const auto source=tsp>>29,destination=(tsp>>26)&7;return(source==1&&destination==1)||(source==4&&destination==6);}
}
OriginalNameEntryPresentation OriginalNameEntryPresentation::load(const std::filesystem::path& root){
    OriginalNameEntryPresentation out;
    const auto base=root/"data/original_assets";
    const std::array<std::filesystem::path,4> paths{base/"menus/v3/v3sS00common",base/"name_entry/v3sS08name",base/"name_entry/select0302",base/"name_entry/select"};
    const std::array<const char*,4> names{"v3sS00common","v3sS08name","select0302","select"};
    for(unsigned i=0;i<4;++i){out.models_[i]=NativeModel::load(paths[i]/(std::string(names[i])+".idasmesh"));out.textures_[i]=NativeTextureBank::load(paths[i]/"textures/textures.idastex");}
    std::ifstream input(base/"name_entry/cursor.bin",std::ios::binary);input.read(reinterpret_cast<char*>(out.positions_.data()),sizeof(out.positions_));
    if(!input||input.peek()!=std::char_traits<char>::eof()||out.models_[1].chunks.size()!=7||out.models_[2].chunks.size()!=13||out.textures_[2].size()!=25)throw std::runtime_error("Original name-entry assets incomplete");
    for(const auto& row:out.positions_)for(float value:row)if(!std::isfinite(value))throw std::runtime_error("Nonfinite original name cursor");
    out.reset();return out;
}
void OriginalNameEntryPresentation::reset(){
    previousInput_=previousOutput_=cursor_={f(0x3ecccccdu),f(0xbf8a3d71u)};pulse_=1;alpha_=255;frames_=0;showroom_.reset();
}
void OriginalNameEntryPresentation::advance(const original::OriginalNameEntryState& state){
    //1B146C clamps the index to0..62;1AE520(k=3,wrap=0) gives a=-2,b=.25.
    const auto selected=std::int32_t(state.selected460)<0?62u:state.selected460>62?0u:state.selected460;
    for(unsigned axis=0;axis<2;++axis){
        const float target=positions_[selected][axis];
        float value=previousInput_[axis]+target;
        const float feedback=-2.f*previousOutput_[axis];value-=feedback;value*=.25f;
        previousInput_[axis]=target;previousOutput_[axis]=value;cursor_[axis]=value;
    }
    cursor_[0]=std::max(cursor_[0],f(0x3eae147b));cursor_[1]=std::min(cursor_[1],f(0xbf8a3d71));
    if(state.length528<5){
        const float cosine=original::originalCosF32(pulse_);const float sum=1.f+cosine;
        const float half=.5f*sum;const float intensity=255.f*half;
        alpha_=unsigned(std::int32_t(intensity));pulse_+=f(0x3dcccccd);
    }
    showroom_.advanceTicks();++frames_;
}
std::vector<OriginalNameEntryDraw> OriginalNameEntryPresentation::drawList(const original::OriginalNameEntryState& state)const{
    if(state.page600>2||state.length528>5)throw std::out_of_range("Name-entry presentation state");
    std::vector<OriginalNameEntryDraw> out;
    const auto draw=[&](unsigned bank,unsigned chunk,float x=0,float y=0,float z=0,float scale=1,int glyph=-1,int alpha=-1){out.push_back({bank,chunk,x,y,z,scale,glyph,alpha});};
    // Common parent child1B1CA0, then the name panel child1B1300.
    draw(8,54);draw(8,46);draw(16,6);draw(16,5);
    draw(32,std::array<unsigned,3>{6,9,11}[state.page600],f(0x3e52f1aa),2.f,f(0x3dcccccd),f(0x3f6b851f));
    draw(16,4,cursor_[0]+f(0x3e4ccccc),cursor_[1]+f(0xbe4ccccc),f(0x3de147ae));
    float x=f(0x3d75c28f);
    for(unsigned i=0;i<5;++i){x+=f(0x3f866666);draw(16,i==state.length528?0:i>state.length528?2:1,x,f(0xc07851eb),0,1,-1,i==state.length528?int(alpha_):-1);}
    x=f(0xbfdeb852);
    for(unsigned i=0;i<5;++i){x+=f(0x3f866666);if(i<state.length528){const auto glyph=state.glyphIds480[i];if(glyph>220)throw std::out_of_range("Name glyph atlas");const unsigned atlas=glyph<=63?0:glyph<=127?1:glyph<=191?2:glyph<=207?3:4;draw(32,atlas,x,f(0xbfcccccc),0,1,int(glyph));}}
    draw(16,3,0,0,f(0xbc23d70a));return out;
}
NativeModelChunk OriginalNameEntryPresentation::materialize(const OriginalNameEntryDraw& draw)const{
    const unsigned bank=bankIndex(draw.bank);auto chunk=models_[bank].chunks.at(draw.chunk);
    for(auto& batch:chunk.batches){
        if(draw.pulseAlpha>=0){batch.ich[2]=(batch.ich[2]&0x03ffffffu)|0x981000c0u;batch.material[3]=(batch.material[3]&0xffffffu)|(unsigned(draw.pulseAlpha)<<24);}
        for(auto& vertex:batch.vertices){
            vertex.position.x=vertex.position.x*draw.scale+draw.x;vertex.position.y=vertex.position.y*draw.scale+draw.y;vertex.position.z+=draw.z;
        }
        if(draw.glyph>=0){
            //1B1820 deliberately passes full glyph IDs, relying on authored wrap.
            const unsigned cells=draw.chunk<=2?8:4;const float step=1.f/cells;
            const float u=float(unsigned(draw.glyph)%cells)*step,v=1.f-float(unsigned(draw.glyph)/cells)*step;
            if(batch.vertices.size()!=4)throw std::runtime_error("Original name glyph quad changed");
            const std::array<float,4> us{u,u+step,u,u+step},vs{v,v,v-step,v-step};
            //0D1E60 builds the modifier's2x2 index from each authored UV:
            //trunc(U)+2*trunc(-V). The supplied glyph quads consequently map
            //vertex order to[1,3,0,2], not the UV array's[0,1,2,3] order.
            //0D19A0 uses that index for both coordinate arrays.
            for(auto& vertex:batch.vertices){
                const int column=int(vertex.u),row=int(-vertex.v);
                if(column<0||column>1||row<0||row>1)throw std::runtime_error("Original name glyph UV grid changed");
                const unsigned corner=unsigned(column+2*row);
                vertex.u=us[corner];vertex.v=vs[corner];
            }
        }
    }
    return chunk;
}
std::vector<OriginalNameEntryDraw> OriginalNameEntryPresentation::timerDraws(std::uint32_t sharedCountdown)const{
    //1BABC0 timer member, constructed by1BA4E0 and fed quotient(countdown,60).
    //12847C uses the compiler unsigned quotient helper2223B8.
    const unsigned seconds=std::min(sharedCountdown/60,99u);
    float x=f(0x40a66666);const float y=f(0xbe23d70a),z=f(0x3a83126f);
    std::vector<OriginalNameEntryDraw> draws;
    if(seconds>=10){draws.push_back({8,seconds/10,x,y,z});x+=f(0x3ef5c28f);}else x+=f(0x3e75c28f);
    draws.push_back({8,seconds%10,x,y,z});return draws;
}
void OriginalNameEntryPresentation::paintDraws(std::span<std::uint32_t> canvas,int width,int height,std::vector<OriginalNameEntryDraw> draws,bool additive)const{
    if(width<=0||height<=0||canvas.size()!=std::size_t(width)*height)throw std::invalid_argument("Name-entry canvas dimensions");
    const float fit=std::min(float(width)/640,float(height)/480),ox=(width-640*fit)*.5f,oy=(height-480*fit)*.5f;
    struct Layer{unsigned bank;NativeModelChunk chunk;float depth;};std::vector<Layer> layers;
    for(const auto& draw:draws){auto chunk=materialize(draw);for(auto& batch:chunk.batches){
        if(additiveTsp(batch.ich[2])!=additive)continue;
        float z=0;for(const auto& vertex:batch.vertices)z+=vertex.position.z;z/=float(batch.vertices.size());
        NativeModelChunk part;part.batches.push_back(std::move(batch));layers.push_back({bankIndex(draw.bank),std::move(part),z});
    }}
    // Positive source UI Z is nearer. Preserve submission order at equal Z.
    std::stable_sort(layers.begin(),layers.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    SpritePlacement placement;placement.scale=100*fit;placement.offsetX=ox;placement.offsetY=oy;placement.invertY=true;placement.authoredHeight=0;placement.straightAlphaOverlay=!additive;
    for(const auto& layer:layers)compositeOriginalMenuChunk(canvas,width,height,textures_[layer.bank],layer.chunk,placement);
}
void OriginalNameEntryPresentation::paintBackground(std::span<std::uint32_t> canvas,int width,int height)const{
    paintDraws(canvas,width,height,{{8,54},{8,46}},false);
}
std::vector<OriginalNameEntryDraw> OriginalNameEntryPresentation::legacyDraws(const original::OriginalNameEntryState& state)const{
    if(state.profileKind1192!=2)return{};
    //128676..128684 offsets the legacy default center(-3.2,2.4,-.15).
    // Retain the shared native100px/unit legacy-model projection boundary.
    std::vector<OriginalNameEntryDraw> draws{{0,22,f(0x40accccd),-f(0x3f666666)}};
    if(state.frame572%20<=9)draws.push_back({0,23,f(0x40accccd),-f(0x3f666666)});
    return draws;
}
void OriginalNameEntryPresentation::paintNameBacking(std::span<std::uint32_t> canvas,int width,int height)const{
    paintDraws(canvas,width,height,{{16,3,0,0,f(0xbc23d70a)}},false);
}
void OriginalNameEntryPresentation::paint(std::span<std::uint32_t> canvas,int width,int height,const original::OriginalNameEntryState& state,std::uint32_t sharedCountdown)const{
    auto draws=drawList(state);std::erase_if(draws,[](const auto& d){return d.bank==8||(d.bank==16&&d.chunk==3);});
    const auto digits=timerDraws(sharedCountdown);draws.insert(draws.end(),digits.begin(),digits.end());
    paintDraws(canvas,width,height,std::move(draws),false);
    // Although128500 submits fade before1BBCC0, its source Z=-.011 is nearer
    // than the UI and car. Apply originalNameEntryFadeArgb after all passes.
}
void OriginalNameEntryPresentation::paintGlow(std::span<std::uint32_t> canvas,int width,int height,const original::OriginalNameEntryState& state)const{
    auto draws=drawList(state);std::erase_if(draws,[](const auto& d){return d.bank!=16||d.chunk>2;});
    // Source SRC_ALPHA + DST_ALPHA over zero produces alpha-weighted RGB.
    // The separate ONE+ONE renderer pass adds this to the opaque destination.
    paintDraws(canvas,width,height,std::move(draws),true);
}
void OriginalNameEntryPresentation::paintCursor(std::span<std::uint32_t> canvas,int width,int height,const original::OriginalNameEntryState& state)const{
    auto draws=drawList(state);std::erase_if(draws,[](const auto& d){return d.bank!=16||d.chunk!=4;});
    paintDraws(canvas,width,height,std::move(draws),true);
}
void OriginalNameEntryPresentation::paintLegacy(std::span<std::uint32_t> canvas,int width,int height,const original::OriginalNameEntryState& state)const{
    paintDraws(canvas,width,height,legacyDraws(state),false);
}
}
