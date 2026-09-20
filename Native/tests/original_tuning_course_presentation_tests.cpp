#include "original_tuning_course_presentation.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
using namespace idas3;using namespace idas3::reference;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool exact(float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);}
struct Matrix {float x=0,y=0,z=0,sx=1,sy=1;};
void bitmap(const std::filesystem::path& path,const std::vector<unsigned>& pixels){unsigned h[]={54+640*480*4,0,54,40,640,480,0x00200001,0,640*480*4,0,0,0,0};std::ofstream o(path,std::ios::binary);o.write("BM",2);o.write((const char*)h,52);for(int y=479;y>=0;--y)o.write((const char*)(pixels.data()+640*y),2560);}
int main(int argc,char**argv){try{
    require(argc>=3,"Canonical image and project root required");RefMemory m(argv[1]);constexpr unsigned object=0xd000000,stack=0xd010000,stop=0xff0000,mat=object+0x1000,child=object+0x2000,positions=object+0x3000;
    m.zeroRegion(object,65536);m.zeroRegion(stack,65536);auto native=OriginalTuningCoursePresentation::load(argv[2]);auto data=original::OriginalTuningData::load(argv[2]);unsigned checks=0;std::uint64_t instructions=0;
    for(unsigned car=0;car<35;++car){original::OriginalTuningCourseMenu state;state.car484=car;state.count504=unsigned(data.cars[car].packages.size());original::OriginalBattleProfile profile;profile.setByte(1192,2);
        native.reset(state,profile);m.write32(0xc340004,0);m.writeFloat(child+20,0);m.writeFloat(child+24,m.readFloat(0xc1b9d7c));m.writeFloat(child+28,0);m.writeFloat(child+48,.5f);m.writeFloat(child+56,1);m.write32(child+32,state.count504);m.write32(child+40,positions);
        for(unsigned i=0;i<state.count504;++i)m.writeFloat(positions+i*4,m.readFloat(0xc2a6fb4+((state.count504-1)*5+i)*4)*m.readFloat(0xc1b0884));
        for(unsigned tick=0;tick<144;++tick){state.selected496=(tick/8)%state.count504;state.phase464=tick%17<8?1:2;state.confirmationFrames460=tick%122;state.frame492=tick+1;const float wheel=float(tick%11)*.1f;
            native.advance(state,profile,wheel);m.write32(object+424,state.count504);m.write32(object+428,state.selected496);m.write32(object+456,tick);m.writeFloat(object+452,state.phase464==2?std::clamp(float(state.confirmationFrames460)/36.f,0.f,1.f):0);
            m.write32(child+36,state.selected496);m.writeFloat(child+44,wheel);
            // Actual12A820 writes authored part-category flags into V3 owner.
            m.write32(object+0x4000+1052,object);RefCpu flags(m);flags.r[4]=object+0x4000;flags.r[5]=car;flags.r[6]=state.selected496;flags.r[15]=stack+0xf000;flags.pr=stop;instructions+=flags.run(0xc12a820,stop,50000);
            std::vector<unsigned> unique;for(const auto& step:data.cars[car].packages[state.selected496].steps){const auto kind=step.words[0],id=step.words[1];if(kind==8||(car==22&&state.selected496==1&&id==33))continue;if(std::find(unique.begin(),unique.end(),id)==unique.end())unique.push_back(id);}
            for(unsigned i=0;i<3;++i)m.write32(object+432+i*4,i<unique.size()?unique[i]%42:~0u);
            int theme=state.selected496==0?(car==12?-1:std::int32_t(m.read32(0xc26b790+car*4))):car==22&&state.selected496==1?24:car==15&&state.selected496==1?20:-1;m.write32(object+444,std::min(theme,30));m.write32(object+460,object+0x5000);m.write32(object+0x5000,0x80035);m.write32(object+0x5004,tick);
            Matrix matrix;std::vector<Matrix> matrices;std::vector<OriginalTuningCourseDraw> captured;unsigned iterator=0;
            const auto hook=[&](RefCpu& c){
                for(unsigned address:{0xc1b90e0u,0xc1baf40u,0xc1bb6e0u,0xc1b8240u})c.callHooks[address]=[](auto& v){v.r[0]=1;};
                c.callHooks[0xc05a8e0]=[](auto& v){v.r[0]=v.r[5];};
                c.callHooks[0xc1f6610]=[&](auto&){matrices.push_back(matrix);};c.callHooks[0xc1f65c0]=[&](auto&){require(!matrices.empty(),"Source matrix stack underflow");matrix=matrices.back();matrices.pop_back();};
                c.callHooks[0xc1f6ac0]=[&](auto& v){matrix.x+=matrix.sx*v.getFloat(4);matrix.y+=matrix.sy*v.getFloat(5);matrix.z+=v.getFloat(6);};c.callHooks[0xc1f69d0]=[&](auto& v){matrix.sx*=v.getFloat(4);matrix.sy*=v.getFloat(5);};
                c.callHooks[0xc1d4be0]=[&](auto& v){iterator=0;m.write32(v.r[5],0);};c.callHooks[0xc1d4c40]=[&](auto& v){m.write32(v.r[5],iterator==0?1:iterator<5?3:0x80000000);m.write32(v.r[6],0);v.r[0]=mat;++iterator;};
                c.callHooks[0xc1d7120]=[&](auto& v){captured.push_back({v.r[4]>>16,v.r[4]&65535,matrix.x,matrix.y,matrix.z,matrix.sx,matrix.sy});};
                c.callHooks[0xc1be600]=[&](auto& v){captured.push_back({v.r[4]>>16,v.r[4]&65535,matrix.x,matrix.y,matrix.z,matrix.sx,matrix.sy});};
            };
            RefCpu cursor(m);cursor.r[4]=child;cursor.r[15]=stack+0xf000;cursor.pr=stop;hook(cursor);instructions+=cursor.run(0xc1ba2a0,stop,5000);
            RefCpu draw(m);draw.r[4]=object;draw.r[15]=stack+0xf000;draw.pr=stop;hook(draw);draw.callHooks[0xc1b8fe0]=[](auto&){};instructions+=draw.run(0xc1af680,stop,30000);
            const auto expected=native.drawList(state,1679);require(expected.size()==captured.size()+2,"Source tuning draw count differs");++checks;
            for(unsigned i=0;i<captured.size();++i){auto&a=captured[i];auto&b=expected[i];if(a.bank!=b.bank||a.chunk!=b.chunk||!exact(a.x,b.x)||!exact(a.y,b.y)||!exact(a.z,b.z)||!exact(a.scaleX,b.scaleX)||!exact(a.scaleY,b.scaleY)){std::cerr<<"car="<<car<<" tick="<<tick<<" i="<<i<<" source="<<a.bank<<":"<<a.chunk<<" "<<std::hex<<std::bit_cast<unsigned>(a.x)<<","<<std::bit_cast<unsigned>(a.y)<<","<<std::bit_cast<unsigned>(a.scaleX)<<" native="<<std::bit_cast<unsigned>(b.x)<<","<<std::bit_cast<unsigned>(b.y)<<","<<std::bit_cast<unsigned>(b.scaleX)<<std::dec<<"\n";throw std::runtime_error("Source tuning draw mismatch");}checks+=7;}
            if(car==0&&tick==120){std::vector<unsigned> pixels(640*480,0xff203050),again=pixels;native.paintCanvas(pixels,640,480,state,1679);native.paintCanvas(again,640,480,state,1679);require(pixels==again,"Repeated tuning paint advanced phase");++checks;if(argc>3)bitmap(argv[3],pixels);}
        }
    }
    std::cout<<"PASS "<<checks<<" tuning UI source draw/animation comparisons, "<<instructions<<" original instructions. Source camera/material checks documented separately.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}

