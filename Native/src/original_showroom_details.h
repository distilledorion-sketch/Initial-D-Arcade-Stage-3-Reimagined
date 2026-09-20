#pragma once
#include "native_assets.h"
#include "original_matrix.h"
#include <bit>
#include <stdexcept>

namespace idas3 {
// Source 10FA00. The model is data/original_models/showroom/selcrs.idasmesh.
// Its shadow chunk4 is untextured, retaining the authored alpha gradient.
struct OriginalShowroomShadow {
    static constexpr std::uint32_t chunk=4;
    static Vec3 scale(unsigned car) {
        if(car>=dimensions.size())throw std::out_of_range("Original showroom car ID");
        float x=std::bit_cast<float>(dimensions[car][0]);
        float z=std::bit_cast<float>(dimensions[car][1]);
        x*=1.6f; x*=.9f; z*=1.9f; z*=.9f;
        return {x,1,z};
    }
    static NativeAssembly assembly(unsigned car,float yaw,const original::OriginalFscaTable& table) {
        auto matrix=original::originalActorMatrix({0,.01f,0},{0,yaw,0},table);
        auto s=scale(car);
        for(unsigned i=0;i<4;++i) {matrix.elements[i]*=s.x;matrix.elements[8+i]*=s.z;}
        NativeAssembly result;
        auto emit=[&](){NativeModelInstance instance;instance.chunk=chunk;
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
                instance.transform[row*4+col]=matrix.elements[col*4+row];
            result.instances.push_back(instance);};
        emit();original::translateOriginalMatrix(matrix,{0,.02f,0});emit();
        return result;
    }
private:
    static constexpr std::array<std::array<std::uint32_t,2>,35> dimensions={{{0x3f3ced91u,0x4009eb85u},{0x3f3ced91u,0x4009eb85u},{0x3f3ced91u,0x4009eb85u},{0x3f5851ecu,0x4000a3d7u},{0x3f58d4feu,0x3ffb851fu},{0x3f58d4feu,0x3ffb851fu},{0x3f58d4feu,0x3ffb851fu},{0x3f608312u,0x400f0a3du},{0x3f62d0e5u,0x4011db23u},{0x3f5851ecu,0x4011999au},{0x3f5ae148u,0x400e6666u},{0x3f5ae148u,0x400e6666u},{0x3f5ba5e3u,0x400c6a7fu},{0x3f5851ecu,0x40172b02u},{0x3f5851ecu,0x40172b02u},{0x3f48b439u,0x40081062u},{0x3f547ae1u,0x400178d5u},{0x3f52b021u,0x400d0e56u},{0x3f558106u,0x3ffeb852u},{0x3f4ccccdu,0x400a6e98u},{0x3f5a5e35u,0x400a3d71u},{0x3f5a5e35u,0x400a3d71u},{0x3f639581u,0x400e147bu},{0x3f639581u,0x400e147bu},{0x3f4ccccdu,0x400a1cacu},{0x3f3ced91u,0x3ffae148u},{0x3f4b020cu,0x3fff7ceeu},{0x3f5c28f6u,0x400c28f6u},{0x3f5c28f6u,0x400c28f6u},{0x3f5c28f6u,0x400c28f6u},{0x3f228f5cu,0x3fd16873u},{0x3f62d0e5u,0x4011db23u},{0x3f5fa440u,0x400d096cu},{0x3f6075f7u,0x400a978du},{0x3f5d35a8u,0x400a233au}}};
};
}
