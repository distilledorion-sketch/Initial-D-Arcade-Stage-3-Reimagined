#include "original_hud.h"
#include "original_countdown_presentation.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float lit(std::uint32_t bits){return std::bit_cast<float>(bits);}
int ftrc(float value){if(std::isnan(value)||value<=-2147483648.f)return INT32_MIN;if(value>=2147483648.f)return INT32_MAX;return int(value);}
void scaleXY(original::OriginalMatrix& m,float value){for(unsigned row=0;row<4;row++){m.elements[row]*=value;m.elements[4+row]*=value;}}
}
unsigned originalTachTypeForCar(unsigned car,std::uint8_t tuning,std::uint8_t selection){
    constexpr std::array<unsigned,35> types{0,0,0,1,0,1,1,2,2,1,0,0,1,1,1,2,2,2,2,1,1,1,1,1,0,0,0,1,1,1,3,1,1,1,2};
    if(car>=types.size())throw std::out_of_range("Original HUD car ID");
    // MOV.B sign-extends before the original unsigned CMP/HI instructions.
    const auto a=std::uint32_t(std::int32_t(std::int8_t(tuning))),b=std::uint32_t(std::int32_t(std::int8_t(selection)));
    return car==0&&a>4&&b<=2?3:types[car];
}
float originalTachDisplayRpm(float rpm,unsigned tachType){
    if(tachType>3)throw std::out_of_range("Original HUD tach type");
    if(!std::isfinite(rpm))return 0.f;
    const float maximum=tachType==3?12000.f:float(tachType+8)*1000.f;
    return std::clamp(rpm,0.f,maximum);
}
OriginalRaceHud OriginalRaceHud::load(const std::filesystem::path& root){
    OriginalRaceHud out;
    const auto poly=root/"data/original_assets/hud/game2d",spr=root/"data/original_assets/menus/game2d";
    out.model_=NativeModel::load(poly/"game2d.idasmesh");
    out.modelTextures_=NativeTextureBank::load(poly/"textures/textures.idastex");
    out.sprites_=NativeSpriteBank::load(spr/"original_rip.tbl",spr/"original_rip.bin");
    out.spriteTextures_=NativeTextureBank::load(spr/"textures/textures.idastex");
    out.trig_=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    if(out.model_.chunks.size()!=212||out.modelTextures_.size()!=76||out.sprites_.sprites.size()!=15||out.spriteTextures_.size()!=41)throw std::runtime_error("Original race HUD bank identity mismatch");
    for(const auto& chunk:out.model_.chunks){
        std::size_t count=0;for(const auto& batch:chunk.batches)count+=batch.vertices.size();
        out.maxChunkVertices_=std::max(out.maxChunkVertices_,count);
    }
    // Center visible atlas texels, not the asymmetric transparent padding.
    // This read-only UV scan runs once and does not enter the UI compositor.
    for(unsigned index=0;index<4;++index){
        const auto& chunk=out.model_.chunks.at(index);
        float minX=INFINITY,maxX=-INFINITY,minY=INFINITY,maxY=-INFINITY;
        for(const auto& batch:chunk.batches){
            const auto& image=out.modelTextures_.at(batch.material[9]);
            for(std::size_t i=0;i<batch.indices.size();i+=3){
                const auto& a=batch.vertices[batch.indices[i]];
                const auto& b=batch.vertices[batch.indices[i+1]];
                const auto& c=batch.vertices[batch.indices[i+2]];
                const float det=(b.v-c.v)*(a.u-c.u)+(c.u-b.u)*(a.v-c.v);
                if(std::abs(det)<1e-9f)continue;
                for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
                    if((image.argb[std::size_t(y)*image.width+x]>>24)<=16)continue;
                    // Authored countdown UVs wrap vertically through -1..0.
                    const float u=(x+.5f)/image.width+std::floor(std::min({a.u,b.u,c.u}));
                    const float v=(y+.5f)/image.height+std::floor(std::min({a.v,b.v,c.v}));
                    const float wa=((b.v-c.v)*(u-c.u)+(c.u-b.u)*(v-c.v))/det;
                    const float wb=((c.v-a.v)*(u-c.u)+(a.u-c.u)*(v-c.v))/det;
                    const float wc=1-wa-wb;
                    if(wa<0||wb<0||wc<0)continue;
                    const auto p=a.position*wa+b.position*wb+c.position*wc;
                    minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
                    minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
                }
            }
        }
        if(!std::isfinite(minX))throw std::runtime_error("Original countdown texture contains no visible ink");
        out.startSignalCenters_[index]={(minX+maxX)*.5f,(minY+maxY)*.5f,0};
    }
    {
        auto& chunk=out.model_.chunks.at(191);
        // Sort once on load: the red needle sits in front of its hub/shadow.
        const auto depth=[](const NativeModelBatch& b){float z=0;for(const auto& v:b.vertices)z+=v.position.z;return b.vertices.empty()?0.f:z/float(b.vertices.size());};
        std::stable_sort(chunk.batches.begin(),chunk.batches.end(),[&](const auto& a,const auto& b){return depth(a)<depth(b);});
    }
    return out;
}
std::vector<OriginalHudDraw> OriginalRaceHud::drawList(const OriginalHudState& s)const{
    if(s.tachType>3||s.gear>6||s.sectionCapacity>4||s.sectionCount>s.sectionCapacity||!std::isfinite(s.speedKmh)||!std::isfinite(s.rpm))throw std::invalid_argument("Original HUD state out of supported bounds");
    std::vector<OriginalHudDraw> out;
    const auto identity=original::originalIdentityMatrix();
    const auto polygon=[&](unsigned index,const original::OriginalMatrix& matrix){out.push_back({OriginalHudDraw::Kind::polygon,index,0xffffffff,matrix});};
    const auto sprite=[&](unsigned index,unsigned texture=0xffffffff){out.push_back({OriginalHudDraw::Kind::sprite,index,texture,identity});};
    // While the announcement is up the instrument groups are not drawn.
    if(s.finishBanner!=OriginalHudState::FinishBanner::none){
        const unsigned chunk=s.finishBanner==OriginalHudState::FinishBanner::finish?182u
            :s.finishBanner==OriginalHudState::FinishBanner::timeUp?183u
            :s.finishBanner==OriginalHudState::FinishBanner::win?184u:185u;
        polygon(chunk,identity);
        return out;
    }
    // The source normal-layout panel and its settled slide fields (+D0/+D4=0).
    // Alternate-layout's separate animated timers remain independently scoped.
    const bool panel=s.timePanel&&!s.alternateLayout;
    const auto timeDigits=[&](original::OriginalMatrix& m,std::uint32_t t){
        const std::array<unsigned,7> digit{(t/3600000)%6,(t/360000)%10,(t/60000)%6,(t/6000)%10,(t/600)%10,(t/60)%10,(t/6)%10};
        original::translateOriginalMatrix(m,{lit(0x3e19999a),0,0});polygon(75,m);
        original::translateOriginalMatrix(m,{lit(0xbe19999a),0,0});if(digit[0])polygon(77+digit[0],m);
        constexpr std::array<std::uint32_t,6> advances{0x3e0f5c29,0x3e4ccccd,0x3e0f5c29,0x3e75c28f,0x3e0f5c29,0x3e0f5c29};
        for(unsigned i=1;i<7;i++){original::translateOriginalMatrix(m,{lit(advances[i-1]),0,0});polygon(77+digit[i],m);}
    };
    const auto remaining=std::uint32_t(std::clamp(s.remainingTicks6000,0,s.extendedCountdown?5994000:594000))/6000;
    if(panel&&(s.flags&4)){
        // 8DB8..8EF8: original TIME/TOTAL TIME/SECTION TIME labels, colored
        // backing strips and authored row labels. Preserve submission order.
        auto m=identity;original::translateOriginalMatrix(m,{-0.f,lit(0xbc23d70a),0});
        polygon(136,m);polygon(123,m);polygon(101,m);
        m=identity;original::translateOriginalMatrix(m,{-0.f,lit(0xbca3d70a),0});
        for(unsigned i=0;i<s.sectionCapacity;i++)polygon(87+i,m);
        m=identity;original::translateOriginalMatrix(m,{-0.f,lit(0x80000000),0});
        polygon(remaining<=5?135:134,m);polygon(137,m);polygon(92,m);
        // 8F10..9510: cumulative records are converted to section durations
        // by065DF0..065E32. Future rows keep the original -1 sentinel.
        m=identity;std::uint32_t previous=0;
        for(unsigned row=0;row<s.sectionCapacity;row++){
            const bool recorded=row<s.sectionCount;
            const bool active=row==s.sectionCount;
            if(!recorded&&!active)break;
            const auto cumulative=recorded?s.sectionTimes6000[row]:s.finishTicks6000!=0xffffffff?s.finishTicks6000:s.elapsedTicks6000;
            const auto duration=cumulative-previous;previous=cumulative;
            if(row==0)original::translateOriginalMatrix(m,{lit(0x3e947ae1),lit(0xbfb0a3d7),0});
            else original::translateOriginalMatrix(m,{lit(0xbf851eb8),lit(0xbe23d70a),0});
            timeDigits(m,duration);
        }
    }
    if(s.flags&4){
        if(!s.alternateLayout&&!panel){auto m=identity;original::translateOriginalMatrix(m,{0,lit(0xbc23d70a),0});polygon(136,m);}
        // 971C..98A2: source elapsed units are6000/sec, with millisecond digits.
        auto m=identity;
        original::translateOriginalMatrix(m,{s.alternateLayout?lit(0x3ecccccd):lit(0x3e75c28f),s.alternateLayout?lit(0xbeae147b):lit(0xbf75c28f),0});
        timeDigits(m,s.elapsedTicks6000);
    }
    if(panel){
        // 98B0..9A7A,1569A0: cap99, suppress leading0, center single digit,
        // select orange warning glyphs only for0..5. Imported courses can opt
        // into three digits using these same glyphs. No host font rendering.
        auto m=identity;
        original::translateOriginalMatrix(m,{remaining>=100?.08f:lit(remaining>=10?0x3ea3d70a:0x3f0ccccd),lit(0xbea3d70a),0});
        if(remaining>=100){scaleXY(m,.8f);polygon(113+remaining/100,m);original::translateOriginalMatrix(m,{lit(0x3eeb851f),0,0});}
        if(remaining>=10){polygon(113+(remaining/10)%10,m);original::translateOriginalMatrix(m,{lit(0x3eeb851f),0,0});}
        polygon((remaining<=5?124:113)+remaining%10,m);
    }
    if(s.flags&0x1000){
        // Original9DA0 face/needle draw. Trig phase quantization and FSCA table
        // are shared with the separately verified original matrix port.
        auto face=identity;original::translateOriginalMatrix(face,{s.alternateLayout?lit(0x40b5c28f):lit(0x40b0a3d7),s.alternateLayout?lit(0xc0823d71):lit(0xc07a3d71),s.alternateLayout?lit(0xba83126f):0});
        scaleXY(face,s.alternateLayout?lit(0x3f333333):1.f);polygon(187+s.tachType,face);
        auto needle=identity;original::translateOriginalMatrix(needle,{lit(0x40b5c28f),lit(0xc0823d71),0});
        if(!s.alternateLayout)original::translateOriginalMatrix(needle,{lit(0xbe23d70a),lit(0x3e23d70a),0});
        const float negRpm=-s.rpm;
        float angle=lit(0x408cbe4c)*negRpm;
        const float denominator=s.tachType==3?12000.f:float(s.tachType+8)*1000.f;
        angle/=denominator;original::rotateOriginalMatrixZ(needle,angle,trig_);scaleXY(needle,s.alternateLayout?lit(0x3f333333):1.f);polygon(191,needle);
    }
    if(s.flags&0x2000){sprite(s.automatic?7:8);sprite(s.gear>0?unsigned(s.gear)+8:9);}
    if(s.flags&0x4000){
        auto m=identity;original::translateOriginalMatrix(m,{s.alternateLayout?lit(0x40b33333):lit(0x40aeb852),s.alternateLayout?lit(0xc078f5c3):lit(0xc075c28f),0});polygon(203,m);sprite(0);
        const int integer=ftrc(s.speedKmh);const unsigned magnitude=integer<0?0u-unsigned(integer):unsigned(integer);
        unsigned value=magnitude;
        // DB160 strips leading zero digits but leaves the units glyph.
        unsigned count=magnitude<10?1:magnitude<100?2:3;
        for(unsigned i=0;i<count;i++){sprite(1+i,1+value%10);value/=10;}
        if(integer<=99)sprite(6);if(integer<=9)sprite(5);
    }
    // Source 0C9C32..0C9C40: display flag 00400000 selects original chunk 4.
    if(s.timeExtended)polygon(4,identity);
    return out;
}
void OriginalRaceHud::paint(std::span<std::uint32_t> argb,int width,int height,const OriginalHudState& state)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid HUD destination");
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const float centeredX=(float(width)-640.f*fit)*.5f,centeredY=(float(height)-480.f*fit)*.5f;
    auto commands=drawList(state);
    // Only positions change per draw. Do not clone every batch, index array,
    // UV and material for every instrument on every frame.
    std::vector<Vec3> transformedPositions;transformedPositions.reserve(maxChunkVertices_);
    if(state.timePanel&&!state.alternateLayout){
        // Original z places the label quads in front of their backing strips.
        // This CPU compositor has no depth buffer, so submit those strips first.
        std::stable_partition(commands.begin(),commands.end(),[](const auto& d){return d.kind==OriginalHudDraw::Kind::polygon&&(d.index==92||d.index==134||d.index==135||d.index==137);});
    }
    // This essential HUD has two recovered depth bands: model view z=-.15
    // plus tiny authored model z, and RIP instruments z=-.1. Submit all of the
    // farther polygons before the nearer sprites in this flat CPU compositor.
    // The drawList retains the original submission order for differential tests.
    for(const auto kind:{OriginalHudDraw::Kind::polygon,OriginalHudDraw::Kind::sprite})for(const auto& draw:commands){
        if(draw.kind!=kind)continue;
        const bool timer=draw.kind==OriginalHudDraw::Kind::polygon&&draw.index<187;
        // The race-end announcement is a centred banner, not an instrument
        // group, so widescreen must not anchor it to an edge.
        const bool centerMessage=draw.kind==OriginalHudDraw::Kind::polygon
            &&(draw.index==4||(draw.index>=182&&draw.index<=185));
        const float offsetX=!state.edgeAnchored||centerMessage?centeredX:timer?0.f:float(width)-640.f*fit;
        const float offsetY=!state.edgeAnchored||centerMessage?centeredY:timer?(state.timePanel&&!state.alternateLayout?0.f:-50.f*fit):float(height)-480.f*fit;
        if(draw.kind==OriginalHudDraw::Kind::polygon){
            const auto& chunk=model_.chunks.at(draw.index);
            transformedPositions.clear();
            for(const auto& batch:chunk.batches)for(const auto& vertex:batch.vertices){const auto p=original::transformOriginalPoint(draw.matrix,{vertex.position.x,vertex.position.y,vertex.position.z});transformedPositions.push_back({p[0],p[1],p[2]});}
            SpritePlacement placement;placement.scale=100.f*fit;placement.offsetX=offsetX;placement.offsetY=offsetY;placement.invertY=true;placement.authoredHeight=0;
            // These original HUD GMPs already carry params6FF and authored
            // constant colors; the menu1B8240 placeholder override is not used.
            compositeOriginalMenuChunk(argb,width,height,modelTextures_,chunk,placement,transformedPositions);
        }else{
            auto sprite=sprites_.sprites.at(draw.index);
            if(draw.textureOverride!=0xffffffff)sprite.texture=draw.textureOverride;
            if(state.alternateLayout){
                // Constructor7D88..7F44 writes these exact top-left positions.
                constexpr std::array<std::array<float,2>,15> xy{{{566,452},{550,447},{534,447},{518,447},{550,447},{534,447},{518,447},{602,444},{602,444},{602,454},{602,454},{602,454},{602,454},{602,454},{602,454}}};
                float minX=sprite.vertices[0].x,minY=sprite.vertices[0].y;for(const auto& v:sprite.vertices){minX=std::min(minX,v.x);minY=std::min(minY,v.y);}
                for(auto& v:sprite.vertices){v.x+=xy[draw.index][0]-minX;v.y+=xy[draw.index][1]-minY;}
            }
            SpritePlacement placement;placement.scale=fit;placement.offsetX=offsetX;placement.offsetY=offsetY;
            compositeOriginalSprite(argb,width,height,spriteTextures_.at(sprite.texture),sprite,placement);
        }
    }
}
void OriginalRaceHud::paintAuthoredChunk(std::span<std::uint32_t> argb,int width,int height,unsigned chunk)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid chunk destination");
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    auto copy=model_.chunks.at(chunk);
    SpritePlacement placement;placement.scale=100.f*fit;
    placement.offsetX=(float(width)-640.f*fit)*.5f;placement.offsetY=(float(height)-480.f*fit)*.5f;
    placement.invertY=true;placement.authoredHeight=0;
    compositeOriginalMenuChunk(argb,width,height,modelTextures_,copy,placement);
}
void OriginalRaceHud::paintChunk(std::span<std::uint32_t> argb,int width,int height,unsigned chunk)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid chunk destination");
    const auto& source=model_.chunks.at(chunk);
    float minX=INFINITY,maxX=-INFINITY,minY=INFINITY,maxY=-INFINITY;
    for(const auto& batch:source.batches)for(const auto& v:batch.vertices){
        minX=std::min(minX,v.position.x);maxX=std::max(maxX,v.position.x);
        minY=std::min(minY,v.position.y);maxY=std::max(maxY,v.position.y);
    }
    if(!(maxX>minX)||!(maxY>minY))return;
    SpritePlacement placement;
    placement.scale=100.f*std::min(float(width)*.9f/((maxX-minX)*100.f),float(height)*.9f/((maxY-minY)*100.f));
    placement.offsetX=width*.5f-(minX+maxX)*.5f*placement.scale;
    placement.offsetY=height*.5f+(minY+maxY)*.5f*placement.scale;
    placement.invertY=true;placement.authoredHeight=0;
    compositeOriginalMenuChunk(argb,width,height,modelTextures_,source,placement);
}
void OriginalRaceHud::paintChunkAt(std::span<std::uint32_t> argb,int width,int height,unsigned chunk,
        float centerX,float centerY,float pixelHeight)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid chunk destination");
    const auto& source=model_.chunks.at(chunk);
    float minX=INFINITY,maxX=-INFINITY,minY=INFINITY,maxY=-INFINITY;
    for(const auto& batch:source.batches)for(const auto& v:batch.vertices){
        minX=std::min(minX,v.position.x);maxX=std::max(maxX,v.position.x);
        minY=std::min(minY,v.position.y);maxY=std::max(maxY,v.position.y);
    }
    if(!(maxY>minY))return;
    SpritePlacement placement;
    placement.scale=pixelHeight/(maxY-minY);
    placement.offsetX=centerX-(minX+maxX)*.5f*placement.scale;
    placement.offsetY=centerY+(minY+maxY)*.5f*placement.scale;
    placement.invertY=true;placement.authoredHeight=0;
    compositeOriginalMenuChunk(argb,width,height,modelTextures_,source,placement);
}
void OriginalRaceHud::paintStartSignal(std::span<std::uint32_t> argb,int width,int height,int digit,unsigned elapsedTicks)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height||digit<0||digit>3)throw std::invalid_argument("Invalid start signal destination/state");
    const unsigned index=digit?unsigned(digit-1):3u;
    const auto& chunk=model_.chunks.at(index);const auto center=startSignalCenters_[index];
    const auto animation=original::originalCountdownPresentation(elapsedTicks);
    SpritePlacement placement;placement.scale=(200.f/3.f)*animation.scale*std::min(float(width)/640.f,float(height)/480.f);
    placement.opacity=animation.opacity;
    placement.offsetX=width*.5f-center.x*placement.scale;
    placement.offsetY=height*.5f+center.y*placement.scale;
    placement.invertY=true;placement.authoredHeight=0;
    compositeOriginalMenuChunk(argb,width,height,modelTextures_,chunk,placement);
}
}
