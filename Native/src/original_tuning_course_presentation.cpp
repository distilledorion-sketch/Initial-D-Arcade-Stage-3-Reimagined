#include "unity_ui_capture.h"
#include "original_tuning_course_presentation.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float f(unsigned word){return std::bit_cast<float>(word);}
unsigned bankIndex(unsigned bank){if(bank==8)return 0;if(bank==15)return 1;if(bank==3)return 2;throw std::out_of_range("Tuning screen bank");}
bool additive(unsigned tsp){return(tsp>>29==1&&((tsp>>26)&7)==1)||(tsp>>29==4&&((tsp>>26)&7)==6);}
template<class T>void read(std::ifstream& file,T& value){file.read(reinterpret_cast<char*>(&value),sizeof(value));if(!file)throw std::runtime_error("Truncated tuning screen data");}
}
struct OriginalTuningCoursePresentation::Impl {
    struct Package {std::array<std::uint8_t,16> flags{};std::array<unsigned,3> parts{};};
    std::array<NativeModel,3> models;std::array<NativeTextureBank,3> textures;
    std::array<std::vector<Package>,35> packages;
    std::array<float,25> cursorX{};std::array<unsigned,42> partSelectors{};
    std::array<unsigned,31> faceSelectors{},nameSelectors{};
    std::array<int,35> themes{};std::array<float,25> sliderPositions{};
    std::array<float,128> cursorWave{};
    std::array<unsigned,4096> pulse{};
    OriginalShowroom showroom;std::uint64_t ticks=0;unsigned kind=0,count=3,drawFrame=0;
    float sliderX=.5f,sliderScale=1;
    NativeModelChunk materialize(const OriginalTuningCourseDraw& d)const {
        auto c=models[bankIndex(d.bank)].chunks.at(d.chunk);
        unsigned vertexNumber=0;
        for(auto& b:c.batches){
            if(d.labelSelection>=0){
                b.material[2]=0x600;
                for(auto& v:b.vertices){v.color0=d.labelSelection?0xffffffffu:(vertexNumber&1)?0xffccccccu:0xff000000u;v.color1=0;++vertexNumber;}
            }
            if(d.labelSelection==-2){
                //1B8240 with the constructor's four {white,zero} colors.
                b.material[2]=0x600;for(auto& v:b.vertices){v.color0=0xffffffff;v.color1=0;}
            }
            if(d.labelSelection==-3){
                const auto q=pulse.at(drawFrame);b.ich[2]|=0x001000c0;b.ich[4]|=0x001000c0;
                b.material[3]=q;b.material[5]=q;
            }
            for(auto& v:b.vertices){v.position.x=v.position.x*d.scaleX+d.x;v.position.y=v.position.y*d.scaleY+d.y;v.position.z+=d.z;}
        }
        return c;
    }
    void paint(std::span<unsigned> canvas,int width,int height,const NativeModelChunk& c,unsigned bank,bool blend)const{
        if(width<=0||height<=0||canvas.size()!=std::size_t(width)*height)throw std::invalid_argument("Tuning canvas dimensions");
        const float fit=std::min(float(width)/640,float(height)/480);
        SpritePlacement p;p.scale=100*fit;p.offsetX=(width-640*fit)*.5f;p.offsetY=(height-480*fit)*.5f;p.invertY=true;p.authoredHeight=0;p.straightAlphaOverlay=!blend;
        compositeOriginalMenuChunk(canvas,width,height,textures[bankIndex(bank)],c,p);
    }
};
OriginalTuningCoursePresentation::OriginalTuningCoursePresentation():impl_(std::make_unique<Impl>()){}
OriginalTuningCoursePresentation::~OriginalTuningCoursePresentation()=default;
OriginalTuningCoursePresentation::OriginalTuningCoursePresentation(OriginalTuningCoursePresentation&&) noexcept=default;
OriginalTuningCoursePresentation& OriginalTuningCoursePresentation::operator=(OriginalTuningCoursePresentation&&) noexcept=default;
OriginalTuningCoursePresentation::OriginalTuningCoursePresentation(const OriginalTuningCoursePresentation& other):impl_(std::make_unique<Impl>(*other.impl_)){}
OriginalTuningCoursePresentation& OriginalTuningCoursePresentation::operator=(const OriginalTuningCoursePresentation& other){if(this!=&other)impl_=std::make_unique<Impl>(*other.impl_);return *this;}
OriginalTuningCoursePresentation OriginalTuningCoursePresentation::load(const std::filesystem::path& root){
    OriginalTuningCoursePresentation out;auto& p=*out.impl_;const auto base=root/"data/original_assets";
    const std::array<const char*,3> names{"v3sS00common","v3sS07tune","v3sK00rivalface"};
    for(unsigned i=0;i<3;++i){const auto path=i?base/"tuning_course"/names[i]:base/"menus/v3"/names[i];p.models[i]=NativeModel::load(path/(std::string(names[i])+".idasmesh"));p.textures[i]=NativeTextureBank::load(path/"textures/textures.idastex");}
    std::ifstream tables(base/"tuning_course/tables.bin",std::ios::binary);read(tables,p.cursorX);read(tables,p.partSelectors);read(tables,p.faceSelectors);read(tables,p.nameSelectors);read(tables,p.themes);read(tables,p.sliderPositions);if(tables.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected tuning table data");
    std::ifstream packages(base/"tuning_course/packages.bin",std::ios::binary);for(auto& car:p.packages){unsigned count;read(packages,count);if((count!=1&&count<3)||count>5)throw std::runtime_error("Original tuning package count");car.resize(count);for(auto& row:car){read(packages,row.flags);read(packages,row.parts);for(auto part:row.parts)if(part!=~0u&&part>=42)throw std::runtime_error("Original tuning part selector");}}
    if(packages.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected tuning package data");
    std::ifstream pulse(base/"tuning_course/pulse.bin",std::ios::binary);read(pulse,p.pulse);if(pulse.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected tuning pulse data");
    std::ifstream wave(base/"tuning_course/cursor_scale.bin",std::ios::binary);read(wave,p.cursorWave);if(wave.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected cursor wave data");
    if(p.models[1].chunks.size()!=73||p.models[2].chunks.size()!=62)throw std::runtime_error("Original tuning assets changed");return out;
}
void OriginalTuningCoursePresentation::reset(const original::OriginalTuningCourseMenu& state,const original::OriginalBattleProfile& profile){impl_->showroom.reset();impl_->ticks=0;impl_->drawFrame=0;impl_->kind=profile.byte(1192);impl_->count=state.count504;impl_->sliderX=.5f;impl_->sliderScale=1;}
void OriginalTuningCoursePresentation::advance(const original::OriginalTuningCourseMenu& state,const original::OriginalBattleProfile& profile,float wheelPosition){
    impl_->kind=profile.byte(1192);impl_->count=state.count504;impl_->drawFrame=unsigned(impl_->ticks);
    if(impl_->drawFrame>=impl_->pulse.size())throw std::out_of_range("Original tuning visit exceeds source timer bound");
    const auto base=(state.count504-1)*5;const float first=impl_->sliderPositions.at(base)*f(0x3c23d70a),last=impl_->sliderPositions.at(base+state.count504-1)*f(0x3c23d70a);
    float range=last-first;range*=wheelPosition-.5f;
    float target=impl_->sliderPositions.at(base+state.selected496)*f(0x3c23d70a);target=std::fma(range,f(0x3dcccccd),target);
    impl_->sliderX=std::fma(target,f(0x3e4ccccd),impl_->sliderX*f(0x3f4ccccd));
    impl_->sliderScale=std::fma(impl_->cursorWave[impl_->drawFrame%128],f(0x3e4ccccd),1.f);
    impl_->showroom.advanceTicks();++impl_->ticks;
}
OriginalShowroomFrame OriginalTuningCoursePresentation::showroomFrame(unsigned car)const{return impl_->showroom.frame(car);}
unsigned OriginalTuningCoursePresentation::selectorPhase()const{return impl_->count==3?2:impl_->count==4?3:4;}
std::uint64_t OriginalTuningCoursePresentation::frames()const{return impl_->ticks;}
std::vector<OriginalTuningCourseDraw> OriginalTuningCoursePresentation::drawList(const original::OriginalTuningCourseMenu& state,unsigned countdown)const{
    const auto& p=*impl_;if(state.car484>=35||(state.count504!=1&&state.count504<3)||state.count504>5||state.selected496>=state.count504||p.packages[state.car484].size()!=state.count504)throw std::out_of_range("Tuning screen state");
    const auto& package=p.packages[state.car484][state.selected496];std::vector<OriginalTuningCourseDraw> out;
    const auto draw=[&](unsigned bank,unsigned chunk,float x=0,float y=0,float z=0,float sx=1,float sy=1,int colors=-1){out.push_back({bank,chunk,x,y,z,sx,sy,colors});};
    //1B8FE0 draws the registered1BA2A0 selector child first.
    draw(8,52,p.sliderX,f(0xbfd70a3d),f(0x3a83126f),p.sliderScale,.5f);
    if(p.kind==1||p.kind==2){draw(8,p.kind==1?21:53,0,0,0,1,1,-3);draw(8,23,0,0,0,1,1,-3);}
    draw(15,1);draw(15,0);draw(15,7);
    const float confirm=state.phase464==2?std::clamp(float(state.confirmationFrames460)/36.f,0.f,1.f):0;
    const bool showHighlight=confirm<f(0x3c23d70a)||(p.drawFrame&6)!=0;
    float x=state.count504<=3?f(0x3fe66666):state.count504==4?f(0x3fb33333):1.f;
    const float step=state.count504==3?f(0x3fb33333):state.count504==4?f(0x3f999999):f(0x3f8ccccd);
    for(unsigned i=0;i<state.count504;++i){const auto chunk=i==state.count504-1?6:i+2;const bool selected=i==state.selected496;
        if(selected&&state.count504>1&&showHighlight)draw(15,chunk+66,x,f(0xbfa8f5c2));
        const float scale=selected&&state.count504>1?1.f:f(0x3f4ccccd);draw(15,chunk,x,f(0xbfa8f5c2),0,scale,scale,selected?1:0);x+=step;
    }
    // Main12B386 supplies a rival/theme card for package A and two special Bs.
    int theme=state.selected496==0?(state.car484==12?-1:p.themes[state.car484]):state.car484==22&&state.selected496==1?24:state.car484==15&&state.selected496==1?20:-1;
    if(theme>30)theme=30;
    if(theme>=0&&p.nameSelectors[theme]!=~0u){draw(3,1);draw(3,3,f(0xbf9ae148),0,f(0x3a83126f));draw(3,p.nameSelectors[theme]&65535,f(0x3ed70a3d),0,f(0x3a83126f));draw(3,p.faceSelectors[theme]&65535,f(0x3fa28f5c),f(0xc00eb852),f(0x3a83126f),f(0x3f4ccccd),f(0x3f4ccccd),-2);}
    const bool empty=package.parts[0]==~0u&&package.parts[1]==~0u&&package.parts[2]==~0u;
    x=f(0x3f999999);
    if(empty)draw(15,50,x,f(0xc088f5c2));else for(auto part:package.parts){if(part!=~0u)draw(15,p.partSelectors[part]&65535,x,f(0xc088f5c2));x+=f(0x3fa8f5c2);}
    if(empty)draw(15,51,f(0x408ccccd),f(0xc068f5c2));else{draw(15,67,f(0x408ccccd),f(0xc068f5c2));for(unsigned i=0;i<16;++i)if(i!=14&&package.flags[i])draw(15,52+(i==15?14:i),f(0x408ccccd),f(0xc068f5c2));}
    draw(8,52,p.cursorX[(state.count504-1)*5+state.selected496]*f(0x3c23d70a),f(0xbfa8f5c2),f(0xbdcccccd),1.5f,1);
    const auto seconds=std::min(countdown/60,99u);x=f(0x40a66666);if(seconds>=10){draw(8,seconds/10,x,f(0xbe23d70a),f(0x3a83126f));x+=f(0x3ef5c28f);}else x+=f(0x3e75c28f);draw(8,seconds%10,x,f(0xbe23d70a),f(0x3a83126f));
    return out;
}
void OriginalTuningCoursePresentation::paintBackground(std::span<unsigned> canvas,int width,int height)const{for(unsigned chunk:{54u,46u})impl_->paint(canvas,width,height,impl_->materialize({8,chunk}),8,false);}
std::vector<OriginalTuningCourseOverlay> OriginalTuningCoursePresentation::overlays(int width,int height,const original::OriginalTuningCourseMenu& state,unsigned countdown)const{
    if(width<=0||height<=0)throw std::invalid_argument("Tuning canvas dimensions");
    struct Layer{unsigned bank;NativeModelChunk chunk;float depth;bool blend;};std::vector<Layer> layers;
    for(const auto& d:drawList(state,countdown)){auto chunk=impl_->materialize(d);for(auto& batch:chunk.batches){float depth=0;for(const auto& v:batch.vertices)depth+=v.position.z;depth/=float(batch.vertices.size());const bool blend=additive(batch.ich[2]);NativeModelChunk part;part.batches.push_back(std::move(batch));layers.push_back({d.bank,std::move(part),depth,blend});}}
    std::stable_sort(layers.begin(),layers.end(),[](auto&a,auto&b){return a.depth<b.depth;});std::vector<OriginalTuningCourseOverlay> out;
    for(const auto& layer:layers){if(out.empty()||out.back().additive!=layer.blend){out.push_back({std::vector<unsigned>(std::size_t(width)*height),layer.blend});unityUiClear(out.back().pixels.data(),width,height);}impl_->paint(out.back().pixels,width,height,layer.chunk,layer.bank,layer.blend);}return out;
}
void OriginalTuningCoursePresentation::paintCanvas(std::span<unsigned> canvas,int width,int height,const original::OriginalTuningCourseMenu& state,unsigned countdown)const{
    paintBackground(canvas,width,height);for(const auto& pass:overlays(width,height,state,countdown)){if(unityUiEnabled()){unityUiCopy(canvas.data(),pass.pixels.data(),width,height,1,1,0,0,true,pass.additive);continue;}for(unsigned i=0;i<canvas.size();++i){const auto a=pass.pixels[i]>>24;unsigned pixel=0xff000000;for(unsigned shift:{0u,8u,16u}){const auto src=(pass.pixels[i]>>shift)&255,dst=(canvas[i]>>shift)&255;pixel|=(pass.additive?std::min(255u,src+dst):(src*a+dst*(255-a)+127)/255)<<shift;}canvas[i]=pixel;}}
}
}
