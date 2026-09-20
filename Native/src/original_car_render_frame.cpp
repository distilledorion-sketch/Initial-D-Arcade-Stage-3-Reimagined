#include "original_car_render_frame.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
bool OriginalCarRenderItem::isGeometry()const{
    using K=CarAssemblyCommandKind;
    return operation==K::DrawMaterial||operation==K::DrawDirect||operation==K::NumberPlate||operation==K::WheelBlur;
}
NativeAssembly OriginalCarRenderFrame::geometry(OriginalCarRenderBank bank)const{
    NativeAssembly result;
    for(const auto& item:items)if(item.isGeometry()&&item.bank==bank){NativeModelInstance instance;instance.chunk=item.chunk;
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)instance.transform[row*4+column]=item.matrix.elements[column*4+row];
        result.instances.push_back(instance);
    }
    return result;
}
OriginalCarRenderFrame originalCarRenderFrame(std::span<const CarAssemblyCommand> commands,
        const OriginalCarRenderContext& context,const OriginalFscaTable& trig){
    using K=CarAssemblyCommandKind;using Bank=OriginalCarRenderBank;
    for(auto digit:context.digits)if(digit>9)throw std::out_of_range("Original number-plate digit");
    for(const auto* matrix:{&context.current,&context.primary,&context.secondary})for(float value:matrix->elements)
        if(!std::isfinite(value))throw std::invalid_argument("Non-finite original car matrix");
    OriginalCarRenderFrame out;out.items.reserve(commands.size());out.wheelBlurDiffuse=context.wheelBlurDiffuse;
    auto current=context.current;std::vector<OriginalMatrix> stack;stack.reserve(8);
    auto submit=[&](K kind,Bank bank=Bank::car,unsigned chunk=0xffffffffu,std::array<unsigned,4> parameters={},unsigned diffuse=0){
        out.items.push_back({kind,bank,chunk,current,parameters,diffuse});
    };
    for(const auto& command:commands){const auto& w=command.words;
        const auto number=[&](unsigned index){const float value=std::bit_cast<float>(w[index]);if(!std::isfinite(value))throw std::invalid_argument("Non-finite original car transform");return value;};
        switch(command.kind){
        case K::Push:stack.push_back(current);break;
        case K::LoadPrimary:stack.push_back(current);current=context.primary;break;
        case K::LoadSecondary:stack.push_back(current);current=context.secondary;break;
        case K::Pop:{const unsigned count=std::bit_cast<std::int32_t>(w[0])>0?w[0]:1;
            if(count>stack.size())throw std::runtime_error("Original car matrix stack underflow");
            current=stack[stack.size()-count];stack.resize(stack.size()-count);break;}
        case K::Translate:translateOriginalMatrix(current,{number(0),number(1),number(2)});break;
        case K::Scale:scaleOriginalMatrix(current,{number(0),number(1),number(2)});break;
        case K::RotateXPhase:case K::RotateYPhase:case K::RotateZPhase:
            rotateOriginalMatrixPhase(current,command.kind==K::RotateXPhase?0:command.kind==K::RotateYPhase?1:2,std::uint16_t(w[0]),trig);break;
        case K::RotateXRadians:rotateOriginalMatrixX(current,number(0),trig);break;
        case K::RotateYRadians:rotateOriginalMatrixY(current,number(0),trig);break;
        case K::DrawMaterial:case K::DrawDirect:{
            //1000 requests the assembly owner's separate identity submission.
            // A later ordinary attempt must not submit it again (car24's
            // optional slot26 can reach this path; source lookup is absent).
            const unsigned semantic=w[1];if(semantic==0xffffffffu||semantic==1000)break;
            if(semantic>=context.semanticChunks.size())throw std::out_of_range("Unresolved original car semantic");
            const int chunk=context.semanticChunks[semantic];if(chunk<0)break;
            if(std::size_t(chunk)>=context.carChunkCount)throw std::out_of_range("Original car model chunk");
            submit(command.kind,Bank::car,unsigned(chunk));break;}
        case K::NumberPlate:{
            //026160 draws the backing, then five glyphs at independent offsets.
            submit(K::NumberPlate,Bank::numberPlate,10);
            const auto base=current;constexpr std::array<float,5> digitX{-.11f,-.065f,.015f,.06f,.105f};
            for(unsigned index=0;index<5;++index){current=base;translateOriginalMatrix(current,{digitX[index],0,0});submit(K::NumberPlate,Bank::numberPlate,context.digits[index]);}
            current=base;break;}
        case K::WheelBlur:{
            //029EE0 changes the shared GMP alpha even for a zero/negative
            // intensity. Only positive intensity submits the translated model.
            const float intensity=number(0),scaled=255.0f*intensity;
            std::uint32_t alpha;
            if(scaled<=-2147483648.0f)alpha=0x80000000u;
            else if(scaled>=2147483648.0f)alpha=0x7fffffffu;
            else alpha=std::uint32_t(std::int32_t(scaled));
            out.wheelBlurDiffuse=(out.wheelBlurDiffuse&0x00ffffffu)|(alpha<<24);
            if(intensity>0){const auto base=current;translateOriginalMatrix(current,{-.025f,0,0});
                submit(K::WheelBlur,Bank::wheelBlur,0,{},out.wheelBlurDiffuse);current=base;}
            break;}
        case K::MainDrawState:case K::LayerDrawState:case K::LayerParameters:case K::BodyEffect:case K::WheelEffect:
            submit(command.kind,Bank::car,0xffffffffu,w);break;
        default:throw std::invalid_argument("Unknown original car render operation");
        }
    }
    if(!stack.empty())throw std::runtime_error("Unbalanced original car matrix stack");
    out.finalMatrix=current;return out;
}
}
