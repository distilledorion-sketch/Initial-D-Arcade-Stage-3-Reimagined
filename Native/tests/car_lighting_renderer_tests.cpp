// CPU color oracle independently checked against unchanged primary DX11 ELAN
// computeColors by the existing reference fixture. Its entrypoint is not run.
#ifdef NOMINMAX
#undef NOMINMAX
#endif
#define main includedElanPrimaryColorFixture
#include "../tools/check_elan_lighting_reference.cpp"
#undef main
#include "renderer.h"
#include "original_car_lighting.h"
#include <bit>
using namespace idas3;
namespace {
unsigned checks=0;
void check(bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);}
F4 rgba(unsigned word){return {float((word>>16)&255)/255,float((word>>8)&255)/255,float(word&255)/255,float(word>>24)/255};}
Lights primaryLights(const original::OriginalCourseLightingPacket&packet){
    Lights result;result.ambientBase[0]=rgba(packet.glm[3]);result.ambientOffset[0]=rgba(packet.glm[4]);
    result.ambientMaterial.x=(packet.glm[1]>>5)&1;result.ambientMaterial.z=(packet.glm[1]>>6)&1;result.overflow=(packet.glm[1]>>9)&1;
    for(unsigned id=0;id<16;++id){const auto masks=packet.glm[2];if(!(masks&(0x10001u<<id)))continue;
        const std::array<unsigned,8>*raw=nullptr;for(unsigned i=0;i<packet.count;++i)if((packet.lights[i][1]&15)==id)raw=&packet.lights[i];
        check(raw!=nullptr,"Primary light ID missing from active packet");const auto&p=*raw;auto&light=result.light[result.count++];
        light.color=rgba(p[1]>>8);light.routing=(p[2]>>24)&15;light.masks={int((masks>>id)&1),0,int((masks>>(16+id))&1),0};
        const auto direction=[&](unsigned high,unsigned low){const int coarse=std::int8_t((p[2]>>high)&255);return -float(coarse*16+int((p[0]>>low)&15))/2047.f;};
        light.direction={direction(16,16),direction(8,4),direction(0,0),0};
        light.position={std::bit_cast<float>(p[3]),std::bit_cast<float>(p[4]),std::bit_cast<float>(p[5]),0};
        const bool parallel=p[0]&(1u<<20);light.parallel=parallel||(p[3]==0&&p[4]==0&&p[5]==0&&p[6]==0&&p[7]==0);
        light.dmode=parallel?(p[2]>>28)&3:(p[1]>>5)&7;light.smode=parallel?0:(p[2]>>28)&3;
        light.distanceA=std::bit_cast<float>((p[6]&65535)<<16);light.distanceB=std::bit_cast<float>(p[6]&0xffff0000);
        light.angleA=std::bit_cast<float>((p[7]&65535)<<16);light.angleB=std::bit_cast<float>(p[7]&0xffff0000);light.distanceMode.x=p[2]>>31;
    }return result;
}
original::OriginalLightMatrix viewMatrix(Vec3 eye,bool rear){
    // Both cameras look toward world-Z with world+Y up. Main is RH, rear LH.
    // Independent world-to-view transforms, not Renderer matrix getters.
    if(rear)return {-1,0,0,0,0,1,0,0,0,0,-1,0,eye.x,-eye.y,eye.z,1};
    return {1,0,0,0,0,1,0,0,0,0,1,0,-eye.x,-eye.y,-eye.z,1};
}
Color oracle(const original::OriginalCourseLighting&set,Vec3 position,Vec3 normal,Color base,Color offset,bool noLight,bool textured,Vec3 eye={},bool rear=false){
    const auto lights=primaryLights(original::originalCourseLightingPacket(set,viewMatrix(eye,rear)));
    Input input;input.base={base.r,base.g,base.b,base.a};input.offset={offset.r,offset.g,offset.b,offset.a};
    const float sign=rear?-1.f:1.f;
    input.position={sign*(position.x-eye.x),position.y-eye.y,sign*(position.z-eye.z),1};input.normal={sign*normal.x,normal.y,sign*normal.z,0};
    Poly poly;poly.gloss.x=OriginalShowroomLighting::glossCoefficient(0x60);poly.constant.x=noLight;
    const auto colors=::expected(lights,poly,input);Color result;
    if(textured){result={colors[0].x*64.f/255+colors[1].x,colors[0].y*128.f/255+colors[1].y,colors[0].z*192.f/255+colors[1].z,1};}
    else result={colors[0].x+colors[1].x,colors[0].y+colors[1].y,colors[0].z+colors[1].z,colors[0].w+colors[1].w};
    return result;
}
void pixel(unsigned actual,Color wanted){const std::array<float,4> channels{wanted.b,wanted.g,wanted.r,wanted.a};
    for(unsigned i=0;i<4;++i){++checks;const auto expected=int(std::lround(std::clamp(channels[i],0.f,1.f)*255));
        if(std::abs(int((actual>>(8*i))&255)-expected)>1)throw std::runtime_error("Car-light framebuffer channel "+std::to_string(i)+" actual "+std::to_string((actual>>(8*i))&255)+" expected "+std::to_string(expected));}
}
Mesh panel(float left,float right,unsigned car,bool course=false){
    Mesh mesh;mesh.quad({left,-20,-10},{right,-20,-10},{right,20,-10},{left,20,-10},{.2f,.4f,.6f,.6f});
    for(auto&r:mesh.ranges){r.original=true;r.courseLighting=course;r.carLighting=car;r.pcw=2;r.tsp=0x20900000;r.isp=0xc0000000;r.gloss=0x60;}
    return mesh;
}
original::OriginalCourseLighting lightSet(original::OriginalLightVector ambient,original::OriginalLightVector color){
    original::OriginalCourseLighting set;set.ambient=ambient;set.count=1;set.lights[0].kind=original::OriginalCourseLightKind::Parallel;
    set.lights[0].incomingDirection={0,0,-1};set.lights[0].color=color;return set;
}
}
int main()try{
    const auto folder=std::filesystem::temp_directory_path()/("idas3-car-light-scope-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(folder);
    const auto image=folder/"frame.bmp",texture=folder/"constant.idastex";
    {std::ofstream out(texture,std::ios::binary);out.write("IDAS3T1\0",8);const std::array<unsigned,7> words{1,1,0,1,1,4,0x20c08040};out.write(reinterpret_cast<const char*>(words.data()),sizeof(words));}
    constexpr unsigned width=320,height=240;Renderer renderer;check(renderer.initialize(nullptr,width,height,true),renderer.error.c_str());
    check(renderer.loadTextures(NativeTextureBank::load(texture)),renderer.error.c_str());
    auto course=lightSet({.1f,.2f,.3f},{.05f,.08f,.2f}),player=lightSet({.3f,.05f,.1f},{.3f,.1f,.05f}),rival=lightSet({.05f,.3f,.1f},{.05f,.25f,.05f});
    renderer.courseLighting=&course;renderer.playerLighting=&player;renderer.rivalLighting=&rival;
    renderer.vehicleLights=renderer.opponentLights=false;
    auto fog=original::originalBootstrapFog();fog.colorRgb=0xff007f;fog.table.fill(0xffff);renderer.courseFog=&fog;
    auto capture=[&](const Mesh&mesh,bool night=false,const OriginalRearViewFrame*rear=nullptr,const OriginalShowroomLighting*showroom=nullptr,Vec3 eye={}){
        check(renderer.draw(mesh,eye,eye+Vec3{0,0,-1},night,false,nullptr,false,showroom,rear),renderer.error.c_str());
        check(renderer.saveBitmap(image.wstring()),renderer.error.c_str());std::ifstream in(image,std::ios::binary);BITMAPFILEHEADER head{};in.read(reinterpret_cast<char*>(&head),sizeof(head));in.seekg(head.bfOffBits);
        std::vector<unsigned> pixels(width*height);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Car-light framebuffer readback");return pixels;
    };
    const Color base{.2f,.4f,.6f,.6f},zero{0,0,0,0};OriginalRearViewFrame rear;rear.eye={};rear.target={0,0,-1};rear.up={0,1,0};
    // ARRAY+88 gain scales each source light before color127 packing; the
    // ambient RGB255 is independent. This direct arithmetic oracle does not
    // use the production descriptor encoder.
    for(float gain:{0.f,.25f,.64f,1.f})for(unsigned owner:{1u,2u}){
        auto&set=owner==1?player:rival;set.gain=gain;const auto frame=capture(panel(-20,20,owner),true,&rear);
        Color expected=base;for(unsigned c=0;c<3;++c){const float ambient=float(unsigned(set.ambient[c]*255))/255.f;
            const float light=float(unsigned((set.lights[0].color[c]*gain)*127))/255.f;
            (&expected.r)[c]=(&base.r)[c]*(ambient+2*light);}
        pixel(frame[120*width+160],expected);pixel(frame[32*width+160],expected);
        set.gain=1.f;
    }
    // The three owners deliberately produce different colors in one draw,
    // protecting both per-range packet switching and the rear-view reload.
    auto combined=panel(-4,-1.4f,1);combined.append(panel(-1.2f,1.2f,0,true));combined.append(panel(1.4f,4,2));
    check(combined.ranges.size()==3,"Distinct light owners merged before submission");
    for(bool night:{false,true}){
        const auto frame=capture(combined,night,&rear);
        for(unsigned y:{70u,120u,180u}){
            pixel(frame[y*width+100],oracle(player,{-2.6f,0,-10},{0,0,1},base,zero,false,false));
            pixel(frame[y*width+160],oracle(course,{0,0,-10},{0,0,1},base,zero,false,false));
            pixel(frame[y*width+220],oracle(rival,{2.6f,0,-10},{0,0,1},base,zero,false,false));
        }
        // Rear's original LH view reverses world X relative to the main view.
        pixel(frame[32*width+198],oracle(player,{-2.6f,0,-10},{0,0,1},base,zero,false,false,{},true));
        pixel(frame[32*width+160],oracle(course,{0,0,-10},{0,0,1},base,zero,false,false,{},true));
        pixel(frame[32*width+122],oracle(rival,{2.6f,0,-10},{0,0,1},base,zero,false,false,{},true));
        check(capture(combined,night,&rear)==frame,"Repeated car main/rear draw changed pixels");
    }
    // Source final-vertex flat lighting, with colored specular added after
    // a nonwhite texture. Constant per-triangle color permits an exact pixel
    // oracle without reproducing the renderer's interpolation implementation.
    for(unsigned owner:{1u,2u})for(bool textured:{false,true})for(bool noLight:{false,true}){
        Mesh mesh;mesh.triangle({-2,-2,-10},{2,-2,-10},{0,2,-10},base);
        auto&r=mesh.ranges[0];r.original=true;r.carLighting=owner;r.pcw=textured?12:0;r.gmp=noLight?512:0;r.tsp=textured?0x20880040:0x20900000;r.isp=0xc0000000;r.gloss=0x60;r.texture=textured?0:0xffffffff;
        const Color offset{.2f,.1f,.15f,.1f};for(unsigned i=0;i<3;++i){mesh.vertices[i].normal=i==2?Vec3{0,0,1}:Vec3{1,0,0};mesh.vertices[i].offsetColor=offset;mesh.vertices[i].u=mesh.vertices[i].v=.5f;}
        const auto expected=oracle(owner==1?player:rival,{0,2,-10},{0,0,1},base,offset,noLight,textured);
        pixel(capture(mesh)[120*width+160],expected);pixel(capture(mesh,true)[120*width+160],expected);
        if(!noLight){auto gouraud=mesh;gouraud.ranges[0].pcw|=2;check(capture(gouraud)!=capture(mesh),"Car Gouraud lighting was incorrectly flattened");}
    }
    // The source registers player SPOT in course+rival ARRAYs and rival
    // SPOT in course+player ARRAYs. Supply deliberately distinct descriptors
    // so accidental use of a common packet cannot pass by matching colors.
    auto makeSpot=[](original::OriginalLightVector position,original::OriginalLightVector color){
        original::OriginalCourseLight light;light.kind=original::OriginalCourseLightKind::Spot;light.position=position;
        light.incomingDirection={0,0,-1};light.color=color;light.coefficientWords[0]=0x40000000; // inverse-distance: min(2/d,1)
        light.angle0=1;light.angle1=2;light.angleCosines={.98f,.5f};return light;
    };
    const auto playerSpot=makeSpot({2,3,-5},{.32f,.12f,.02f}),rivalSpot=makeSpot({-3,1,-4},{.02f,.2f,.4f});
    const auto courseBase=course,playerBase=player,rivalBase=rival;
    course.lights[course.count++]=playerSpot;course.lights[course.count++]=rivalSpot;
    player.lights[player.count++]=rivalSpot;rival.lights[rival.count++]=playerSpot;
    const Vec3 rearEye{.2f,.1f,2};rear.eye=rearEye;rear.target=rearEye+Vec3{0,0,-1};
    for(unsigned owner:{0u,1u,2u}){
        Mesh mesh;mesh.triangle({-8,-8,-10},{8,-8,-10},{0,8,-10},base);
        auto&r=mesh.ranges[0];r.original=true;r.courseLighting=owner==0;r.carLighting=owner;r.pcw=0;r.tsp=0x20900000;r.isp=0xc0000000;r.gloss=0x60;
        const Color offset{.2f,.1f,.15f,.1f};for(auto&v:mesh.vertices){v.normal={0,0,1};v.offsetColor=offset;}
        const auto&set=owner==0?course:owner==1?player:rival;
        const auto lit=capture(mesh,true,&rear);
        pixel(lit[120*width+160],oracle(set,{0,8,-10},{0,0,1},base,offset,false,false));
        pixel(lit[32*width+160],oracle(set,{0,8,-10},{0,0,1},base,offset,false,false,rearEye,true));
        auto sourceNoLight=mesh;sourceNoLight.ranges[0].gmp=512;
        const auto unlit=capture(sourceNoLight,true,&rear);
        pixel(unlit[120*width+160],{.4f,.5f,.75f,.7f});pixel(unlit[32*width+160],{.4f,.5f,.75f,.7f});
        // Extra native beams/street pools must not run after the original
        // color calculation, even when source car noLight is selected.
        renderer.vehicleLights=renderer.opponentLights=true;renderer.courseLampPositions={{0,3,-10}};
        check(capture(mesh,true,&rear)==lit&&capture(sourceNoLight,true,&rear)==unlit,"Native beam/street light double-lit source car ARRAY");
        renderer.vehicleLights=renderer.opponentLights=false;renderer.courseLampPositions.clear();
        auto replaced=mesh;replaced.ranges[0].tsp=(replaced.ranges[0].tsp&~(3u<<22))|(3u<<22);
        const auto replacedWithSpots=capture(replaced,true,&rear);
        course=courseBase;player=playerBase;rival=rivalBase;
        check(capture(replaced,true,&rear)==replacedWithSpots,"Source fog-mode3 discarded base retained a car light contribution");
        check(capture(mesh,true,&rear)!=lit,"Opposite-car SPOT did not affect scoped surface");
        course.lights[course.count++]=playerSpot;course.lights[course.count++]=rivalSpot;
        player.lights[player.count++]=rivalSpot;rival.lights[rival.count++]=playerSpot;
    }
    course=courseBase;player=playerBase;rival=rivalBase;
    rear.eye={};rear.target={0,0,-1};
    // Happo's borrowed Point light is omnidirectional: authored inverse
    // distance coefficients are -14 + 420/d, transitioning at28..30 metres.
    // Compare arithmetic pixels independently of packet generation, including
    // gain and the zero/full boundaries. No direction scratch history assumed.
    const auto authoredPoint=original::originalHappoCarPointLight();
    check(authoredPoint.position==original::OriginalLightVector{-1085.5f,947.5f,-249.5f}&&authoredPoint.coefficientWords[0]==0x43d2c160,
        "Happo authored Point source binding changed");
    for(unsigned owner:{1u,2u})for(float distance:{14.f,28.f,29.f,30.f,45.f})for(float gain:{.64f,1.f}){
        auto&set=owner==1?player:rival;const auto saved=set;set={};set.count=1;set.gain=gain;set.ambient={.1f,.2f,.3f};
        set.lights[0]=authoredPoint;set.lights[0].position={0,2,-10+distance};
        Mesh mesh;mesh.triangle({-20,-20,-10},{20,-20,-10},{0,2,-10},base);
        auto&r=mesh.ranges[0];r.original=true;r.carLighting=owner;r.pcw=0;r.tsp=0x20900000;r.isp=0xc0000000;
        for(auto&v:mesh.vertices)v.normal={0,0,1};
        const auto frame=capture(mesh,true,&rear);Color expected=base;
        const float attenuation=std::clamp(420.f/distance-14.f,0.f,1.f);
        for(unsigned channel=0;channel<3;++channel){const float ambient=float(unsigned(set.ambient[channel]*255))/255;
            const float packed=float(unsigned((set.lights[0].color[channel]*gain)*127))/255;
            (&expected.r)[channel]=(&base.r)[channel]*(ambient+2*packed*attenuation);}
        pixel(frame[120*width+160],expected);pixel(frame[32*width+160],expected);
        set=saved;
    }
    for(unsigned owner:{1u,2u}){
        auto mesh=panel(-20,20,owner);mesh.ranges[0].viewMask=0;const auto absent=capture(mesh,true,&rear);
        mesh.ranges[0].viewMask=3;const auto both=capture(mesh,true,&rear);
        for(unsigned mask:{1u,2u,3u}){mesh.ranges[0].viewMask=mask;const auto frame=capture(mesh,true,&rear);
            check(frame[120*width+160]==((mask&1)?both:absent)[120*width+160],"Car main-view visibility ignored range mask");
            check(frame[32*width+160]==((mask&2)?both:absent)[32*width+160],"Car mirror visibility ignored range mask");
        }
    }
    // Unscoped menu geometry and projected noLight materials cannot inherit
    // either car packet. Showroom remains its explicitly separate light path.
    auto unscoped=panel(-20,20,0),projected=unscoped;projected.ranges[0].pcw=0x8a00071e;projected.ranges[0].tsp=0x4489a464;projected.ranges[0].isp=0x93800000;projected.ranges[0].gmp=0x222;projected.ranges[0].emissive=true;projected.ranges[0].texture=0;
    const auto menu=capture(unscoped),projection=capture(projected,true);OriginalShowroomLighting showroom;const auto preview=capture(unscoped,false,nullptr,&showroom);
    renderer.playerLighting=renderer.rivalLighting=nullptr;
    check(capture(unscoped)==menu&&capture(projected,true)==projection,"Car lightsets leaked into unscoped menu/projector ranges");
    check(capture(unscoped,false,nullptr,&showroom)==preview,"Car lightsets leaked into showroom lighting");
    auto missing=panel(-20,20,1);const auto missingPlayer=capture(missing);missing.ranges[0].carLighting=2;
    check(capture(missing)==missingPlayer,"Absent car lightsets have inconsistent fallbacks");
    renderer.playerLighting=&player;renderer.rivalLighting=&rival;
    auto merged=panel(-20,20,1),other=merged;other.ranges[0].carLighting=2;merged.append(other);
    check(merged.ranges.size()==2&&merged.ranges[0].carLighting==1&&merged.ranges[1].carLighting==2,"Mesh append coalesced player/rival ownership");
    auto same=panel(-20,20,1);same.append(panel(-20,20,1));check(same.ranges.size()==1,"Same-owner material unexpectedly split");
    same.ranges[0].carLighting=3;check(!renderer.draw(same,{},Vec3{0,0,-1},false,false)&&renderer.error=="Invalid car light scope","Out-of-range car owner was accepted");
    std::filesystem::remove(image);std::filesystem::remove(texture);std::filesystem::remove(folder);
    std::cout<<"PASS "<<checks<<" per-car lighting framebuffer checks: independent primary color oracle, distinct course/player/rival packets with opposite-car SPOT, day/night, main/rear and visibility masks, flat/Gouraud and textured specular, noLight, fog-mode3, native beam exclusion, unscoped menu/projector/showroom, null fallback and range coalescing. WARP only.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
