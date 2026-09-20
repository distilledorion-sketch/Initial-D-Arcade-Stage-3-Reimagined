#include "original_tuning_preview.h"
#include "original_demo_presentation.h"
#include <fstream>
#include <iostream>
#include <cstring>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and output directory required");
    const std::filesystem::path root=argv[1],out=argv[2];
    auto profile=original::makeOriginalFreshBattleProfile();
    original::OriginalTuningPreviewPresentation menu;menu.load(root,profile,false,true);
    std::ofstream ranges(out/"menu-ranges.csv");ranges<<"range,first,count,texture,pcw,isp,tsp,gmp,minx,miny,minz,maxx,maxy,maxz\n";
    const auto dump=[&](const Mesh& mesh,std::ofstream& file){unsigned index=0;
        for(const auto& r:mesh.ranges){Vec3 lo{1e9f,1e9f,1e9f},hi=-lo;
            for(unsigned j=r.first;j<r.first+r.count;++j){const auto p=mesh.vertices[j].position;
                lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
                hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
            file<<index++<<','<<r.first<<','<<r.count<<','<<r.texture<<','<<r.pcw<<','<<r.isp<<','<<r.tsp<<','<<r.gmp<<','<<lo.x<<','<<lo.y<<','<<lo.z<<','<<hi.x<<','<<hi.y<<','<<hi.z<<'\n';}
    };
    dump(menu.mesh(),ranges);
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    auto data=original::OriginalDemoData::load(root);OriginalDemoPresentation demo;demo.load(root,data);
    std::ofstream cameras(out/"camera-clearance.csv");cameras<<"shot,frame,pullback,bodyClearance\n";
    unsigned cameraChecks=0,adjustedShots=0;
    for(unsigned s=0;s<data.shots().size();++s){const auto& shot=data.shots()[s];
        const float pullback=length(demo.cameraOffset(shot.frames[2]));adjustedShots+=pullback>0;
        for(unsigned f=shot.frames[2];f<=shot.frames[3];++f){
            const auto offset=demo.cameraOffset(f);++cameraChecks;
            require(std::isfinite(length(offset))&&length(offset)<3&&offset.y==0,"Camera pullback invalid or moved road clearance");
            require(std::abs(length(offset)-pullback)<.00001f,"Camera clearance pumps within a shot");
            if(f%10!=0&&f!=shot.frames[2]&&f!=shot.frames[3])continue;
            const auto& mesh=demo.mesh(data,{s,f},f);const auto& m=data.camera(f).world;
            const Vec3 eye=Vec3{m[12],m[13],m[14]}+offset;
            Vec3 low[2]={{1e9f,1e9f,1e9f},{1e9f,1e9f,1e9f}},high[2]={-low[0],-low[1]};
            // Independently bound actual emitted triangles in each car's yaw
            // frame. Their vertices already include ride height/pitch/roll.
            for(const auto& r:mesh.ranges)if(r.sourceFaceCulling&&!r.billboard){
                for(unsigned j=r.first;j<r.first+r.count;++j){const auto v=mesh.vertices[j].position;
                    unsigned slot=0;float nearest=1e9f;
                    for(unsigned k=0;k<2;++k){const auto& a=data.actor(f,k);const float d=length(v-Vec3{a.f(0),a.f(4),a.f(8)});if(d<nearest){nearest=d;slot=k;}}
                    const auto& a=data.actor(f,slot);const auto d=v-Vec3{a.f(0),a.f(4),a.f(8)};
                    const Vec3 p{dot(d,right(a.f(28))),d.y,dot(d,forward(a.f(28)))};
                    low[slot]={std::min(low[slot].x,p.x),std::min(low[slot].y,p.y),std::min(low[slot].z,p.z)};
                    high[slot]={std::max(high[slot].x,p.x),std::max(high[slot].y,p.y),std::max(high[slot].z,p.z)};
                }
            }
            float clearance=1e9f;
            for(unsigned slot=0;slot<2;++slot){const auto& a=data.actor(f,slot);const auto d=eye-Vec3{a.f(0),a.f(4),a.f(8)};
                const Vec3 p{dot(d,right(a.f(28))),d.y,dot(d,forward(a.f(28)))};
                const Vec3 gap{std::max({low[slot].x-p.x,0.f,p.x-high[slot].x}),std::max({low[slot].y-p.y,0.f,p.y-high[slot].y}),std::max({low[slot].z-p.z,0.f,p.z-high[slot].z})};
                clearance=std::min(clearance,length(gap));++cameraChecks;
            }
            cameras<<s<<','<<f<<','<<pullback<<','<<clearance<<'\n';
            require(clearance>.1f,"Attract camera intersects emitted car envelope");
        }
    }
    require(length(demo.cameraOffset(250))>.3f,"Reported side pass was not corrected");
    require(length(demo.cameraOffset(0))==0&&length(demo.cameraOffset(480))==0,"Unobstructed adjacent shots moved");
    original::OriginalDemoCursor cursor;while(cursor.frame<250)data.step(cursor);
    std::ofstream intro(out/"intro-ranges.csv");intro<<"range,first,count,texture,pcw,isp,tsp,gmp,minx,miny,minz,maxx,maxy,maxz\n";
    dump(demo.mesh(data,cursor,cursor.frame),intro);
    std::cout<<"Attract: "<<cameraChecks<<" camera checks, "<<adjustedShots<<" corrected shots; emitted body clearance, horizontal-only and constant shot correction passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
