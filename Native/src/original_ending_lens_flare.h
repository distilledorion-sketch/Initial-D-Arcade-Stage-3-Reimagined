#pragma once
#include "original_lens_flare.h"
#include "original_ending_camera.h"
#include "renderer.h"
#include <fstream>

namespace idas3 {
// 180C60's daylight ending owner. Simulation prepares immutable view-space
// pieces; the host submission maps them into the world for its normal camera.
// No host fade/depth override and no additional random-number consumption.
class OriginalEndingLensFlare {
public:
    void reset(){active_=false;visible_=false;updates_=0;strength_=0;}
    bool active()const{return active_;}
    bool visible()const{return visible_;}
    unsigned updates()const{return updates_;}
    float strength()const{return strength_;}
    const NativeTextureBank& textures()const{return textures_;}
    const auto& matrices()const{return matrices_;}
    void begin(const std::filesystem::path& root,unsigned course,bool night,bool wet,bool reverse){
        reset();if(course>=8||night||wet)return;
        if(model_.chunks.empty())load(root);
        course_=course;reverse_=reverse;active_=true;
    }
    void advance(std::int32_t pathIndex,const OriginalEndingCamera& camera,const original::OriginalFscaTable& trig){
        visible_=false;strength_=0;if(!active_)return;++updates_;
        if(!original::originalLensFlareRoadVisible(tables_[course_],pathIndex,reverse_))return;
        const auto direction=original::originalLensFlareDirection(camera.worldToView(),suns_[course_]);
        if(!original::originalLensFlareFacing(direction))return;
        strength_=original::originalLensFlareStrength(direction);
        const auto eye=camera.eye(),target=camera.target();
        cameraWorld_=original::originalLookAtCameraWorld({eye.x,eye.y,eye.z},{target.x,target.y,target.z});
        for(unsigned index=1;index<pieces_.size();++index){const auto& piece=pieces_[index];
            const auto screen=original::originalLensFlareScreenPoint(direction,piece.distance,
                original::originalEndingLensFlareProjection(),original::originalLensFlareViewport());
            matrices_[index-1]=original::originalLensFlarePieceMatrix(screen,index,piece.size,strength_,trig);
        }
        visible_=true;
    }
    unsigned append(Mesh& mesh,unsigned textureBase)const{
        if(!active_||!visible_)return 0;
        const auto unpack=[](unsigned value){return Color{float((value>>16)&255)/255.f,
            float((value>>8)&255)/255.f,float(value&255)/255.f,float(value>>24)/255.f};};
        for(unsigned index=1;index<pieces_.size();++index){
            const auto& matrix=matrices_[index-1];
            for(const auto& batch:model_.chunks[pieces_[index].chunk].batches){
                mesh.beginRange(textureBase+batch.material[9],batch.ich[2],batch.ich[0],batch.ich[1],
                    batch.material[2],true,true,batch.material[1],false,std::nullopt,false,1);
                const auto color=unpack(original::originalLensFlareColor(batch.material[3],strength_));
                const auto offset=(batch.material[2]&2)?unpack(batch.material[4]):Color{0,0,0,0};
                for(auto vertex:batch.indices){const auto& v=batch.vertices[vertex];
                    const auto local=original::transformOriginalPoint(matrix,{v.position.x,v.position.y,v.position.z});
                    const auto world=original::transformOriginalPoint(cameraWorld_,local);
                    mesh.vertices.push_back({{world[0],world[1],world[2]},{0,1,0},color,v.u,v.v,offset});
                    ++mesh.ranges.back().count;
                }
            }
        }
        return 11;
    }
private:
    void load(const std::filesystem::path& root){
        const auto base=root/"data/original_assets/environment";
        auto model=NativeModel::load(base/"flare/flare.idasmesh");
        auto textures=NativeTextureBank::load(base/"flare/textures.idastex");
        if(model.chunks.size()!=7||textures.size()!=7)throw std::runtime_error("Original ending flare assets");
        for(const auto& chunk:model.chunks)for(const auto& batch:chunk.batches)
            if(batch.ich[6]!=0xau||(batch.material[2]&0x201u)!=0x201u||batch.material[9]>=textures.size())
                throw std::runtime_error("Original ending flare material");
        std::ifstream file(base/"environment.bin",std::ios::binary);
        const auto read=[&](void* p,std::size_t n){if(!file.read(static_cast<char*>(p),std::streamsize(n)))throw std::runtime_error("Incomplete ending flare data");};
        std::array<char,8> magic;read(magic.data(),magic.size());
        if(std::string(magic.data(),magic.size())!="IDASENV1")throw std::runtime_error("Original ending flare data identity");
        read(suns_.data(),sizeof(suns_));read(pieces_.data(),sizeof(pieces_));
        for(const auto& piece:pieces_)if(piece.chunk>=model.chunks.size()||!std::isfinite(piece.distance)||!std::isfinite(piece.size))
            throw std::runtime_error("Original ending flare piece");
        for(auto& table:tables_){unsigned count;read(&count,sizeof(count));
            if(!count||count>10000)throw std::runtime_error("Original ending flare visibility table");
            table.resize(count);read(table.data(),count);
        }
        model_=std::move(model);textures_=std::move(textures);
    }
    NativeModel model_;
    NativeTextureBank textures_;
    std::array<std::array<float,3>,8> suns_{};
    std::array<original::OriginalLensFlarePiece,12> pieces_{};
    std::array<std::vector<std::uint8_t>,8> tables_;
    std::array<original::OriginalMatrix,11> matrices_{};
    original::OriginalMatrix cameraWorld_;
    bool active_=false,visible_=false,reverse_=false;
    unsigned course_=0,updates_=0;
    float strength_=0;
};
}
