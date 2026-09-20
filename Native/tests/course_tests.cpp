#include "course.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void writeU32(std::ofstream& f,std::uint32_t n) { for(int i=0;i<4;++i) f.put(static_cast<char>(n>>(8*i))); }
void writePath(const std::filesystem::path& file,float x) {
    std::ofstream f(file,std::ios::binary);writeU32(f,3);writeU32(f,3);
    for(Vec3 p : {Vec3{x,10,0},Vec3{x,11,10},Vec3{x,12,20}})
        for(float v:{p.x,p.y,p.z}) writeU32(f,std::bit_cast<std::uint32_t>(v));
}
void writeCustom(const std::filesystem::path& file,const std::vector<Vec3>& points) {
    std::ofstream f(file,std::ios::binary);writeU32(f,static_cast<std::uint32_t>(points.size()));writeU32(f,3);
    for(auto p:points) for(float v:{p.x,p.y,p.z}) writeU32(f,std::bit_cast<std::uint32_t>(v));
}
bool rejects(const std::filesystem::path& folder) {
    try { Course::load(folder,"test");return false;} catch(const std::runtime_error&) {return true;}
}
}
int main(int argc,char** argv) {
    try {
        const auto folder=std::filesystem::temp_directory_path()/"idas3_course_import_test";
        std::filesystem::create_directories(folder);
        writePath(folder/"test_path.bin",0);writePath(folder/"test_path_l.bin",-5);writePath(folder/"test_path_r.bin",5);
        auto c=Course::load(folder,"test","Test");
        require(c.points.size()==3,"Point count");
        require(std::abs(c.length-2*std::sqrt(101.f))<1e-4,"3D authored length");
        require(std::abs(c.sample(c.length*.5f).center.z-10)<1e-4,"Distance sampling");
        require(c.sample(-50).center.z==0 && c.sample(500).center.z==20,"Endpoint clamp");
        auto projection=c.project({2,11,10});
        require(std::abs(projection.lateral-2)<1e-4,"Right-positive projection");
        require(std::abs(projection.squaredDistance-4)<1e-4,"Projection distance");
        auto reversed=Course::load(folder,"test","Test",true);
        require(reversed.points.front().z==20 && reversed.sample(0).tangent.z<0,"Reverse travel");
        require(reversed.sample(0).right.x<0,"Reversed boundary ordering");
        require(reversed.provenanceSha256==c.provenanceSha256,"Reversing must retain source provenance");
        {std::ofstream f(folder/"test_path.bin",std::ios::binary);writeU32(f,0xffffffff);writeU32(f,3);}
        require(rejects(folder),"Reject invalid count/header");
        writeCustom(folder/"test_path.bin",{{0,0,0},{0,std::numeric_limits<float>::quiet_NaN(),10},{0,0,20}});
        require(rejects(folder),"Reject non-finite coordinates");
        writePath(folder/"test_path.bin",0);
        writeCustom(folder/"test_path_r.bin",{{5,0,0},{5,0,20}});
        require(rejects(folder),"Reject mismatched edge counts");
        writeCustom(folder/"test_path.bin",{{0,0,0},{0,0,0},{0,0,20}});
        writePath(folder/"test_path_r.bin",5);
        require(rejects(folder),"Reject duplicate interior points");
        // Optional real-data validation: all files must parse without graphics.
        if(argc>1) {
            const char* ids[]={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu"};
            const std::size_t counts[]={1069,1401,3301,4089,2080,2951,3301,3729};
            for(int i=0;i<8;++i) {
                auto real=Course::load(argv[1],ids[i]);
                require(real.points.size()==counts[i],"Original course record count");
                auto mid=real.sample(real.length*.5f);
                auto onPath=real.project(mid.center,mid.segmentIndex);
                require(onPath.squaredDistance<.001,"Real centerline projection round trip");
                require(real.provenanceSha256.size()==64,"SHA256 length");
                auto reverse=Course::load(argv[1],ids[i],ids[i],true);
                require(reverse.provenanceSha256==real.provenanceSha256,"Reverse original source hash");
                require(idas3::length(reverse.points.front()-real.points.back())<1e-5,"Reverse original endpoints");
                auto reverseMid=reverse.sample(reverse.length*.5f);
                auto reverseProjection=reverse.project(reverseMid.center,reverseMid.segmentIndex);
                require(reverseProjection.squaredDistance<.001,"Reverse real projection round trip");
                if(real.closed) {
                    auto beginning=real.project(real.points.front(),0);
                    auto ending=real.project(real.points.back(),real.points.size()-2);
                    require(beginning.sample.distance<.01,"Closed start must not become finish");
                    require(ending.sample.distance>real.length-.01,"Closed finish must not become start");
                    // A physics step can pass beyond the seam, where segment
                    // zero is strictly nearer rather than merely tied. Keep
                    // this lap's endpoint until the race owns the wrap.
                    for(const Course* loop:{&real,&reverse}) {
                        auto crossedFinish=loop->project(loop->sample(.5f).center,loop->points.size()-2);
                        require(crossedFinish.sample.distance>loop->length-.01,"Crossing closed finish must preserve completed lap");
                        auto beforeStart=loop->project(loop->sample(loop->length-.5f).center,0);
                        require(beforeStart.sample.distance<.01,"Crossing backward before start must preserve lap start");
                        auto resetLap=loop->project(loop->sample(.5f).center,0);
                        require(std::abs(resetLap.sample.distance-.5f)<.01,"Explicit hint reset permits next lap");
                    }
                }
                std::cout<<ids[i]<<" "<<real.points.size()<<" "<<real.length<<" "<<real.provenanceSha256<<"\n";
            }
        }
        std::cout<<"Course importer tests passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
