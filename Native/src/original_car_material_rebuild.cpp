#include "original_car_material_rebuild.h"
#include "original_car_color_catalog.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
template<class T>T read(std::istream& in){T out{};if(!in.read(reinterpret_cast<char*>(&out),sizeof out))throw std::runtime_error("Truncated original material layout");return out;}
bool paintable(const std::array<std::uint32_t,16>& m){return (m[3]&0xffffff)==0xffffff&&m[9]==0xffffffff;}
}
OriginalCarMaterialRebuild OriginalCarMaterialRebuild::load(const std::filesystem::path& path,unsigned car){
    std::ifstream file(path,std::ios::binary);const auto magic=read<std::array<char,8>>(file);
    if(std::memcmp(magic.data(),"ID3CML1\0",8)||read<unsigned>(file)!=1||car>=35||read<unsigned>(file)!=car)throw std::runtime_error("Original material layout identity");
    OriginalCarMaterialRebuild out;out.car_=car;out.authoredCount_=read<unsigned>(file);
    if(!out.authoredCount_||out.authoredCount_>1000)throw std::runtime_error("Original material chunk bound");
    out.slots_=read<std::array<int,212>>(file);
    for(auto slot:out.slots_)if(slot< -1||slot>=int(out.authoredCount_))throw std::runtime_error("Original material semantic bound");
    for(unsigned i=0;i<out.authoredCount_;++i){Chunk c;c.sourceOffset=read<unsigned>(file);c.sourceSize=read<unsigned>(file);
        const unsigned materials=read<unsigned>(file),batches=read<unsigned>(file);
        if(materials>10000||batches>20000||c.sourceSize>0x400000)throw std::runtime_error("Original material record count bound");
        for(unsigned j=0;j<materials;++j){Material m;m.sourceOffset=read<unsigned>(file);m.words=read<std::array<unsigned,16>>(file);
            if(m.sourceOffset<c.sourceOffset||std::uint64_t(m.sourceOffset)+64>std::uint64_t(c.sourceOffset)+c.sourceSize||(j&&m.sourceOffset<=c.materials.back().sourceOffset))throw std::runtime_error("Original material offset bound/order");
            c.materials.push_back(m);}
        for(unsigned j=0;j<batches;++j){Batch b;b.sourceOffset=read<unsigned>(file);b.material=read<unsigned>(file);b.words=read<std::array<unsigned,8>>(file);
            if(b.material>=materials||b.sourceOffset<c.sourceOffset||std::uint64_t(b.sourceOffset)+32>std::uint64_t(c.sourceOffset)+c.sourceSize||(j&&b.sourceOffset<=c.batches.back().sourceOffset))throw std::runtime_error("Original material batch bounds");
            c.batches.push_back(b);}
        out.authored_.push_back(std::move(c));
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing original material layout");
    out.initialize();return out;
}
void OriginalCarMaterialRebuild::initialize(){
    chunks_=authored_;copySources_.fill(-1);
    // Original materialization publishes null for the authored eight-byte
    // nongeometry marker (NB8C chunk53), without renumbering the bank.
    for(auto& slot:slots_)if(slot>=0&&authored_[slot].sourceSize==8)slot=-1;
    //026890 copies slots80..87 and100..138 before any material changes.
    for(unsigned slot=140;slot<=186;++slot){const unsigned source=slot-(slot>147?48:60);const int chunk=slots_[source];
        copySources_[slot-140]=chunk;
        if(chunk<0||chunks_[chunk].materials.empty()){slots_[slot]=-1;continue;}
        const auto copy=chunks_[chunk];slots_[slot]=int(chunks_.size());chunks_.push_back(copy);
    }
    //0268F2..02699E configures translucent draw descriptors.
    for(unsigned slot=80;slot<=138;++slot)if(slots_[slot]>=0)for(auto& b:chunks_[slots_[slot]].batches){
        b.words[2]=(b.words[2]&0xffffffc0u)|26;
        b.words[3]=(b.words[3]&0xc7ffffffu)|0x0c000000u;
    }
    //0269B4: only the first GMP in each80..186 chunk is the alpha handle.
    for(unsigned slot=80;slot<=186;++slot)if(slots_[slot]>=0){auto& c=chunks_[slots_[slot]];
        if(!c.materials.empty())c.materials[0].words[3]=(c.materials[0].words[3]&0xffffff)|0x3f000000;
        if(slot<88||slot>99)for(auto& b:c.batches)b.words[2]=(b.words[2]&0xe3f7ffffu)|0x04000000u;
    }
    //026A60 enumerates GMPs in slots0..79, excluding52..58, for its256-bit
    // paint mask. Repeated semantic references consume separate mask bits.
    unsigned ordinal=0;
    for(unsigned slot=0;slot<80&&ordinal<256;++slot)if((slot<52||slot>58)&&slots_[slot]>=0){const unsigned chunk=unsigned(slots_[slot]);
        for(unsigned n=0;n<chunks_[chunk].materials.size()&&ordinal<256;++n,++ordinal)if(paintable(chunks_[chunk].materials[n].words)){
            state_.paintMask[ordinal/32]|=1u<<(ordinal%32);paint_.push_back({chunk,n});}
    }
    //026B7A popup light model's material/vertex-color lighting flags.
    if(slots_[18]>=0){auto& c=chunks_[slots_[18]];
        for(auto& m:c.materials)m.words[2]&=0xfffff9ffu;
        for(auto& b:c.batches)b.words[2]&=0xff3fffffu;
    }
    //026C18 builds up to100 specular-exponent GMP references, including
    // copied models and tail slots187..211. Paintability uses constructor RGB.
    for(unsigned slot=0;slot<212;++slot)if(slots_[slot]>=0){const unsigned chunk=unsigned(slots_[slot]);
        for(unsigned n=0;n<chunks_[chunk].materials.size();++n)if(gloss_.size()<100&&paintable(chunks_[chunk].materials[n].words)){
            const auto& m=chunks_[chunk].materials[n].words;gloss_.push_back({chunk,n});
            if(!state_.gloss){state_.gloss=m[1];state_.specular=m[4]&255;}
        }
    }
}
void OriginalCarMaterialRebuild::alpha(unsigned slot,float value){
    const int chunk=slots_.at(slot);if(chunk<0)return;
    auto& c=chunks_[chunk];if(c.materials.empty())throw std::runtime_error("Original alpha handle missing");
    auto& color=c.materials[0].words[3];color=(color&0xffffff)|(unsigned(255.0f*value)<<24);
}
void OriginalCarMaterialRebuild::rebuild(OriginalCarAppearanceConfig& config,std::uint32_t condition){
    if(config.car!=car_)throw std::invalid_argument("Original material car/config mismatch");
    if(config.paintDirty){const unsigned color=(config.word>>25)&7;
        if(color>=originalCarColorCounts[car_])throw std::out_of_range("Original material paint outside palette");
        const unsigned rgb=originalCarPaintRgb[car_][color];state_.rgb={rgb>>16,(rgb>>8)&255,rgb&255};
        for(const auto& p:paint_){auto& m=chunks_[p.chunk].materials[p.material].words;m[3]=(m[3]&0xff000000)|rgb;}
        config.paintDirty=false;
    }
    // The source uses signed remainder, including negative scene variants.
    const bool alternate=std::bit_cast<std::int32_t>(config.materialVariant)%8>3;
    for(unsigned slot=80;slot<=139;++slot)alpha(slot,alternate?0.28f:0.35f);
    const float glass=alternate?0.4f:0.35f;alpha(81,glass);
    if(car_==25){alpha(111,glass);alpha(112,glass);}
    for(unsigned slot=88;slot<=99;++slot)alpha(slot,0.7f);
    state_.bodyShadowAlpha=0.16f;state_.glassShadowAlpha=0.32f;
    for(unsigned slot=140;slot<=186;++slot)alpha(slot,0.65f);
    state_.bodyAlpha=state_.glassAlpha=0.35f;
    state_.specular=unsigned(0.2f*255.0f);
    //029A60 encodes the original5-bit mantissa /3-bit exponent. Integer
    // powers of two have exact F32 results; no host libm approximation.
    const float target=(condition==1?230.0f:250.0f)/10.0f;
    unsigned exponent=0;while(exponent<=7&&target>float(1u<<exponent))++exponent;
    const float denominator=exponent?float(1u<<(exponent-1)):0.5f;
    state_.gloss=(exponent<<5)|unsigned((target/denominator-1.0f)*32.0f);
    for(const auto& p:paint_){auto& m=chunks_[p.chunk].materials[p.material].words;m[4]=(m[4]&0xff000000)|(state_.specular*0x010101);}
    for(const auto& p:gloss_)chunks_[p.chunk].materials[p.material].words[1]=state_.gloss|(state_.gloss<<8);
}
NativeModel OriginalCarMaterialRebuild::apply(const NativeModel& model)const{
    if(model.chunks.size()!=authoredCount_)throw std::runtime_error("Original material authored model shape");
    for(unsigned n=0;n<authoredCount_;++n){const auto& c=model.chunks[n];const auto& a=authored_[n];
        if(c.sourceOffset!=a.sourceOffset||c.sourceSize!=a.sourceSize||c.batches.size()!=a.batches.size())throw std::runtime_error("Original material authored chunk shape");
        for(unsigned b=0;b<a.batches.size();++b){const auto& original=a.batches[b];const auto& current=c.batches[b];
            if(current.sourceOffset!=original.sourceOffset||current.ich!=original.words||current.material!=a.materials[original.material].words)throw std::runtime_error("Original material authored batch mismatch");}
    }
    NativeModel out=model;
    for(unsigned s=140;s<=186;++s)if(slots_[s]>=int(authoredCount_)){auto copy=model.chunks[copySources_[s-140]];copy.index=unsigned(out.chunks.size());out.chunks.push_back(std::move(copy));}
    if(out.chunks.size()!=chunks_.size())throw std::runtime_error("Original material clone order");
    for(unsigned c=0;c<chunks_.size();++c)for(unsigned b=0;b<chunks_[c].batches.size();++b){
        const auto& selected=chunks_[c].batches[b];out.chunks[c].batches[b].ich=selected.words;
        out.chunks[c].batches[b].material=chunks_[c].materials[selected.material].words;
    }
    return out;
}
}
