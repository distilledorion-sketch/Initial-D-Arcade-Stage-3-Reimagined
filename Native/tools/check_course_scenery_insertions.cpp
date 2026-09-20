// Standalone integration check: load the shipped catalog and both new sidecars
// at every authored index, then use the driving owner's separate reverse cell
// and reverse point conventions (099460). No renderer or game loop is linked.
#include "course.h"
#include "course_scene_catalog.h"
#include "original_course_animation.h"
#include "original_course_billboards.h"
#include "original_course_light_path.h"
#include "original_course_objects.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace idas3;
namespace {
template<class T>T read(std::istream& in){T v{};if(!in.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("Truncated integration fixture");return v;}
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool samePoint(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
// Independently read the source capture's expected static counts, including
// lamps, instead of deriving the expected count from the catalog itself.
std::vector<unsigned> expectedCounts(const std::filesystem::path& file){
    std::ifstream in(file,std::ios::binary);const auto magic=read<std::array<char,8>>(in);
    require(!std::memcmp(magic.data(),"ID3OBJ1\0",8)&&read<unsigned>(in)==1,"Object fixture header");
    const auto paths=read<unsigned>(in),owners=read<unsigned>(in),selections=read<unsigned>(in),changes=read<unsigned>(in);
    require(paths>=2&&paths<=20000&&owners<=8&&selections&&selections<=20000&&changes&&changes<=paths,"Object fixture extent");
    for(unsigned i=0;i<owners;++i){(void)read<unsigned>(in);(void)read<std::array<unsigned,24>>(in);const auto n=read<unsigned>(in);require(n<=20000,"Placement fixture bound");for(unsigned j=0;j<n;++j)(void)read<std::array<unsigned,11>>(in);}
    std::vector<unsigned> counts;
    for(unsigned i=0;i<selections;++i){counts.push_back(read<unsigned>(in));const auto n=read<unsigned>(in);require(n<=64,"Selection fixture bound");for(unsigned j=0;j<n;++j)(void)read<std::array<unsigned,7>>(in);}
    std::vector<unsigned> expected(paths);unsigned previous=0,choice=0;
    for(unsigned i=0;i<changes;++i){const auto start=read<unsigned>(in),next=read<unsigned>(in);require(start<paths&&next<counts.size()&&(!i?start==0:start>previous),"Transition fixture bound");std::fill(expected.begin()+previous,expected.begin()+start,counts[choice]);previous=start;choice=next;}
    std::fill(expected.begin()+previous,expected.end(),counts[choice]);require(in.peek()==std::char_traits<char>::eof(),"Trailing object fixture");return expected;
}
struct Totals{std::size_t cases=0,driving=0,trees=0,billboards=0,lamps=0,replacements=0,ties=0,maxTrees=0,maxBillboards=0;};
void check(const OriginalCourseScene& scene,const OriginalCourseObjects& trees,const OriginalCourseBillboards& billboards,
           const OriginalCourseAnimation& animation,const original::OriginalFscaTable& trig,
           std::size_t index,Vec3 point,unsigned expected,Totals& totals){
    const auto& base=scene.assemblyForPathIndex(index);require(base.instances.size()==expected,"Catalog count differs from source lamp-inclusive static count");
    auto inserted=billboards.insertionsForPathIndex(index,base.instances.size());
    const auto objects=trees.insertionsForPathIndex(index,point,base.instances.size());std::size_t treeCount=0,billboardCount=0;
    for(const auto& b:inserted){billboardCount+=b.assembly.instances.size();for(const auto& t:objects)if(b.before==t.before)++totals.ties;}
    for(const auto& t:objects){treeCount+=t.assembly.instances.size();for(const auto& instance:t.assembly.instances)require(!instance.billboard,"Tree changed to view-facing geometry");}
    inserted.insert(inserted.end(),objects.begin(),objects.end());
    if(animation.active())for(std::size_t i=0;i<base.instances.size();++i)if(base.instances[i].chunk==animation.chunk()){
        NativeAssemblyInsertion replacement;replacement.before=i;replacement.replaceCount=1;replacement.assembly.instances.push_back(base.instances[i]);
        require(animation.apply(replacement.assembly,trig)==1,"Animated source instance missing");inserted.push_back(std::move(replacement));++totals.replacements;
    }
    std::stable_sort(inserted.begin(),inserted.end(),[](const auto& a,const auto& b){return a.before==b.before?a.replaceCount<b.replaceCount:a.before<b.before;});
    std::size_t cursor=0,staticKept=0,staticReplaced=0;
    for(const auto& insertion:inserted){
        require(insertion.before>=cursor&&insertion.before<=base.instances.size(),"Combined insertions overlap or exceed base");
        require(insertion.replaceCount<=base.instances.size()-insertion.before,"Replacement exceeds base");
        staticKept+=insertion.before-cursor;staticReplaced+=insertion.replaceCount;cursor=insertion.before+insertion.replaceCount;
        for(const auto& instance:insertion.assembly.instances){require(instance.chunk<scene.model.chunks.size(),"Inserted chunk outside original bank");for(float v:instance.transform)require(std::isfinite(v),"Nonfinite inserted transform");}
    }
    staticKept+=base.instances.size()-cursor;require(staticKept+staticReplaced==base.instances.size(),"Static submission lost or duplicated");
    for(const auto& instance:base.instances)totals.lamps+=instance.billboard;
    totals.trees+=treeCount;totals.billboards+=billboardCount;totals.maxTrees=std::max(totals.maxTrees,treeCount);totals.maxBillboards=std::max(totals.maxBillboards,billboardCount);++totals.cases;
}
// Mesh::originalCar packs lit planar billboard normals into a lossless 24-bit
// carrier. Validate every referenced chunk once against that renderer contract.
void checkBillboardChunks(const NativeModel& model,const std::set<unsigned>& chunks){
    for(auto chunk:chunks)for(const auto& batch:model.chunks.at(chunk).batches)for(const auto& vertex:batch.vertices){
        require(vertex.position.z==0,"Billboard is not planar");
        if(!(batch.material[2]&512))for(float value:{vertex.normal.x,vertex.normal.y,vertex.normal.z}){
            const auto q=std::lround(value*127.f);require(q>=-128&&q<=127&&float(q)/127.f==value,"Billboard normal cannot use exact source-byte carrier");
        }
    }
}
}
int main(int argc,char** argv)try{
    require(argc==2,"Usage: check_course_scenery_insertions native-root");const std::filesystem::path root=argv[1];
    const auto trig=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    constexpr std::array<const char*,9> ids={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"};Totals totals;unsigned variants=0;
    for(unsigned ci=0;ci<ids.size();++ci){const auto forward=Course::load(root/"data/courses",ci==8?"k_df":ids[ci]);
        for(bool night:{false,true})for(bool wet:{false,true})for(bool reverse:{false,true}){
            const auto variant=std::string(night?"night":"day")+(reverse?"_reverse":"_forward")+(wet?"_wet":"");
            try{
                const auto course=Course::load(root/"data/courses",ci==8?"k_df":ids[ci],"",reverse);
                const auto expected=expectedCounts(root/"data/original_models/courses"/ids[ci]/("scene_"+variant+".idasobjects"));require(expected.size()==course.points.size(),"Route/sidecar count mismatch");
                const auto scene=OriginalCourseScene::load(root,ids[ci],night,reverse,wet);
                const auto trees=OriginalCourseObjects::load(root,ids[ci],night,reverse,wet,scene.model.chunks.size());
                const auto billboards=OriginalCourseBillboards::load(root,ids[ci],night,reverse,wet,scene.model.chunks.size());
                OriginalCourseAnimation animation;animation.reset(ci,night,wet);animation.advance();std::set<unsigned> billboardChunks;
                for(std::size_t index=0;index<expected.size();++index){
                    const auto route=reverse?course.points.size()-1-index:index;require(samePoint(course.points[route],forward.points[index]),"Reversed route does not preserve source point");
                    check(scene,trees,billboards,animation,trig,index,course.points[route],expected[index],totals);
                    for(const auto& b:billboards.insertionsForPathIndex(index,expected[index]))for(const auto& v:b.assembly.instances)billboardChunks.insert(v.chunk);
                }
                const int period=int(course.points.size()-1);
                for(int ownerIndex=-1;ownerIndex<=period;++ownerIndex){
                    const auto wrapped=(ownerIndex%period+period)%period;const auto sourceCell=std::size_t(reverse?period-1-wrapped:wrapped);
                    const auto reference=originalCourseLightReference(course.points,ownerIndex);const auto sourcePoint=std::size_t(reverse?period-wrapped:wrapped);
                    require(samePoint(reference,forward.points[sourcePoint]),"Driving route reference differs from099460 reverse point");
                    check(scene,trees,billboards,animation,trig,sourceCell,reference,expected[sourceCell],totals);++totals.driving;
                }
                checkBillboardChunks(scene.model,billboardChunks);++variants;
                std::cout<<ids[ci]<<' '<<variant<<" PASS authored="<<expected.size()<<" driving="<<period+2<<" billboard_chunks="<<billboardChunks.size()<<'\n';
            }catch(const std::exception& e){throw std::runtime_error(std::string(ids[ci])+" "+variant+": "+e.what());}
        }
    }
    std::cout<<"PASS variants="<<variants<<" combined_path_cases="<<totals.cases<<" driving_reference_cases="<<totals.driving<<" tree_instances="<<totals.trees<<" billboard_instances="<<totals.billboards<<" existing_lamp_instances="<<totals.lamps<<" animation_replacements="<<totals.replacements<<" tied_insertion_groups="<<totals.ties<<" max_trees="<<totals.maxTrees<<" max_billboards="<<totals.maxBillboards<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
