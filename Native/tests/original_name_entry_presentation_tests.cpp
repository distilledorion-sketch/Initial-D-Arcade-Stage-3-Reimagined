#include "original_name_entry_presentation.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <iostream>
#include <map>
#include <fstream>
#include <algorithm>
using namespace idas3;
using namespace idas3::reference;
void require(bool v,const char*message){if(!v)throw std::runtime_error(message);}
bool exact(float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);}
struct Matrix{float x=0,y=0,z=0,scale=1;};
void bitmap(const std::filesystem::path& p,const std::vector<std::uint32_t>& pixels){
    unsigned h[]={54+640*480*4,0,54,40,640,480,0x00200001,0,640*480*4,0,0,0,0};std::ofstream o(p,std::ios::binary);o.write("BM",2);o.write(reinterpret_cast<char*>(h),52);
    for(int y=479;y>=0;--y)o.write(reinterpret_cast<const char*>(pixels.data()+640*y),640*4);
}
// Execute the original constructor and UV writer against untouched source
// polygon bytes. The older draw test hooked0D19A0 and therefore only compared
// its input arrays; that did not prove their binding to authored vertex order.
std::array<unsigned,4> verifyGlyphBinding(RefMemory& m,const std::filesystem::path& image,
        const OriginalNameEntryPresentation& native,std::uint64_t& instructions,unsigned& checks){
    constexpr unsigned raw=0xd200000,descriptor=0xd220000,tls=0xd221000,owner=0xd222000,
        uv=0xd223000,stack=0xd230000,stop=0xff0000;
    m.zeroRegion(raw,0x80000);
    const auto source=image.parent_path()/"driveA/HOSTFS/model/select0302";
    const auto read=[](const auto& path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Original glyph polygon source missing");return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)),{});};
    const auto bytes=read(source/"select0302_pol.bin.nz"),table=read(source/"select0302_pol.tbl");
    require(bytes.size()==6048&&table.size()==52,"Original glyph polygon dimensions changed");
    const auto word=[](const auto& b,unsigned i){return unsigned(b.at(i))|(unsigned(b.at(i+1))<<8)|(unsigned(b.at(i+2))<<16)|(unsigned(b.at(i+3))<<24);};
    std::array<unsigned,5> chunks{};for(unsigned i=0;i<chunks.size();++i)chunks[i]=word(table,i*4);
    const auto restore=[&]{for(unsigned i=0;i<bytes.size();++i)m.write8(raw+i,bytes[i]);};restore();
    m.write32(tls+4,tls+8);m.write32(tls+8,0);unsigned heap=0xd240000;
    RefCpu ctor(m);ctor.r[4]=descriptor;ctor.r[5]=raw;ctor.r[6]=2;ctor.r[7]=2;ctor.r[15]=stack+0x10000;ctor.pr=stop;
    ctor.callHooks[0xc221fc0]=[&](auto& c){c.r[0]=tls;};
    ctor.callHooks[0xc0219a0]=[](auto& c){c.r[0]=c.r[4];};
    ctor.callHooks[0xc021960]=[&](auto& c){c.r[0]=heap;heap+=(c.r[5]+31)&~31u;};
    ctor.callHooks[0xc021980]=[](auto&){};
    instructions+=ctor.run(0xc0d1e60,stop,20000);
    require(m.read32(descriptor+12)==2&&m.read32(descriptor+16)==2&&m.read32(descriptor+20)==0xA&&m.read32(descriptor+24)==4,"Original glyph descriptor differs");checks+=4;
    std::array<unsigned,4> mapping{},offsets{};
    for(unsigned i=0;i<4;++i){mapping[i]=m.read32(m.read32(descriptor+4)+i*4);offsets[i]=m.read32(m.read32(descriptor+8)+i*4);}
    require(mapping==std::array<unsigned,4>{1,3,0,2},"Source-derived glyph corner mapping changed");++checks;
    require(offsets==std::array<unsigned,4>{0,24,48,72},"Source glyph vertex offsets differ");++checks;
    m.write32(owner+420,uv);m.write32(owner+424,descriptor);
    m.write32(uv+24,uv+64);m.write32(uv+28,uv+80);m.writeFloat(uv+16,-1);m.writeFloat(uv+20,-1);
    for(unsigned glyph=0;glyph<=220;++glyph){
        restore();const unsigned atlas=glyph<=63?0:glyph<=127?1:glyph<=191?2:glyph<=207?3:4;
        RefCpu cpu(m);cpu.r[4]=owner;cpu.r[5]=glyph;cpu.r[6]=atlas;cpu.r[15]=stack+0x10000;cpu.pr=stop;
        cpu.callHooks[0xc1baf40]=[](auto& c){c.r[0]=1;};cpu.callHooks[0xc1bb6e0]=[](auto& c){c.r[0]=32;};
        cpu.callHooks[0xc05a8e0]=[&](auto& c){require(c.r[5]==0x200000+atlas,"Source glyph resource selector differs");c.r[0]=raw+chunks[atlas];};
        //0D19A0 and the GMP iterator run with no hooks. The existing
        // unsigned-quotient service boundary is retained for1B1820.
        cpu.callHooks[0xc2223b8]=[](auto& c){c.fpul=c.r[4]/c.r[5];};
        instructions+=cpu.run(0xc1b1820,stop,10000);
        const auto materialized=native.materialize({32,atlas,0,0,0,1,int(glyph)});
        const auto authored=native.materialize({32,atlas});
        require(materialized.batches.size()==1&&authored.batches.size()==1,"Original glyph batches changed");++checks;
        const auto& batch=authored.batches[0];const unsigned vertexBase=batch.sourceOffset+32;
        for(unsigned i=0;i<4;++i){
            //Native sourceOffset is absolute within the supplied polygon file.
            const unsigned address=raw+vertexBase+i*24;const auto& vertex=materialized.batches[0].vertices[i];
            require(exact(vertex.u,m.readFloat(address+16))&&exact(vertex.v,m.readFloat(address+20)),"Final source glyph UV-to-vertex binding differs");checks+=2;
            require(exact(vertex.position.x,m.readFloat(address+4))&&exact(vertex.position.y,m.readFloat(address+8))&&exact(vertex.position.z,m.readFloat(address+12)),"Glyph fix changed authored geometry");checks+=3;
        }
        const unsigned first=chunks[atlas],last=atlas<4?chunks[atlas+1]:word(table,20);
        for(unsigned i=first;i<last;++i){
            const bool uvByte=i>=vertexBase&&i<vertexBase+96&&(i-vertexBase)%24>=16;
            if(!uvByte){require(m.read8(raw+i)==bytes[i],"Original glyph modifier altered a non-UV byte");++checks;}
        }
    }
    return mapping;
}
int main(int argc,char**argv){try{
    if(argc<3)throw std::runtime_error("Canonical image and project root required");
    RefMemory m(argv[1]);constexpr unsigned object=0xd000000,stack=0xd010000,stop=0xff0000,uv=object+0x1000,u=object+0x1100,v=object+0x1140,modifier=object+0x1200,vt=object+0x1300,mat=object+0x1400;
    m.zeroRegion(object,0x10000);m.zeroRegion(stack,0x10000);
    auto native=OriginalNameEntryPresentation::load(argv[2]);
    // Initial source filter constructor1AE520(k3,initialcursor,nonangular).
    std::uint64_t instructions=0;unsigned checks=0;
    const auto glyphCornerMap=verifyGlyphBinding(m,argv[1],native,instructions,checks);
    for(unsigned a=0;a<2;++a){RefCpu c(m);c.r[4]=object+468+a*20;c.r[5]=0;c.r[15]=stack+0xf000;c.pr=stop;c.setFloat(4,3);c.setFloat(5,m.readFloat(0xc33fce0+4*a));instructions+=c.run(0xc1ae520,stop);}
    m.writeFloat(object+508,1);m.write32(object+420,uv);m.write32(uv+24,u);m.write32(uv+28,v);m.write32(object+428,modifier);m.write32(modifier,vt);m.write32(vt+20,0xd005000);m.write32(vt+28,0xd005004);
    for(unsigned tick=0;tick<900;++tick){
        original::OriginalNameEntryState state;state.selected460=(tick*7)%53;state.page600=(tick/60)%3;state.length528=(tick/31)%6;
        for(unsigned i=0;i<5;++i)state.glyphIds480[i]=(tick*13+i*57)%221;
        m.write32(object+412,state.selected460);m.write32(object+416,state.page600);m.write32(object+464,state.length528);for(unsigned i=0;i<5;++i)m.write32(object+444+i*4,state.glyphIds480[i]);
        native.advance(state);RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;
        Matrix matrix;std::vector<Matrix> matrices;std::vector<OriginalNameEntryDraw> captured;
        std::vector<std::array<float,8>> capturedUvs;unsigned iterator=0;
        for(unsigned address:{0xc1b90e0u,0xc1b8fe0u,0xc1baf40u,0xc1bb6e0u,0xd005000u,0xd005004u})c.callHooks[address]=[](auto&cpu){cpu.r[0]=1;};
        c.callHooks[0xc05a8e0]=[](auto&cpu){cpu.r[0]=cpu.r[5];};
        c.callHooks[0xc1f6610]=[&](auto&){matrices.push_back(matrix);};
        c.callHooks[0xc1f65c0]=[&](auto&){require(!matrices.empty(),"Original name matrix underflow");matrix=matrices.back();matrices.pop_back();};
        c.callHooks[0xc1f6ac0]=[&](auto&cpu){matrix.x+=matrix.scale*cpu.getFloat(4);matrix.y+=matrix.scale*cpu.getFloat(5);matrix.z+=cpu.getFloat(6);};
        c.callHooks[0xc1f69d0]=[&](auto&cpu){require(exact(cpu.getFloat(4),cpu.getFloat(5)),"Original name nonuniform scale");matrix.scale*=cpu.getFloat(4);};
        c.callHooks[0xc2223b8]=[](auto&cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
        c.callHooks[0xc1d4be0]=[&](auto&cpu){iterator=0;m.write32(cpu.r[5],0);m.write32(mat+8,0x2408245b);};
        c.callHooks[0xc1d4c40]=[&](auto&cpu){const unsigned kind=iterator==0?2:iterator==1?1:0x80000000u;m.write32(cpu.r[5],kind);m.write32(cpu.r[6],0);cpu.r[0]=mat;iterator++;};
        c.callHooks[0xc0d19a0]=[&](auto&){std::array<float,8> value;for(unsigned i=0;i<4;++i){value[i]=m.readFloat(u+i*4);value[4+i]=m.readFloat(v+i*4);}capturedUvs.push_back(value);};
        c.callHooks[0xc1d5400]=[](auto&cpu){cpu.r[0]=cpu.r[4];};
        c.callHooks[0xc1d7120]=[&](auto&cpu){auto selector=cpu.r[4];const int alpha=selector==0x100000?int(m.read32(mat+12)>>24):-1;captured.push_back({selector>>16,selector&65535,matrix.x,matrix.y,matrix.z,matrix.scale,-1,alpha});};
        instructions+=c.run(0xc1b1300,stop,20000);
        const auto draws=native.drawList(state);require(captured.size()+2==draws.size(),"Name source draw count differs");
        unsigned glyphIndex=0;
        for(unsigned i=0;i<captured.size();++i){const auto&a=captured[i];const auto&b=draws[i+2];
            if(a.bank!=b.bank||a.chunk!=b.chunk||!exact(a.x,b.x)||!exact(a.y,b.y)||!exact(a.z,b.z)||!exact(a.scale,b.scale)||a.pulseAlpha!=b.pulseAlpha){std::cerr<<"tick "<<tick<<" draw "<<i<<" source "<<a.bank<<':'<<a.chunk<<" XYZ "<<a.x<<','<<a.y<<','<<a.z<<" scale "<<a.scale<<" alpha "<<a.pulseAlpha<<" native "<<b.bank<<':'<<b.chunk<<" XYZ "<<b.x<<','<<b.y<<','<<b.z<<" scale "<<b.scale<<" alpha "<<b.pulseAlpha<<'\n';throw std::runtime_error("Name source draw mismatch");}checks+=8;
            const auto chunk=native.materialize(b);
            if(b.glyph>=0){require(glyphIndex<capturedUvs.size(),"Source glyph modifier absent");const auto& values=capturedUvs[glyphIndex++];for(unsigned k=0;k<4;++k){const auto& vertex=chunk.batches[0].vertices[k];require(exact(vertex.u,values[glyphCornerMap[k]])&&exact(vertex.v,values[4+glyphCornerMap[k]]),"Source glyph UV differs");checks+=2;}}
            if(b.pulseAlpha>=0){require(chunk.batches[0].ich[2]==m.read32(mat+8),"Source slot blend word differs");++checks;}
        }
        require(glyphIndex==capturedUvs.size(),"Extra source glyph modifier");++checks;
        require(exact(native.pulsePhase(),m.readFloat(object+508)),"Source name pulse phase differs");++checks;
        for(unsigned a=0;a<2;++a){const float clamped=a?std::min(m.readFloat(object+480+a*20),m.readFloat(0xc1b1604)):std::max(m.readFloat(object+480),m.readFloat(0xc1b1600));require(exact(native.cursor()[a],clamped),"Source cursor filter differs");++checks;}
        if(tick==120){std::vector<std::uint32_t> canvas(640*480,0xff203050),foreground(640*480),glow(640*480);
            const auto compose=[&](const auto& overlay,bool additive){for(unsigned i=0;i<canvas.size();++i){const auto a=overlay[i]>>24;std::uint32_t pixel=0xff000000;for(unsigned shift:{0u,8u,16u}){const auto src=(overlay[i]>>shift)&255,dst=(canvas[i]>>shift)&255;pixel|=(additive?std::min(255u,src+dst):(src*a+dst*(255-a)+127)/255)<<shift;}canvas[i]=pixel;}};
            native.paintBackground(canvas,640,480);native.paintNameBacking(foreground,640,480);compose(foreground,false);
            native.paintGlow(glow,640,480,state);compose(glow,true);
            std::fill(foreground.begin(),foreground.end(),0);native.paint(foreground,640,480,state,4879);auto repeated=std::vector<std::uint32_t>(640*480);native.paint(repeated,640,480,state,4879);require(foreground==repeated,"Repeated name paint changed phase");
            require((foreground[270*640+320]>>24)==0,"Name foreground covers car aperture");compose(foreground,false);
            std::fill(glow.begin(),glow.end(),0);native.paintCursor(glow,640,480,state);compose(glow,true);
            if(argc>3){const auto path=std::filesystem::path(argv[3]);bitmap(path,canvas);state.profileKind1192=2;state.frame572=0;std::fill(foreground.begin(),foreground.end(),0);native.paintLegacy(foreground,640,480,state);compose(foreground,false);bitmap(path.parent_path()/(path.stem().string()+"-migration.bmp"),canvas);}checks+=2;}

    }
    {
        RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;std::vector<unsigned> selectors;
        for(unsigned address:{0xc1b90e0u,0xc1b8fe0u,0xc1baf40u,0xc1bb6e0u,0xc1f6610u,0xc1f65c0u})c.callHooks[address]=[](auto&cpu){cpu.r[0]=1;};
        c.callHooks[0xc05a8e0]=[](auto&cpu){cpu.r[0]=cpu.r[5];};c.callHooks[0xc1d7120]=[&](auto&cpu){selectors.push_back(cpu.r[4]);};
        instructions+=c.run(0xc1b1ca0,stop,2000);require(selectors==std::vector<unsigned>{0x80036,0x8002e},"Original name common-background selectors differ");checks+=2;
    }
    for(std::int32_t countdown:{-600,-1,0,59,60,599,600,5999,6000,2147483647}){
        m.writeFloat(object+20,std::bit_cast<float>(0x40a66666u));m.writeFloat(object+24,std::bit_cast<float>(0xbe23d70au));m.writeFloat(object+28,0);m.write32(object+32,std::uint32_t(countdown)/60);
        RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;Matrix matrix;std::vector<OriginalNameEntryDraw> captured;
        for(unsigned address:{0xc1f6610u,0xc1f65c0u,0xc1baf40u,0xc1bb6e0u})c.callHooks[address]=[](auto&cpu){cpu.r[0]=1;};
        c.callHooks[0xc2223b8]=[](auto&cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
        c.callHooks[0xc05a8e0]=[](auto&cpu){cpu.r[0]=cpu.r[5];};
        c.callHooks[0xc1f6ac0]=[&](auto&cpu){matrix.x+=cpu.getFloat(4);matrix.y+=cpu.getFloat(5);matrix.z+=cpu.getFloat(6);};
        c.callHooks[0xc1d7120]=[&](auto&cpu){captured.push_back({cpu.r[4]>>16,cpu.r[4]&65535,matrix.x,matrix.y,matrix.z});};
        instructions+=c.run(0xc1babc0,stop,2000);const auto expected=native.timerDraws(std::uint32_t(countdown));require(captured.size()==expected.size(),"Source timer digit count differs");++checks;
        for(unsigned i=0;i<captured.size();++i){const auto&a=captured[i];const auto&b=expected[i];require(a.bank==b.bank&&a.chunk==b.chunk&&exact(a.x,b.x)&&exact(a.y,b.y)&&exact(a.z,b.z),"Source name timer placement differs");checks+=5;}
    }
    // The fade's actual ICH constructor and authored vertex Z establish its
    // depth, independently from the misleading source submission order.
    RefCpu fade(m);fade.r[4]=object+0x6000;fade.r[5]=0x01000002;fade.r[6]=4;fade.r[15]=stack+0xf000;fade.pr=stop;
    instructions+=fade.run(0xc1d0260,stop,5000);
    require(m.read32(object+0x60c4)==0x9bc00000&&m.read32(object+0x60c8)==0x941804c0,"Source fade ICH differs");
    require(m.read32(0xc0c5150)==0xbc343958,"Source fade depth differs");checks+=3;
    require(m.read16(0xc1286e0)==8192,"Name car projection FOV differs");++checks;
    for(unsigned kind=0;kind<4;++kind)for(unsigned frame=0;frame<40;++frame){
        original::OriginalNameEntryState state;state.profileKind1192=std::uint8_t(kind);state.frame572=frame;
        m.write8(0xc31ce44,std::uint8_t(kind));m.write32(object+572,frame);m.write32(object+436,object+0x6000);
        RefCpu c(m);c.r[13]=object;c.r[12]=572;c.r[11]=stack+0x100c;c.r[14]=stack+0x1000;c.r[15]=stack+0xf000;c.r[10]=0xc1f65c0;
        Matrix matrix;std::vector<unsigned> captured;
        c.callHooks[0xc128fc0]=[&](auto&cpu){for(unsigned i=0;i<3;++i)m.write32(cpu.r[2]+i*4,m.read32(0xc145408+i*4));};
        for(unsigned a:{0xc1fcc60u,0xc1f65c0u,0xc1fbd60u})c.callHooks[a]=[](auto&){};
        c.callHooks[0xc1f6ac0]=[&](auto&cpu){matrix={cpu.getFloat(4),cpu.getFloat(5),cpu.getFloat(6),1};};
        c.callHooks[0xc128f80]=[&](auto&cpu){captured.push_back(cpu.r[5]);};c.callHooks[0xc2223b8]=[](auto&cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
        instructions+=c.run(0xc128648,0xc1286ca,2000);const auto draws=native.legacyDraws(state);require(draws.size()==captured.size(),"Legacy name blink selection differs");++checks;
        for(unsigned i=0;i<draws.size();++i){if(!exact(matrix.x-m.readFloat(0xc145408),draws[i].x)||!exact(matrix.y-m.readFloat(0xc14540c),draws[i].y))std::cerr<<std::hex<<"legacy source "<<std::bit_cast<unsigned>(matrix.x-m.readFloat(0xc145408))<<","<<std::bit_cast<unsigned>(matrix.y-m.readFloat(0xc14540c))<<" expected "<<std::bit_cast<unsigned>(draws[i].x)<<","<<std::bit_cast<unsigned>(draws[i].y)<<std::dec<<"\n";require(draws[i].chunk==captured[i]&&exact(matrix.x-m.readFloat(0xc145408),draws[i].x)&&exact(matrix.y-m.readFloat(0xc14540c),draws[i].y),"Legacy name source translation differs");checks+=3;}
    }
    // Name car camera literal preparation and yaw increment execute verbatim.
    RefCpu camera(m);camera.r[14]=stack;camera.r[11]=stack;camera.r[15]=stack+0xf000;
    camera.callHooks[0xc1fc2a0]=[&](auto&cpu){const auto source=native.showroomFrame(0);const float expected[]={source.eye.x,source.eye.y,source.eye.z,source.target.x,source.target.y,source.target.z};for(unsigned i=0;i<6;++i){require(m.read32((i<3?cpu.r[4]:cpu.r[5])+(i%3)*4)==std::bit_cast<unsigned>(expected[i]),"Name camera source literal differs");++checks;}};
    instructions+=camera.run(0xc128552,0xc128592,1000);
    std::cout<<"PASS "<<checks<<" original name draw/filter/camera comparisons, "<<instructions<<" original instructions; 900 varied page/cursor/name ticks. Glyph constructor/iterator/final writer unhooked for all221 glyphs; other draw matrix/GMP/raster boundaries hooked; native pixel determinism checked.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
