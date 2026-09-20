#include "original_course_objects.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace idas3 {
namespace {
template<class T>T read(std::istream& f){T v{};if(!f.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("Truncated original course objects");return v;}
std::filesystem::path objectPath(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    constexpr std::array<std::string_view,9> ids={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"};
    if(std::find(ids.begin(),ids.end(),id)==ids.end())throw std::runtime_error("Unknown original object course");
    return root/"data/original_models/courses"/std::string(id)/(std::string("scene_")+(night?"night":"day")+(reverse?"_reverse":"_forward")+(wet?"_wet":"")+".idasobjects");
}
// SH4 FTRC truncates toward zero. Reject nonfinite/overflow data rather than
// letting native float-to-int conversion produce undefined behavior.
std::int32_t integer(float value){
    if(!std::isfinite(value)||value<=-2147483648.f||value>=2147483648.f)throw std::runtime_error("Original object grid conversion outside bounds");
    return std::int32_t(value);
}
original::OriginalMatrix multiply(const original::OriginalMatrix& a,const original::OriginalMatrix& b){
    original::OriginalMatrix out;
    for(unsigned col=0;col<4;++col){const auto v=original::transformOriginalVector(a,{b.elements[col*4],b.elements[col*4+1],b.elements[col*4+2],b.elements[col*4+3]});
        for(unsigned row=0;row<4;++row)out.elements[col*4+row]=v[row];}
    return out;
}
}
bool OriginalCourseObjects::available(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet){
    return std::filesystem::is_regular_file(objectPath(root,id,night,reverse,wet));
}
OriginalCourseObjects OriginalCourseObjects::load(const std::filesystem::path& root,std::string_view id,bool night,bool reverse,bool wet,std::size_t chunkCount){
    OriginalCourseObjects out;out.chunkCount_=chunkCount;
    std::ifstream f(objectPath(root,id,night,reverse,wet),std::ios::binary);
    const auto magic=read<std::array<char,8>>(f);
    if(std::memcmp(magic.data(),"ID3OBJ1\0",8)||read<std::uint32_t>(f)!=1)throw std::runtime_error("Invalid original course object format");
    out.pathCount_=read<std::uint32_t>(f);
    const auto ownerCount=read<std::uint32_t>(f),selectionCount=read<std::uint32_t>(f),transitionCount=read<std::uint32_t>(f);
    if(!out.pathCount_||out.pathCount_>20000||ownerCount>8||!selectionCount||selectionCount>20000||!transitionCount||transitionCount>20000||!chunkCount||chunkCount>4096)throw std::runtime_error("Original course object bounds");
    const auto trig=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    const auto sc=trig.sinCos(8192);
    for(unsigned n=0;n<ownerCount;++n){
        Owner owner;owner.sectorMode=read<std::uint32_t>(f);
        const auto settings=read<std::array<std::uint32_t,24>>(f);
        original::OriginalMatrix basis;for(unsigned i=0;i<16;++i)basis.elements[i]=std::bit_cast<float>(settings[i]);
        const float maxX=std::bit_cast<float>(settings[16]),minX=std::bit_cast<float>(settings[17]);
        const float maxZ=std::bit_cast<float>(settings[18]),minZ=std::bit_cast<float>(settings[19]);
        owner.columns=std::bit_cast<std::int32_t>(settings[20]);owner.rows=std::bit_cast<std::int32_t>(settings[21]);
        const float nearRadius=std::bit_cast<float>(settings[22]),farRadius=std::bit_cast<float>(settings[23]);
        for(float v:basis.elements)if(!std::isfinite(v))throw std::runtime_error("Nonfinite original object matrix");
        if(owner.sectorMode>2||owner.columns<=0||owner.rows<=0||owner.columns>512||owner.rows>512||!(maxX>minX)||!(maxZ>minZ)||!(nearRadius>0)||!(farRadius>=nearRadius)||!std::isfinite(farRadius))throw std::runtime_error("Invalid original object grid settings");
        owner.gridMatrix=original::originalIdentityMatrix();original::translateOriginalMatrix(owner.gridMatrix,{-minX,0,-minZ});owner.gridMatrix=multiply(owner.gridMatrix,basis);
        owner.cellX=(maxX-minX)/float(owner.columns);owner.cellZ=(maxZ-minZ)/float(owner.rows);
        owner.innerX=integer((nearRadius*sc[0])/owner.cellX)-1;owner.innerZ=integer((nearRadius*sc[1])/owner.cellZ)-1;
        owner.outerX=integer((farRadius*sc[0])/owner.cellX)+1;owner.outerZ=integer((farRadius*sc[1])/owner.cellZ)+1;
        if(owner.outerX<0||owner.outerZ<0||owner.outerX>512||owner.outerZ>512)throw std::runtime_error("Original object grid neighborhood bounds");
        owner.farSquared=farRadius*farRadius;
        const auto count=read<std::uint32_t>(f);if(count>20000)throw std::runtime_error("Original object placement count bound");
        owner.cells.resize(std::size_t(owner.columns)*owner.rows);owner.placements.reserve(count);
        for(unsigned i=0;i<count;++i){
            const auto raw=read<std::array<std::uint32_t,10>>(f);const auto sector=read<std::uint32_t>(f);
            Placement p;p.instance.chunk=raw[0];p.position={std::bit_cast<float>(raw[1]),std::bit_cast<float>(raw[2]),std::bit_cast<float>(raw[3])};
            const auto scale=std::bit_cast<float>(raw[6]);
            if(raw[0]>=chunkCount||!std::isfinite(p.position.x)||!std::isfinite(p.position.y)||!std::isfinite(p.position.z)||!std::isfinite(scale)||scale<=0||raw[9]||sector>31)throw std::runtime_error("Invalid original object placement");
            auto matrix=original::originalIdentityMatrix();original::translateOriginalMatrix(matrix,{p.position.x,p.position.y,p.position.z});
            original::rotateOriginalMatrixPhase(matrix,1,std::uint16_t(raw[4]),trig);original::rotateOriginalMatrixPhase(matrix,0,std::uint16_t(raw[5]),trig);
            original::scaleOriginalMatrix(matrix,{scale,scale,scale});
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)p.instance.transform[row*4+col]=matrix.elements[col*4+row];
            p.bySector=raw[7]!=0&&owner.sectorMode!=0;p.sector=sector;owner.placements.push_back(p);
            if(p.bySector)owner.sectors[sector].push_back(i);
            else{
                const auto grid=original::transformOriginalPoint(owner.gridMatrix,{p.position.x,p.position.y,p.position.z});
                const auto cell=std::int64_t(integer(grid[0]/owner.cellX))+std::int64_t(integer(grid[2]/owner.cellZ))*owner.columns;
                //084E4E..084E58 checks the flattened index only, including the
                //source's edge wrapping behavior; do not clamp each axis.
                if(cell>=0&&cell<std::int64_t(owner.cells.size()))owner.cells[std::size_t(cell)].push_back(i);
            }
        }
        out.owners_.push_back(std::move(owner));
    }
    for(unsigned n=0;n<selectionCount;++n){Selection s;s.staticCount=read<std::uint32_t>(f);const auto count=read<std::uint32_t>(f);
        if(s.staticCount>20000||count>64)throw std::runtime_error("Original object selection bound");
        for(unsigned i=0;i<count;++i){Draw d;d.owner=read<std::uint32_t>(f);d.chunkBase=read<std::uint32_t>(f);d.before=read<std::uint32_t>(f);d.ranges=read<std::array<std::int32_t,4>>(f);
            if(d.owner>=ownerCount||d.before>s.staticCount||(i&&d.before<s.draws.back().before))throw std::runtime_error("Invalid original object selection");
            for(const auto& p:out.owners_[d.owner].placements)if(std::uint64_t(p.instance.chunk)+d.chunkBase>=chunkCount)throw std::runtime_error("Original object chunk outside selected course bank");
            s.draws.push_back(d);}
        out.selections_.push_back(std::move(s));}
    for(unsigned i=0;i<transitionCount;++i){const auto start=read<std::uint32_t>(f),choice=read<std::uint32_t>(f);
        if(start>=out.pathCount_||choice>=selectionCount||(!i&&start)||(i&&start<=out.starts_.back()))throw std::runtime_error("Invalid original object path transition");
        out.starts_.push_back(start);out.choices_.push_back(choice);}
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original course object data");return out;
}
std::size_t OriginalCourseObjects::placementCount()const{std::size_t count=0;for(const auto& owner:owners_)count+=owner.placements.size();return count;}
void OriginalCourseObjects::setMinimumDrawDistance(float metres){
    if(!std::isfinite(metres)||metres<0||metres>2000)throw std::invalid_argument("Course draw distance outside bounds");
    minimumDrawDistance_=metres;
}
void OriginalCourseObjects::appendOwner(NativeAssembly& target,const Draw& draw,Vec3 position)const{
    const auto& owner=owners_.at(draw.owner);
    const auto append=[&](std::uint32_t index){auto instance=owner.placements[index].instance;instance.chunk+=draw.chunkBase;target.instances.push_back(instance);};
    const float extendedSquared=minimumDrawDistance_*minimumDrawDistance_;
    if(extendedSquared>owner.farSquared){
        const auto within=[&](std::uint32_t index){const auto& p=owner.placements[index].position;
            const float x=position.x-p.x,y=position.y-p.y,z=position.z-p.z;
            return double(x)*x+double(y)*y+double(z)*z<double(extendedSquared);
        };
        // Retain source-enabled sector trees, and admit neighboring sector
        // trees by distance so their shorter sector gates cannot cut them off.
        if(owner.sectorMode==2)for(std::size_t sector=0;sector<owner.sectors.size();++sector)
            for(const auto index:owner.sectors[sector])
                if((int(sector)>=draw.ranges[1]&&int(sector)<draw.ranges[2])||within(index))append(index);
        // Source grid membership and linked-list order stay intact. Walking
        // each cell once avoids duplicated trees from a widened wrapped grid
        // rectangle; only admitted trees reach the mesh builder.
        for(const auto& cell:owner.cells)for(const auto index:cell)if(within(index))append(index);
        return;
    }
    if(owner.sectorMode==2){
        for(auto sector=std::max(0,draw.ranges[1]);sector<draw.ranges[2]&&sector<32;++sector)for(const auto index:owner.sectors[std::size_t(sector)])append(index);
    }
    const auto grid=original::transformOriginalPoint(owner.gridMatrix,{position.x,position.y,position.z});
    const auto center=std::int64_t(integer(grid[0]/owner.cellX))+std::int64_t(integer(grid[2]/owner.cellZ))*owner.columns;
    for(auto dz=-owner.outerZ;dz<=owner.outerZ;++dz)for(auto dx=-owner.outerX;dx<=owner.outerX;++dx){
        const auto cell=center+std::int64_t(dz)*owner.columns+dx;
        if(cell<0||cell>=std::int64_t(owner.cells.size()))continue;
        const bool inner=dz>=-owner.innerZ&&dz<=owner.innerZ&&dx>=-owner.innerX&&dx<=owner.innerX;
        for(const auto index:owner.cells[std::size_t(cell)]){const auto& p=owner.placements[index];
            if(!inner){const float x=position.x-p.position.x,y=position.y-p.position.y,z=position.z-p.position.z;
                const float squared=float(double(x)*x+double(y)*y+double(z)*z);
                if(!(owner.farSquared>squared))continue;}
            append(index);
        }
    }
}
std::vector<NativeAssemblyInsertion> OriginalCourseObjects::insertionsForPathIndex(std::size_t pathIndex,Vec3 position,std::size_t staticCount)const{
    if(pathIndex>=pathCount_||starts_.empty())throw std::runtime_error("Original object path index outside course");
    const auto segment=std::size_t(std::upper_bound(starts_.begin(),starts_.end(),pathIndex)-starts_.begin()-1);
    const auto& selection=selections_.at(choices_[segment]);
    if(staticCount!=selection.staticCount)throw std::runtime_error("Original object insertion does not match static scene");
    std::vector<NativeAssemblyInsertion> out;out.reserve(selection.draws.size());
    for(const auto& draw:selection.draws){
        if(out.empty()||out.back().before!=draw.before)out.push_back({draw.before,{}});
        appendOwner(out.back().assembly,draw,position);
    }
    return out;
}
void OriginalCourseObjects::appendTo(NativeAssembly& target,std::size_t pathIndex,Vec3 position)const{
    const auto insertions=insertionsForPathIndex(pathIndex,position,target.instances.size());
    if(insertions.empty())return;
    NativeAssembly combined;combined.instances.reserve(target.instances.size()+256);std::size_t previous=0;
    for(const auto& insertion:insertions){combined.instances.insert(combined.instances.end(),target.instances.begin()+previous,target.instances.begin()+insertion.before);combined.instances.insert(combined.instances.end(),insertion.assembly.instances.begin(),insertion.assembly.instances.end());previous=insertion.before;}
    combined.instances.insert(combined.instances.end(),target.instances.begin()+previous,target.instances.end());target=std::move(combined);
}
}
