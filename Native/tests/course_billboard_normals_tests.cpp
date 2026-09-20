#include "renderer.h"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace idas3;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("asset-root normal-fixture required");
    std::ifstream input(argv[2]);std::string file;unsigned count,batches=0,vertices=0,lit=0;
    while(input>>std::quoted(file)>>count){
        const auto model=NativeModel::load(std::filesystem::path(argv[1])/file);
        for(unsigned ci=0;ci<count;++ci){unsigned chunk;if(!(input>>chunk))throw std::runtime_error("Incomplete billboard normal fixture");
            auto piece=model;piece.chunks={model.chunks.at(chunk)};
            NativeAssembly assembly;assembly.instances={{0,{1,0,0,123,0,1,0,456,0,0,1,789,0,0,0,1},true}};
            Mesh mesh;mesh.originalCar(piece,assembly,{},0);std::size_t cursor=0;
            for(const auto& batch:piece.chunks[0].batches){++batches;if(!(batch.material[2]&512))++lit;
                for(auto index:batch.indices){const auto& source=batch.vertices[index];const auto& actual=mesh.vertices.at(cursor++);++vertices;
                    if(actual.normal.x!=123||actual.normal.y!=456||actual.normal.z!=789||actual.position.x!=source.position.x||actual.position.y!=source.position.y)
                        throw std::runtime_error("Billboard geometry or anchor changed");
                    if(!(batch.material[2]&512)){
                        if(batch.ich[6]!=0xa||source.position.z!=0)throw std::runtime_error("Unexpected original spectator vertex format");
                        const unsigned packed=unsigned(actual.position.z*16777216.f);
                        if(packed!=(source.header&0xffffffu))throw std::runtime_error("Source packed normal bytes did not round-trip");
                        Vec3 decoded{float(std::int8_t(packed&255))/127.f,float(std::int8_t((packed>>8)&255))/127.f,float(std::int8_t((packed>>16)&255))/127.f};
                        if(decoded.x!=source.normal.x||decoded.y!=source.normal.y||decoded.z!=source.normal.z)throw std::runtime_error("Billboard source normal changed");
                        // Main and reflected cameras use separate billboard
                        // bases. Retaining the exact local normal is required
                        // in both; source normals need not point along Z.
                        for(float handedness:{-1.f,1.f}){
                            const Vec3 right{.8f,0,.6f},up{0,1,0},facing=cross(right,up)*handedness;
                            const auto original=right*source.normal.x+up*source.normal.y+facing*source.normal.z;
                            const auto rendered=right*decoded.x+up*decoded.y+facing*decoded.z;
                            if(original.x!=rendered.x||original.y!=rendered.y||original.z!=rendered.z)throw std::runtime_error("Billboard view normal changed");
                        }
                    }else if(actual.position.z!=source.position.z)throw std::runtime_error("Unlit billboard changed");
                }
            }
        }
    }
    if(!input.eof()||batches!=165)throw std::runtime_error("Incomplete original billboard material coverage");
    std::cout<<"PASS "<<batches<<" source batches ("<<lit<<" lit), "<<vertices<<" drawn vertices; exact normal bytes, main/mirror directions, anchors and positions\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
