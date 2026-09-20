#include "car_presentation.h"
#include "car_catalog.h"
#include "original_car_color_catalog.h"
#include "car_lamp_catalog.h"
#include "original_rival_appearance_catalog.h"
#include "original_number_plate.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
template<class T>T read(std::istream& f){T value{};if(!f.read(reinterpret_cast<char*>(&value),sizeof value))throw std::runtime_error("Truncated car wheel program");return value;}
void phaseRotate(original::OriginalMatrix& m,unsigned axis,std::uint16_t phase,const original::OriginalFscaTable& trig){
    const auto sc=trig.sinCos(phase);const float s=sc[0],c=sc[1];
    auto r=original::originalIdentityMatrix();
    if(axis==0){r.elements[5]=c;r.elements[6]=s;r.elements[9]=-s;r.elements[10]=c;}
    else if(axis==1){r.elements[0]=c;r.elements[2]=-s;r.elements[8]=s;r.elements[10]=c;}
    else{r.elements[0]=c;r.elements[1]=s;r.elements[4]=-s;r.elements[5]=c;}
    original::OriginalMatrix next;
    for(unsigned col=0;col<4;++col){const auto v=original::transformOriginalVector(m,{r.elements[col*4],r.elements[col*4+1],r.elements[col*4+2],r.elements[col*4+3]});for(unsigned row=0;row<4;++row)next.elements[col*4+row]=v[row];}
    m=next;
}
}
CarPresentation CarPresentation::load(const std::filesystem::path& root,unsigned carId,std::size_t chunks,unsigned factoryColor){
    if(carId>=originalCarFolders.size())throw std::runtime_error("Car presentation ID outside original catalog");
    if(factoryColor>=original::originalCarColorCounts[carId])throw std::out_of_range("Factory paint outside original car palette");
    const auto directory=std::filesystem::path("appearance_v2")/("color_0"+std::to_string(factoryColor));
    return loadAppearance(root,root/"data/original_models"/originalCarFolders[carId]/directory,carId,chunks);
}
CarPresentation CarPresentation::loadRival(const std::filesystem::path& root,unsigned carId,unsigned enemyId,std::size_t chunks){
    if(enemyId>=originalRivalAppearances.size()||originalRivalAppearances[enemyId].car!=carId)throw std::runtime_error("Original rival appearance car/preset mismatch");
    const std::string folder="enemy_"+std::string(enemyId<10?"0":"")+std::to_string(enemyId);
    return loadAppearance(root,root/"data/original_models/rivals_v2"/folder,carId,chunks);
}
CarPresentation CarPresentation::loadPlayerProfile(const std::filesystem::path& root,const original::OriginalBattleProfile& profile,unsigned materialVariant){
    return loadConfiguredAppearance(root,profile,original::originalPlayerAppearanceConfig(profile,materialVariant));
}
CarPresentation CarPresentation::loadConfiguredAppearance(const std::filesystem::path& root,const original::OriginalBattleProfile& profile,original::OriginalCarAppearanceConfig appearance,
        unsigned overlayLayers,std::optional<unsigned> environmentTexture){
    if(appearance.car!=profile.u(16))throw std::invalid_argument("Configured appearance and driver car differ");
    if(overlayLayers>3)throw std::invalid_argument("Configured car overlay mask");
    CarPresentation out;
    out.profileInput_.appearance=appearance;
    const auto car=out.profileInput_.appearance.car;
    const auto base=root/"data/original_models"/originalCarFolders.at(car);
    out.profileMaterials_=original::OriginalCarMaterialRebuild::load(base/"material_layout.bin",car);
    out.profileMaterials_->rebuild(out.profileInput_.appearance,profile.u(32));
    out.profileInput_.visibility=original::originalCarVisibility(out.profileInput_.appearance);
    out.profileInput_.wheelOffsets=original::originalCarWheelOffsets(out.profileInput_.appearance);
    // Ordinary cars keep mask0. Explicit showroom consumers can request the
    // source layer geometry and materials; complete ELAN list-state behavior
    // remains the renderer's separate boundary, as in the result preview.
    out.profileInput_.overlayLayers=overlayLayers;out.profileInput_.effects=0;
    //026402..02642A initializes both layer matrices to the current matrix,
    // with zero layer parameters.11E532/12B792 select only the second layer.
    out.profileInput_.primaryUsesCurrent=out.profileInput_.secondaryUsesCurrent=true;
    out.profileEnvironmentTexture_=environmentTexture;
    out.profileParts_=original::OriginalCarParts::load(base/"assembly_parts.bin");
    out.profileContext_.semanticChunks=out.profileMaterials_->semanticChunks();
    out.profileContext_.carChunkCount=out.profileMaterials_->chunks().size();
    out.profileContext_.digits=OriginalNumberPlate::playerDigits(profile);
    out.trig_=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    out.pose({});out.resetHeadlights();return out;
}
void CarPresentation::advanceOriginalFrame(bool lightsOn){
    if(!profileMaterials_){headlights_.advance(lightsOn);return;}
    profileInput_.lights=lightsOn;profileInput_.headlights=headlights_;
    original::advanceOriginalCarHeadlights(profileInput_);headlights_=profileInput_.headlights;
}
CarPresentation CarPresentation::loadAppearance(const std::filesystem::path& root,const std::filesystem::path& base,unsigned carId,std::size_t chunks){
    CarPresentation out;
    out.assembly_=NativeAssembly::load(base/"car.idasasm",chunks);
    out.baseCount_=out.assembly_.instances.size();out.lampChunks_=originalCarLampChunks[carId];
    unsigned rearCount=0;
    for(std::size_t i=0;i<out.baseCount_;++i)if(out.assembly_.instances[i].chunk==std::uint32_t(out.lampChunks_[0])){out.rearLampInstance_=i;++rearCount;}
    if(rearCount!=1)throw std::runtime_error("Original rear lamp instance missing or ambiguous");
    for(const auto chunk:out.lampChunks_)if(chunk<0||std::size_t(chunk)>=chunks)throw std::runtime_error("Original lamp chunk outside model");
    out.trig_=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    std::ifstream file(base/"wheel_motion.bin",std::ios::binary);
    const auto magic=read<std::array<char,8>>(file);
    if(std::memcmp(magic.data(),"ID3MOT1\0",8)!=0||read<std::uint32_t>(file)!=1)throw std::runtime_error("Invalid car wheel program format");
    const auto count=read<std::uint32_t>(file);if(count<4||count>16)throw std::runtime_error("Invalid car wheel program count");
    for(unsigned i=0;i<count;++i){Program p;p.instance=read<std::uint32_t>(file);const auto n=read<std::uint32_t>(file);
        if(p.instance>=out.assembly_.instances.size()||n==0||n>32)throw std::runtime_error("Car wheel program bound");
        for(unsigned j=0;j<n;++j){Operation op;op.kind=read<std::uint32_t>(file);op.channel=read<std::int32_t>(file);op.values=read<std::array<float,16>>(file);
            if(op.kind>8||op.channel<-9||op.channel>9)throw std::runtime_error("Invalid car wheel operation");
            for(const auto v:op.values)if(!std::isfinite(v))throw std::runtime_error("Non-finite car wheel operation");
            p.operations.push_back(op);}
        out.programs_.push_back(std::move(p));}
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing car wheel program bytes");
    std::ifstream materials(base/"materials.bin",std::ios::binary);
    const auto materialMagic=read<std::array<char,8>>(materials);
    if(std::memcmp(materialMagic.data(),"ID3CMP1\0",8)||read<std::uint32_t>(materials)!=1)throw std::runtime_error("Original car material format");
    const auto materialCount=read<std::uint32_t>(materials);if(materialCount>20000)throw std::runtime_error("Original car material count bound");
    std::uint64_t previous=0;
    for(unsigned i=0;i<materialCount;++i){MaterialPatch p;p.chunk=read<std::uint32_t>(materials);p.batch=read<std::uint32_t>(materials);
        p.before=read<std::array<std::uint32_t,24>>(materials);p.after=read<std::array<std::uint32_t,24>>(materials);
        const auto key=(std::uint64_t(p.chunk)<<32)|p.batch;
        if(p.chunk>=chunks||p.batch>=20000||(i&&key<=previous))throw std::runtime_error("Original car material index bound/order");
        if(p.before[0]!=p.after[0]||p.before[16]!=p.after[16]||p.before[22]!=p.after[22]||p.before[23]!=p.after[23])throw std::runtime_error("Original material changes geometry descriptor");
        for(unsigned n=8;n<16;++n)if(p.before[n]!=p.after[n])throw std::runtime_error("Original material changes texture identity");
        previous=key;out.materials_.push_back(p);
    }
    if(materials.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original car material bytes");
    std::ifstream headlights(base/"headlights.bin",std::ios::binary);
    const auto headlightMagic=read<std::array<char,8>>(headlights);
    if(std::memcmp(headlightMagic.data(),"ID3POP1\0",8)||read<std::uint32_t>(headlights)!=1)throw std::runtime_error("Original headlight format");
    out.headlightInstance_=read<std::uint32_t>(headlights);out.headlights_.maximumPhase=read<std::uint32_t>(headlights);
    if(read<std::uint32_t>(headlights)!=2||out.headlights_.maximumPhase>65535)throw std::runtime_error("Original headlight state bound");
    const bool absent=out.headlightInstance_==0xffffffffu;
    if(!absent&&(out.headlightInstance_>=out.baseCount_||out.headlightInstance_==out.rearLampInstance_))throw std::runtime_error("Original headlight instance bound");
    for(auto& p:out.headlightPrograms_){p.chunk=read<std::uint32_t>(headlights);const auto count=read<std::uint32_t>(headlights);
        // The source disables both variants for enemy29. Preserve that preset;
        // do not substitute its stock popup parts into the selected body.
        if(absent){if(p.chunk!=0xffffffffu||count)throw std::runtime_error("Disabled original headlight has geometry");continue;}
        if(p.chunk>=chunks||count==0||count>32)throw std::runtime_error("Original headlight program bound");
        for(unsigned i=0;i<count;++i){Operation op;op.kind=read<std::uint32_t>(headlights);op.channel=read<std::int32_t>(headlights);op.values=read<std::array<float,16>>(headlights);
            if(op.kind>8||op.channel<0||op.channel>1||(op.channel&&op.kind!=3))throw std::runtime_error("Invalid original headlight operation");
            for(const auto value:op.values)if(!std::isfinite(value))throw std::runtime_error("Non-finite original headlight operation");
            p.operations.push_back(op);}
    }
    if(!absent&&out.assembly_.instances[out.headlightInstance_].chunk!=out.headlightPrograms_[0].chunk)throw std::runtime_error("Original closed headlight source mismatch");
    if(headlights.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original headlight bytes");
    std::ifstream lighting(base/"lighting.bin",std::ios::binary);
    const auto lightingMagic=read<std::array<char,8>>(lighting);
    if(std::memcmp(lightingMagic.data(),"ID3LIT1\0",8)||read<std::uint32_t>(lighting)!=1||read<std::uint32_t>(lighting)!=4)throw std::runtime_error("Original lighting state format");
    for(unsigned state=0;state<4;++state){auto& selected=out.lightingStates_[state];
        const auto extras=read<std::uint32_t>(lighting);if(extras>64)throw std::runtime_error("Original lighting attachment bound");
        for(unsigned i=0;i<extras;++i){NativeModelInstance instance;instance.chunk=read<std::uint32_t>(lighting);instance.transform=read<std::array<float,16>>(lighting);
            if(instance.chunk>=chunks)throw std::runtime_error("Original lighting attachment chunk bound");
            for(float v:instance.transform)if(!std::isfinite(v))throw std::runtime_error("Non-finite original lighting transform");
            selected.extras.instances.push_back(instance);
        }
        const auto count=read<std::uint32_t>(lighting);const auto expected=out.baseCount_+unsigned(bool(state&1))+unsigned(bool(state&2))+extras;
        if(count!=expected)throw std::runtime_error("Original lighting draw count");
        std::vector<bool> present(count);
        for(unsigned i=0;i<count;++i){const auto index=read<std::uint32_t>(lighting);if(index>=count||present[index])throw std::runtime_error("Original lighting draw permutation");present[index]=true;selected.order.push_back(index);}
    }
    if(lighting.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original lighting bytes");
    out.pose({});out.resetHeadlights();return out;
}
void CarPresentation::applyMaterials(NativeModel& model)const{
    if(profileMaterials_){
        model=profileMaterials_->apply(model);
        if(profileEnvironmentTexture_){
            const auto& slots=profileMaterials_->semanticChunks();const auto environmentChunk=slots.at(140);
            if(environmentChunk<0)throw std::runtime_error("Configured car environment material missing");
            const auto& records=profileMaterials_->chunks().at(unsigned(environmentChunk)).materials;
            if(records.empty())throw std::runtime_error("Configured car environment material empty");
            const auto sourceTexture=records.back().words[9];
            if(sourceTexture==0xffffffffu)throw std::runtime_error("Configured car environment texture missing");
            //026CBC obtains the shared handle from semantic140.029DA0(3)
            // replaces it; copied secondary materials retain that same handle.
            for(unsigned slot=140;slot<=186;++slot)if(const auto chunk=slots[slot];chunk>=0)
                for(auto& batch:model.chunks.at(unsigned(chunk)).batches)if(batch.material[9]==sourceTexture)
                    batch.material[9]=*profileEnvironmentTexture_;
        }
        return;
    }
    for(const auto& p:materials_){
        if(p.chunk>=model.chunks.size()||p.batch>=model.chunks[p.chunk].batches.size())throw std::runtime_error("Original car material/model shape mismatch");
        const auto& b=model.chunks[p.chunk].batches[p.batch];std::array<std::uint32_t,24> words;
        std::copy(b.material.begin(),b.material.end(),words.begin());std::copy(b.ich.begin(),b.ich.end(),words.begin()+16);
        if(words!=p.before&&words!=p.after)throw std::runtime_error("Original car material source words mismatch");
    }
    for(const auto& p:materials_){auto& b=model.chunks[p.chunk].batches[p.batch];
        std::copy_n(p.after.begin(),16,b.material.begin());std::copy_n(p.after.begin()+16,8,b.ich.begin());}
}
const NativeAssembly& CarPresentation::pose(const CarWheelPose& wheels,bool lightsOn,bool braking){
    if(profileMaterials_){
        if(headlights_.counter<0||!profileInput_.visibility.popupMotorEnabled)advanceOriginalFrame(lightsOn);
        profileInput_.lights=lightsOn;profileInput_.braking=braking;profileInput_.headlights=headlights_;
        profileInput_.steering=wheels.steeringRadians;profileInput_.suspension=wheels.suspensionY;profileInput_.spin=wheels.rotationRadians;
        const auto frame=original::originalCarRenderFrame(original::originalCarAssemblyPose(profileInput_,profileParts_),profileContext_,trig_);
        posedAssembly_=frame.geometry(original::OriginalCarRenderBank::car);
        profilePlateAssembly_=frame.geometry(original::OriginalCarRenderBank::numberPlate);
        illuminatedChunks_.clear();
        const auto addLamp=[&](unsigned slot){const auto semantic=profileInput_.visibility.slots[slot];
            if(semantic<212){const int chunk=profileContext_.semanticChunks[semantic];if(chunk>=0)illuminatedChunks_.push_back(unsigned(chunk));}};
        if(lightsOn)addLamp(3);if(braking)addLamp(lightsOn?4:2);if(lightsOn)addLamp(79);
        return posedAssembly_;
    }
    if(headlights_.counter<0||headlights_.maximumPhase==0)headlights_.advance(lightsOn);
    // Original026EB2..026F9C: byte81 substitutes night rear3 for day rear1;
    // byte80 adds brake4/2; illuminated plate79 is added whenever lights are on.
    assembly_.instances.resize(baseCount_);illuminatedChunks_.clear();
    auto& rear=assembly_.instances[rearLampInstance_];rear.chunk=std::uint32_t(lampChunks_[lightsOn?2:0]);
    const auto lampTransform=rear.transform;
    if(lightsOn)illuminatedChunks_.push_back(rear.chunk);
    const auto addLamp=[&](unsigned index){const auto chunk=std::uint32_t(lampChunks_[index]);assembly_.instances.push_back({chunk,lampTransform});illuminatedChunks_.push_back(chunk);};
    if(braking)addLamp(lightsOn?3:1);
    if(lightsOn)addLamp(4);
    const std::array<float,10> channels={0,wheels.steeringRadians,wheels.suspensionY[0],wheels.suspensionY[1],wheels.suspensionY[2],wheels.suspensionY[3],wheels.rotationRadians[0],wheels.rotationRadians[1],wheels.rotationRadians[2],wheels.rotationRadians[3]};
    for(const auto value:channels)if(!std::isfinite(value))throw std::runtime_error("Non-finite original wheel pose");
    for(const auto& program:programs_){auto m=original::originalIdentityMatrix();
        for(const auto& op:program.operations){auto v=op.values;
            if(op.channel){const float value=channels[std::abs(op.channel)]*(op.channel<0?-1.0f:1.0f);v[op.kind==0?1:0]=value;}
            switch(op.kind){
            case 0:original::translateOriginalMatrix(m,{v[0],v[1],v[2]});break;
            case 1:for(unsigned col=0;col<3;++col)for(unsigned row=0;row<4;++row)m.elements[col*4+row]*=v[col];break;
            case 2:m.elements=v;break;
            case 3:case 4:case 5:phaseRotate(m,op.kind-3,std::uint16_t(std::uint32_t(v[0])),trig_);break;
            case 6:original::rotateOriginalMatrixX(m,v[0],trig_);break;
            case 7:original::rotateOriginalMatrixY(m,v[0],trig_);break;
            case 8:original::rotateOriginalMatrixZ(m,v[0],trig_);break;
            }
        }
        auto& target=assembly_.instances[program.instance].transform;
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)target[row*4+col]=m.elements[col*4+row];
    }
    if(headlightInstance_!=0xffffffffu){
        const auto& program=headlightPrograms_[headlights_.visible?1:0];auto m=original::originalIdentityMatrix();
        for(const auto& op:program.operations){auto v=op.values;if(op.channel)v[0]=float(headlights_.phase);
            switch(op.kind){
            case 0:original::translateOriginalMatrix(m,{v[0],v[1],v[2]});break;
            case 1:for(unsigned col=0;col<3;++col)for(unsigned row=0;row<4;++row)m.elements[col*4+row]*=v[col];break;
            case 2:m.elements=v;break;
            case 3:case 4:case 5:phaseRotate(m,op.kind-3,std::uint16_t(std::uint32_t(v[0])),trig_);break;
            case 6:original::rotateOriginalMatrixX(m,v[0],trig_);break;
            case 7:original::rotateOriginalMatrixY(m,v[0],trig_);break;
            case 8:original::rotateOriginalMatrixZ(m,v[0],trig_);break;
            }
        }
        auto& instance=assembly_.instances[headlightInstance_];instance.chunk=program.chunk;
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)instance.transform[row*4+col]=m.elements[col*4+row];
    }
    const auto& selected=lightingStates_[unsigned(braking)+2*unsigned(lightsOn)];
    assembly_.instances.insert(assembly_.instances.end(),selected.extras.instances.begin(),selected.extras.instances.end());
    posedAssembly_.instances.clear();posedAssembly_.instances.reserve(selected.order.size());
    for(auto index:selected.order)posedAssembly_.instances.push_back(assembly_.instances[index]);
    return posedAssembly_;
}
}
