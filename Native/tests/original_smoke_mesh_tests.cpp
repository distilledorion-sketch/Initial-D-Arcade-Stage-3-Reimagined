#include "original_smoke_mesh.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned mesh=0xc800000,grid=0xc810000,mapping=0xc811000,tls=0xc812000,node=tls+256;
constexpr unsigned stack=0xc820000,heap=0xc830000,display=0xc840000,view=display+0x1000,wrapper=view+0x1000,stop=0xcfffffc;
std::uint64_t checks=0,cases=0,instructions=0;
void check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
void equal(unsigned actual,unsigned expected,const char* reason){++checks;if(actual!=expected){
    std::cerr<<"case "<<cases<<' '<<reason<<" source="<<std::hex<<actual<<" native="<<expected<<std::dec<<'\n';throw std::runtime_error("Original smoke mesh mismatch");}}
void equalFloat(float actual,float expected,const char* reason){equal(std::bit_cast<unsigned>(actual),std::bit_cast<unsigned>(expected),reason);}
std::vector<unsigned char> read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),{}};
    if(bytes.empty())throw std::runtime_error("Original smoke polygon asset required");return bytes;
}
unsigned word(std::span<const unsigned char> bytes,unsigned at){
    if(at+4>bytes.size())throw std::runtime_error("Truncated original smoke polygon");
    return unsigned(bytes[at])|(unsigned(bytes[at+1])<<8)|(unsigned(bytes[at+2])<<16)|(unsigned(bytes[at+3])<<24);
}
NativeModelChunk decode(std::span<const unsigned char> bytes){
    // Independent minimal source reader, bounded to the three actual grids.
    check(word(bytes,0)==0x100&&word(bytes,24)==bytes.size(),"Original grid chunk header");
    NativeModelChunk chunk;chunk.sourceSize=unsigned(bytes.size());
    for(unsigned i=0;i<24;++i)chunk.header[i]=word(bytes,i*4);
    NativeModelBatch batch;batch.sourceOffset=160;
    for(unsigned i=0;i<16;++i)batch.material[i]=word(bytes,96+i*4);
    for(unsigned i=0;i<8;++i)batch.ich[i]=word(bytes,160+i*4);
    check(((batch.material[0]>>8)&15)==5&&((batch.ich[0]>>8)&15)==7&&batch.ich[6]==10,"Authored UV mesh packet layout");
    check(192+batch.ich[7]*24==bytes.size()&&batch.material[9]==3,"Original smoke palette and vertex count");
    for(unsigned i=0;i<batch.ich[7];++i){
        const unsigned at=192+i*24;NativeModelVertex v;v.header=word(bytes,at);
        v.position={std::bit_cast<float>(word(bytes,at+4)),std::bit_cast<float>(word(bytes,at+8)),std::bit_cast<float>(word(bytes,at+12))};
        v.u=std::bit_cast<float>(word(bytes,at+16));v.v=std::bit_cast<float>(word(bytes,at+20));
        batch.vertices.push_back(v);
    }
    chunk.batches.push_back(std::move(batch));return chunk;
}
void runMesh(RefMemory& memory,std::span<const unsigned char> bytes,unsigned dimension,int uvShift,const NativeModelChunk& exported){
    memory.clear();memory.zeroRegion(mesh,0x50000);
    for(unsigned i=0;i<bytes.size();++i)memory.write8(mesh+i,bytes[i]);
    const auto decoded=decode(bytes);auto source=exported;
    check(source.header==decoded.header&&source.sourceSize==bytes.size()&&source.batches.size()==1,"Exported source chunk identity");
    check(source.batches[0].ich==decoded.batches[0].ich&&source.batches[0].material==decoded.batches[0].material,"Exported material/packet identity");
    check(source.batches[0].vertices.size()==decoded.batches[0].vertices.size(),"Exported vertex count");
    for(unsigned i=0;i<source.batches[0].vertices.size();++i){const auto& a=source.batches[0].vertices[i];const auto& b=decoded.batches[0].vertices[i];
        equal(a.header,b.header,"Exported header");equalFloat(a.position.x,b.position.x,"Exported X");equalFloat(a.position.y,b.position.y,"Exported Y");equalFloat(a.position.z,b.position.z,"Exported Z");
        equalFloat(a.u,b.u,"Exported U without rounding");equalFloat(a.v,b.v,"Exported V without rounding");
    }
    // Exercise the original negative-minimum normalization with the same
    // authored geometry and strip order, in addition to unchanged assets.
    for(unsigned i=0;i<source.batches[0].vertices.size();++i){auto& v=source.batches[0].vertices[i];
        v.u+=float(uvShift);v.v-=float(uvShift);
        memory.writeFloat(mesh+192+i*24+16,v.u);memory.writeFloat(mesh+192+i*24+20,v.v);
    }
    memory.write32(tls+4,node);memory.write32(node,node);memory.write32(node+4,node);
    RefCpu cpu(memory);unsigned allocation=heap;
    cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tls;};
    cpu.callHooks[0xc0219a0]=[](RefCpu&){}; // base object has no relevant fields
    cpu.callHooks[0xc021960]=[&](RefCpu& c){check(c.r[5]<0x10000,"Bounded source allocation");c.r[0]=allocation;memory.zeroRegion(allocation,c.r[5]);allocation+=(c.r[5]+15)&~15u;};
    cpu.callHooks[0xc021980]=[](RefCpu&){};
    const auto execute=[&](unsigned address){cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(address,stop,1000000);};
    cpu.r[4]=grid;cpu.r[5]=dimension;cpu.r[6]=dimension;cpu.setFloat(4,.9f);cpu.setFloat(5,-1.f);execute(0xc0d1640);
    equal(memory.read32(grid+4),dimension,"Grid width");equal(memory.read32(grid+8),dimension,"Grid height");
    equalFloat(memory.readFloat(grid+16),.9f,"Original constant U");equalFloat(memory.readFloat(grid+20),-1.f,"Original variable V flag");
    check(memory.read32(grid+24)==0&&memory.read32(grid+28)!=0,"Only density/V allocated");
    for(unsigned i=0;i<dimension*dimension;++i)equalFloat(memory.readFloat(memory.read32(grid+28)+i*4),.008f,"Initial density");
    cpu.r[4]=mapping;cpu.r[5]=mesh;cpu.r[6]=dimension;cpu.r[7]=dimension;execute(0xc0d1e60);
    const auto nativeMapping=mapOriginalSmokeGrid(source,dimension,dimension);
    const auto count=unsigned(source.batches[0].vertices.size());
    equal(memory.read32(mapping+20),10,"Mapping format");equal(memory.read32(mapping+24),count,"Mapping count");
    for(unsigned i=0;i<count;++i){
        equal(memory.read32(memory.read32(mapping+4)+i*4),nativeMapping.cells[0][i],"Actual UV-derived cell index");
        equal(memory.read32(memory.read32(mapping+8)+i*4),i*24,"Actual source vertex offset");
    }
    memory.write32(display+20,wrapper);memory.write32(wrapper+20,mesh);memory.write32(display+64,view);
    memory.writeFloat(view+168,1.f);unsigned random=0xa419ce31;
    const auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    std::array<float,3> translation{},scale{};unsigned submitted=0,pushed=0,popped=0;
    cpu.callHooks[0xc1fcc60]=[&](RefCpu&){++pushed;};
    cpu.callHooks[0xc1fd060]=[&](RefCpu& c){for(unsigned i=0;i<3;++i)translation[i]=memory.readFloat(c.r[4]+4*i);};
    cpu.callHooks[0xc1f69d0]=[&](RefCpu& c){for(unsigned i=0;i<3;++i)scale[i]=c.getFloat(4+i);};
    cpu.callHooks[0xc1d5400]=[&](RefCpu& c){equal(c.r[4],mesh,"Original chunk lookup");c.r[0]=mesh;};
    cpu.callHooks[0xc1d7120]=[&](RefCpu& c){equal(c.r[4],mesh,"Original submitted model");++submitted;};
    cpu.callHooks[0xc1f65c0]=[&](RefCpu& c){equal(c.r[4],1,"Matrix pop count");++popped;};
    for(unsigned trial=0;trial<64;++trial){
        OriginalSmokeDisplay cloud;cloud.visible=true;
        cloud.position={float(int(next()%4097)-2048)*.125f,float(int(next()%1025)-512)*.125f,-float(1+next()%512)*.125f};
        cloud.width=float(1+next()%640);cloud.height=float(1+next()%480);cloud.density.resize(dimension*dimension);
        for(auto& v:cloud.density)v=trial==0?.008f:trial==1?.99f:float(next()%100000)*.00001f;
        const float focal=trial%2?240.f:320.f;
        const unsigned color=trial%4==0?0xffffffffu:trial%4==1?0xff8b5439u:trial%4==2?0u:next();
        // Each draw starts from a private immutable source chunk. This checks
        // both non-white packet mutation and white/no-override paths.
        for(unsigned i=96;i<192;++i)memory.write8(mesh+i,bytes[i]);
        for(unsigned i=0;i<cloud.density.size();++i)memory.writeFloat(memory.read32(grid+28)+4*i,cloud.density[i]);
        cpu.r[4]=grid;cpu.r[5]=mesh;cpu.r[6]=mapping;execute(0xc0d19a0);
        for(unsigned i=0;i<3;++i)memory.writeFloat(display+24+i*4,cloud.position[i]);
        memory.writeFloat(display+36,cloud.width);memory.writeFloat(display+40,cloud.height);
        memory.write32(display+52,color);memory.writeFloat(view+180,focal);
        cpu.r[4]=display;execute(0xc1401e0);
        const auto output=prepareOriginalSmokeMesh(source,nativeMapping,cloud,focal,color);const auto& batch=output.chunk.batches[0];
        check(batch.indices==source.batches[0].indices,"Preserved authored strip topology");
        for(unsigned i=0;i<3;++i){equalFloat(translation[i],output.translation[i],"Draw translation");equalFloat(scale[i],output.scale[i],"Draw scale");}
        for(unsigned i=0;i<24;++i)equal(memory.read32(mesh+i*4),output.chunk.header[i],"Preserved model header");
        for(unsigned i=0;i<16;++i)equal(memory.read32(mesh+96+i*4),batch.material[i],"Original material word");
        for(unsigned i=0;i<8;++i)equal(memory.read32(mesh+160+i*4),batch.ich[i],"Original draw packet word");
        for(unsigned i=0;i<count;++i){const auto& v=batch.vertices[i];const auto at=mesh+192+i*24;
            equal(memory.read32(at),v.header,"Preserved vertex/strip header");
            equalFloat(memory.readFloat(at+4),v.position.x,"Preserved X");equalFloat(memory.readFloat(at+8),v.position.y,"Preserved Y");equalFloat(memory.readFloat(at+12),v.position.z,"Preserved Z");
            equalFloat(memory.readFloat(at+16),v.u,"Original palette U");equalFloat(memory.readFloat(at+20),v.v,"Original density V");
            equalFloat(source.batches[0].vertices[i].u,float(int(nativeMapping.cells[0][i]%dimension)+uvShift),"Immutable source U");
        }
        ++cases;
    }
    check(submitted==64&&pushed==64&&popped==64,"Balanced original draw boundaries");
}
void writePreview(const NativeModel& model,const NativeTextureBank& textures,const std::filesystem::path& directory){
    if(std::filesystem::exists(directory))throw std::runtime_error("Preview output must be a new directory");
    std::filesystem::create_directories(directory);const auto kernel=originalSmokeDensityKernel(textures.at(1));
    for(unsigned gridIndex=0;gridIndex<3;++gridIndex){
        const unsigned dimension=gridIndex==0?16:gridIndex==1?20:32;
        std::vector<OriginalSmokeDisplayParticle> particles;
        for(unsigned i=0;i<12;++i){OriginalSmokeDisplayParticle p;p.particle.flags=0x110;p.particle.remaining=12-i/2;p.particle.size=.1f+float(i)*.02f;
            p.cameraPosition={-.4f+float(i)*.065f,float(i%3)*.035f,-1.25f-float(i)*.075f};particles.push_back(p);}
        const auto cloud=projectOriginalSmokeDisplay(particles,originalSmokeParameters(0,false),320.f,.2f,
            dimension,dimension,kernel,128,128,[](float farZ,float){return farZ;});
        const auto gridMapping=mapOriginalSmokeGrid(model.chunks[gridIndex],dimension,dimension);
        const auto draw=prepareOriginalSmokeMesh(model.chunks[gridIndex],gridMapping,cloud,320.f,0xff8b5439u);
        std::vector<std::uint32_t> canvas(640*480);for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)
            canvas[y*640+x]=((x/24+y/24)&1)?0xff404040u:0xff505050u;
        const auto before=canvas;std::vector<Vec3> positions;
        for(const auto& b:draw.chunk.batches)for(const auto& v:b.vertices)
            positions.push_back({320.f+cloud.left+(v.position.x+.5f)*cloud.width,240.f-cloud.bottom-v.position.y*cloud.height,0});
        SpritePlacement placement;placement.softwareOnly=true;
        compositeOriginalMenuChunk(canvas,640,480,textures,draw.chunk,placement,positions);
        unsigned changed=0;for(unsigned i=0;i<canvas.size();++i)changed+=canvas[i]!=before[i];
        check(changed>100,"Authored density palette produces visible pixels");
        std::ofstream file(directory/("smoke-grid-"+std::to_string(dimension)+".ppm"),std::ios::binary);
        file<<"P6\n640 480\n255\n";for(auto pixel:canvas){const std::array<char,3> rgb{char(pixel>>16),char(pixel>>8),char(pixel)};file.write(rgb.data(),3);}
        if(!file)throw std::runtime_error("Could not write smoke preview");
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=3&&argc!=4)throw std::invalid_argument("Canonical image, adjacent HOSTFS assets, Native root and optional new preview directory required");
    const std::filesystem::path image=argv[1],assets=image.parent_path()/"driveA/HOSTFS/model/effect/smoke";
    RefMemory memory(image);const auto payload=read(assets/"smoke_pol.bin"),table=read(assets/"smoke_pol.tbl");
    const auto exported=NativeModel::load(std::filesystem::path(argv[2])/"data/original_assets/effects/smoke/smoke_grids.idasmesh");
    check(exported.chunks.size()==3,"Exported smoke grid selection");
    const auto textures=NativeTextureBank::load(std::filesystem::path(argv[2])/"data/original_assets/effects/smoke/textures.idastex");
    const auto kernel=originalSmokeDensityKernel(textures.at(1));
    const auto sourceTextures=read(assets/"smoke_tex.bin"),textureTable=read(assets/"smoke_tex.tbl");
    const unsigned kernelOffset=word(textureTable,24);
    for(unsigned i=0;i<kernel.size();++i)equal((sourceTextures.at(kernelOffset+2*i)>>4)&15u,(kernel[i]>>4)&15u,"Runtime kernel matches original green nibble");
    check(table.size()==32,"Eight original smoke model chunks");
    for(unsigned i=0;i<3;++i){const unsigned first=word(table,i*4),last=word(table,(i+1)*4);
        check(first<last&&last<=payload.size(),"Original smoke grid chunk bounds");
        for(int shift:{0,-7})runMesh(memory,std::span(payload).subspan(first,last-first),i==0?16:i==1?20:32,shift,exported.chunks[i]);
    }
    if(argc==4)writePreview(exported,textures,argv[3]);
    std::cout<<"PASS "<<cases<<" original smoke mesh draws, "<<checks<<" bitwise/boundary checks, "<<instructions
        <<" original instructions. Actual16/20/32 HOSTFS grids; source constructor, packet iteration, UV mapping/application and draw packet mutation execute. Hooks: TLS, allocation/base object, matrix submission, chunk handle lookup and GPU submission. Rasterization and footprint depth are not covered.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
