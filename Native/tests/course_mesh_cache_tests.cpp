#include "renderer.h"
#include "course_scene_catalog.h"
#include <cstring>
#include <iostream>
#include <set>
#include <map>
using namespace idas3;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void equal(const Mesh& a,const Mesh& b){
    require(a.vertices.size()==b.vertices.size()&&a.ranges.size()==b.ranges.size(),"Cached course mesh size changed");
    require(!std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Vertex)),"Cached course vertex changed");
    for(std::size_t i=0;i<a.ranges.size();++i){const auto& x=a.ranges[i];const auto& y=b.ranges[i];
        require(x.first==y.first&&x.count==y.count&&x.texture==y.texture&&x.tsp==y.tsp&&x.pcw==y.pcw&&x.isp==y.isp&&x.gmp==y.gmp&&x.original==y.original&&x.emissive==y.emissive&&x.gloss==y.gloss&&x.billboard==y.billboard&&x.originalLightDirection==y.originalLightDirection&&x.courseLighting==y.courseLighting&&x.viewMask==y.viewMask&&x.carLighting==y.carLighting&&x.courseGeometry==y.courseGeometry&&x.sourceFaceCulling==y.sourceFaceCulling,"Cached course material/order/ownership changed");}
}
void verifyIdentities(const Mesh& mesh){
    static std::map<std::uint64_t,std::vector<Vertex>> previous;
    std::map<std::uint64_t,std::vector<Vertex>> current;
    for(const auto& range:mesh.ranges){
        if(!range.geometryId)continue;
        const auto* first=mesh.vertices.data()+range.first;
        auto old=previous.find(range.geometryId);
        if(old!=previous.end())require(old->second.size()==range.count&&
            !std::memcmp(old->second.data(),first,range.count*sizeof(Vertex)),"Reused geometry identity changed bytes");
        auto [it,inserted]=current.try_emplace(range.geometryId,first,first+range.count);
        if(!inserted)require(it->second.size()==range.count&&
            !std::memcmp(it->second.data(),first,range.count*sizeof(Vertex)),"One geometry identity described different ranges");
    }
    previous=std::move(current);
}
void identityBoundaries(){
    Mesh source;source.triangle({0,0,0},{1,0,0},{0,1,0},{1,1,1});
    source.ranges[0].geometryId=77;
    Mesh copied;copied.append(source);
    require(copied.ranges[0].geometryId==77,"Complete append lost identity");
    copied.append(source);
    require(copied.ranges.size()==1&&copied.ranges[0].geometryId==0,"Merged append retained an invalid identity");
    Mesh dynamic;dynamic.append(source);dynamic.triangle({2,0,0},{3,0,0},{2,1,0},{1,1,1});
    require(dynamic.ranges[0].geometryId==0,"Dynamic vertices retained static identity");
    source.ranges[0].texture=123;copied.append(source);
    require(copied.ranges.size()==2&&copied.ranges[1].geometryId==77,"Distinct complete range lost identity");
}
void ownershipBoundary(){
    Mesh scenery;scenery.triangle({0,0,0},{1,0,0},{0,1,0},{1,1,1});
    scenery.ranges.front().courseGeometry=true;
    Mesh car;car.triangle({0,0,1},{1,0,1},{0,1,1},{1,1,1});
    Mesh combined;combined.append(scenery);combined.append(scenery);combined.append(car);combined.append(scenery);
    require(combined.ranges.size()==3,"Different geometry owners coalesced or matching scenery failed to coalesce");
    require(combined.ranges[0].first==0&&combined.ranges[0].count==6&&combined.ranges[0].courseGeometry&&
        combined.ranges[1].first==6&&combined.ranges[1].count==3&&!combined.ranges[1].courseGeometry&&
        combined.ranges[2].first==9&&combined.ranges[2].count==3&&combined.ranges[2].courseGeometry,
        "Append changed scenery/car ownership boundaries");
    for(const auto& range:combined.ranges)require(!range.courseLighting&&range.carLighting==0&&range.viewMask==3,
        "Geometry ownership changed lighting or camera scope");
    Mesh copied;copied.append(combined);equal(combined,copied);
    scenery.triangle({0,0,2},{1,0,2},{0,1,2},{1,1,1});
    require(scenery.ranges.size()==2&&!scenery.ranges.back().courseGeometry,"Default geometry inherited the previous scenery owner");

    Mesh culled=car;culled.ranges.front().sourceFaceCulling=true;
    Mesh faceGroups;faceGroups.append(culled);faceGroups.append(culled);faceGroups.append(car);faceGroups.append(culled);
    require(faceGroups.ranges.size()==3&&faceGroups.ranges[0].count==6&&faceGroups.ranges[0].sourceFaceCulling&&
        !faceGroups.ranges[1].sourceFaceCulling&&faceGroups.ranges[2].sourceFaceCulling,
        "Append lost ordinary/reflected face-culling ownership boundaries");
    Mesh faceCopy;faceCopy.append(faceGroups);equal(faceGroups,faceCopy);
}
void ordinaryFaceEligibility(){
    // A synthetic source triangle exercises the exact long-overload boundary:
    // normal, mirrored, singular, billboard and nonuniform proper instances.
    // Geometry and authored ISP2/3 must not change when the marker is requested.
    for(unsigned isp:{0x80000000u,0x88000000u,0x90000000u,0x98000000u}){
        NativeModel model;model.chunks.resize(1);auto& batch=model.chunks[0].batches.emplace_back();
        batch.ich[0]=0x88000002u;batch.ich[1]=isp;batch.material[2]=1;batch.material[3]=0xffffffffu;batch.material[9]=7;
        for(Vec3 p:std::array<Vec3,3>{{{0,0,0},{1,0,0},{0,1,0}}}){NativeModelVertex v;v.position=p;v.normal={0,0,1};batch.vertices.push_back(v);}
        batch.indices={0,1,2};NativeAssembly assembly;
        for(unsigned i=0;i<5;++i){NativeModelInstance v;v.transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            if(i==1)v.transform[0]=-1;if(i==2)v.transform[0]=0;if(i==3)v.billboard=true;if(i==4)v.transform[0]=2;
            assembly.instances.push_back(v);}
        Mesh baseline,tagged;baseline.originalCar(model,assembly,{12,34,56},.4f,.2f,-.1f,11);
        tagged.originalCar(model,assembly,{12,34,56},.4f,.2f,-.1f,11,{},true);
        require(baseline.vertices.size()==15&&tagged.vertices.size()==baseline.vertices.size()&&
            !std::memcmp(baseline.vertices.data(),tagged.vertices.data(),baseline.vertices.size()*sizeof(Vertex)),
            "Source culling marker changed source geometry, normals, colors or UVs");
        const auto rangeAt=[](const Mesh& mesh,unsigned vertex)->const MeshRange&{
            for(const auto& range:mesh.ranges)if(vertex>=range.first&&vertex<range.first+range.count)return range;
            throw std::runtime_error("Missing test range");};
        for(unsigned i=0;i<15;++i){const auto& before=rangeAt(baseline,i);const auto& after=rangeAt(tagged,i);
            require(!before.sourceFaceCulling&&after.sourceFaceCulling==(i/3==0||i/3==4),
                "Mirrored, singular or billboard instance received ordinary face culling");
            require(before.texture==after.texture&&before.tsp==after.tsp&&before.pcw==after.pcw&&before.isp==after.isp&&
                after.isp==isp&&before.gmp==after.gmp&&before.billboard==after.billboard&&before.emissive==after.emissive&&
                !after.courseGeometry&&!after.courseLighting&&after.carLighting==0&&after.viewMask==3,
                "Face-culling eligibility changed authored materials or render scope");}
        Mesh copy;copy.append(tagged);equal(tagged,copy);
        Mesh shortOverload;shortOverload.originalCar(model,assembly,{},0,11);
        for(const auto& range:shortOverload.ranges)require(!range.sourceFaceCulling,"Short overload changed its default culling contract");
        Mesh reflected;reflected.originalCar(model,assembly,{12,34,56},.4f,.2f,-.1f,11,{},true,true);
        require(!std::memcmp(reflected.vertices.data(),baseline.vertices.data(),baseline.vertices.size()*sizeof(Vertex)),"Reflection culling altered geometry");
        for(unsigned i=0;i<15;++i){const auto& r=rangeAt(reflected,i);const bool mirror=i/3==1;
            require(r.sourceFaceCulling==(i/3==0||mirror||i/3==4),"Menu reflection ownership eligibility");
            require(r.isp==(mirror&&((isp>>27)&3)>=2?(isp^(1u<<27)):isp),"Menu reflection changed depth state or wrong winding mode");}
    }
}
void insertedScenery(const NativeModel& model,const NativeAssembly& base){
    require(!base.instances.empty(),"Insertion fixture requires static geometry");
    CourseMeshCache cache;
    std::vector<NativeAssemblyInsertion> insertions;
    for(const auto before:{std::size_t(0),std::size_t(0),base.instances.size()/2,base.instances.size()}){
        NativeAssemblyInsertion item;item.before=before;item.assembly.instances.push_back(base.instances.front());
        item.assembly.instances[0].transform[3]+=float(insertions.size()+1)*17.f;
        insertions.push_back(std::move(item));
    }
    NativeAssemblyInsertion replacement;replacement.before=base.instances.size()/2;replacement.replaceCount=1;
    replacement.assembly.instances.push_back(base.instances[replacement.before]);insertions.push_back(replacement);
    std::stable_sort(insertions.begin(),insertions.end(),[](const auto& a,const auto& b){return a.before==b.before?a.replaceCount<b.replaceCount:a.before<b.before;});
    for(unsigned frame=0;frame<4;++frame){
        // Repeated frames, changed poses, and an object leaving the visible set.
        if(frame==2)for(auto& insertion:insertions)insertion.assembly.instances[0].transform[7]+=3.f;
        if(frame==3)insertions.erase(insertions.begin());
        NativeAssembly merged;std::size_t cursor=0;
        for(const auto& item:insertions){
            merged.instances.insert(merged.instances.end(),base.instances.begin()+cursor,base.instances.begin()+item.before);
            merged.instances.insert(merged.instances.end(),item.assembly.instances.begin(),item.assembly.instances.end());
            cursor=item.before+item.replaceCount;
        }
        merged.instances.insert(merged.instances.end(),base.instances.begin()+cursor,base.instances.end());
        for(bool tagged:{false,true}){
            Mesh expected,actual;expected.originalCar(model,merged,{},0);
            for(auto& range:expected.ranges)range.courseGeometry=tagged;
            cache.appendWithInsertions(actual,model,base,insertions,tagged);equal(expected,actual);verifyIdentities(actual);
        }
    }
}
void movingObjectWindow(const NativeModel& model,const NativeAssembly& base){
    for(bool tagged:{false,true}){
        CourseMeshCache cache;NativeAssemblyInsertion item;item.before=base.instances.size()/2;
        for(int n=0;n<12;++n){auto instance=base.instances.front();instance.transform[3]+=n*17.f;item.assembly.instances.push_back(instance);}
        for(unsigned frame=0;frame<9;++frame){
            auto& instances=item.assembly.instances;
            if(frame==1)instances.erase(instances.begin()+3);
            if(frame==2)instances.push_back(instances.front());
            if(frame==3)std::reverse(instances.begin(),instances.end());
            if(frame==4)instances[5].transform[7]+=1.f;
            if(frame==5)instances[4].transform[0]*=1.25f;
            if(frame==6)instances.clear();
            if(frame==7)instances.push_back(base.instances.back());
            NativeAssembly merged=base;merged.instances.insert(merged.instances.begin()+item.before,instances.begin(),instances.end());
            Mesh expected,actual;expected.originalCar(model,merged,{},0);
            for(auto& range:expected.ranges)range.courseGeometry=tagged;
            cache.appendWithInsertions(actual,model,base,std::span<const NativeAssemblyInsertion>(&item,1),tagged);
            equal(expected,actual);verifyIdentities(actual);
        }
    }
}
int main(int argc,char** argv)try{
    require(argc==2,"game-root required");const std::filesystem::path root=argv[1];
    ownershipBoundary();
    identityBoundaries();
    ordinaryFaceEligibility();
    CourseMeshCache cache;unsigned scenes=0,assemblies=0;std::uint64_t vertices=0;
    for(const auto& folder:std::filesystem::directory_iterator(root/"data/original_models/courses")){
        if(!folder.is_directory())continue;
        const auto id=folder.path().filename().string();
        for(const auto& file:std::filesystem::directory_iterator(folder.path())){
            const auto name=file.path().filename().string();if(!name.starts_with("scene_")||file.path().extension()!=".idasscene")continue;
            cache.invalidate();
            auto scene=OriginalCourseScene::load(root,id,name.find("night")!=std::string::npos,name.find("reverse")!=std::string::npos,name.find("_wet")!=std::string::npos);
            std::set<const NativeAssembly*> seen;
            for(unsigned index=0;index<20000;++index){
                const auto& assembly=scene.assemblyForPathIndex(index);if(!seen.insert(&assembly).second)continue;
                Mesh expected;expected.triangle({-1,0,0},{0,1,0},{1,0,0},{.3f,.4f,.5f});
                expected.originalCar(scene.model,assembly,{0,0,0},0);
                // Repeat same material across the append boundary, too.
                expected.originalCar(scene.model,assembly,{0,0,0},0);
                expected.triangle({2,0,0},{3,1,0},{4,0,0},{.3f,.4f,.5f});
                for(unsigned repeat=0;repeat<3;++repeat){
                    Mesh actual;actual.triangle({-1,0,0},{0,1,0},{1,0,0},{.3f,.4f,.5f});
                    cache.appendTo(actual,scene.model,assembly);cache.appendTo(actual,scene.model,assembly);
                    actual.triangle({2,0,0},{3,1,0},{4,0,0},{.3f,.4f,.5f});equal(expected,actual);verifyIdentities(actual);
                }
                ++assemblies;vertices+=expected.vertices.size();
            }
            // In-place scene replacement can preserve the object address.
            // Explicit invalidation must pick up changes at that same address.
            const auto& first=scene.assemblyForPathIndex(0);
            insertedScenery(scene.model,first);
            movingObjectWindow(scene.model,first);
            Mesh old;cache.appendTo(old,scene.model,first);
            if(scenes==0){
                // Same model/assembly pointers with a different owner must
                // invalidate the cached tag while retaining all source data.
                for(bool tagged:{true,true,false,false,true}){
                    Mesh expected=old,actual;
                    for(auto& range:expected.ranges)range.courseGeometry=tagged;
                    cache.appendTo(actual,scene.model,first,tagged);equal(expected,actual);
                }
            }
            for(auto& chunk:scene.model.chunks)for(auto& batch:chunk.batches)for(auto& v:batch.vertices)v.position.y+=.25f;
            cache.invalidate();Mesh expected,actual;expected.originalCar(scene.model,first,{0,0,0},0);cache.appendTo(actual,scene.model,first);equal(expected,actual);
            ++scenes;
        }
    }
    require(scenes>=36&&assemblies>100,"Course catalog coverage unexpectedly small");
    std::cout<<"PASS "<<scenes<<" scene variants, "<<assemblies<<" original assemblies, "<<vertices<<" vertices per comparison sweep; cold/hot cache, repeated appends, material/ownership boundaries, same-assembly owner changes and in-place reload invalidation.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
