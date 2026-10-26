#include "original_demo_presentation.h"
#include <iostream>
#include <fstream>
using namespace idas3;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Native root and output required");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    auto data=original::OriginalDemoData::load(root);OriginalDemoPresentation demo;demo.load(root,data);
    Renderer renderer;if(!renderer.initialize(nullptr,960,720,false)||!demo.upload(renderer))throw std::runtime_error(renderer.error);
    Renderer publication;if(!publication.initializeSceneCapture(1280,720)||!demo.upload(publication))throw std::runtime_error(publication.error);
    renderer.nearClip=.01f;renderer.farClip=10000;renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
    std::ofstream report(out/"frames.csv");report<<"frame,shot,vertices,ranges,beam_vertices,glow_ranges\n";
    original::OriginalDemoCursor cursor;
    for(unsigned frame:{250u,534u,535u,700u,1200u,1450u,1800u,2240u,2800u,3500u,4600u,5400u,5550u,5720u}){
        while(cursor.frame<frame)data.step(cursor);
        const auto& camera=data.camera(frame);const auto&m=camera.world;
        const Vec3 eye=Vec3{m[12],m[13],m[14]}+demo.cameraOffset(frame),target=eye-Vec3{m[8],m[9],m[10]};
        renderer.cameraUp={m[4],m[5],m[6]};renderer.verticalFieldOfView=camera.verticalFovRadians();
        const auto&mesh=demo.mesh(data,cursor,frame);
        demo.bindLighting(renderer);
        if(renderer.vehicleLights||renderer.opponentLights)throw std::runtime_error("Broad substitute beam still enabled");
        unsigned beamVertices=0,glowRanges=0;
        for(const auto&r:mesh.ranges){
            if(r.tsp==0x4489a464){beamVertices+=r.count;if(!r.emissive)throw std::runtime_error("Road projection was relit");}
            if(r.courseGeometry&&r.texture==0xffffffffu&&r.gmp==0x6ff){++glowRanges;if(!r.emissive)throw std::runtime_error("Lamp glow is being darkened");}
        }
        // Independent source timeline boundaries, including the staggered off cues.
        const unsigned enabled=frame<535?0:frame<5550?2:frame<5720?1:0;
        if(beamVertices!=24*enabled)throw std::runtime_error("Wrong projected headlight count/timeline");
        if(!renderer.draw(mesh,eye,target,true,false)||!renderer.saveBitmap((out/(std::to_string(frame)+".bmp")).wstring()))throw std::runtime_error(renderer.error);
        demo.bindLighting(publication);publication.cameraUp=renderer.cameraUp;publication.verticalFieldOfView=renderer.verticalFieldOfView;
        if(!publication.draw(mesh,eye,target,true,false))throw std::runtime_error(publication.error);
        const auto& packet=publication.sceneCapture()->frame();
        std::array<unsigned,4> scopes{};unsigned packetBeams=0,packetGlows=0;
        for(unsigned i=0;i<packet.rangeCount;++i){const auto& r=packet.ranges[i];
            if(r.texture!=0xffffffffu&&r.texture>=packet.textureCount)throw std::runtime_error("Unbound texture in Unity intro packet: "+std::to_string(r.texture));
            if(r.lightScope>=scopes.size())throw std::runtime_error("Invalid Unity light scope");
            ++scopes[r.lightScope];
            if(r.tsp==0x4489a464){packetBeams+=r.count;if(!(r.flags&2)||r.lightScope)throw std::runtime_error("Unity relights the road projection");}
            if((r.flags&16)&&r.texture==0xffffffffu&&r.gmp==0x6ff){++packetGlows;if(!(r.flags&2)||r.lightScope)throw std::runtime_error("Unity darkens the fixture glow");}
        }
        if(!scopes[1]||!scopes[2]||!scopes[3]||packetBeams!=beamVertices||packetGlows!=glowRanges)
            throw std::runtime_error("Intro lighting missing from Unity scene publication");
        if(frame==250||frame==535){
            auto ablated=mesh;
            for(auto& r:ablated.ranges){
                if(frame==250&&r.courseGeometry&&r.texture==0xffffffffu&&r.gmp==0x6ff)r.count=0;
                if(frame==535&&r.tsp==0x4489a464)r.count=0;
            }
            if(!renderer.draw(ablated,eye,target,true,false)||!renderer.saveBitmap((out/(std::to_string(frame)+"-disabled.bmp")).wstring()))throw std::runtime_error(renderer.error);
        }
        report<<frame<<','<<cursor.shot<<','<<mesh.vertices.size()<<','<<mesh.ranges.size()<<','<<beamVertices<<','<<glowRanges<<'\n';
    }
    const auto scene=OriginalCourseScene::loadMetadata(root,root/"data/original_models/attract/demo/scene.idasscene");
    std::ofstream chunks(out/"chunks.csv");chunks<<"chunk,batch,vertices,texture,gmp,pcw,tsp,minx,miny,minz,maxx,maxy,maxz\n";
    for(const auto&c:scene.model.chunks){unsigned index=0;for(const auto&b:c.batches){
        Vec3 low{1e9f,1e9f,1e9f},high=-low;
        for(const auto&v:b.vertices){const auto p=v.position;low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
        chunks<<c.index<<','<<index++<<','<<b.vertices.size()<<','<<b.material[9]<<','<<b.material[2]<<','<<b.ich[0]<<','<<b.ich[2]<<','<<low.x<<','<<low.y<<','<<low.z<<','<<high.x<<','<<high.y<<','<<high.z<<'\n';
    }}
    std::cout<<"PASS offscreen attract captures; no window or audio device opened\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
