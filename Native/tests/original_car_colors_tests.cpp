#include "car_catalog.h"
#include "car_presentation.h"
#include "original_car_color_catalog.h"
#include "original_number_plate.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace idas3;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<char> bytes(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Missing factory appearance file: "+path.string());
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
bool equalAssembly(const NativeAssembly& a,const NativeAssembly& b){
    if(a.instances.size()!=b.instances.size())return false;
    for(std::size_t i=0;i<a.instances.size();++i){
        if(a.instances[i].chunk!=b.instances[i].chunk)return false;
        for(unsigned j=0;j<16;++j)if(std::bit_cast<std::uint32_t>(a.instances[i].transform[j])!=std::bit_cast<std::uint32_t>(b.instances[i].transform[j]))return false;
    }
    return true;
}
std::uint64_t geometryHash(const NativeModel& model){
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t v){hash^=v;hash*=1099511628211ull;};
    for(const auto& chunk:model.chunks){
        word(chunk.index);word(chunk.sourceOffset);word(chunk.sourceSize);for(auto w:chunk.header)word(w);
        for(const auto& batch:chunk.batches){
            word(batch.sourceOffset);word(std::uint32_t(batch.vertices.size()));word(std::uint32_t(batch.indices.size()));
            for(const auto& v:batch.vertices){
                word(v.header);word(v.color0);word(v.color1);
                for(float f:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(f));
            }
            for(auto index:batch.indices)word(index);
        }
    }
    return hash;
}
std::uint64_t materialHash(const NativeModel& model){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& chunk:model.chunks)for(const auto& batch:chunk.batches){
        for(auto word:batch.material){hash^=word;hash*=1099511628211ull;}
        for(auto word:batch.ich){hash^=word;hash*=1099511628211ull;}
    }
    return hash;
}
void validateModel(const NativeModel& model,const NativeTextureBank& textures){
    require(!model.chunks.empty()&&textures.size()!=0,"Factory model/textures are empty");
    for(const auto& chunk:model.chunks)for(const auto& batch:chunk.batches){
        require(batch.indices.size()%3==0,"Factory model indices are not triangles");
        for(auto index:batch.indices)require(index<batch.vertices.size(),"Factory model vertex index is outside its batch");
        for(const auto& vertex:batch.vertices)for(float f:{vertex.position.x,vertex.position.y,vertex.position.z,vertex.normal.x,vertex.normal.y,vertex.normal.z,vertex.u,vertex.v})
            require(std::isfinite(f),"Factory model contains non-finite geometry");
        if(batch.material[9]!=0xffffffffu){
            const auto& image=textures.at(batch.material[9]);
            require(image.width&&image.height&&image.argb.size()==std::size_t(image.width)*image.height,"Factory model texture is incomplete");
        }
    }
}
std::size_t validateAssembly(const NativeAssembly& assembly,const NativeModel& model){
    require(!assembly.instances.empty(),"Factory assembly is empty");
    std::size_t vertices=0;
    for(const auto& instance:assembly.instances){
        require(instance.chunk<model.chunks.size(),"Factory assembly references an absent chunk");
        for(float f:instance.transform)require(std::isfinite(f),"Factory instance transform is non-finite");
        require(instance.transform[12]==0&&instance.transform[13]==0&&instance.transform[14]==0&&instance.transform[15]==1,"Factory instance transform is not affine");
        const auto& m=instance.transform;
        for(const auto& batch:model.chunks[instance.chunk].batches)for(const auto& v:batch.vertices){
            const auto& p=v.position;
            const float x=m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3];
            const float y=m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7];
            const float z=m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11];
            require(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z),"Factory instance produces non-finite transformed vertices");
            ++vertices;
        }
    }
    require(vertices!=0,"Factory assembly has no renderable geometry");return vertices;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("project-root required");
    const std::filesystem::path root=argv[1];
    unsigned appearances=0,defaults=0;std::size_t patches=0,poses=0,vertices=0,animated=0;
    bool s13AlternateBody=false;
    for(unsigned car=0;car<originalCarFolders.size();++car){
        const std::string folder(originalCarFolders[car]);const auto basePath=root/"data/original_models"/folder;
        const auto source=NativeModel::load(basePath/(folder+".idasmesh"));
        const auto textures=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        validateModel(source,textures);const auto geometry=geometryHash(source);
        auto implicit=CarPresentation::load(root,car,source.chunks.size());auto implicitModel=source;implicit.applyMaterials(implicitModel);
        const auto defaultAssembly=implicit.assembly();const auto implicitPlate=OriginalNumberPlate::load(root,car);
        // Fresh defaults remain byte-identical to their independently captured
        // color00 files, including full part programs and plate placements.
        for(const char* name:{"car.idasasm","wheel_motion.bin","headlights.bin","materials.bin","plate.bin"})
            require(bytes(basePath/"fresh_player"/name)==bytes(basePath/"colors/color_00"/name),"Factory expansion changed a pre-existing fresh default");
        for(unsigned color=0;color<original::originalCarColorCounts[car];++color)try{
            auto presentation=CarPresentation::load(root,car,source.chunks.size(),color);auto model=source;
            presentation.applyMaterials(model);require(geometryHash(model)==geometry,"Factory material patch changed source geometry");
            const auto materials=materialHash(model);presentation.applyMaterials(model);
            require(materialHash(model)==materials,"Factory material application is not idempotent");
            validateModel(model,textures);require(presentation.materialPatchCount()!=0,"Factory appearance omitted its material program");
            bool havePaint=false;
            for(std::size_t c=0;c<source.chunks.size();++c)for(std::size_t b=0;b<source.chunks[c].batches.size();++b){
                const auto& before=source.chunks[c].batches[b];const auto& after=model.chunks[c].batches[b];
                if((before.material[3]&0xffffffu)==0xffffffu&&before.material[9]==0xffffffffu&&
                    (after.material[3]&0xffffffu)==original::originalCarPaintRgb[car][color])havePaint=true;
            }
            require(havePaint,"Factory appearance has no material using its original palette RGB");
            patches+=presentation.materialPatchCount();require(presentation.animatedInstanceCount()>=4,"Factory appearance omitted a wheel program");
            animated+=presentation.animatedInstanceCount();const auto neutral=presentation.pose({});
            if(car==9&&color==1){
                // This source color selects alternate S13 body geometry. A
                // paint-only implementation would silently retain color00.
                if(neutral.instances.size()!=defaultAssembly.instances.size())s13AlternateBody=true;
                else for(std::size_t i=0;i<neutral.instances.size();++i)s13AlternateBody|=neutral.instances[i].chunk!=defaultAssembly.instances[i].chunk;
            }
            for(unsigned state=0;state<3;++state){
                const bool lights=state==2,brake=state!=0;
                CarWheelPose wheel;if(state){wheel.steeringRadians=state==1?.31f:-.27f;wheel.suspensionY={.03f,-.04f,.07f,-.02f};wheel.rotationRadians={1.5f,-2.f,3.f,-4.5f};}
                for(unsigned tick=0;tick<40;++tick)presentation.advanceOriginalFrame(lights);
                const auto assembly=presentation.pose(wheel,lights,brake);
                require(assembly.instances.size()==neutral.instances.size()+unsigned(brake)+unsigned(lights)+presentation.lightingAttachmentCount(lights,brake),"Factory lamp assembly lost base parts or duplicated dynamic lamps");
                vertices+=validateAssembly(assembly,model);++poses;
                require(presentation.illuminatedChunks().size()==unsigned(brake)+2*unsigned(lights),"Factory lamp illumination list is incomplete");
                for(auto chunk:presentation.illuminatedChunks())require(chunk<model.chunks.size(),"Factory illuminated chunk is out of bounds");
                const auto counter=presentation.headlightState().counter;
                require(equalAssembly(assembly,presentation.pose(wheel,lights,brake))&&presentation.headlightState().counter==counter,"Repeated render advanced wheel/headlight state");
            }
            presentation.resetHeadlights();require(equalAssembly(neutral,presentation.pose({})),"Factory presentation failed to return to its neutral pose");
            auto plate=OriginalNumberPlate::load(root,car,color);validateModel(plate.model,plate.textures);
            vertices+=validateAssembly(plate.assembly(),plate.model);require(plate.assembly().instances.size()==12,"Factory appearance omitted a plate or digit");
            for(unsigned side=0;side<2;++side){
                require(plate.assembly().instances[side*6].chunk==10,"Factory number plate backing missing");
                for(unsigned n=0;n<5;++n)require(plate.assembly().instances[side*6+n+1].chunk==OriginalNumberPlate::freshDigits()[n],"Factory appearance changed fresh plate digits");
            }
            if(color==0){
                require(equalAssembly(neutral,defaultAssembly)&&materialHash(model)==materialHash(implicitModel)&&equalAssembly(plate.assembly(),implicitPlate.assembly()),"Explicit color0 changed default car or plate loading");++defaults;
            }
            const auto originalPlate=plate.assembly();plate.setDigits({9,0,1,8,4});
            for(unsigned i=0;i<12;++i)require(plate.assembly().instances[i].transform==originalPlate.instances[i].transform,"Changing plate digits moved a factory plate");
            ++appearances;
        }catch(const std::exception& e){throw std::runtime_error("car "+std::to_string(car)+" color "+std::to_string(color)+": "+e.what());}
        bool carRejected=false,plateRejected=false;
        try{(void)CarPresentation::load(root,car,source.chunks.size(),original::originalCarColorCounts[car]);}catch(const std::out_of_range&){carRejected=true;}
        try{(void)OriginalNumberPlate::load(root,car,original::originalCarColorCounts[car]);}catch(const std::out_of_range&){plateRejected=true;}
        require(carRejected&&plateRejected,"Appearance loaders accepted a color beyond the original palette");
    }
    require(appearances==181&&defaults==35&&s13AlternateBody,"Factory color/default/alternate-body coverage is incomplete");
    std::cout<<"PASS181 complete factory appearances,35 unchanged fresh defaults, S13 alternate body, "<<patches<<" material patches, "<<animated<<" wheel programs, "<<poses<<" day/brake/night poses and "<<vertices<<" finite transformed vertices, two plates per color. Native integration validation; source-instruction parity is tested separately.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
