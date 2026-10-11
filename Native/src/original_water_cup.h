#pragma once
#include "native_assets.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace idas3 {
// Recovered 17D2A0 state machine. Contact cues are read-only copies; the cup
// never changes the driving state or consumes its random stream.
struct OriginalWaterCup {
    unsigned frame=0;
    int water=29,rightSplash=-1,leftSplash=-1,mode=0,unused=0,severity=0;
    void reset(){*this={};}
    void advance(float movement,const std::array<std::uint32_t,5>& cues){
        if(mode!=3&&mode!=4)mode=cues[4]?2:std::abs(movement)>.02f?1:0;
        const auto impact=[&](int side,int strength){
            mode=side;severity=strength;frame=0;rightSplash=leftSplash=-1;
        };
        if(cues[2])impact(3,0);
        if(cues[3])impact(4,0);
        const float a=std::bit_cast<float>(cues[0]),b=std::bit_cast<float>(cues[1]);
        if(a>0)impact(4,a<.05f?1:2);
        if(b>0)impact(3,b<.05f?1:2);
        if(mode==0)water=29;
        else if(mode==1){const int f=frame%14;water=f<=6?26+f:40-f;}
        else if(mode==2){const int f=frame%7;water=f<=2?22+4*f:36-4*(f-3);}
        else {
            // 323BE4: peak, outward duration, total duration, frame step.
            constexpr int curves[6][4]={{45,15,30,1},{50,10,20,2},{59,5,10,6},
                {15,15,30,1},{10,10,20,2},{0,5,10,6}};
            const auto& c=curves[severity+(mode==4?3:0)];
            const int f=int(frame),sign=mode==3?1:-1;
            water=f<c[1]?(mode==3?30:29)+sign*f*c[3]:c[0]-sign*(f-c[1])*c[3];
            auto& splash=mode==3?rightSplash:leftSplash;
            if(severity==2)splash=f>=4&&f<=6?splash+1:-1;
            if(f>=c[2]){mode=1;unused=0;splash=-1;}
        }
        water=std::clamp(water,0,59);++frame;
    }
};

