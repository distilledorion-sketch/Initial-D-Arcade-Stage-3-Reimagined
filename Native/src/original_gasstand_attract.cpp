#include "unity_ui_capture.h"
#include "original_gasstand_attract.h"
#include "original_gasstand_data.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
using namespace gasstand_data;
std::span<const Message> script(unsigned id){switch(id){case 0:return script0;case 1:return script1;case 2:return script2;default:throw std::invalid_argument("Invalid original gasstand script");}}
int glyph(char c){const auto it=std::find(fontCharacters.begin(),fontCharacters.end(),c);return it==fontCharacters.end()?-1:int(it-fontCharacters.begin());}
unsigned lineCount(const Message& m){return !m.lines[2].empty()?3:!m.lines[1].empty()?2:1;}
void validate(const OriginalGasstandState& s){const auto rows=script(s.script);if(s.phase>3||s.message>rows.size()||s.speaker>2||s.fade<0||s.fade>15||!std::isfinite(s.brandX)||!std::isfinite(s.bubbleX)||!std::isfinite(s.bubbleY))throw std::invalid_argument("Invalid original gasstand state");}
// Tile57 is one exact fixed material: white diffuse, no offset, replace
// destination, ignore texture alpha, wrappingUV. Preserve the general
// compositor's coverage and barycentric operation order, but specialize away
// material switches and floating-point floor library calls for bounded UVs.
void opaqueTile(std::span<std::uint32_t> target,int width,int height,const NativeModelBatch& batch,const NativeImage& image,const SpritePlacement& p){
    struct V{float x,y,u,v;};std::vector<V> vertices;
    for(const auto& v:batch.vertices)vertices.push_back({v.position.x*p.scale+p.offsetX,(0.f-v.position.y)*p.scale+p.offsetY,v.u,v.v});
    const auto edge=[](const V& a,const V& b,float x,float y){return(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
    const auto topLeft=[](const V& a,const V& b){return a.y>b.y||(a.y==b.y&&a.x<b.x);};
    const auto floorInt=[](float x){const int integer=int(x);return integer-int(x<float(integer));};
    for(std::size_t i=0;i<batch.indices.size();i+=3){auto a=vertices[batch.indices[i]],b=vertices[batch.indices[i+1]],c=vertices[batch.indices[i+2]];
        float area=edge(a,b,c.x,c.y);if(std::abs(area)<1e-7f)continue;if(area<0){std::swap(b,c);area=-area;}
        const int minx=std::max(0,floorInt(std::min({a.x,b.x,c.x}))),maxx=std::min(width,-floorInt(-std::max({a.x,b.x,c.x})));
        const int miny=std::max(0,floorInt(std::min({a.y,b.y,c.y}))),maxy=std::min(height,-floorInt(-std::max({a.y,b.y,c.y})));
        const bool ab=topLeft(a,b),bc=topLeft(b,c),ca=topLeft(c,a);
        for(int y=miny;y<maxy;++y)for(int x=minx;x<maxx;++x){
            const float px=x+.5f,py=y+.5f,ea=edge(b,c,px,py),eb=edge(c,a,px,py),ec=edge(a,b,px,py);
            if(ea<0||eb<0||ec<0||(ea==0&&!bc)||(eb==0&&!ca)||(ec==0&&!ab))continue;
            const float wa=ea/area,wb=eb/area,wc=ec/area,u=wa*a.u+wb*b.u+wc*c.u,v=wa*a.v+wb*b.v+wc*c.v;
            const float uf=u-float(floorInt(u)),vf=v-float(floorInt(v));
            const int tx=std::clamp(int(uf*image.width),0,int(image.width)-1),ty=std::clamp(int(vf*image.height),0,int(image.height)-1);
            target[std::size_t(y)*width+x]=image.argb[std::size_t(ty)*image.width+tx]|0xff000000u;
        }
    }
}
}
void resetOriginalGasstand(OriginalGasstandState& s,unsigned id){(void)script(id);s={};s.script=id;s.brandX=gasstand_data::lit_0C0D095C;}
unsigned nextOriginalGasstandScript(unsigned previous,bool skipBattle,bool skipCustomization){if(previous>2)throw std::invalid_argument("Invalid prior gasstand script");unsigned next=(previous+1)%3;if(skipCustomization&&next==0)next=1;if(skipBattle&&next==1)next=2;return next;}
void stepOriginalGasstand(OriginalGasstandState& s){
    validate(s);if(s.completed)return;const auto rows=script(s.script);
    //0D0D20 four-state Main. The renderer also owns two source animation
    // writes, so advance those once here rather than at host refresh rate.
    switch(s.phase){
    case 0:if(--s.fade<0){s.fade=0;s.fadeEnabled=false;++s.phase;}break;
    case 1:{
        const auto& row=rows[s.message];s.speaker=row.speaker;s.bubble=true;
        s.bubbleX=gasstand_data::speakerXY[s.speaker][0]*gasstand_data::lit_0C0D0F30-gasstand_data::lit_0C0D0F34;
        s.bubbleY=std::fma(-gasstand_data::speakerXY[s.speaker][1],gasstand_data::lit_0C0D0F30,gasstand_data::lit_0C0D0F38);
        if(s.script==0&&s.message==6)s.brands=true;
        if(s.script==2&&s.message==7)s.ranking=true;
        ++s.phase;break;}
    case 2:{unsigned count=0;for(const auto line:rows[s.message].lines)count+=unsigned(line.size());
        if(++s.elapsed>count*3+40){s.elapsed=0;++s.message;s.bubble=false;
            if(s.message>=rows.size()){++s.phase;s.fadeEnabled=true;}else s.phase=1;}
        break;}
    case 3:if(++s.fade>15){s.fade=15;s.completed=true;}break;
    }
    s.displayedBackgroundPhase=s.backgroundPhase;
    if(float(++s.backgroundPhase)*gasstand_data::lit_0C112E28>=gasstand_data::lit_0C112F88)s.backgroundPhase=0;
    if(s.script==0&&s.brands)s.brandX-=gasstand_data::lit_0C0D14F0;
}
std::uint32_t originalGasstandFadeArgb(const OriginalGasstandState& s){validate(s);return s.fadeEnabled?std::uint32_t(float(s.fade)/15.f*255.f)<<24:0;}
float originalAlphabetTextWidth(std::string_view text){float sum=0;for(const char c:text){const int i=glyph(c);sum=std::fma(i<0?5.f:gasstand_data::word(gasstand_data::fontWidthWords[std::size_t(i)]),1.f,sum);}return sum;}
std::vector<OriginalGasstandDraw> originalGasstandDraws(const OriginalGasstandState& s){
    validate(s);using namespace gasstand_data;using Bank=OriginalGasstandDraw::Bank;
    std::vector<OriginalGasstandDraw> out;out.reserve(180);
    const auto draw=[&](Bank bank,unsigned i,Vec3 pos=Vec3{},float sx=1,float sy=1){out.push_back({bank,i,pos,sx,sy});};
    const float phase=float(s.displayedBackgroundPhase)*lit_0C112E28;
    //112E60 repeats original tile57: columns-1..2, rows-1..1. Its setter
    // uses Z=-100, with145920's depth-compensating scale at draw time.
    for(int row=-1;row<2;++row)for(int col=-1;col<3;++col){
        const float x=std::fma(float(col),lit_0C112F88,phase),y=std::fma(float(row),lit_0C112F88,phase);
        // Keep getter/setter single-precision roundoff at the source origin.
        const float originX=word(0xc04ccccc),originY=word(0x40199999);
        draw(Bank::gasstand,57,{(originX+x)-originX,(originY-y)-originY,-100.f-word(0xbe19999a)});
    }
    draw(Bank::gasstand,13,{0,lit_0C0D1130,0});
    const auto rows=script(s.script);
    const Message* message=s.message<rows.size()?&rows[s.message]:nullptr;
    const unsigned lines=message?lineCount(*message):1;
    if(s.phase==2&&message){
        constexpr std::array<std::array<float,3>,3> lineOffsets{{{-10,0,0},{-36,-10,0},{-36,-10,16}}};
        for(unsigned line=0;line<lines;++line){
            float advance=0;const float x=speakerXY[s.speaker][0],y=speakerXY[s.speaker][1]+lineOffsets[lines-1][line]+24.f;
            for(const char c:message->lines[line]){const int i=glyph(c);
                if(i>=0){draw(Bank::alphabet,unsigned(i),{x+advance,y,gasstand_data::word(0x3727c5ac)});
                    //1CB200 color4 with dimming argument-1 yieldsRGB010101;
                    //1E7E80 restores full alpha from descriptor opacity1.
                    out.back().color=0xff010101;}
                advance=std::fma(i<0?5.f:word(fontWidthWords[std::size_t(i)]),1.f,advance);
            }
        }
    }
    if(s.bubble&&message){
        const unsigned left=lines==1?0:lines==2?3:7,middle=lines==1?1:lines==2?4:8,right=lines==1?5:lines==2?6:9;
        Vec3 p{s.bubbleX,s.bubbleY,0};if(lines==1)p.y-=lit_0C0D114C;
        draw(Bank::gasstand,left,p,lit_0C0D1208,lit_0C0D1208);
        Vec3 tail{s.bubbleX,s.bubbleY-lit_0C0D1210,0};
        if(lines==1)tail.y+=lit_0C0D1214;else if(lines==3)tail.y-=lit_0C0D13B0;
        tail.x+=lit_0C0D13B4;if(s.speaker==1)tail.x+=lit_0C0D13B8;
        draw(Bank::gasstand,2,tail,lit_0C0D13BC,lit_0C0D13C0);
        p.x+=lit_0C0D13C4;float widest=0;for(auto line:message->lines)widest=std::max(widest,originalAlphabetTextWidth(line));
        draw(Bank::gasstand,middle,p,lit_0C0D13CC*widest,lit_0C0D13C0);
        p.x=std::fma(widest*lit_0C0D13D0,lit_0C0D13D4,p.x);
        draw(Bank::gasstand,right,p,lit_0C0D13C0,lit_0C0D13C0);
    }
    draw(Bank::etc,0);draw(Bank::etc,1,{0,lit_0C0D13D8,0});
    switch(s.script){
    case 0:
        if(s.brands)for(unsigned i=0;i<brandOrder.size();++i){
            const float x=std::fma(float(i/2),lit_0C0D14F8,s.brandX);
            const float y=-float(i%2)*lit_0C0D14FC-lit_0C0D1500;
            draw(Bank::gasstand,brandOrder[i]+14,{x,y,0});
        }
        draw(Bank::etc,2);break;
    case 1:draw(Bank::etc,3);break;
    case 2:if(s.ranking)draw(Bank::gasstand,56);draw(Bank::etc,4);break;
    }
    return out;
}
bool OriginalGasstandAttract::available(const std::filesystem::path& root){
    for(const char* file:{"data/original_assets/attract/gasstand/gasstand.idasmesh","data/original_assets/attract/gasstand/textures/textures.idastex","data/original_assets/attract/alphabet/textures/textures.idastex","data/original_assets/menus/v3/v3sA00etc/v3sA00etc.idasmesh","data/original_assets/menus/v3/v3sA00etc/textures/textures.idastex"})
        if(!std::filesystem::is_regular_file(root/file))return false;
    return true;
}
void OriginalGasstandAttract::load(const std::filesystem::path& root){
    const auto gas=root/"data/original_assets/attract/gasstand",etc=root/"data/original_assets/menus/v3/v3sA00etc";
    gasstand_=NativeModel::load(gas/"gasstand.idasmesh");gasstandTextures_=NativeTextureBank::load(gas/"textures/textures.idastex");
    etc_=NativeModel::load(etc/"v3sA00etc.idasmesh");etcTextures_=NativeTextureBank::load(etc/"textures/textures.idastex");
    alphabet_=NativeTextureBank::load(root/"data/original_assets/attract/alphabet/textures/textures.idastex");
    if(gasstand_.chunks.size()!=58||gasstandTextures_.size()!=15||alphabet_.size()!=62||etc_.chunks.size()<5)throw std::runtime_error("Original gasstand asset identity mismatch");reset();
    preparedAlphabet_.clear();preparedAlphabet_.reserve(alphabet_.size());
    for(unsigned i=0;i<alphabet_.size();++i){auto glyph=alphabet_.at(i);
        for(auto& color:glyph.argb){if((color&0xffffff)!=0xffffff)throw std::runtime_error("Original alphabet expects white source glyphs");color=(color&0xff000000)|0x010101;}
        preparedAlphabet_.push_back(std::move(glyph));}
}
void OriginalGasstandAttract::paint(std::span<std::uint32_t> target,int width,int height)const{paintImpl(target,width,height,true);}
void OriginalGasstandAttract::paintReference(std::span<std::uint32_t> target,int width,int height)const{paintImpl(target,width,height,false);}
void OriginalGasstandAttract::paintText(std::span<std::uint32_t> target,int width,int height,std::string_view text,float x,float y,float scale,std::uint32_t color)const{
    SpritePlacement p;p.scale=scale;p.offsetX=x;p.offsetY=y;
    float advance=0;
    for(const char c:text){const int i=glyph(c);if(i>=0){
        OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{advance,0,0,0,1,color,0},{advance,32,0,0,0,color,0},{advance+32,0,0,1,1,color,0},{advance+32,32,0,1,0,color,0}};
        compositeOriginalSprite(target,width,height,alphabet_.at(unsigned(i)),sprite,p);
        advance+=gasstand_data::word(gasstand_data::fontWidthWords[unsigned(i)]);
    }else advance+=5;}
}
void OriginalGasstandAttract::extendBackdrop(std::span<std::uint32_t> target,int width,int height)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid attract backdrop destination");
    const float fit=std::min(width/640.f,height/480.f);const int drawWidth=std::max(1,int(640*fit)),drawHeight=std::max(1,int(480*fit));
    const int left=(width-drawWidth)/2,top=(height-drawHeight)/2;
    if(left==0)return;
    const auto& batch=gasstand_.chunks.at(57).batches.at(0);const auto& image=gasstandTextures_.at(batch.material[9]);
    const auto& a=batch.vertices.at(batch.indices.at(0));const auto& b=batch.vertices.at(batch.indices.at(1));const auto& c=batch.vertices.at(batch.indices.at(2));
    const float dx1=b.position.x-a.position.x,dy1=b.position.y-a.position.y,dx2=c.position.x-a.position.x,dy2=c.position.y-a.position.y,det=dx1*dy2-dx2*dy1;
    const float ux=((b.u-a.u)*dy2-(c.u-a.u)*dy1)/det,uy=(dx1*(c.u-a.u)-dx2*(b.u-a.u))/det;
    const float vx=((b.v-a.v)*dy2-(c.v-a.v)*dy1)/det,vy=(dx1*(c.v-a.v)-dx2*(b.v-a.v))/det;
    if(std::abs(uy)>1e-5f||std::abs(vx)>1e-5f)throw std::runtime_error("Attract tile is not axis aligned");
    const auto wrap=[](float uv,int size){const int i=int(std::floor(uv*size));return (i%size+size)%size;};
    const float phase=float(state_.displayedBackgroundPhase)*gasstand_data::lit_0C112E28;
    const auto& gradientBatch=etc_.chunks.at(1).batches.at(0);const auto& gradient=etcTextures_.at(gradientBatch.material[9]);
    const auto& ga=gradientBatch.vertices.at(0);const auto& gb=gradientBatch.vertices.at(1);
    if(unityUiEnabled()){
        // The native extension stretches each already-authored edge pixel for
        // the header/footer and extends the original tile+gradient in between.
        for(const auto side:std::array<std::array<float,3>,2>{{{0.f,float(left),float(left)},{float(left+drawWidth),float(width-left-drawWidth),float(left+drawWidth-1)}}}){
            const float xx=side[0],ww=side[1],edge=side[2];
            unityUiCopyRegion(target.data(),width,height,edge,float(top),edge+1,top+63*fit,xx,float(top),ww,63*fit);
            unityUiCopyRegion(target.data(),width,height,edge,top+424*fit,edge+1,float(top+drawHeight),xx,top+424*fit,ww,drawHeight-424*fit);
            const float y1=top+63*fit,y2=top+424*fit;
            auto uv=[&](float x,float y){const float sx=(x-left)/fit/100.f-phase,sy=(y-top)/fit;return std::array<float,2>{(sx-a.position.x)*ux+a.u,(-sy/100.f+phase-a.position.y)*vy+a.v};};
            auto vertex=[&](float x,float y){const auto p=uv(x,y);return UnityUiVertex{x,y,p[0],p[1],0xffffffff,0};};
            const auto aa=vertex(xx,y1),bb=vertex(xx+ww,y1),cc=vertex(xx,y2),dd=vertex(xx+ww,y2);
            unityUiTriangle(target.data(),width,height,image,aa,bb,cc,1,batch.ich[2],true,batch.ich[0]);unityUiTriangle(target.data(),width,height,image,cc,bb,dd,1,batch.ich[2],true,batch.ich[0]);
            float gy1=std::max(63.f,(-std::max(ga.position.y,gb.position.y)-gasstand_data::lit_0C0D13D8)*100.f);
            float gy2=std::min(424.f,(-std::min(ga.position.y,gb.position.y)-gasstand_data::lit_0C0D13D8)*100.f);
            if(gy2>gy1){auto gv=[&](float x,float y){const float gy=-y/100.f-gasstand_data::lit_0C0D13D8;return UnityUiVertex{x,top+y*fit,0,ga.v+(gy-ga.position.y)*(gb.v-ga.v)/(gb.position.y-ga.position.y),0xffffffff,0};};
                const auto a1=gv(xx,gy1),b1=gv(xx+ww,gy1),c1=gv(xx,gy2),d1=gv(xx+ww,gy2);
                unityUiTriangle(target.data(),width,height,gradient,a1,b1,c1,1,(1u<<29)|(1u<<26)|(3u<<6),true,8);unityUiTriangle(target.data(),width,height,gradient,c1,b1,d1,1,(1u<<29)|(1u<<26)|(3u<<6),true,8);}
        }return;
    }
    std::vector<int> columns(std::size_t(width),0);
    for(int x=0;x<width;++x)if(x<left||x>=left+drawWidth){const float sourceX=(x-left+.5f)/fit/100.f-phase;
        columns[std::size_t(x)]=wrap((sourceX-a.position.x)*ux+a.u,int(image.width));}
    for(int y=top;y<top+drawHeight;++y){
        const float sourceY=(y-top+.5f)/fit;auto* row=target.data()+std::size_t(y)*width;
        if(sourceY<63.f||sourceY>=424.f){
            std::fill(row,row+left,row[left]);std::fill(row+left+drawWidth,row+width,row[left+drawWidth-1]);
        }else{
            const int ty=wrap((-sourceY/100.f+phase-a.position.y)*vy+a.v,int(image.height));const auto* texture=image.argb.data()+std::size_t(ty)*image.width;
            // etc chunk1 adds the original vertical gradient over the moving
            // tile. Extend that layer too, preventing a seam at the 4:3 edge.
            const float gy=-sourceY/100.f-gasstand_data::lit_0C0D13D8;std::uint32_t glow=0;
            if(gy>=std::min(ga.position.y,gb.position.y)&&gy<=std::max(ga.position.y,gb.position.y)){
                const float uv=ga.v+(gy-ga.position.y)*(gb.v-ga.v)/(gb.position.y-ga.position.y);
                glow=gradient.argb[std::size_t(wrap(uv,int(gradient.height)))*gradient.width];
            }
            const auto shade=[&](std::uint32_t pixel){
                return 0xff000000u|(std::min(255u,((pixel>>16)&255)+((glow>>16)&255))<<16)|
                    (std::min(255u,((pixel>>8)&255)+((glow>>8)&255))<<8)|std::min(255u,(pixel&255)+(glow&255));};
            for(int x=0;x<left;++x)row[x]=shade(texture[columns[std::size_t(x)]]);
            for(int x=left+drawWidth;x<width;++x)row[x]=shade(texture[columns[std::size_t(x)]]);
        }
    }
}
void OriginalGasstandAttract::paintImpl(std::span<std::uint32_t> target,int width,int height,bool optimized)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid gasstand destination");
    if(unityUiEnabled())optimized=false;
    using Bank=OriginalGasstandDraw::Bank;auto commands=originalGasstandDraws(state_);
    // These source depth bands put the moving tile behind the source full
    // panel, and text at1e-5 in front. Preserve the original list separately.
    const float fit=std::min(float(width)/640.f,float(height)/480.f),ox=(width-640.f*fit)*.5f,oy=(height-480.f*fit)*.5f;
    struct Batch{NativeModelChunk chunk;const NativeTextureBank* textures;float depth;bool tile;};std::vector<Batch> batches;
    for(const auto& d:commands){if(d.bank==Bank::alphabet)continue;
        const auto& model=d.bank==Bank::gasstand?gasstand_:etc_;const auto* textures=d.bank==Bank::gasstand?&gasstandTextures_:&etcTextures_;
        for(auto batch:model.chunks.at(d.index).batches){float depth=0;
            for(auto& v:batch.vertices){v.position.x=v.position.x*d.scaleX+d.position.x;v.position.y=v.position.y*d.scaleY+d.position.y;v.position.z+=d.position.z;depth+=v.position.z;}
            const bool tile=optimized&&d.bank==Bank::gasstand&&d.index==57&&batch.ich[2]==0x2008246d&&batch.material[3]==0xffffffff&&batch.material[4]==0;
            depth/=float(batch.vertices.size());NativeModelChunk chunk;chunk.batches.push_back(std::move(batch));batches.push_back({std::move(chunk),textures,depth,tile});
        }
    }
    std::stable_sort(batches.begin(),batches.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    SpritePlacement p;p.scale=100.f*fit;p.offsetX=ox;p.offsetY=oy;p.invertY=true;p.authoredHeight=0;
    for(const auto& b:batches){if(b.tile)opaqueTile(target,width,height,b.chunk.batches[0],b.textures->at(b.chunk.batches[0].material[9]),p);
        else compositeOriginalMenuChunk(target,width,height,*b.textures,b.chunk,p);}
    p={};p.scale=fit;p.offsetX=ox;p.offsetY=oy;
    for(const auto& d:commands)if(d.bank==Bank::alphabet){const float x=d.position.x,y=d.position.y;
        const auto color=optimized?0xffffffffu:d.color;
        OriginalSprite glyphSprite;glyphSprite.vertices={OriginalSpriteVertex{x,y,0,0,1,color,0},{x,y+32,0,0,0,color,0},{x+32,y,0,1,1,color,0},{x+32,y+32,0,1,0,color,0}};
        compositeOriginalSprite(target,width,height,optimized?preparedAlphabet_.at(d.index):alphabet_.at(d.index),glyphSprite,p);
    }
}
}
