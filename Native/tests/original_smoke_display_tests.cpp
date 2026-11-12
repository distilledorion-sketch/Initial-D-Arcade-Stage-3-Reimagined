#include "original_smoke_display.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned display=0xc800000,parent=0xc810000,view=0xc820000,grid=0xc830000,buffer=0xc840000;
constexpr unsigned particleBase=0xc850000,sentinel=0xc860000,stack=0xc870000,kernelBase=0xc880000;
constexpr unsigned resolution=0xc811000,wrapper=0xc812000,vtable=0xc814000,stop=0xcfffffc;
std::uint64_t cases=0,checks=0,instructions=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void equal(unsigned source,unsigned native,const char* field){++checks;if(source!=native){
    std::cerr<<"case "<<cases<<' '<<field<<" source="<<std::hex<<source<<" native="<<native<<std::dec<<'\n';throw std::runtime_error("Smoke display reference mismatch");}}
void equalFloat(float source,float native,const char* field){equal(std::bit_cast<unsigned>(source),std::bit_cast<unsigned>(native),field);}
std::vector<std::uint16_t> readKernel(const std::filesystem::path& image){
    const auto directory=image.parent_path()/"driveA/HOSTFS/model/effect/smoke";
    std::ifstream table(directory/"smoke_tex.tbl",std::ios::binary);std::array<unsigned char,64> bytes{};
    if(!table.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("Original smoke texture table required");
    const auto u16=[&](unsigned at){return unsigned(bytes[at])|(unsigned(bytes[at+1])<<8);};
    const auto u32=[&](unsigned at){return u16(at)|(u16(at+2)<<16);};
    check(u16(16)==128&&u16(18)==128&&bytes[20]==2,"Source cloud kernel descriptor");
    std::vector<std::uint16_t> pixels(128*128);
    std::ifstream file(directory/"smoke_tex.bin",std::ios::binary);file.seekg(u32(24));
    if(!file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*2))throw std::runtime_error("Original cloud texels required");
    return pixels;
}
void runCase(RefMemory& memory,std::span<const std::uint16_t> kernel,
        std::vector<OriginalSmokeDisplayParticle> input,const OriginalSmokeParameters& params,
        float focal,float gain,unsigned dimension,bool farDepth){
    memory.clear();memory.zeroRegion(display,100);memory.zeroRegion(parent,64);memory.zeroRegion(view,192);
    memory.zeroRegion(grid,32);memory.zeroRegion(buffer,32*32*4);memory.zeroRegion(stack,8192);
    memory.zeroRegion(resolution,12);memory.zeroRegion(wrapper,24);memory.zeroRegion(vtable,32);
    memory.write32(display,vtable);memory.write32(vtable+28,0xc140bc0);
    memory.write32(display+20,wrapper);memory.write32(display+48,grid);memory.writeFloat(display+56,gain);
    memory.write32(display+60,parent);memory.write32(display+64,view);
    memory.writeFloat(view+168,1.f);memory.writeFloat(view+180,focal);
    memory.write32(grid+4,dimension);memory.write32(grid+8,dimension);memory.write32(grid+28,buffer);
    memory.write32(0xc8ff170,resolution);memory.write32(resolution+4,dimension);memory.write32(resolution+8,dimension);
    memory.write32(parent+36,sentinel);memory.write32(parent+40,input.empty()?sentinel:particleBase+4);
    for(unsigned i=0;i<input.size();++i){
        const unsigned address=particleBase+i*64;const auto& p=input[i].particle;memory.zeroRegion(address,60);
        memory.write32(address+8,i+1<input.size()?address+68:sentinel);
        for(unsigned axis=0;axis<3;++axis)memory.writeFloat(address+24+axis*4,p.position[axis]);
        memory.writeFloat(address+48,p.size);memory.write32(address+52,p.remaining);memory.write32(address+56,p.flags);
    }
    for(unsigned i=0;i<9;++i)memory.writeFloat(0xca9badc+i*4,params.values[i]);
    memory.zeroRegion(0xca9bb00,20*unsigned(input.size()));memory.zeroRegion(0xc31ddf4,24);
    memory.write16(0xc31ddf4,128);memory.write16(0xc31ddf6,128);memory.write32(0xc31de04,kernelBase);
    for(unsigned i=0;i<kernel.size();++i)memory.write16(kernelBase+i*2,kernel[i]);
    RefCpu cpu(memory);cpu.r[4]=display;cpu.r[15]=stack+4096;cpu.pr=stop;
    unsigned current=0,applied=0;
    // Only camera stack/transform, sphere visibility, footprint depth and final
    // model-UV application are external boundaries. Projection, clipping,
    // original texel sampling, density accumulation and clamping execute.
    cpu.callHooks[0xc1fcc60]=[](RefCpu&){};
    cpu.callHooks[0xc1fc5a0]=[&](RefCpu& c){equal(c.r[4],view+64,"camera matrix input");};
    cpu.callHooks[0xc1fd060]=[&](RefCpu& c){check(c.r[4]>=particleBase+24,"particle translation address");current=(c.r[4]-particleBase-24)/64;check(current<input.size(),"particle transform index");};
    cpu.callHooks[0xc1f63c0]=[&](RefCpu& c){for(unsigned i=0;i<3;++i)memory.writeFloat(c.r[4]+i*4,input[current].cameraPosition[i]);};
    cpu.callHooks[0xc1f65c0]=[](RefCpu&){};
    cpu.callHooks[0xc1d0b60]=[&](RefCpu& c){equal(c.r[4],view,"frustum owner");equal(c.r[5],particleBase+current*64+24,"frustum position");c.setFloat(0,input[current].inFrustum?1.f:-1.f);};
    cpu.callHooks[0xc140bc0]=[&](RefCpu& c){equal(c.r[4],display,"depth selector owner");c.setFloat(0,c.getFloat(farDepth?4:5));};
    cpu.callHooks[0xc0d19a0]=[&](RefCpu& c){equal(c.r[4],grid,"density application");++applied;};
    instructions+=cpu.run(0xc13fc00,0xc13fece,50000);
    const unsigned circleCount=cpu.r[7],scratch=cpu.r[14];
    instructions+=cpu.run(0xc13fece,stop,500000);
    const auto output=projectOriginalSmokeDisplay(input,params,focal,gain,dimension,dimension,kernel,128,128,
        [&](float minZ,float maxZ){return farDepth?minZ:maxZ;});
    equal(cpu.r[0],unsigned(output.visible),"visible");equal(circleCount,unsigned(output.circles.size()),"projected circle count");
    equal(applied,unsigned(output.visible),"applied grid count");
    for(unsigned i=0;i<input.size();++i)equal(memory.read32(particleBase+i*64+56),input[i].particle.flags,"particle phase cleanup");
    for(unsigned i=0;i<output.circles.size();++i){const auto& circle=output.circles[i];const auto at=0xca9bb00+i*20;
        equalFloat(memory.readFloat(at),circle.x,"circle X");equalFloat(memory.readFloat(at+4),circle.y,"circle Y");
        equalFloat(memory.readFloat(at+12),circle.radius,"circle radius");equalFloat(memory.readFloat(at+16),circle.weight,"circle weight");}
    if(output.visible){
        equalFloat(memory.readFloat(display+36),output.width,"display width");equalFloat(memory.readFloat(display+40),output.height,"display height");
        equalFloat(memory.readFloat(scratch),output.right,"right");equalFloat(memory.readFloat(scratch+4),output.top,"top");
        equalFloat(memory.readFloat(scratch+12),output.left,"left");equalFloat(memory.readFloat(scratch+16),output.bottom,"bottom");
        for(unsigned i=0;i<3;++i)equalFloat(memory.readFloat(display+24+i*4),output.position[i],"display translation");
        for(unsigned i=0;i<output.density.size();++i)equalFloat(memory.readFloat(buffer+i*4),output.density[i],"density texel");
    }
    ++cases;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required, with adjacent HOSTFS smoke assets");
    RefMemory memory(argv[1]);const auto kernel=readKernel(argv[1]);unsigned randomState=0xa631718f;
    const auto random=[&](){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;};
    for(unsigned dimension:{16u,20u,32u})for(unsigned preset=0;preset<3;++preset)for(unsigned trial=0;trial<96;++trial){
        const auto params=originalSmokeParameters(preset==2?0:preset,preset==2);
        std::vector<OriginalSmokeDisplayParticle> input;
        const unsigned count=trial%13;
        for(unsigned i=0;i<count;++i){
            OriginalSmokeDisplayParticle item;item.particle.flags=(random()&0xfffffe0fu)|0x110u;
            item.particle.position={float(i)*.125f,9.f,35.f};
            item.cameraPosition={float(int(random()%401)-200)*.125f,float(int(random()%241)-120)*.125f,-float(1+random()%80)*.5f};
            item.particle.size=float(1+random()%256)*.0078125f;item.particle.remaining=random()%26;
            if(trial%9==0)item.particle.flags&=~0xf0u;
            if(trial%11==0)item.particle.flags&=~0x100u;
            if(trial%7==0&&i%2==0)item.inFrustum=false;
            if(trial%13==0)item.cameraPosition[2]=-item.cameraPosition[2];
            if(trial%17==0)item.particle.size=0;
            if(trial%19==0)item.cameraPosition={.125f,.25f,-.03125f}; // too close / radius limit
            input.push_back(item);
        }
        runCase(memory,kernel,input,params,trial%2?320.f:240.f,trial%4==0?0.f:trial%4==1?.2f:1.f,dimension,trial%2!=0);
    }
    std::cout<<"PASS "<<cases<<" original smoke display cases, "<<checks<<" bitwise comparisons, "<<instructions
        <<" original instructions. Source128x128 kernel;16/20/32 grids; all parameter presets; expiry; empty/overlapping/behind/near/offscreen particles; clipping, density and placement. Hooks: camera transform/stack, sphere visibility, footprint depth and final model-UV application. Full render submission is not covered.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
