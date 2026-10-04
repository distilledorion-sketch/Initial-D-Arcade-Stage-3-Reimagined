#pragma once
#include "renderer.h"
#include "original_car_appearance_config.h"
#include "original_matrix.h"

namespace idas3 {
// Recovered bkfire chunks 0/1, exhaust mounts and rotations. The host binds
// the flash to its accepted engine cue instead of adding the original draw
// owner's random draw to gameplay/network state. No extra sound is played.
class BackfirePresentation {
public:
    NativeTextureBank textures;
    void load(const std::filesystem::path& root){
        if(!model_.chunks.empty())return;
        const auto base=root/"data/original_assets/effects/bkfire";
        model_=NativeModel::load(base/"bkfire.idasmesh");textures=NativeTextureBank::load(base/"textures.idastex");
        if(model_.chunks.size()!=9||textures.size()!=8)throw std::runtime_error("Unexpected original backfire assets");
        trig_=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    }
    bool append(Mesh& mesh,int frame,const original::OriginalCarAppearanceConfig& appearance,
            Vec3 body,float yaw,float pitch,float roll,unsigned textureBase)const{
        const unsigned muffler=(appearance.word>>19)&7u;
        if(frame<0||frame>1||appearance.car!=19||muffler<1||muffler>3||model_.chunks.empty())return false;
        //17CB32..72: authored exhaust table and +0.05 depth. Adapt the
        // owner's placement to body-local coordinates: applying its +0.3 Y
        // here puts the flash above the rendered pipe (verified in captures).
        constexpr std::array<Vec3,3> mounts{{{.561f,.0062f,-2.1171f},{.5675f,-.0087f,-2.1164f},{.56f,-.005f,-2.104f}}};
        constexpr std::array<unsigned,3> phase{64285,63764,63305}; //323BD8, rotate Y
        const auto point=mounts[muffler-1]+Vec3{0,0,.05f};
        auto matrix=original::originalIdentityMatrix();
        original::translateOriginalMatrix(matrix,{point.x,point.y,point.z});
        original::rotateOriginalMatrixPhase(matrix,1,std::uint16_t(phase[muffler-1]),trig_);
        if(muffler==3){original::rotateOriginalMatrixPhase(matrix,0,0x059e,trig_);original::scaleOriginalMatrix(matrix,{1.05f,1.05f,1});}
        NativeAssembly assembly;auto& instance=assembly.instances.emplace_back();instance.chunk=unsigned(frame);
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)instance.transform[row*4+col]=matrix.elements[col*4+row];
        const auto first=mesh.ranges.size();mesh.originalCar(model_,assembly,body,yaw,pitch,roll,textureBase);
        for(auto i=first;i<mesh.ranges.size();++i){mesh.ranges[i].emissive=true;mesh.ranges[i].viewMask=1;}
        return true;
    }
private:
    NativeModel model_;
    original::OriginalFscaTable trig_;
};
}