class OriginalWaterCupArtwork {
public:
    void load(const std::filesystem::path& root){
        const auto folder=root/"data/original_assets/hud/cupwater";
        model_=NativeModel::load(folder/"cupwater.idasmesh");
        textures_=NativeTextureBank::load(folder/"textures/textures.idastex");
        if(model_.chunks.size()!=70||textures_.size()!=7)throw std::runtime_error("Incomplete original water-cup artwork");
        prepareArtwork();
    }
    void paint(std::span<std::uint32_t> pixels,int width,int height,const OriginalWaterCup& state,
               float x,float y,float scale)const{
        SpritePlacement p;p.scale=scale;p.invertY=true;p.authoredHeight=0;
        p.offsetX=x;p.offsetY=y;p.straightAlphaOverlay=true;
        const auto draw=[&](unsigned chunk){compositeOriginalMenuChunk(pixels,width,height,textures_,model_.chunks.at(chunk),p);};
        // The separate source surround is farther away than the cup. PVR's
        // translucent depth ordering puts it behind the water and splash even
        // though the source submits it after popping the cup transform.
        draw(69);
        // The source depth buffer keeps the water behind the front wall and
        // rim, although its draw follows the cup. Split those opaque surfaces
        // at the two translucent depth planes once at load so both software
        // and Unity's ordered UI triangles preserve the same occlusion.
        const auto body=[&](unsigned band){compositeOriginalMenuChunk(pixels,width,height,textures_,bodyBands_[band],p);};
        body(0);draw(9+unsigned(state.water));body(1);
        if(state.rightSplash>=0)draw(6+unsigned(state.rightSplash));
        if(state.leftSplash>=0)draw(3+unsigned(state.leftSplash));
        body(2);
    }
private:
    static float depthPlane(const NativeModelChunk& chunk){
        double depth=0;std::size_t count=0;
        for(const auto& batch:chunk.batches)for(const auto& v:batch.vertices){depth+=v.position.z;++count;}
        if(!count)throw std::runtime_error("Empty original water-cup depth plane");
        const auto result=float(depth/double(count));
        for(const auto& batch:chunk.batches)for(const auto& v:batch.vertices)
            if(std::abs(v.position.z-result)>.0000001f)throw std::runtime_error("Original water-cup overlay is not planar");
        return result;
    }
    static NativeModelVertex interpolate(const NativeModelVertex& a,const NativeModelVertex& b,float t){
        auto out=a;out.position=a.position+(b.position-a.position)*t;out.normal=a.normal+(b.normal-a.normal)*t;
        out.u=a.u+(b.u-a.u)*t;out.v=a.v+(b.v-a.v)*t;
        const auto color=[&](std::uint32_t x,std::uint32_t y){
            std::uint32_t value=0;
            for(unsigned shift:{0u,8u,16u,24u})value|=std::uint32_t(std::clamp(std::lround(float((x>>shift)&255)+(float((y>>shift)&255)-float((x>>shift)&255))*t),0l,255l))<<shift;
            return value;
        };
        out.color0=color(a.color0,b.color0);out.color1=color(a.color1,b.color1);return out;
    }
    static std::vector<NativeModelVertex> clipDepth(const std::vector<NativeModelVertex>& source,float plane,bool nearer){
        std::vector<NativeModelVertex> out;if(source.empty())return out;out.reserve(source.size()+1);
        auto previous=source.back();bool previousInside=nearer?previous.position.z>=plane:previous.position.z<=plane;
        for(const auto& current:source){
            const bool inside=nearer?current.position.z>=plane:current.position.z<=plane;
            if(inside!=previousInside){
                auto intersection=interpolate(previous,current,(plane-previous.position.z)/(current.position.z-previous.position.z));
                intersection.position.z=plane;out.push_back(intersection);
            }
            if(inside)out.push_back(current);
            previous=current;previousInside=inside;
        }
        return out;
    }
    static bool sourceTriangleVisible(const NativeModelBatch& batch,const NativeModelVertex& a,const NativeModelVertex& b,const NativeModelVertex& c){
        const float area=(b.position.x-a.position.x)*(c.position.y-a.position.y)-(b.position.y-a.position.y)*(c.position.x-a.position.x);
        if(area==0)return false;
        // Source ISP winding, before the screen's Y inversion. The front wall
        // is mode3; the back wall, rim, water and splash use mode2.
        const unsigned cull=(batch.ich[1]>>27)&3;
        return cull==2?area<0:cull==3?area>0:true;
    }
    static void project(NativeModelChunk& chunk,bool surround){
        for(auto& batch:chunk.batches)for(auto& v:batch.vertices){
            const auto p=v.position;float x,y;
            if(surround){
                const float depth=.76800001f-p.z;
                x=320.f+76.8f*(p.x-2.57249999f)/depth;
                y=240.f-76.8f*(p.y-1.75290000f)/depth;
            }else{
                // Recovered source depth: the cup's local Z+1.5 is scaled by
                // .533330023 before the view's -1.5 translation. Preserve that
                // transform arithmetic rather than rounding
                // 1.5 - .53333*1.5 to .7 for the 640x480 perspective.
                const float depth=1.5f-.533330023f*(p.z+1.5f);
                x=320.f+150.f*(.533330023f*p.x-1.190000057f)/depth;
                y=240.f-150.f*(.533330023f*p.y-.776499927f)/depth;
            }
            // Hud::map supplies (70,408), scale100 at the original resolution.
            // Keep that public placement contract for custom sizes/positions.
            v.position={(x-70.f)/100.f,(408.f-y)/100.f,p.z};
        }
    }
    void prepareArtwork(){
        const float water=depthPlane(model_.chunks[38]),splash=depthPlane(model_.chunks[3]);
        if(water>=splash||model_.chunks[0].batches.size()!=3)throw std::runtime_error("Unexpected original water-cup surface layout");
        for(unsigned index=9;index<=68;++index)
            if(std::abs(depthPlane(model_.chunks[index])-water)>.0000001f)throw std::runtime_error("Original water frames do not share their depth plane");
        for(unsigned index=3;index<=8;++index)
            if(std::abs(depthPlane(model_.chunks[index])-splash)>.0000001f)throw std::runtime_error("Original splash frames do not share their depth plane");
        bodyBands_={};
        // The opaque back wall is behind the front wall and rim wherever
        // their screen projections overlap. This order also holds in each
        // band after clipping against the flat water and splash surfaces.
        for(unsigned band=0;band<3;++band)for(unsigned batchIndex:{2u,0u,1u}){
            const auto& source=model_.chunks[0].batches[batchIndex];auto prepared=source;
            prepared.vertices.clear();prepared.indices.clear();
            for(std::size_t i=0;i<source.indices.size();i+=3){
                const auto& a=source.vertices[source.indices[i]];const auto& b=source.vertices[source.indices[i+1]];const auto& c=source.vertices[source.indices[i+2]];
                if(!sourceTriangleVisible(source,a,b,c))continue;
                std::vector<NativeModelVertex> polygon{a,b,c};
                if(band==0)polygon=clipDepth(polygon,water,false);
                else if(band==1){polygon=clipDepth(polygon,water,true);polygon=clipDepth(polygon,splash,false);}
                else polygon=clipDepth(polygon,splash,true);
                if(polygon.size()<3)continue;
                const auto first=std::uint32_t(prepared.vertices.size());
                prepared.vertices.insert(prepared.vertices.end(),polygon.begin(),polygon.end());
                for(std::uint32_t j=1;j+1<polygon.size();++j)prepared.indices.insert(prepared.indices.end(),{first,first+j,first+j+1});
            }
            if(!prepared.indices.empty())bodyBands_[band].batches.push_back(std::move(prepared));
        }
        for(auto& band:bodyBands_)project(band,false);
        for(unsigned index=3;index<70;++index){
            auto& chunk=model_.chunks[index];
            for(auto& batch:chunk.batches){
                std::vector<std::uint32_t> visible;visible.reserve(batch.indices.size());
                for(std::size_t i=0;i<batch.indices.size();i+=3)
                    if(sourceTriangleVisible(batch,batch.vertices[batch.indices[i]],batch.vertices[batch.indices[i+1]],batch.vertices[batch.indices[i+2]]))
                        visible.insert(visible.end(),{batch.indices[i],batch.indices[i+1],batch.indices[i+2]});
                batch.indices=std::move(visible);
            }
            project(chunk,index==69);
        }
    }
    NativeModel model_;
    NativeTextureBank textures_;
    std::array<NativeModelChunk,3> bodyBands_;
};
}
