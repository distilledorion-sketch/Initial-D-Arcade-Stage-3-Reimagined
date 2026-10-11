#include "original_water_cup.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4&&(argc!=5||std::string(argv[4])!="--render-all"))throw std::runtime_error("Arguments: Native root, original image, evidence folder [--render-all]");
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
    const auto render=[&](){
        std::vector<unsigned> pixels(320*240,0xff304038);art.paint(pixels,320,240,cup,160,100,180);
        check(std::count_if(pixels.begin(),pixels.end(),[](auto p){return p!=0xff304038;})>1000,"Cup art is empty");
        return pixels;
    };
    // These regions were measured with an independent per-pixel depth render
    // of the unmodified source geometry. Water must animate inside the opening
    // while the opaque front wall and the source square surround stay intact.
    cup.reset();const auto resting=render();
    check(resting[177*320+160]==0xffefefef,"Missing source front-wall surface");
    check(resting[65*320+62]==0xff212121&&resting[28*320+160]==0xff212121,"Missing source cup surround");
    std::array<bool,38*18> movingOpening{};
    for(int water=0;water<60;++water){
        cup.reset();cup.water=water;const auto pixels=render();
        bool stableFront=true;
        for(int y=172;y<182;++y)for(int x=140;x<180;++x)
            stableFront&=pixels[y*320+x]==resting[y*320+x];
        check(stableFront,"Water is drawing over the opaque front wall");
        for(int y=125;y<143;++y)for(int x=140;x<178;++x)
            movingOpening[(y-125)*38+x-140]|=pixels[y*320+x]!=resting[y*320+x];
    }
    check(std::count(movingOpening.begin(),movingOpening.end(),true)>600,"Water is hidden or frozen inside the cup");
    const auto paint=[&](const std::string& name){
        const auto pixels=render();
        std::ofstream image(out/(name+".ppm"),std::ios::binary);image<<"P6\n320 240\n255\n";
        for(auto p:pixels){const char rgb[]{char(p>>16),char(p>>8),char(p)};image.write(rgb,3);}
        check(bool(image),"Write water-cup preview");
    };
    for(unsigned pose=0;pose<3;++pose){
        cup.reset();std::array<unsigned,5> cues{};
        if(pose)cues[pose-1]=std::bit_cast<unsigned>(.06f);
        cup.advance(.1f,cues);cues={};for(unsigned i=0;i<5;++i)cup.advance(.1f,cues);
        paint("water-cup-"+std::to_string(pose));
    }
    // Export the real renderer's full water bank for comparison with an
    // independent source-geometry depth rasterizer. Motion correctness alone
    // cannot detect water drawn over the exterior of the cup.
    if(argc==5){
        for(int water=0;water<60;++water){
            cup.reset();cup.water=water;paint("water-frame-"+std::to_string(water));
        }
        for(int side=0;side<2;++side)for(int frame=0;frame<3;++frame){
            cup.reset();cup.water=side?59:0;
            (side?cup.rightSplash:cup.leftSplash)=frame;
            paint("splash-"+std::to_string(side)+"-"+std::to_string(frame));
        }
    }
    std::cout<<"PASS "<<checks<<" water-cup checks, "<<instructions<<" original instructions; timer, magnitude and division hooks explicit.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
