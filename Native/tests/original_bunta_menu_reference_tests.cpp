#include "original_bunta_menu.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <map>

using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
struct Matrix {float x=0,y=0,z=0,scale=1;};
int main(int argc,char** argv)try{
    if(argc<2)throw std::runtime_error("canonical-image [native-project] required");
    RefMemory memory(argv[1]);std::size_t cases=0,checks=0,instructions=0,submissions=0;
    const auto equal=[&](unsigned actual,unsigned expected,const std::string& label){
        ++checks;if(actual!=expected)throw std::runtime_error(label+" expected="+hex(expected)+" actual="+hex(actual));
    };
    constexpr unsigned object=0x0d000000,stack=0x0d010000,stop=0x00ff0000,buffer=0x0d020000;
    for(int level=0;level<=16;++level)for(unsigned frame=0;frame<=40;++frame){
        memory.clear();memory.zeroRegion(object,1024);memory.zeroRegion(stack,65536);
        memory.write32(object+428,buffer);memory.write32(object+436,unsigned(level));memory.write32(object+440,frame);
        RefCpu cpu(memory);cpu.r[4]=object;cpu.r[15]=stack+0xf000;cpu.pr=stop;
        Matrix matrix;std::vector<Matrix> matrices;std::array<unsigned,14> colors;colors.fill(0xffffffffu);
        const auto expected=originalBuntaMenuDraws(level,frame);std::size_t next=0,colorWrites=0;
        // Shared course child, bank lookup and graphics submissions are
        // explicit boundaries. All Bunta arithmetic executes original bytes.
        for(unsigned entry:{0x0c194ae0u,0x0c1baf40u,0x0c1bb6e0u})cpu.callHooks[entry]=[](auto& c){c.r[0]=1;};
        cpu.callHooks[0x0c05a8e0]=[](auto& c){c.r[0]=c.r[5];};
        cpu.callHooks[0x0c1b81c0]=[&](auto& c){
            equal(c.r[4],buffer,"Bunta progress buffer");
            if(c.r[5]<8||c.r[5]>13)throw std::runtime_error("Unexpected source Bunta color index");
            colors.at(c.r[5])=c.r[6];++colorWrites;
        };
        cpu.callHooks[0x0c1b8240]=[&](auto& c){equal(c.r[4],buffer,"Bunta applied color buffer");};
        cpu.callHooks[0x0c1f6610]=[&](auto&){matrices.push_back(matrix);};
        cpu.callHooks[0x0c1f65c0]=[&](auto&){
            if(matrices.empty())throw std::runtime_error("Bunta matrix underflow");matrix=matrices.back();matrices.pop_back();
        };
        cpu.callHooks[0x0c1f6ac0]=[&](auto& c){matrix.x+=matrix.scale*c.getFloat(4);matrix.y+=matrix.scale*c.getFloat(5);matrix.z+=c.getFloat(6);};
        cpu.callHooks[0x0c1f69d0]=[&](auto& c){
            equal(std::bit_cast<unsigned>(c.getFloat(4)),std::bit_cast<unsigned>(c.getFloat(5)),"Uniform Bunta star scale");matrix.scale*=c.getFloat(4);
        };
        cpu.callHooks[0x0c1d7120]=[&](auto& c){
            const auto draw=next++;
            if(level==16&&draw==16){
                equal(c.r[4],0x0004001b,"Completion sentinel extra source star");
                equal(std::bit_cast<unsigned>(matrix.x),memory.read32(0x0c2a339c),"Source extra star reads following ASCII X");
                equal(std::bit_cast<unsigned>(matrix.y),memory.read32(0x0c2a33a0),"Source extra star reads following ASCII Y");
                if(matrix.x<1.e10f)throw std::runtime_error("Completion sentinel star unexpectedly visible");
                ++submissions;return;
            }
            const OriginalChoiceDraw backdrop{0x00010000u|expected.backdropChunk,0,0,.009999999776482582f,1};
            if(draw>expected.stars.size())throw std::runtime_error("Extra original Bunta draw");
            const auto& e=draw?expected.stars[draw-1]:backdrop;
            const std::string context="level="+std::to_string(level)+" frame="+std::to_string(frame)+" draw="+std::to_string(draw);
            equal(e.selector,c.r[4],context+" selector");
            equal(std::bit_cast<unsigned>(e.x),std::bit_cast<unsigned>(matrix.x),context+" X");
            equal(std::bit_cast<unsigned>(e.y),std::bit_cast<unsigned>(matrix.y),context+" Y");
            equal(std::bit_cast<unsigned>(e.z),std::bit_cast<unsigned>(matrix.z),context+" Z");
            equal(std::bit_cast<unsigned>(e.scale),std::bit_cast<unsigned>(matrix.scale),context+" scale");++submissions;
        };
        instructions+=cpu.run(0x0c19a980,stop,20000);
        equal(unsigned(next),unsigned(expected.stars.size()+1+(level==16&&frame>=30)),"All visible Bunta draws submitted; completion sentinel offscreen star bounded");
        equal(unsigned(colorWrites),6,"Six source gradient writes");
        for(unsigned i=8;i<14;++i)equal(expected.progressColors[(i-8)/2],colors[i],"Exact Bunta gradient color");
        equal(memory.read32(object+440),frame+1,"Original Bunta draw frame increment");
        equal(unsigned(matrices.size()),0,"Balanced Bunta matrices");++cases;
    }
    for(unsigned oldCourse=0;oldCourse<9;++oldCourse)for(unsigned selected=0;selected<9;++selected){
        memory.clear();memory.zeroRegion(object,1024);memory.zeroRegion(stack,65536);
        memory.write32(object+424,oldCourse);memory.write32(object+440,31);
        RefCpu cpu(memory);cpu.r[4]=object;cpu.r[5]=selected;cpu.r[15]=stack+0xf000;cpu.pr=stop;
        instructions+=cpu.run(0x0c19a940,stop,100);
        equal(memory.read32(object+424),selected,"Source selected course");
        equal(memory.read32(object+440),oldCourse==selected?31u:0u,"Source resets stars only on course change");
        cpu.r[4]=object;cpu.r[5]=12;instructions+=cpu.run(0x0c19a960,stop,100);
        equal(memory.read32(object+436),12,"Source displays stored cleared count without adding one");
    }
    equal(unsigned(originalBuntaMenuDraws(-1,34).stars.size()),0,"Corrupt negative progress bounded");
    equal(unsigned(originalBuntaMenuDraws(1000000,34).stars.size()),15,"Corrupt progress cannot overrun source table");
    if(argc>2){
        const auto model=NativeModel::load(std::filesystem::path(argv[2])/"data/original_assets/menus/v3/v3sB01course/v3sB01course.idasmesh");
        for(int level=0;level<=15;++level){
            const auto draws=originalBuntaMenuDraws(level,34);const auto& source=model.chunks.at(draws.backdropChunk);
            const auto recolored=materializeOriginalBuntaColors(source,draws.progressColors);unsigned index=0;
            for(unsigned batchIndex=0;batchIndex<recolored.batches.size();++batchIndex){
                const auto& batch=recolored.batches[batchIndex];equal(batch.material[2],0x600,"Source UI color-buffer material");
                for(unsigned vertexIndex=0;vertexIndex<batch.vertices.size();++vertexIndex){
                    const auto& vertex=batch.vertices[vertexIndex];const auto& original=source.batches[batchIndex].vertices[vertexIndex];
                    equal(vertex.color0,index>=8?draws.progressColors[(index-8)/2]:0xffffffffu,"Materialized original gradient");
                    equal(vertex.color1,0,"Source secondary color");
                    equal(std::bit_cast<unsigned>(vertex.position.x),std::bit_cast<unsigned>(original.position.x),"Original X unchanged");
                    equal(std::bit_cast<unsigned>(vertex.position.y),std::bit_cast<unsigned>(original.position.y),"Original Y unchanged");++index;
                }
            }
            equal(index,14,"Complete original Bunta backdrop vertices");
        }
    }
    std::cout<<"PASS Bunta course presentation: "<<cases<<" source-frame cases, "<<checks<<" comparisons, "<<instructions
        <<" original instructions, "<<submissions<<" captured submissions. Exact 0..15 visible stars plus completed16 sentinel, tier selectors/colors, all pop frames, and selection reset; graphics/bank/shared-course calls explicitly hooked.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
