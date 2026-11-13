#include "original_car_lighting_reference.h"
#include "original_lens_flare.h"
#include "original_ending_camera.h"
#include <random>
using namespace idas3::reference;
using namespace idas3::original;
namespace {
constexpr unsigned owner=0xd030000,refs=owner+0x1000,actor=owner+0x2000;
unsigned checks=0,cases=0;
void equal(unsigned expected,unsigned actual,const char* why){
    ++checks;if(expected!=actual){std::cerr<<why<<" case="<<cases<<" expected="<<hex(expected)<<" actual="<<hex(actual)<<'\n';
        throw std::runtime_error("Original ending flare mismatch");}
}
void sameFloat(float expected,float actual,const char* why){equal(std::bit_cast<unsigned>(expected),std::bit_cast<unsigned>(actual),why);}
}
int main(int argc,char**argv)try{
    if(argc!=4)throw std::runtime_error("Canonical image, FSCA table and environment data required");
    CarLightReference r(argv[1],argv[2]);
    const auto trig=OriginalFscaTable::load(argv[2]);
    // Run the constructor's actual anchor/FOV arithmetic, including its
    // software-double routines. No host arithmetic substitution is hooked.
    r.c.r[14]=CarLightReference::stack-0x200;r.c.r[15]=r.c.r[14];
    r.m.write32(r.c.r[14]+0xa0,owner);
    r.run(0xc180640,0xc1806ba);
    sameFloat(originalLensFlareAnchorScale(),r.m.readFloat(owner+0xd4),"Original constructor anchor scale");
    for(unsigned axis=0;axis<3;++axis)
        sameFloat(originalLensFlareAnchor[axis],r.m.readFloat(owner+0xc8+axis*4),"Original anchor");
    std::ifstream file(argv[3],std::ios::binary);std::array<char,8> magic;
    file.read(magic.data(),8);if(std::string(magic.data(),8)!="IDASENV1")throw std::runtime_error("Environment data identity");
    std::array<std::array<float,3>,8> suns;file.read(reinterpret_cast<char*>(suns.data()),sizeof(suns));
    std::array<OriginalLensFlarePiece,12> pieces;static_assert(sizeof(pieces)==12*12);
    file.read(reinterpret_cast<char*>(pieces.data()),sizeof(pieces));
    for(unsigned i=0;i<12;++i){
        sameFloat(pieces[i].distance,r.m.readFloat(0xc28e5c0+i*12),"Exported piece distance");
        sameFloat(pieces[i].size,r.m.readFloat(0xc28e5c4+i*12),"Exported piece size");
        equal(pieces[i].chunk,r.m.read32(0xc28e5c8+i*12),"Exported piece chunk");
    }
    std::array<std::vector<std::uint8_t>,8> tables;
    for(auto& table:tables){unsigned count=0;file.read(reinterpret_cast<char*>(&count),4);
        if(count==0||count>10000)throw std::runtime_error("Environment table length");table.resize(count);file.read(reinterpret_cast<char*>(table.data()),count);}
    if(!file)throw std::runtime_error("Incomplete environment data");
    r.m.write32(owner+32,refs);r.m.write32(refs,actor);
    unsigned visible=0,hidden=0;
    for(unsigned course=0;course<8;++course){
        const auto pointer=r.m.read32(0xc337358+course*4),last=r.m.read32(0xc337378+course*4);
        equal(last+1,unsigned(tables[course].size()),"Inclusive final source segment");
        for(unsigned axis=0;axis<3;++axis)sameFloat(suns[course][axis],r.m.readFloat(0xc28e650+course*12+axis*4),"Exported source sun");
        r.m.write32(owner+0xc4,course);
        for(bool reverse:{false,true})for(unsigned segment=0;segment<=last;++segment){
            equal(tables[course][segment],r.m.read32(pointer+segment*4),"Exported course visibility");
            r.m.write32(actor+0x9dc,segment);r.m.writeFloat(actor+0x9e0,segment%2?.999f:0.f);r.m.write32(actor+0xae0,reverse);
            r.c.r[4]=owner;r.c.r[15]=CarLightReference::stack;r.run(0xc180b00);
            const bool expected=originalLensFlareRoadVisible(tables[course],segment,reverse);
            equal(expected,r.c.r[0],"Road/reverse visibility");visible+=expected;hidden+=!expected;++cases;
        }
        equal(0,originalLensFlareRoadVisible(tables[course],-1,false),"Reject invalid negative segment");
        equal(0,originalLensFlareRoadVisible(tables[course],last+1,true),"Reject invalid final segment");
    }
    // 1FA9E0 updates text-cursor globals, not model blend/depth overrides.
    // Let it execute along with the owner instead of treating it as rendering.
    r.m.zeroRegion(0xc985f00,0x300);
    std::mt19937 random(0x180c60);unsigned facing=0,away=0;
    for(unsigned sample=0;sample<1600;++sample){
        const unsigned course=sample%8;const auto& sun=suns[course];
        const std::array<float,3> eye{float(int(random()%8001)-4000),float(int(random()%1001)-500),float(int(random()%8001)-4000)};
        auto target=eye;for(unsigned axis=0;axis<3;++axis)target[axis]+=float(int(random()%2001)-1000);
        if(sample%3==0)target={eye[0]+sun[0],sun[1],eye[2]+sun[2]};
        const auto view=originalCameraWorldToView(originalLookAtCameraWorld(eye,target));
        for(unsigned i=0;i<16;++i)r.c.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
        for(unsigned i=0;i<3;++i)r.m.writeFloat(owner+0xa8+4*i,sun[i]);
        r.c.r[4]=owner;r.c.r[15]=CarLightReference::stack;r.run(0xc180c60,0xc180cda);
        const auto expected=originalLensFlareDirection(view,sun);
        for(unsigned i=0;i<3;++i)sameFloat(expected[i],r.m.readFloat(r.c.r[14]+4*i),"Original camera-relative sun direction");
        const unsigned segment=random()%tables[course].size();const bool reverse=bool(sample&1);
        r.m.write32(owner+0xc4,course);r.m.write32(actor+0x9dc,segment);r.m.write32(actor+0xae0,reverse);
        const bool admitted=originalLensFlareFacing(expected)&&originalLensFlareRoadVisible(tables[course],segment,reverse);
        r.run(0xc180cda,admitted?0xc180cf6:0xc180e24);
        if(admitted){r.run(0xc180cf6,0xc180d20);sameFloat(originalLensFlareStrength(expected),r.c.getFloat(12),"Source flare opacity");++facing;
            r.run(0xc180d20,0xc180d36);
            const auto axis=originalLensFlareAxis(expected);
            for(unsigned i=8;i<16;++i)sameFloat(axis.elements[i],std::bit_cast<float>(r.c.xf[i]),"Original helper used axis/translation columns");
        }
        else ++away;
        //Restore the source stack after deliberately stopping inside the draw.
        r.m.write16(0xc98ad0c,0);r.m.write32(0xc98ad14,0xce00000);++cases;
    }
    if(!visible||!hidden||facing<100||away<100)throw std::runtime_error("Insufficient flare admission coverage");
    // Bind the actual source camera/projection and viewport routines. Only
    // the final projection upload crosses the rendering-device boundary.
    r.m.zeroRegion(0xc980000,0x20000);
    for(unsigned j=0;j<4;++j){const unsigned base=0xc98ad0c+j*12,save=0xce00000+j*0x1000;
        r.m.write32(base,0x00200000);r.m.write32(base+4,save);r.m.write32(base+8,save);
        for(unsigned i=0;i<16;++i)r.m.writeFloat(save+i*4,i%5==0?1.f:0.f);
    }
    constexpr unsigned camera=owner+0x6000;
    r.c.r[15]=CarLightReference::stack;r.c.r[4]=camera;r.run(0xc1d0940);
    r.m.write32(camera+152,idas3::OriginalEndingCamera::fieldOfViewPhase());
    r.c.r[4]=camera;r.run(0xc1d09e0);
    OriginalMatrix projection,viewport;
    r.c.callHooks[0xc1d9600]=[&](auto&q){for(unsigned i=0;i<16;++i)projection.elements[i]=std::bit_cast<float>(q.xf[i]);};
    r.c.r[4]=camera;r.run(0xc1d0880);
    r.c.callHooks.erase(0xc1d9600);
    r.c.r[15]=CarLightReference::stack;
    r.c.setFloat(4,0);r.c.setFloat(5,0);r.c.setFloat(6,640);r.c.setFloat(7,480);
    r.run(0xc1fe1a0,0xc1fe252); //Stop before hardware clipping-register setup.
    for(unsigned i=0;i<16;++i){
        viewport.elements[i]=r.m.readFloat(0xc98af1c+i*4);
        sameFloat(projection.elements[i],r.m.readFloat(r.m.read32(0xc98ad0c+44)+i*4),"Source projection stack binding");
        sameFloat(originalEndingLensFlareProjection().elements[i],projection.elements[i],"Ending projection constant follows source camera/FOV");
        sameFloat(originalLensFlareViewport().elements[i],viewport.elements[i],"Source viewport matrix");
    }
    std::cout<<"Projection";for(float v:projection.elements)std::cout<<' '<<hex(std::bit_cast<unsigned>(v));std::cout<<'\n';
    std::cout<<"Viewport";for(float v:viewport.elements)std::cout<<' '<<hex(std::bit_cast<unsigned>(v));std::cout<<'\n';
    // The shipped source bank is uncompressed. Execute the real material
    // iterator against its complete original bytes, including both colors.
    constexpr unsigned raw=owner+0x7000;
    const auto source=std::filesystem::path(argv[1]).parent_path()/"driveA/HOSTFS/model/effect/flare";
    std::ifstream polygons(source/"flare_pol.bin.nz",std::ios::binary),tableFile(source/"flare_pol.tbl",std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(polygons)),{});
    std::array<unsigned,7> offsets{};tableFile.read(reinterpret_cast<char*>(offsets.data()),sizeof(offsets));
    if(bytes.size()!=2016||!tableFile||offsets.back()!=1728)throw std::runtime_error("Original flare polygon identity");
    for(unsigned i=0;i<bytes.size();++i)r.m.write8(raw+i,bytes[i]);
    for(unsigned chunk=0;chunk<7;++chunk)for(unsigned alpha=0;alpha<=204;++alpha){
        const auto material=raw+offsets[chunk]+96;
        const unsigned diffuse=0xa5341289u+chunk,specular=0x19bace07u+chunk;
        r.m.write32(material+12,diffuse);r.m.write32(material+20,specular);
        const float strength=float(alpha)/255.f;
        r.c.r[4]=owner;r.c.r[5]=raw+offsets[chunk];r.c.r[15]=CarLightReference::stack;r.c.setFloat(4,strength);
        r.run(0xc180a60);
        equal(originalLensFlareColor(diffuse,strength),r.m.read32(material+12),"Source diffuse alpha with preserved RGB");
        equal(originalLensFlareColor(specular,strength),r.m.read32(material+20),"Source second diffuse alpha with preserved RGB");
    }
    for(unsigned i=0;i<bytes.size();++i)r.m.write8(raw+i,bytes[i]);
    unsigned submitted=0,drawCases=0;std::array<float,3> currentDirection{};
    r.c.callHooks[0xc05a8e0]=[&](auto&q){if(q.r[5]>=7)throw std::runtime_error("Unknown flare chunk");q.r[0]=raw+offsets[q.r[5]];};
    r.c.callHooks[0xc1d7120]=[&](auto&q){
        const unsigned index=q.r[12];if(index<1||index>11)throw std::runtime_error("Flare loop index");
        const auto& piece=pieces[index];equal(raw+offsets[piece.chunk],q.r[4],"Flare draw order/chunk");
        for(unsigned at:{108u,116u})equal(originalLensFlareColor(0xffffffffu,originalLensFlareStrength(currentDirection)),
            r.m.read32(q.r[4]+at),"Full owner material alpha");
        const auto screen=originalLensFlareScreenPoint(currentDirection,piece.distance,projection,viewport);
        for(unsigned i=0;i<3;++i)sameFloat(screen[i],r.m.readFloat(q.r[14]+88+i*4),"Full source projected flare point");
        const auto transform=originalLensFlarePieceMatrix(screen,index,piece.size,originalLensFlareStrength(currentDirection),trig);
        for(unsigned i=0;i<16;++i)sameFloat(transform.elements[i],std::bit_cast<float>(q.xf[i]),"Full source flare model transform");
        ++submitted;
    };
    for(unsigned sample=0;sample<240;++sample){
        const unsigned course=sample%8;const auto& sun=suns[course];
        const std::array<float,3> eye{float(int(random()%8001)-4000),float(int(random()%1001)-500),float(int(random()%8001)-4000)};
        const std::array<float,3> target{eye[0]+sun[0],sun[1],eye[2]+sun[2]};
        const auto view=originalCameraWorldToView(originalLookAtCameraWorld(eye,target));
        currentDirection=originalLensFlareDirection(view,sun);
        const unsigned segment=random()%tables[course].size();const bool reverse=bool(sample&1);
        r.m.write32(owner+0xc4,course);r.m.write32(actor+0x9dc,segment);r.m.write32(actor+0xae0,reverse);
        for(unsigned i=0;i<3;++i)r.m.writeFloat(owner+0xa8+i*4,sun[i]);
        for(unsigned i=0;i<16;++i)r.c.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
        r.m.write16(0xc98ad0c,0);r.m.write32(0xc98ad14,0xce00000);
        r.c.r[4]=owner;r.c.r[15]=CarLightReference::stack;const unsigned before=submitted;
        r.run(0xc180c60);
        const bool admitted=originalLensFlareFacing(currentDirection)&&originalLensFlareRoadVisible(tables[course],segment,reverse);
        equal(admitted?11:0,submitted-before,"Complete owner draw admission");
        equal(0,r.m.read16(0xc98ad0c),"Flare draw matrix stack balance");
        ++drawCases;++cases;
    }
    if(submitted<400)throw std::runtime_error("Insufficient full flare draw coverage");
    // Trace the real model submission too. Only the seven verified store-queue
    // transfers and final GPU queue are replaced; authored GMP/ICH is retained.
    r.c.callHooks.erase(0xc1d7120);
    constexpr unsigned sq=0xe0010000;
    for(unsigned pc:{0xc1dbff2u,0xc1dc024u,0xc1dc040u,0xc1dc064u,0xc1dc082u,0xc1d80f4u,0xc1d810cu}){
        equal(0x0083,r.m.read16(pc)&0xf0ff,"Verified store-queue transfer");r.m.write16(pc,9);
    }
    r.m.zeroRegion(sq,0x1000);r.m.write32(0xc980264,0x10010000);r.m.write32(0xc989ff0,0);
    r.m.write32(0xc37f110,0);r.m.writeFloat(0xc980234+16,.01f);
    r.m.writeFloat(0xc980234+20,10000.f);r.m.writeFloat(0xc980234+24,1.f);
    unsigned packets=0;
    r.c.callHooks[0xc205640]=[&](auto&q){equal(2,q.r[4],"Flare translucent queue");equal(224,r.m.read32(q.r[5]+4),"Flare model packet size");++packets;};
    for(unsigned chunk=0;chunk<7;++chunk)for(unsigned overrideTsp:{0u,0x00100000u,0x00800000u}){
        const auto pointer=raw+offsets[chunk];const auto authoredTsp=r.m.read32(pointer+168);
        r.m.write32(0xc980264,0x10010000);r.m.write32(0xc989ff4,overrideTsp);const auto before=packets;
        r.c.r[4]=pointer;r.c.r[15]=CarLightReference::stack;r.run(0xc1d7120);
        equal(1,packets-before,"Exactly one flare chunk submission");
        equal(overrideTsp,r.m.read32(sq+168),"Flare forwards modelTSP override verbatim");
        equal(((pointer+96)&0x01fffff8u)|0x80000000u,r.m.read32(sq+176),"Flare original GMP/ICH pointer");
        equal(192,r.m.read32(sq+184),"Flare original GMP/ICH payload bytes");
        equal(authoredTsp,r.m.read32(pointer+168),"Flare submission preserves authored TSP");
        equal(overrideTsp,r.m.read32(0xc989ff4),"Flare submission preserves global TSP");
    }
    std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<r.instructions<<" original instructions; "
        <<visible<<" road-visible / "<<hidden<<" road-hidden / "<<facing<<" camera admissions / "<<away
        <<" camera rejections / "<<drawCases<<" full owner cases / "<<submitted<<" submitted pieces / "<<packets<<" GPU packets. Full road visibility, constructor arithmetic, camera/projection, material iterator/alpha, matrix, draw loop and model submission execute. Resource lookup, verified SQ transfers and final GPU queue are explicit boundaries. Model overrides are varied; every scene's prior global state is not proven.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
