#include "original_body_contact.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
#include <regex>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("canonical-image data-directory primary-FSCA-header required");RefMemory memory(argv[1]);const std::filesystem::path root(argv[2]);const auto data=OriginalRivalData::load(root/"original_rival");const auto fsca=OriginalFscaTable::load(root/"original_physics/fsca_table.bin");
    std::ifstream primary(argv[3]);if(!primary)throw std::runtime_error("Missing primary FSCA");const std::string text{std::istreambuf_iterator<char>(primary),{}};const std::regex regex("0x([0-9A-Fa-f]{8})");std::vector<std::uint32_t> halfWave;for(std::sregex_iterator i(text.begin(),text.end(),regex),end;i!=end;++i)halfWave.push_back(std::stoul((*i)[1].str(),nullptr,16));if(halfWave.size()!=32768)throw std::runtime_error("Invalid primary FSCA");
    std::mt19937 random(0x157880);std::size_t checks=0,steps=0,cases=0,hits=0,misses=0;const auto equal=[&](unsigned actual,unsigned expected,const std::string& label){++checks;if(actual!=expected)throw std::runtime_error(label+" actual="+hex(actual)+" original="+hex(expected));};
    for(unsigned car=0;car<35;++car)for(unsigned w=0;w<11;++w)equal(data.word(0x0c2716a8+car*44+w*4),memory.read32(0x0c2700f4+car*44+w*4),"body geometry source identity");
    const auto number=[&](){return float(int(random()%4097)-2048)/1024.f;};
    constexpr unsigned shapeBase=0x0c401b04,player=0x0c8ff388,secondary=0x0c8ff430;
    for(unsigned sample=0;sample<1020;++sample){memory.clear();memory.zeroRegion(0x0c8ff000,0x1c0000);memory.zeroRegion(0x0cff0000,0x10000);memory.zeroRegion(0x0ce00000,32*64);
        memory.write32(0x0c98ad0c,0x00200000);memory.write32(0x0c98ad10,0x0ce00000);memory.write32(0x0c98ad14,0x0ce00000);
        OriginalBodyContactState state;OriginalPublishedActors published;
        for(unsigned a=0;a<2;++a)for(unsigned w=0;w<156;++w){state.shapes0C401B04[a].words[w]=random();memory.write32(shapeBase+a*624+w*4,state.shapes0C401B04[a].words[w]);}
        state.count0CA9B360=23;memory.write32(0x0ca9b360,23);for(unsigned i=0;i<32;++i)for(unsigned k=0;k<3;++k){state.intersections0CA9B364[i][k]=number();memory.writeFloat(0x0ca9b364+i*12+k*4,state.intersections0CA9B364[i][k]);}
        for(auto& w:published.player0C8FF388)w=random();for(auto& w:published.secondary0C8FF430)w=random();const unsigned car0=sample%35,car1=(sample*17+5)%35;
        published.player0C8FF388[20]=(published.player0C8FF388[20]&~0x1f3fu)|car0;published.secondary0C8FF430[20]=(published.secondary0C8FF430[20]&~0x1f3fu)|0x100|car1;
        std::array<float,3> origin{number()*1000,number()*10,number()*1000},delta{number()*1.5f,number()*.5f,number()*2.f},angles0{number()*.1f,number()*3.f,number()*.1f},angles1{number()*.1f,number()*3.f,number()*.1f};
        if(sample<420){const auto variant=sample/35;origin={0.f,0.f,0.f};angles0={0.f,0.f,0.f};angles1={0.f,0.f,0.f};delta={0.f,0.f,0.f};
            switch(variant){case 0:delta={100.f,0.f,0.f};break;case 1:delta={-100.f,0.f,0.f};break;case 2:delta={0.f,100.f,0.f};break;case 3:delta={0.f,0.f,100.f};break;case 4:delta={.25f,0.f,0.f};break;case 5:delta={-.25f,0.f,0.f};break;case 6:delta={0.f,0.f,.5f};break;case 7:delta={0.f,.25f,0.f};break;case 8:angles1[1]=1.5707963705062866f;break;case 9:delta[0]=data.scalar(0x0c2716b8+car0*44)+data.scalar(0x0c2716b8+car1*44);break;case 10:delta[2]=data.scalar(0x0c2716c0+car0*44)+data.scalar(0x0c2716c0+car1*44);break;case 11:angles0={-.2f,1.2f,.1f};angles1={.1f,-.6f,.4f};delta={.15f,-.25f,.1f};break;}
        }
        for(unsigned k=0;k<3;++k){published.player0C8FF388[k]=std::bit_cast<unsigned>(origin[k]);published.secondary0C8FF430[k]=std::bit_cast<unsigned>(origin[k]+delta[k]);published.player0C8FF388[6+k]=std::bit_cast<unsigned>(angles0[k]);published.secondary0C8FF430[6+k]=std::bit_cast<unsigned>(angles1[k]);}
        for(unsigned w=0;w<42;++w){memory.write32(player+w*4,published.player0C8FF388[w]);memory.write32(secondary+w*4,published.secondary0C8FF430[w]);}
        RefCpu cpu(memory);cpu.r[15]=0x0cfff000;cpu.pr=0x0f000000;cpu.fscaHalfWave=halfWave;for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(number());const auto beforeMatrix=cpu.xf;
        steps+=cpu.run(0x0c157880,0x0c1578c4,200000);const auto result=produceOriginalBodyContact(published,state,data,fsca);const auto label="case"+std::to_string(sample);
        for(unsigned a=0;a<2;++a)for(unsigned w=0;w<156;++w)equal(state.shapes0C401B04[a].words[w],memory.read32(shapeBase+a*624+w*4),label+" body"+std::to_string(a)+"+"+hex(w*4));
        equal(state.count0CA9B360,memory.read32(0x0ca9b360),label+" count");for(unsigned i=0;i<32;++i)for(unsigned k=0;k<3;++k)equal(std::bit_cast<unsigned>(state.intersections0CA9B364[i][k]),memory.read32(0x0ca9b364+i*12+k*4),label+" intersection"+std::to_string(i)+" axis"+std::to_string(k));
        equal(result.active,memory.read32(shapeBase+40),label+" active");equal(std::bit_cast<unsigned>(result.x),memory.read32(shapeBase+76),label+" responseX");equal(std::bit_cast<unsigned>(result.z),memory.read32(shapeBase+84),label+" responseZ");
        for(unsigned i=0;i<16;++i)equal(cpu.xf[i],beforeMatrix[i],label+" enclosing matrix preserved");for(unsigned w=0;w<42;++w){equal(published.player0C8FF388[w],memory.read32(player+w*4),label+" player unchanged");equal(published.secondary0C8FF430[w],memory.read32(secondary+w*4),label+" rival unchanged");}
        result.active?++hits:++misses;++cases;
    }
    if(!hits||!misses)throw std::runtime_error("Missing collision branch coverage");std::cout<<"PASS original body contact: "<<cases<<" cases,"<<hits<<" contacts,"<<misses<<" separated,"<<checks<<" exact comparisons,"<<steps<<" original instructions,zero hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
