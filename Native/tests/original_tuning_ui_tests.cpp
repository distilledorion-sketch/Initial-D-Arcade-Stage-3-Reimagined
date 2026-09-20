#include "original_tuning_ui.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <fstream>
using namespace idas3;using namespace idas3::original;using namespace idas3::reference;
namespace {constexpr unsigned child=0x0d000000,stack=0x0d100000,vtable=0x0d010000,stop=0x00ff0000,drawDirect=stop+4,drawPosition=stop+8,textDraw=stop+12;}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image game-root required");RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);const auto ui=OriginalTuningUi::load(argv[2]);
    std::size_t checks=0,cases=0,instructions=0;unsigned car=0,step=0,flags=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" car "+std::to_string(car)+" step "+std::to_string(step)+" flags "+std::to_string(flags)+": "+hex(a)+" vs "+hex(b));};
    for(car=0;car<35;++car)for(bool basic:{true,false}){
        const auto& d=data.car(car);const unsigned packages=basic?unsigned(d.packages.size()):1;
        for(unsigned package=0;package<packages;++package){const auto count=basic?unsigned(d.packages[package].steps.size()):unsigned(d.performance.size());
            for(step=0;step<count;++step)for(unsigned drawFlags:{1u,5u,31u,63u,65u}){
                flags=drawFlags;if(step+1==count)flags&=~32u;
                if(!basic)flags&=~(2u|8u|64u);
                m.clear();m.zeroRegion(child,0x20000);m.zeroRegion(stack,0x10000);
                OriginalTuningChild s;s.kind=basic?OriginalTuningChildKind::basic:OriginalTuningChildKind::performance;s.car=car;s.package=package;s.selected=step;s.current=int(step);s.next=step+1<count?int(step+1):-1;s.flags=flags;
                if(basic){s.extraIndex=d.packages[package].steps[step].words[4];if(std::int32_t(s.extraIndex)<0)s.flags&=~2u;}s.nextThreshold=s.next<0?0xffffffff:basic?d.packages[package].steps[s.next].words[2]:d.performance[s.next].words[1];s.completionX=-2.4f;
                for(const auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{20,car},{24,unsigned(d.packages.size())},{28,package},{32,step},{64,s.extraIndex},{68,s.nextThreshold},{88,s.flags}})m.write32(child+offset,value);
                m.writeFloat(child+92,s.completionX);const unsigned source=basic?d.packages[package].sourceAddress:d.sourceRow[6],stride=basic?20:12;
                m.write32(child+(basic?76:104),source+step*stride);m.write32(child+(basic?80:108),s.next<0?0:source+unsigned(s.next)*stride);
                for(unsigned bank=0;bank<4;++bank){m.write32(child+0x200+bank*128,vtable);}
                m.write32(child+4,child+0x200);m.write32(child+8,child+0x280);m.write32(child+84,child+0x600);m.write32(child+0x600,vtable);
                m.write32(vtable+44,drawDirect);m.write32(vtable+60,drawPosition);m.write32(vtable+20,textDraw);
                RefCpu c(m);c.r[4]=child;c.r[5]=0xffffffff;c.r[15]=stack+0xf000;c.pr=stop;
                std::vector<OriginalTuningUiDraw> actual;unsigned textCalls=0;
                const auto bankOf=[&](unsigned identity){return OriginalTuningUiDraw::Bank((identity-(child+0x200))/128);};
                c.callHooks[drawDirect]=[&](auto& cpu){actual.push_back({bankOf(cpu.r[4]),cpu.r[5],0,0,0,1,1});};
                c.callHooks[drawPosition]=[&](auto& cpu){actual.push_back({bankOf(cpu.r[4]),cpu.r[5],m.readFloat(cpu.r[6]),m.readFloat(cpu.r[6]+4),m.readFloat(cpu.r[6]+8),1,1});};
                c.callHooks[textDraw]=[&](auto&){++textCalls;};
                for(unsigned address:{0x0c1458c0u,0x0c055d60u})c.callHooks[address]=[](auto&){};
                c.callHooks[0x0c2223b8]=[](auto& cpu){cpu.fpul=unsigned(std::int32_t(cpu.r[4])/std::int32_t(cpu.r[5]));};
                instructions+=c.run(basic?0x0c116360:0x0c1170c0,stop,20000);const auto expected=ui.drawList(s,data);
                equal(unsigned(actual.size()),unsigned(expected.size()),"Draw count");equal(textCalls,(s.flags&16)?1:0,"Original description draw");
                for(unsigned i=0;i<actual.size();++i){const auto&a=actual[i];const auto&b=expected[i];try{equal(unsigned(a.bank),unsigned(b.bank),"Bank");equal(a.index,b.index,"Chunk");equal(std::bit_cast<unsigned>(a.x),std::bit_cast<unsigned>(b.x),"X");equal(std::bit_cast<unsigned>(a.y),std::bit_cast<unsigned>(b.y),"Y");equal(std::bit_cast<unsigned>(a.z),std::bit_cast<unsigned>(b.z),"Z");}catch(...){std::cerr<<"draw "<<i<<" native "<<b.index<<" source "<<a.index<<'\n';throw;}}
                ++cases;
            }
        }
    }
    //118100 optional-parts drawing. Only model submission, cloning, text
    // submission/last-position query and PR1 division cross hook boundaries.
    for(car=0;car<35;++car)for(step=0;step<data.car(car).optional.size();++step)
    for(unsigned choice:{0u,1u,0xffffffffu})for(unsigned scenario=0;scenario<12;++scenario){
        constexpr unsigned balances[]{0,9,10,99,100,999,1000,9999,10000,999999,123456789,999999999};
        constexpr unsigned timers[]{0,1,59,60,119,539,599,600,659,839,840,879};
        const auto& d=data.car(car);flags=choice;
        OriginalTuningChild s;s.kind=OriginalTuningChildKind::optionalPart;s.car=car;s.optionalIndex=step;s.choice=choice;s.balance=balances[scenario];
        m.clear();m.zeroRegion(child,0x20000);m.zeroRegion(stack,0x10000);
        m.write32(child+20,car);m.write32(child+48,s.balance);m.write32(child+72,d.sourceRow[2]+step*24);m.write32(child+80,choice);
        m.write32(0x0c31c99c+1176,timers[scenario]);
        for(unsigned bank=0;bank<4;++bank)m.write32(child+0x200+bank*128,vtable);
        m.write32(child+4,child+0x200);m.write32(child+8,child+0x280);m.write32(child+56,child+0x300);m.write32(child+60,child+0x380);
        m.write32(child+76,child+0x600);m.write32(child+0x600,vtable);m.write32(child+0x380+16,child+0x800);
        m.write32(child+0x800,child+0x900);m.write32(child+0x804,child+0xa00);
        m.write32(vtable+60,drawPosition);m.write32(vtable+20,textDraw);
        RefCpu c(m);c.r[4]=child;c.r[5]=0xffffffff;c.r[15]=stack+0xf000;c.pr=stop;
        std::vector<OriginalTuningUiDraw> actual;unsigned textCalls=0;
        std::array<std::pair<float,float>,4> scales;scales.fill({1.f,1.f});std::unordered_map<unsigned,int> colors;
        const auto bankOf=[&](unsigned identity){return unsigned((identity-(child+0x200))/128);};
        c.callHooks[drawPosition]=[&](auto& cpu){const auto bank=bankOf(cpu.r[4]);actual.push_back({OriginalTuningUiDraw::Bank(bank),cpu.r[5],m.readFloat(cpu.r[6]),m.readFloat(cpu.r[6]+4),m.readFloat(cpu.r[6]+8),scales.at(bank).first,scales.at(bank).second,cpu.r[7]?colors.at(cpu.r[7]):-1});};
        c.callHooks[textDraw]=[&](auto&){++textCalls;};
        c.callHooks[0x0c1458c0]=[](auto&){};
        c.callHooks[0x0c145900]=[&](auto& cpu){scales.at(bankOf(cpu.r[4]))={cpu.getFloat(4),cpu.getFloat(5)};};
        c.callHooks[0x0c1d5400]=[](auto& cpu){cpu.r[0]=cpu.r[4]+0x1000;};
        c.callHooks[0x0c1153c0]=[&](auto& cpu){colors[cpu.r[4]]=int(cpu.r[5]);};
        c.callHooks[0x0c0c6c60]=[&](auto& cpu){for(unsigned i=0;i<3;++i)m.writeFloat(cpu.r[2]+i*4,0);};
        c.callHooks[0x0c2223b8]=[](auto& cpu){cpu.fpul=unsigned(std::int32_t(cpu.r[4])/std::int32_t(cpu.r[5]));};
        instructions+=c.run(0x0c118100,stop,20000);const auto expected=ui.drawList(s,data,timers[scenario]);
        equal(unsigned(actual.size()),unsigned(expected.size()),"Optional draw count");equal(textCalls,1,"Optional description draw");
        for(unsigned i=0;i<actual.size();++i){const auto&a=actual[i];const auto&b=expected[i];try{
            equal(unsigned(a.bank),unsigned(b.bank),"Optional bank");equal(a.index,b.index,"Optional chunk");
            equal(std::bit_cast<unsigned>(a.x),std::bit_cast<unsigned>(b.x),"Optional X");equal(std::bit_cast<unsigned>(a.y),std::bit_cast<unsigned>(b.y),"Optional Y");equal(std::bit_cast<unsigned>(a.z),std::bit_cast<unsigned>(b.z),"Optional Z");
            equal(std::bit_cast<unsigned>(a.scaleX),std::bit_cast<unsigned>(b.scaleX),"Optional X scale");equal(std::bit_cast<unsigned>(a.scaleY),std::bit_cast<unsigned>(b.scaleY),"Optional Y scale");equal(unsigned(a.choiceColors),unsigned(b.choiceColors),"Optional selection colors");
        }catch(...){std::cerr<<"scenario "<<scenario<<" draw "<<i<<" native "<<b.index<<" source "<<a.index<<'\n';throw;}}++cases;
    }
    // Optional constructor text setup: original font sizes, position and row
    // description pointer. Allocation/text methods are captured, not emulated.
    for(car=0;car<35;++car)for(step=0;step<data.car(car).optional.size();++step){
        m.clear();m.zeroRegion(child,0x20000);m.zeroRegion(stack,0x10000);
        RefCpu c(m);c.r[14]=stack+0xc000;c.r[15]=c.r[14];c.pr=stop;
        m.write32(c.r[14]+4148,child+0x600);m.write32(c.r[14]+4156,child);
        m.write32(child+8,data.car(car).sourceRow[2]+step*24);m.write32(child+12,child+0x600);m.write32(child+0x600,vtable);m.write32(vtable+44,textDraw);
        float size=0,x=0,y=0;unsigned address=0;
        c.callHooks[0x0c0c5420]=[&](auto& cpu){size=cpu.getFloat(4);equal(cpu.fr[4],cpu.fr[5],"Optional font X/Y size");equal(cpu.fr[4],cpu.fr[6],"Optional font advance size");cpu.r[0]=child+0x600;};
        instructions+=c.run(0x0c117c12,0x0c117c26,1000);
        c.callHooks[0x0c0c6c20]=[&](auto& cpu){x=cpu.getFloat(4);y=cpu.getFloat(5);};
        c.callHooks[textDraw]=[](auto&){};c.callHooks[0x0c0c6ca0]=[](auto&){};
        c.callHooks[0x0c0c6a00]=[&](auto& cpu){address=cpu.r[5];};
        instructions+=c.run(0x0c117c88,0x0c117cc4,1000);
        OriginalTuningChild s;s.kind=OriginalTuningChildKind::optionalPart;s.car=car;s.optionalIndex=step;
        const auto native=OriginalTuningUi::optionalDescription(s,data);
        equal(address,native.address,"Optional description address");equal(std::bit_cast<unsigned>(size),std::bit_cast<unsigned>(native.size),"Optional description size");
        equal(std::bit_cast<unsigned>(x),std::bit_cast<unsigned>(native.x),"Optional description X");equal(std::bit_cast<unsigned>(y),std::bit_cast<unsigned>(native.y),"Optional description Y");++cases;
    }
    //1153C0 runs on every original YES/NO vertex. Parser events alone are
    // supplied from the imported chunks; all color/material instructions run.
    const auto choices=NativeModel::load(std::filesystem::path(argv[2])/"data/original_assets/tuning/v3sK17continue/v3sK17continue.idasmesh");
    for(unsigned index:{0u,1u})for(bool selected:{false,true})for(bool extraStrip:{false,true}){
        auto native=choices.chunks.at(index);m.clear();m.zeroRegion(child,0x20000);m.zeroRegion(stack,0x10000);
        if(extraStrip){const auto batches=native.batches;native.batches.insert(native.batches.end(),batches.begin(),batches.end());}
        struct Event{unsigned kind,address,layout;};std::vector<Event> events;
        unsigned cursor=child;std::vector<unsigned> materials,headers,vertices;
        for(const auto& batch:native.batches){
            materials.push_back(cursor);events.push_back({1,cursor,0});for(unsigned word:batch.material){m.write32(cursor,word);cursor+=4;}
            headers.push_back(cursor);events.push_back({2,cursor,batch.ich[6]});for(unsigned word:batch.ich){m.write32(cursor,word);cursor+=4;}
            for(const auto& v:batch.vertices){vertices.push_back(cursor);events.push_back({3,cursor,batch.ich[6]});m.write32(cursor+24,v.color0);m.write32(cursor+28,v.color1);cursor+=32;}
        }
        events.push_back({0x80000000,0,0});unsigned event=0;
        RefCpu c(m);c.r[4]=child;c.r[5]=selected?1:0;c.r[15]=stack+0xf000;c.pr=stop;
        c.callHooks[0x0c1d4be0]=[](auto&){};
        c.callHooks[0x0c1d4c40]=[&](auto& cpu){const auto& e=events.at(event++);m.write32(cpu.r[5],e.kind);m.write32(cpu.r[6],e.layout);cpu.r[0]=e.address;};
        instructions+=c.run(0x0c1153c0,stop,200000);OriginalTuningUi::applyChoiceColors(native,selected);
        unsigned v=0;for(unsigned b=0;b<native.batches.size();++b){const auto& batch=native.batches[b];
            for(unsigned w=0;w<batch.material.size();++w)equal(m.read32(materials[b]+w*4),batch.material[w],"Choice GMP");
            for(unsigned w=0;w<batch.ich.size();++w)equal(m.read32(headers[b]+w*4),batch.ich[w],"Choice ICH");
            for(const auto& vertex:batch.vertices){equal(m.read32(vertices[v]+24),vertex.color0,"Choice color 0");equal(m.read32(vertices[v]+28),vertex.color1,"Choice color 1");++v;}
        }++cases;
    }
    // Full original0C6A00/191060 text layout, no font/spacing/math hooks.
    std::ifstream fontMap(std::filesystem::path(argv[2])/"data/original_assets/tuning/option/font.bin",std::ios::binary);std::vector<unsigned char> fontBytes((std::istreambuf_iterator<char>(fontMap)),{});
    for(const auto& [address,text]:data.descriptions)for(float size:{24.f,26.f}){
        m.clear();m.zeroRegion(child,0x20000);m.zeroRegion(stack,0x10000);
        for(unsigned i=0;i<fontBytes.size();++i)m.write8(child+0x10000+i,fontBytes[i]);
        m.writeFloat(child+16,size);m.writeFloat(child+20,size);m.writeFloat(child+24,size);m.writeFloat(child+32,100);m.writeFloat(child+36,388);m.write32(child+56,child+0x10000);m.write32(child+68,child+0x1000);
        RefCpu c(m);c.r[4]=child;c.r[5]=address;c.r[6]=0;c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0x0c0c6a00,stop,200000);
        auto glyphs=ui.descriptionGlyphs(data,{address,100,388,size});equal(m.read32(child+52),unsigned(glyphs.size()),"Source JIS glyph count");
        for(unsigned i=0;i<glyphs.size();++i){equal(m.read32(child+0x1000+i*16),glyphs[i].index,"JIS glyph index");equal(m.read32(child+0x1004+i*16),std::bit_cast<unsigned>(glyphs[i].x),"JIS glyph X");equal(m.read32(child+0x1008+i*16),std::bit_cast<unsigned>(glyphs[i].y),"JIS glyph Y");equal(m.read32(child+0x100c+i*16),std::bit_cast<unsigned>(glyphs[i].z),"JIS glyph Z");}++cases;
    }
    const auto previewDir=std::filesystem::path(argv[2])/"verification/original-tuning";std::filesystem::create_directories(previewDir);
    for(unsigned preview=0;preview<4;++preview){
        const bool basic=preview==0,optional=preview>=2;
        OriginalTuningChild s;s.kind=optional?OriginalTuningChildKind::optionalPart:basic?OriginalTuningChildKind::basic:OriginalTuningChildKind::performance;s.current=0;s.next=1;s.flags=basic?63:53;s.extraIndex=data.car(0).packages[0].steps[0].words[4];s.nextThreshold=basic?10000:150000;s.balance=125000;s.choice=preview-2;
        if(std::int32_t(s.extraIndex)<0)s.flags&=~2u;
        OriginalTuningUiDescription desc;desc.address=basic?data.car(0).packages[0].steps[0].words[3]:data.car(0).performance[0].words[2];desc.x=basic?250.f:90.f;
        if(optional)desc=OriginalTuningUi::optionalDescription(s,data);
        std::vector<unsigned> pixels(1280*960,0xff05070a);ui.paint(pixels,1280,960,s,data,desc);
        std::array<unsigned char,54> header{};header[0]='B';header[1]='M';const auto put=[&](unsigned at,unsigned n){for(unsigned i=0;i<4;++i)header[at+i]=std::uint8_t(n>>(8*i));};put(2,54+unsigned(pixels.size()*4));put(10,54);put(14,40);put(18,1280);put(22,unsigned(-960));header[26]=1;header[28]=32;
        constexpr const char* names[]{"basic.bmp","performance.bmp","optional-yes.bmp","optional-no.bmp"};
        std::ofstream file(previewDir/names[preview],std::ios::binary);file.write(reinterpret_cast<char*>(header.data()),header.size());file.write(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
    }
    std::cout<<"PASS "<<cases<<" original tuning draw/font/material captures, "<<checks<<" exact comparisons, "<<instructions<<" original instructions. Bank draw/clone/scale, text draw/query, PR1 division and material parser-event hooks; full font arithmetic unhooked.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
