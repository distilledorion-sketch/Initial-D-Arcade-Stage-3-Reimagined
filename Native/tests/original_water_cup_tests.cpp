#include "original_water_cup.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("Arguments: Native root, original image, evidence folder");
    const std::filesystem::path root=argv[1],out=argv[3];std::filesystem::create_directories(out);
    RefMemory memory(argv[2]);constexpr unsigned object=0xd000000,stack=0xd010000,stop=0xd020000;
    memory.zeroRegion(object,128);memory.zeroRegion(stack,4096);memory.zeroRegion(0xc900e5c,20);
    OriginalWaterCup cup;unsigned checks=0,instructions=0;
    auto check=[&](bool c,const char* why){++checks;if(!c)throw std::runtime_error(why);};
    // Cover stopped, normal motion, ditch oscillation, both splash sides and
    // all three impact strengths against original instructions, frame by frame.
    for(unsigned scenario=0;scenario<10;++scenario){
        cup.reset();memory.write32(object+4,0);memory.write32(object+8,29);
        memory.write32(object+12,-1);memory.write32(object+16,-1);
        memory.write32(object+20,0);memory.write32(object+24,0);memory.write32(object+28,0);
        for(unsigned tick=0;tick<130;++tick){
            std::array<unsigned,5> cues{};const float motion=scenario==0?0.f:.1f;
            if(scenario==2)cues[4]=1;
            if(tick==3||tick==60){
                if(scenario==3)cues[2]=1;
                if(scenario==4)cues[3]=1;
                if(scenario>=5&&scenario<=8)cues[(scenario-5)%2]=std::bit_cast<unsigned>(scenario<7?.04f:.06f);
                if(scenario==9){cues[0]=std::bit_cast<unsigned>(.06f);cues[1]=std::bit_cast<unsigned>(.06f);}
            }
            for(unsigned i=0;i<5;++i)memory.write32(0xc900e5c+i*4,cues[i]);
            RefCpu cpu(memory);cpu.r[4]=object;cpu.r[5]=0;cpu.r[15]=stack+4000;cpu.pr=stop;
            cpu.callHooks[0xc1fa9e0]=[](RefCpu&){}; // source diagnostic timer
            cpu.callHooks[0xc1f6c90]=[&](RefCpu& c){c.setFloat(0,motion);}; // input-vector magnitude
            cpu.callHooks[0xc2223b8]=[](RefCpu& c){c.fpul=c.r[4]/c.r[5];};
            instructions+=unsigned(cpu.run(0xc17d2a0,stop,2000));cup.advance(motion,cues);
            const std::array<unsigned,7> expected{cup.frame,unsigned(cup.water),unsigned(cup.rightSplash),unsigned(cup.leftSplash),unsigned(cup.mode),unsigned(cup.unused),unsigned(cup.severity)};
            for(unsigned i=0;i<7;++i)check(memory.read32(object+4+i*4)==expected[i],"Water-cup state differs from original motion owner");
        }
    }
    OriginalWaterCupArtwork art;art.load(root);
    for(unsigned pose=0;pose<3;++pose){
        cup.reset();std::array<unsigned,5> cues{};
        if(pose)cues[pose-1]=std::bit_cast<unsigned>(.06f);
        cup.advance(.1f,cues);cues={};for(unsigned i=0;i<5;++i)cup.advance(.1f,cues);
        std::vector<unsigned> pixels(320*240,0xff304038);art.paint(pixels,320,240,cup,160,100,180);
        check(std::count_if(pixels.begin(),pixels.end(),[](auto p){return p!=0xff304038;})>1000,"Cup art is empty");
        std::ofstream image(out/("water-cup-"+std::to_string(pose)+".ppm"),std::ios::binary);image<<"P6\n320 240\n255\n";
        for(auto p:pixels){const char rgb[]{char(p>>16),char(p>>8),char(p)};image.write(rgb,3);}
        check(bool(image),"Write water-cup preview");
    }
    std::cout<<"PASS "<<checks<<" water-cup checks, "<<instructions<<" original instructions; timer, magnitude and division hooks explicit.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
