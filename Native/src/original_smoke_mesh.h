#pragma once
#include "native_assets.h"
#include "original_smoke_display.h"

namespace idas3::original {
// Texture 1 is the original linear128x128 ARGB4444 density kernel. The
// existing runtime bank expands each four-bit channel by17; recover the
// exact green nibble sampled by1400D4 without shipping another texture copy.
inline std::vector<std::uint16_t> originalSmokeDensityKernel(const NativeImage& image){
    if(image.width!=128||image.height!=128||image.argb.size()!=128*128)
        throw std::invalid_argument("Original smoke kernel dimensions");
    std::vector<std::uint16_t> kernel;kernel.reserve(image.argb.size());
    for(auto color:image.argb){
        const auto green=(color>>8)&255u;
        if(green%17u||(color&255u)!=green||((color>>16)&255u)!=green)
            throw std::invalid_argument("Original smoke kernel channel encoding");
        kernel.push_back(std::uint16_t((green/17u)<<4));
    }
    return kernel;
}
// The authored smoke meshes encode integer grid coordinates in their UVs.
// 0D1E60 records (trunc(U), trunc(-V)), shifts negative minima to zero, then
// stores row*width+column. This is not a projection of the vertex positions.
struct OriginalSmokeGridMapping {
    unsigned width=0,height=0;
    std::vector<std::vector<unsigned>> cells;
};
inline OriginalSmokeGridMapping mapOriginalSmokeGrid(const NativeModelChunk& source,
        unsigned width,unsigned height){
    if(width<2||height<2||width>32||height>32||source.batches.empty())
        throw std::invalid_argument("Original smoke mesh dimensions");
    int minimumX=10000000,minimumY=10000000;
    std::vector<std::vector<std::array<int,2>>> coordinates;
    for(const auto& batch:source.batches){
        // All three original smoke grids are packed-normal UV (0xA).
        if(batch.ich[6]!=0xau||batch.vertices.empty())
            throw std::invalid_argument("Original smoke mesh vertex layout");
        auto& points=coordinates.emplace_back();points.reserve(batch.vertices.size());
        for(const auto& vertex:batch.vertices){
            // Authored UVs are small finite integers. Reject corrupt asset data
            // before converting; source FTRC truncation is retained in range.
            if(!std::isfinite(vertex.u)||!std::isfinite(vertex.v)||
               std::abs(vertex.u)>1000000.f||std::abs(vertex.v)>1000000.f)
                throw std::invalid_argument("Original smoke mesh grid coordinate");
            const int x=int(vertex.u),y=int(-vertex.v);
            points.push_back({x,y});minimumX=(std::min)(minimumX,x);minimumY=(std::min)(minimumY,y);
        }
    }
    OriginalSmokeGridMapping mapping{width,height,{}};
    const int shiftX=minimumX<0?-minimumX:0,shiftY=minimumY<0?-minimumY:0;
    for(const auto& points:coordinates){
        auto& cells=mapping.cells.emplace_back();cells.reserve(points.size());
        for(const auto& point:points){
            const int x=point[0]+shiftX,y=point[1]+shiftY;
            if(x<0||y<0||unsigned(x)>=width||unsigned(y)>=height)
                throw std::invalid_argument("Original smoke mesh outside density grid");
            cells.push_back(unsigned(y)*width+unsigned(x));
        }
    }
    return mapping;
}

struct OriginalSmokeMeshDraw {
    NativeModelChunk chunk;
    std::array<float,3> translation{},scale{};
};
inline OriginalSmokeMeshDraw prepareOriginalSmokeMesh(const NativeModelChunk& source,
        const OriginalSmokeGridMapping& mapping,const OriginalSmokeDisplay& display,
        float focalScale,std::uint32_t color){
    if(!display.visible||!std::isfinite(focalScale)||focalScale<=0||
       mapping.cells.size()!=source.batches.size()||
       display.density.size()!=std::size_t(mapping.width)*mapping.height)
        throw std::invalid_argument("Original smoke mesh display binding");
    OriginalSmokeMeshDraw draw{source,display.position,
        {(display.width/focalScale)*-display.position[2],
         (display.height/focalScale)*-display.position[2],1.f}};
    for(std::size_t b=0;b<draw.chunk.batches.size();++b){
        auto& batch=draw.chunk.batches[b];const auto& cells=mapping.cells[b];
        if(cells.size()!=batch.vertices.size()||batch.ich[6]!=0xau)
            throw std::invalid_argument("Original smoke mesh mapping mismatch");
        for(std::size_t i=0;i<cells.size();++i){
            if(cells[i]>=display.density.size())throw std::invalid_argument("Original smoke mesh density index");
            // 13FA6A supplies U=.9, V=-1 to 0D1640. The negative V selects
            // the density array in 0D19A0; density is a texture coordinate,
            // not vertex alpha. Keep the authored texture/material binding.
            batch.vertices[i].u=std::bit_cast<float>(0x3f666666u);
            batch.vertices[i].v=display.density[cells[i]];
        }
        // 1401E0 applies these packet changes only for a non-white mask.
        if(color!=0xffffffffu){
            batch.ich[2]=(batch.ich[2]&0x03ffffffu)|0x981000c0u;
            batch.material[3]&=color;batch.material[5]&=color;
        }
    }
    return draw;
}
}
