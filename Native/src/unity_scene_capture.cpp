#include "unity_scene_capture.h"
#include "renderer.h"
#include "unity_ui_capture.h"
#if defined(IDAS3_PORTABLE_SCENE)
#include "../android/scene_matrix.h"
#else
#include <DirectXMath.h>
#endif
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace idas3 {
void UnitySceneCapture::loadTextures(const NativeTextureBank& bank,bool append){
    if(!append)images_.clear();
    for(unsigned i=0;i<bank.size();++i)images_.push_back(bank.at(i));
    textures_.clear();textures_.reserve(images_.size());
    for(const auto& image:images_)textures_.push_back({image.width,image.height,image.argb.size(),image.argb.data()});
    ++frame_.textureGeneration;
}
void UnitySceneCapture::capture(const Renderer& r,const Mesh& mesh,Vec3 eye,Vec3 target,bool night,bool wet,
        const uint32_t* overlay,bool behind,const OriginalShowroomLighting* showroom,const OriginalRearViewFrame* rear,
        std::span<const OverlayPass> foreground){
#if !defined(IDAS3_PORTABLE_SCENE)
    using namespace DirectX;
#endif
    std::size_t vertexCount=mesh.vertices.size();
    if(mesh.borrowCachedGeometry){vertexCount=0;for(const auto& range:mesh.ranges)vertexCount+=range.count;}
    if(vertexCount>2000000)throw std::runtime_error("Scene geometry budget exceeded");
    vertices_.resize(vertexCount);
    static_assert(sizeof(Vertex)==sizeof(Idas3SceneVertex));
    const auto previousRanges=ranges_.size();
    static const bool disableIdentities=std::getenv("IDAS3_GEOMETRY_IDS_OFF")!=nullptr;
    if(disableIdentities&&!mesh.borrowCachedGeometry&&!vertices_.empty())std::memcpy(vertices_.data(),mesh.vertices.data(),vertices_.size()*sizeof(Vertex));
    ranges_.resize(mesh.ranges.size());geometryIds_.resize(mesh.ranges.size());
    std::size_t covered=0;bool contiguous=true;
    for(unsigned i=0;i<mesh.ranges.size();++i){const auto& s=mesh.ranges[i];
        if((!s.borrowedVertices&&size_t(s.first)+s.count>mesh.vertices.size())||s.carLighting>2)throw std::runtime_error("Invalid captured mesh range");
        const auto first=mesh.borrowCachedGeometry?std::uint32_t(covered):s.first;
        const auto* source=s.borrowedVertices?s.borrowedVertices:mesh.vertices.data()+s.first;
        contiguous=contiguous&&first==covered;covered=size_t(first)+s.count;
        // Keep an owned snapshot for managed readers, but do not copy static
        // course bytes again when the identical range still occupies this slot.
        // Changed order, slices, merged ranges and all dynamic geometry copy.
        const bool retained=(disableIdentities&&!mesh.borrowCachedGeometry)||(!disableIdentities&&s.geometryId&&i<previousRanges&&geometryIds_[i]==s.geometryId&&
            ranges_[i].first==first&&ranges_[i].count==s.count);
        if(!retained&&s.count)std::memcpy(vertices_.data()+first,source,size_t(s.count)*sizeof(Vertex));
        static const bool validateRanges=std::getenv("IDAS3_VALIDATE_GEOMETRY_IDS")!=nullptr;
        if(validateRanges&&s.count&&std::memcmp(vertices_.data()+first,source,size_t(s.count)*sizeof(Vertex)))
            throw std::runtime_error("Cached scene range differs from source geometry");
        Idas3SceneRange range{first,s.count,s.texture,s.tsp,s.pcw,s.isp,s.gmp,
            unsigned(s.original)|(unsigned(s.emissive)<<1)|(unsigned(s.billboard)<<2)|(unsigned(bool(s.originalLightDirection))<<3)|(unsigned(s.courseGeometry)<<4)|(unsigned(s.sourceFaceCulling)<<5),
            s.gloss,s.carLighting?s.carLighting+1:s.courseLighting?1u:0u,s.viewMask,i,{}};
        if(s.originalLightDirection)std::copy(s.originalLightDirection->begin(),s.originalLightDirection->end(),range.lightDirection);
        ranges_[i]=range;
        geometryIds_[i]=s.geometryId;
    }
    if(!mesh.borrowCachedGeometry&&(!contiguous||covered!=mesh.vertices.size())&&!vertices_.empty())
        std::memcpy(vertices_.data(),mesh.vertices.data(),vertices_.size()*sizeof(Vertex));
    static const bool validateIdentities=std::getenv("IDAS3_VALIDATE_GEOMETRY_IDS")!=nullptr;
    if(validateIdentities&&!mesh.borrowCachedGeometry&&!vertices_.empty()&&std::memcmp(vertices_.data(),mesh.vertices.data(),vertices_.size()*sizeof(Vertex)))
        throw std::runtime_error("Cached scene snapshot differs from source geometry");
    overlays_.clear();
    if(overlay)overlays_.push_back({overlay,unsigned(r.width),unsigned(r.height),unsigned(behind),0});
    for(const auto& pass:foreground)if(pass.pixels)overlays_.push_back({pass.pixels,unsigned(pass.originalCanvas?640:r.width),unsigned(pass.originalCanvas?480:r.height),unsigned(pass.additive)*2u+unsigned(pass.originalCanvas)*4u,0});
    for(const auto& pass:overlays_)unityUiSubmit(pass.surface,int(pass.width),int(pass.height),(pass.flags&1)!=0,(pass.flags&2)!=0,(pass.flags&4)!=0);
    frame_.width=r.width;frame_.height=r.height;frame_.vertexCount=unsigned(vertices_.size());frame_.rangeCount=unsigned(ranges_.size());
    frame_.textureCount=unsigned(textures_.size());frame_.overlayCount=unsigned(overlays_.size());frame_.viewCount=rear&&!showroom?2:1;
    frame_.screenFadeArgb=r.screenFadeArgb;frame_.vertices=vertices_.data();frame_.ranges=ranges_.data();frame_.textures=textures_.data();frame_.overlays=overlays_.data();
    frame_.frameConstants=frameConstants_.data();frame_.lightConstants=lightConstants_.data();frame_.fogConstants=fogConstants_.data();
    frameConstants_.fill(0);lightConstants_.fill(0);fogConstants_.fill(0);
    const float fit=std::min(float(r.width)/640,float(r.height)/480),ox=(r.width-640*fit)*.5f,oy=(r.height-480*fit)*.5f;
    auto& camera=frame_.cameras[0];camera={};
    std::memcpy(camera.eye,&eye,12);std::memcpy(camera.target,&target,12);std::memcpy(camera.up,&r.cameraUp,12);
    camera.viewport[2]=float(r.width);camera.viewport[3]=float(r.height);
    if((overlay&&behind)||r.fitOriginalViewport){camera.viewport[0]=ox;camera.viewport[1]=oy;camera.viewport[2]=640*fit;camera.viewport[3]=480*fit;}
    if(r.sceneViewport.width>0&&r.sceneViewport.height>0){
        camera.viewport[0]=ox+r.sceneViewport.x*fit;camera.viewport[1]=oy+r.sceneViewport.y*fit;
        camera.viewport[2]=r.sceneViewport.width*fit;camera.viewport[3]=r.sceneViewport.height*fit;
        camera.leftHanded|=2;
    }
    camera.verticalFov=r.verticalFieldOfView;camera.aspect=r.projectionAspect>0?r.projectionAspect:camera.viewport[2]/camera.viewport[3];camera.nearClip=r.nearClip;camera.farClip=r.farClip;
    frame_.cameras[1]={};
    if(frame_.viewCount==2){auto& c=frame_.cameras[1];
        std::memcpy(c.eye,&rear->eye,12);std::memcpy(c.target,&rear->target,12);std::memcpy(c.up,&rear->up,12);
        c.verticalFov=rear->verticalFieldOfView;c.aspect=5;c.nearClip=.2f;c.farClip=5000;c.leftHanded=1;
        c.viewport[0]=ox+rear->left*fit;c.viewport[1]=oy+rear->top*fit;c.viewport[2]=rear->width*fit;c.viewport[3]=rear->height*fit;
    }
    Color sky=night?Color{.025f,.045f,.075f,1}:wet?Color{.25f,.31f,.36f,1}:Color{.34f,.43f,.50f,1};
    if(r.overrideClearColor)sky=r.clearColor;
    for(unsigned v=0;v<frame_.viewCount;++v){
        const auto& c=frame_.cameras[v];const Vec3 e{c.eye[0],c.eye[1],c.eye[2]},t{c.target[0],c.target[1],c.target[2]},u{c.up[0],c.up[1],c.up[2]};
        const auto f=normalized(t-e),right=(c.leftHanded&1)?normalized(cross(u,f)):normalized(cross(f,u)),up=(c.leftHanded&1)?normalized(cross(f,right)):normalized(cross(right,f));
#if defined(IDAS3_PORTABLE_SCENE)
        const auto view=portable::lookAt(e,t,u,(c.leftHanded&1)!=0);
        const auto projection=portable::perspective(c.verticalFov,c.aspect,c.nearClip,c.farClip,(c.leftHanded&1)!=0);
        auto* data=frameConstants_.data()+92*v;auto matrix=portable::multiply(view,projection);std::memcpy(data,&matrix,64);
#else
        const auto ev=XMVectorSet(e.x,e.y,e.z,1),tv=XMVectorSet(t.x,t.y,t.z,1),uv=XMVectorSet(u.x,u.y,u.z,0);
        const auto view=(c.leftHanded&1)?XMMatrixLookAtLH(ev,tv,uv):XMMatrixLookAtRH(ev,tv,uv);
        const auto projection=(c.leftHanded&1)?XMMatrixPerspectiveFovLH(c.verticalFov,c.aspect,c.nearClip,c.farClip):XMMatrixPerspectiveFovRH(c.verticalFov,c.aspect,c.nearClip,c.farClip);
        auto* data=frameConstants_.data()+92*v;XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,view*projection);std::memcpy(data,&matrix,64);
#endif
        auto set=[&](unsigned row,float x,float y,float z,float w){data[row*4]=x;data[row*4+1]=y;data[row*4+2]=z;data[row*4+3]=w;};
        set(4,e.x,e.y,e.z,1);set(5,sky.r,sky.g,sky.b,night?1.f:0.f);
        set(6,r.vehiclePosition.x,r.vehiclePosition.y+.6f,r.vehiclePosition.z,float(r.vehicleLights));
        set(7,r.vehicleForward.x,r.vehicleForward.y,r.vehicleForward.z,float(night&&r.supplementalCourseLampLighting));
        set(8,right.x,right.y,right.z,0);set(9,up.x,up.y,up.z,0);
        set(12,r.opponentPosition.x,r.opponentPosition.y+.6f,r.opponentPosition.z,float(r.opponentLights));set(13,r.opponentForward.x,r.opponentForward.y,r.opponentForward.z,0);
        if(showroom){const auto& p=showroom->parameters;set(10,p.directionToLight.x,p.directionToLight.y,p.directionToLight.z,1);set(11,p.ambientBase,p.diffuseSpecularFactor,0,0);set(22,p.color.x,p.color.y,p.color.z,1);}
        if(night&&!showroom&&r.courseLampLighting){
            std::array<std::pair<float,Vec3>,8> nearest;for(auto& n:nearest)n.first=std::numeric_limits<float>::max();
            // Native mirror reuses the MAIN eye's chosen eight lamps.
            for(const auto& p:r.courseLampPositions){const float d=dot(p-eye,p-eye);for(unsigned i=0;i<8;++i)if(d<nearest[i].first){for(unsigned j=7;j>i;--j)nearest[j]=nearest[j-1];nearest[i]={d,p};break;}}
            for(unsigned i=0;i<8;++i)if(nearest[i].first<std::numeric_limits<float>::max()){const auto p=nearest[i].second;set(14+i,p.x,p.y,p.z,1.f/(26.f*26.f));}
        }
        const std::array<const original::OriginalCourseLighting*,4> lights{nullptr,r.courseLighting,r.playerLighting,r.rivalLighting};
#if defined(IDAS3_PORTABLE_SCENE)
        matrix=view;
#else
        XMStoreFloat4x4(&matrix,view);
#endif
        original::OriginalLightMatrix sourceView;std::memcpy(sourceView.data(),&matrix,64);
        for(unsigned scope=0;scope<4;++scope){auto* words=lightConstants_.data()+(v*4+scope)*156;std::memcpy(words,&matrix,64);
            if(!lights[scope]||showroom)continue;const auto packet=original::originalCourseLightingPacket(*lights[scope],sourceView);
            std::memcpy(words+16,packet.glm.data(),32);std::memcpy(words+24,packet.lights.data(),512);words[152]=1;words[153]=packet.count;
        }
    }
    if(r.courseFog&&!showroom){const auto& fog=*r.courseFog;const auto rgb=fog.colorRgb;
        const std::array<float,8> values{float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255,original::originalFogDecodedDensity(fog.packedDensity),
            r.originalVertexFogColor.r,r.originalVertexFogColor.g,r.originalVertexFogColor.b,1};
        std::memcpy(fogConstants_.data(),values.data(),32);for(unsigned i=0;i<128;++i)fogConstants_[8+i]=fog.table[i];
    }
    // Alpha reference travels in the spare fourth component of frame light terms.
    for(unsigned v=0;v<2;++v)frameConstants_[v*92+47]=float(r.originalAlphaReference);
    ++frame_.frameGeneration;
}
}
