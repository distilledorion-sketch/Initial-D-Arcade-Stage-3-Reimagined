#include "original_collision.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <span>

namespace idas3::original {
namespace {
float asFloat(std::uint32_t value){return std::bit_cast<float>(value);}
std::uint32_t read32(std::span<const std::uint8_t> bytes,std::size_t offset){
    if(offset>bytes.size()||bytes.size()-offset<4)throw std::runtime_error("Truncated RCL1 field");
    return std::uint32_t(bytes[offset])|(std::uint32_t(bytes[offset+1])<<8)|
        (std::uint32_t(bytes[offset+2])<<16)|(std::uint32_t(bytes[offset+3])<<24);
}
std::int32_t outsideTriangleEdge(const OriginalCollisionData& data,std::int32_t index,
        std::int32_t previous,float x,float z){
    const auto& triangle=data.triangles.at(std::size_t(index));
    auto vertex=[&](std::int16_t i)->const std::array<std::uint32_t,8>&{return data.vertices.at(std::size_t(i));};
    const auto& last=vertex(triangle[2]);float previousX=asFloat(last[0]),previousZ=asFloat(last[2]);
    for(std::int32_t edge=0;edge<3;++edge){
        const auto& next=vertex(triangle[edge]);const float nextX=asFloat(next[0]),nextZ=asFloat(next[2]);
        if(triangle[3+edge]!=previous){
            const float cross=(nextZ-previousZ)*(x-previousX)-(nextX-previousX)*(z-previousZ);
            if(0.0f>cross)return edge;
        }
        previousX=nextX;previousZ=nextZ;
    }
    return -1;
}
}
OriginalCollisionData OriginalCollisionData::load(const std::filesystem::path& source){
    std::ifstream file(source,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("Original RCL1 collision data unavailable");
    const auto length=file.tellg();if(length<48||length>16*1024*1024)throw std::runtime_error("Invalid RCL1 collision size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),length))throw std::runtime_error("Truncated RCL1 collision data");
    if(read32(bytes,0)!=0x52434C31u||read32(bytes,4)!=1)throw std::runtime_error("Unsupported original collision format");
    if(read32(bytes,32)!=0)throw std::runtime_error("RCL1 optional section needs an explicit decoder");
    OriginalCollisionData out;std::size_t cursor=48;
    const auto decode=[&](auto& target,std::size_t countOffset,std::size_t offsetOffset,std::size_t stride){
        const auto count=read32(bytes,countOffset),offset=read32(bytes,offsetOffset);
        if(count>1000000||offset!=cursor||std::uint64_t(count)*stride>bytes.size()-cursor)throw std::runtime_error("Invalid RCL1 section bounds");
        target.resize(count);
        for(std::size_t i=0;i<count;++i)for(std::size_t word=0;word<stride/4;++word)target[i][word]=read32(bytes,cursor+i*stride+word*4);
        cursor+=count*stride;
    };
    decode(out.materials,8,12,36);decode(out.vertices,16,20,32);
    const auto triangles=read32(bytes,24),offset=read32(bytes,28);
    if(triangles>1000000||offset!=cursor||std::uint64_t(triangles)*16>bytes.size()-cursor)throw std::runtime_error("Invalid RCL1 triangle bounds");
    out.triangles.resize(triangles);
    for(std::size_t i=0;i<triangles;++i)for(std::size_t pair=0;pair<4;++pair){
        const auto word=read32(bytes,cursor+i*16+pair*4);
        out.triangles[i][pair*2]=std::bit_cast<std::int16_t>(std::uint16_t(word));
        out.triangles[i][pair*2+1]=std::bit_cast<std::int16_t>(std::uint16_t(word>>16));
    }
    cursor+=triangles*16;
    if(read32(bytes,36)!=cursor)throw std::runtime_error("Unexpected RCL1 optional-section offset");
    decode(out.coarseCells,40,44,56);
    if(cursor!=bytes.size())throw std::runtime_error("Unexpected RCL1 trailing data");
    for(const auto& triangle:out.triangles)for(std::size_t i=0;i<3;++i)
        if(triangle[i]<0||std::size_t(triangle[i])>=out.vertices.size())throw std::runtime_error("Invalid RCL1 triangle vertex");
    return out;
}
float originalCoarseCellDistance(const std::array<std::uint32_t,14>& c,const std::array<float,3>& p){
    float distance=asFloat(c[1])*p[1];
    distance=std::fma(asFloat(c[0]),p[0],distance);
    distance=std::fma(asFloat(c[2]),p[2],distance);distance+=asFloat(c[3]);
    if(0.0f>distance)return distance;
    std::size_t previous=10;
    for(std::size_t vertex=4;vertex<=10;vertex+=3){
        const float prevX=asFloat(c[previous]),prevZ=asFloat(c[previous+2]);
        const float cross=(asFloat(c[vertex+2])-prevZ)*(p[0]-prevX)-
            (asFloat(c[vertex])-prevX)*(p[2]-prevZ);
        if(0.0f>cross)return asFloat(0xBF800000u);
        previous=vertex;
    }
    return distance;
}
std::int32_t findOriginalCoarseCell(const OriginalCollisionData& data,std::int32_t hint,const std::array<float,3>& point){
    const auto count=std::int32_t(data.coarseCells.size());
    if(hint<0){
        float closest=asFloat(0x7F7FFFFFu);std::int32_t selected=-1;
        for(std::int32_t i=0;i<count;++i){const float distance=originalCoarseCellDistance(data.coarseCells[i],point);if(!(0.0f>distance)&&closest>distance){closest=distance;selected=i;}}
        return selected;
    }
    if(hint>=count)throw std::out_of_range("Invalid original coarse-cell hint");
    std::int32_t forward=hint,backward=hint;
    for(;;){
        if(!(0.0f>originalCoarseCellDistance(data.coarseCells[forward],point)))return forward;
        if(++forward>=count)forward=0;
        if(forward==backward)return -1;
        if(--backward<0)backward=count-1;
        if(!(0.0f>originalCoarseCellDistance(data.coarseCells[backward],point)))return backward;
        if(forward==backward)return -1;
    }
}
std::int32_t findOriginalTriangle(const OriginalCollisionData& data,std::int32_t starting,
        const std::array<float,3>& point,OriginalTriangleSearchTrace& trace){
    trace.count0C99AA94=0;std::int32_t current=starting,previous=-2;
    for(;;){
        trace.indices0C99A904.at(trace.count0C99AA94++)=current;
        const auto edge=outsideTriangleEdge(data,current,previous,point[0],point[2]);
        if(edge<0)return current;
        previous=current;current=data.triangles.at(std::size_t(current))[3+edge];
        if(current<0||trace.count0C99AA94>99)return -1;
    }
}
void clearOriginalCollisionQuery(OriginalCollisionQuery& q){
    q.setu(56,0xFFFFFFFFu);q.setu(60,0xFFFFFFFFu);
    q.setf(0,0);q.setf(4,1);q.setf(8,0);q.setf(24,0);
}
bool locateOriginalCollision(const OriginalCollisionData& data,OriginalCollisionQuery& q,OriginalTriangleSearchTrace& trace){
    const std::array<float,3> point{q.f(32),q.f(36),q.f(40)};
    const auto coarse=findOriginalCoarseCell(data,std::bit_cast<std::int32_t>(q.u(56)),point);
    std::uint32_t failure=0x80000001u;
    if(coarse>=0){
        if(std::uint32_t(coarse)!=q.u(56)||std::bit_cast<std::int32_t>(q.u(60))<0){
            q.setu(56,std::uint32_t(coarse));
            q.setu(60,std::uint32_t(std::int32_t(std::bit_cast<std::int16_t>(std::uint16_t(data.coarseCells[coarse][13])))));
        }
        const auto triangle=std::bit_cast<std::int32_t>(q.u(60));failure=0x80000003u;
        if(triangle>=0){
            const auto located=findOriginalTriangle(data,triangle,point,trace);q.setu(60,std::uint32_t(located));
            if(located>=0)return true;
            failure=0x80000002u;
        }
    }
    clearOriginalCollisionQuery(q);q.setu(28,failure);return false;
}
void buildOriginalSurfaceCoefficients(const OriginalCollisionData& data,std::int32_t index,
        OriginalSurfaceScratch& s){
    const auto& triangle=data.triangles.at(std::size_t(index));
    const auto& a=data.vertices.at(std::size_t(triangle[0]));
    const auto& b=data.vertices.at(std::size_t(triangle[1]));
    const auto& c=data.vertices.at(std::size_t(triangle[2]));
    s.words[0]=std::uint32_t(index);
    for(std::size_t axis=0;axis<3;++axis){
        s.setf(8+axis*4,asFloat(b[axis])-asFloat(a[axis]));
        s.setf(20+axis*4,asFloat(c[axis])-asFloat(b[axis]));
        s.setf(32+axis*4,asFloat(a[axis])-asFloat(c[axis]));
    }
    const float product1=s.f(20)*s.f(16),product2=s.f(8)*s.f(28);
    s.setf(4,(product1-product2)*0.5f);
    const float ay=asFloat(a[1]),by=asFloat(b[1]),cy=asFloat(c[1]);
    s.setf(44,ay);s.setf(48,((ay+ay)+by)/3.0f);s.setf(52,((ay+ay)+cy)/3.0f);
    s.setf(56,(ay+(by+by))/3.0f);s.setf(60,((ay+by)+cy)/3.0f);
    s.setf(64,(ay+(cy+cy))/3.0f);s.setf(68,by);s.setf(72,((by+by)+cy)/3.0f);
    s.setf(76,(by+(cy+cy))/3.0f);s.setf(80,cy);
}
bool evaluateOriginalCollisionSurface(const OriginalCollisionData& data,OriginalCollisionQuery& q,
        OriginalSurfaceScratch& s){
    const auto index=std::bit_cast<std::int32_t>(q.u(60));
    buildOriginalSurfaceCoefficients(data,index,s);
    const auto& triangle=data.triangles.at(std::size_t(index));
    const auto& a=data.vertices.at(std::size_t(triangle[0]));
    const auto& b=data.vertices.at(std::size_t(triangle[1]));
    const auto& c=data.vertices.at(std::size_t(triangle[2]));
    const float x=q.f(32),z=q.f(40),inverseArea=0.5f/s.f(4);
    // Ordered differences, products and subtractions match the original.
    const float wa=(((x-asFloat(b[0]))*s.f(28))-((z-asFloat(b[2]))*s.f(20)))*inverseArea;
    const float wb=(((x-asFloat(c[0]))*s.f(40))-((z-asFloat(c[2]))*s.f(32)))*inverseArea;
    const float wc=(((x-asFloat(a[0]))*s.f(16))-((z-asFloat(a[2]))*s.f(8)))*inverseArea;
    const float threeB=wb*3.0f,threeC=wc*3.0f,threeA=wa*3.0f;
    float groupA=s.f(48)*threeB;
    groupA=std::fma(wa,s.f(44),groupA);groupA=std::fma(threeC,s.f(52),groupA);
    float groupB=s.f(68)*wb;
    groupB=std::fma(threeA,s.f(56),groupB);groupB=std::fma(threeC,s.f(72),groupB);
    float height=(wb*wb)*groupB;
    height=std::fma(wa*wa,groupA,height);
    float groupC=threeB*s.f(76);
    groupC=std::fma(threeA,s.f(64),groupC);groupC=std::fma(wc,s.f(80),groupC);
    height=std::fma(wc*wc,groupC,height);
    const float mixed=((6.0f*wa)*wb)*wc;
    height=std::fma(mixed,s.f(60),height);
    std::array<float,3> normal{};
    for(std::size_t axis=0;axis<3;++axis){
        normal[axis]=asFloat(b[axis+3])*wb;
        normal[axis]=std::fma(wa,asFloat(a[axis+3]),normal[axis]);
        normal[axis]=std::fma(asFloat(c[axis+3]),wc,normal[axis]);
    }
    //023960 uses two FMACs then sqrt/divide, unlike FIPR/FSRRA helpers.
    float normSquared=normal[1]*normal[1];
    normSquared=std::fma(normal[0],normal[0],normSquared);
    normSquared=std::fma(normal[2],normal[2],normSquared);
    const float inverseLength=1.0f/std::sqrt(normSquared);
    q.setf(0,normal[0]*inverseLength);q.setf(4,normal[1]*inverseLength);
    q.setf(8,inverseLength*normal[2]);
    q.setf(24,q.f(36)-height);q.setu(28,std::uint16_t(triangle[7]));
    q.setf(12,x);q.setf(16,height);q.setf(20,z);
    return true;
}
bool queryOriginalCollisionSurface(const OriginalCollisionData& data,OriginalCollisionQuery& query,
        OriginalTriangleSearchTrace& trace,OriginalSurfaceScratch& scratch){
    return locateOriginalCollision(data,query,trace)&&evaluateOriginalCollisionSurface(data,query,scratch);
}
bool beginOriginalCollisionSweep(const OriginalCollisionData& data,const OriginalCollisionQuery& q,
        OriginalTriangleSearchTrace& trace,OriginalSweepScratch& s){
    s.setf(4,q.f(44));s.setf(8,q.f(52));s.setf(12,q.f(32));s.setf(16,q.f(40));
    s.setf(20,q.f(32)-q.f(44));s.setf(24,q.f(40)-q.f(52));
    s.setu(32,std::uint32_t(data.triangles.size()));s.setu(0,1);s.setu(36,0);
    const auto& cell=data.coarseCells.at(q.u(56));
    const auto starting=std::int32_t(std::bit_cast<std::int16_t>(std::uint16_t(cell[13])));
    const auto located=findOriginalTriangle(data,starting,{q.f(44),q.f(48),q.f(52)},trace);
    s.setu(28,std::uint32_t(located));
    if(located>=0){
        const float lengthSquared=std::fma(s.f(20),s.f(20),s.f(24)*s.f(24));
        if(!(asFloat(0x00800000u)>lengthSquared))return true;
    }
    s.setu(0,0);return false;
}
bool advanceOriginalCollisionSweep(const OriginalCollisionData& data,OriginalSweepScratch& s){
    if(s.u(0)==0)return true;
    const auto index=s.u(28);const auto& triangle=data.triangles.at(index);
    const auto prior=std::bit_cast<std::int32_t>(s.u(32));
    const auto* previous=&data.vertices.at(std::size_t(triangle[2]));
    for(std::size_t edge=0;edge<3;++edge){
        const auto& next=data.vertices.at(std::size_t(triangle[edge]));
        const auto neighbor=std::int32_t(triangle[3+edge]);
        if(neighbor!=prior){
            const float px=asFloat((*previous)[0]),pz=asFloat((*previous)[2]);
            const float edgeX=asFloat(next[0])-px,edgeZ=asFloat(next[2])-pz;
            const float determinant=edgeZ*s.f(20)-edgeX*s.f(24);
            const float edgeParameter=((px-s.f(4))*s.f(24)-s.f(20)*(pz-s.f(8)))/determinant;
            if(!(0.0f>edgeParameter)&&!(edgeParameter>1.0f)){
                const float endCross=(s.f(16)-pz)*edgeX-(s.f(12)-px)*edgeZ;
                if(endCross>0.0f){
                    s.setu(32,index);s.setu(28,std::uint32_t(neighbor));
                    if(neighbor>=0&&data.triangles.at(std::size_t(neighbor))[7]>=0)return false;
                    const float edgeLength=std::sqrt(std::fma(edgeX,edgeX,edgeZ*edgeZ));
                    const float inverseLength=1.0f/edgeLength;
                    s.setf(40,edgeZ*inverseLength);s.setf(44,(-edgeX)*inverseLength);
                    s.setf(48,-std::fma(s.f(40),px,s.f(44)*pz));
                    s.setu(36,1);s.setf(52,endCross/determinant);return true;
                }
            }
        }
        previous=&next;
    }
    return true;
}
void publishOriginalCollisionSweep(const OriginalCollisionData& data,OriginalCollisionQuery& q,
        const OriginalSweepScratch& s){
    q.setf(0,s.f(40));q.setf(4,0);q.setf(8,s.f(44));
    float distance=s.f(44)*q.f(40);distance=std::fma(s.f(40),q.f(32),distance);
    distance+=s.f(48);q.setf(24,-distance);
    q.setu(28,std::uint16_t(data.triangles.at(q.u(60))[7])|0x8000u);
    for(std::size_t axis=0;axis<3;++axis){
        const auto currentOffset=32+axis*4,previousOffset=44+axis*4;
        const float correction=s.f(52)*(q.f(previousOffset)-q.f(currentOffset));
        q.setf(12+axis*4,q.f(currentOffset)-correction);
    }
}
bool queryOriginalCollisionSwept(const OriginalCollisionData& data,OriginalCollisionQuery& q,
        OriginalTriangleSearchTrace& trace,OriginalSurfaceScratch& surface){
    if(data.importedSweepFromPrevious){
        auto previous=q;
        for(unsigned axis=0;axis<3;++axis)previous.setu(32+axis*4,q.u(44+axis*4));
        if(locateOriginalCollision(data,previous,trace)&&data.triangles.at(previous.u(60))[7]>=0){
            // Keep the original segment and contact response. Only the lookup
            // seed changes: folded outer strips at hairpins must not prevent
            // a segment starting on the road from reaching its first wall.
            auto seeded=q;seeded.setu(56,previous.u(56));seeded.setu(60,previous.u(60));
            OriginalSweepScratch sweep;
            if(beginOriginalCollisionSweep(data,seeded,trace,sweep)){
                bool complete=false;
                for(int remaining=100;remaining>0;--remaining)if(advanceOriginalCollisionSweep(data,sweep)){complete=true;break;}
                if(complete&&sweep.u(0)!=0&&sweep.u(36)!=0){
                    publishOriginalCollisionSweep(data,seeded,sweep);q=seeded;return true;
                }
                if(complete&&sweep.u(0)!=0&&sweep.u(36)==0&&locateOriginalCollision(data,q,trace))
                    return evaluateOriginalCollisionSurface(data,q,surface);
            }
        }
    }
    if(locateOriginalCollision(data,q,trace)){
        OriginalSweepScratch sweep;
        if(beginOriginalCollisionSweep(data,q,trace,sweep)){
            for(std::int32_t remaining=99;remaining!=-1;--remaining)
                if(advanceOriginalCollisionSweep(data,sweep))break;
            if(sweep.u(0)!=0){
                if(sweep.u(36)!=0){publishOriginalCollisionSweep(data,q,sweep);return true;}
                return evaluateOriginalCollisionSurface(data,q,surface);
            }
        }
    }
    clearOriginalCollisionQuery(q);return false;
}
} // namespace idas3::original
