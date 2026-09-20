#include "original_car_assembly.h"
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
OriginalCarParts OriginalCarParts::load(const std::filesystem::path& path){
    OriginalCarParts out;std::ifstream f(path,std::ios::binary);
    for(auto& t:out.transforms)for(auto* values:{&t.scale,&t.rotationDegrees,&t.translation})for(float& value:*values){
        if(!f.read(reinterpret_cast<char*>(&value),4)||!std::isfinite(value))throw std::runtime_error("Invalid original car part transform");}
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Original car part transform extent");return out;
}
void advanceOriginalCarHeadlights(OriginalCarAssemblyInput& in){
    in.headlights.maximumPhase=in.visibility.maximumPopupPhase;
    if(in.visibility.popupMotorEnabled)in.headlights.advance(in.lights);
    else{in.headlights.counter=in.lights?1:0;in.headlights.phase=0;in.headlights.visible=in.lights;in.headlights.fraction=in.lights?1.f:0.f;}
}
std::vector<CarAssemblyCommand> originalCarAssemblyCommands(OriginalCarAssemblyInput& in,const OriginalCarParts& parts){
    advanceOriginalCarHeadlights(in);
    return originalCarAssemblyPose(in,parts);
}
std::vector<CarAssemblyCommand> originalCarAssemblyPose(const OriginalCarAssemblyInput& in,const OriginalCarParts& parts){
    using K=CarAssemblyCommandKind;std::vector<CarAssemblyCommand> out;out.reserve(300);
    const auto word=in.appearance.word,car=in.appearance.car;
    const unsigned front=word&7,bonnet=(word>>3)&7,roof=(word>>7)&7,side=(word>>10)&7,rear=(word>>13)&7,upper=(word>>16)&7,spoiler=(word>>19)&7,wheel=(word>>22)&7;
    if(car>=35||front>5||bonnet>5||roof>5||side>5||rear>5||upper>5||spoiler>5||wheel>6)throw std::out_of_range("Assembly selection outside authored part tables");
    auto emit=[&](K kind,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0){out.push_back({kind,{a,b,c,d}});};
    auto floats=[&](K kind,float a,float b=0,float c=0,float d=0){emit(kind,std::bit_cast<unsigned>(a),std::bit_cast<unsigned>(b),std::bit_cast<unsigned>(c),std::bit_cast<unsigned>(d));};
    auto push=[&](){emit(K::Push);};auto pop=[&](){emit(K::Pop,1);};
    auto translate=[&](float x,float y,float z){floats(K::Translate,x,y,z);};
    auto draw=[&](unsigned slot,bool direct=false,bool identity=false){if(slot>=212)throw std::out_of_range("Assembly semantic slot");emit(direct?K::DrawDirect:K::DrawMaterial,slot,identity?slot:in.visibility.slots[slot]);};
    auto phase=[&](unsigned axis,float angle){float value=angle*65536.0f;value/=360.0f;value+=0.5f;
        if(!std::isfinite(value)||value>=2147483648.0f||value< -2147483648.0f)throw std::out_of_range("Assembly angle conversion");
        emit(axis==0?K::RotateXPhase:axis==1?K::RotateYPhase:K::RotateZPhase,unsigned(std::int32_t(value))&0xffff);};
    auto rotations=[&](const OriginalCarPartTransform& t,float extraY=0.0f){phase(2,t.rotationDegrees[2]);phase(1,t.rotationDegrees[1]+extraY);phase(0,t.rotationDegrees[0]);};
    auto part=[&](unsigned index,bool scale){const auto& t=parts.transforms.at(index);translate(t.translation[0],t.translation[1],t.translation[2]);rotations(t);
        if(scale)floats(K::Scale,t.scale[0],t.scale[1],t.scale[2]);};
    auto spoilerOffset=[&](){
        if((car==30||car==25)&&rear==2)translate(0,0,-0.04f);
        if(car==6&&rear==0){if(spoiler==1)translate(0.0362f,0.0285f,0);if(spoiler==2)translate(0.061f,0.0163f,0);}
    };
    auto effect=[&](bool body){floats(body?K::BodyEffect:K::WheelEffect,body?2.5f:0.5f,in.effectParameters[0],in.effectParameters[1],in.effectParameters[2]);};
    push();emit(K::MainDrawState,31);draw(0);draw(78);draw(17);
    draw(5+front);draw(11+bonnet);draw(20+roof);draw(28+side);draw(34+rear);
    draw(car==33&&(word&0x0e070000)==0x02000000?45:40+upper);
    for(unsigned slot=0;slot<212;++slot)if(in.visibility.slots[slot]==1000)draw(slot,false,true);
    if(in.lights){draw(3);draw(187+front);if(in.braking){draw(4);draw(193+upper);}draw(79);}
    else{draw(1);if(in.braking){draw(2);draw(193+upper);}}
    if(word&0x10000000)draw(26);if(word&0x20000000)draw(27);
    if(in.effects&1)effect(true);pop();push();spoilerOffset();draw(46+spoiler,true);pop();
    if(in.hasParts){
        push();part(front,true);emit(K::NumberPlate);pop();
        push();part(6+rear,true);emit(K::NumberPlate);pop();
        push();part(in.headlights.visible?12:13,true);if(in.headlights.visible)emit(K::RotateXPhase,in.headlights.phase);draw(in.headlights.visible?19:18,true);pop();
        for(unsigned index=0;index<4;++index){const bool left=(index&1)!=0;const auto& t=parts.transforms[index<2?14:15];const float offset=index<2?in.wheelOffsets.front:in.wheelOffsets.rear;
            push();translate(left?-t.translation[0]-offset:t.translation[0]+offset,t.translation[1]+0.02f,t.translation[2]);
            if(index<2)floats(K::RotateYRadians,in.steering);
            rotations(t,left?180.0f:0.0f);translate(0,in.suspension[index],0);floats(K::RotateXRadians,left?-in.spin[index]:in.spin[index]);
            floats(K::WheelBlur,in.wheelBlur[index]);draw(52+wheel,true);if(in.effects&2)effect(false);pop();
        }
        constexpr std::array<unsigned,4> suspensionMap{1,0,3,2};
        for(unsigned index=0;index<4;++index){const auto& t=parts.transforms[16+index];push();translate(t.translation[0],t.translation[1]+0.02f,t.translation[2]);
            if(index<2)floats(K::RotateYRadians,in.steering);rotations(t);translate(0,in.suspension[suspensionMap[index]],0);draw(60+index,true);pop();}
        for(unsigned index=0;index<3;++index){push();part(20+index,false);draw(199+index,true);pop();}
    }
    // Both overlay layers retain source matrix pushes, clipping parameters,
    // semantic selections and their different popup-selection conditions.
    for(unsigned layer=0;layer<2;++layer)if(in.overlayLayers&(1u<<layer)){
        const bool current=layer?in.secondaryUsesCurrent:in.primaryUsesCurrent;
        auto layerMatrix=[&](){emit(current?K::Push:layer?K::LoadSecondary:K::LoadPrimary);};
        auto state=[&](){if(layer)floats(K::LayerParameters,in.secondaryParameters[0],in.secondaryParameters[1]);else floats(K::LayerParameters,0,0);emit(K::LayerDrawState,21);};
        push();part((layer?in.lights:in.headlights.visible)?12:13,true);layerMatrix();state();emit(K::RotateXPhase,in.headlights.phase);draw(layer?155:107);pop();pop();
        layerMatrix();state();draw(layer?140:80);draw(layer?141:81);draw(layer?154:106);
        draw((layer?142:82)+front);draw((layer?148:100)+bonnet);draw((layer?157:109)+roof);
        if(car==25)draw((layer?159:111)+(roof==1?1:0));
        draw((layer?163:115)+side);draw((layer?169:121)+rear);draw((layer?175:127)+upper);
        push();spoilerOffset();draw((layer?181:133)+spoiler,true);pop();pop();
        push();translate(0,0,-0.003f);draw(88+bonnet,true);draw(94+bonnet,true);pop();
    }
    return out;
}
}
