#include "renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
unsigned checks=0;
void check(bool okay,const char* what){++checks;if(!okay)throw std::runtime_error(what);}
void pixel(std::uint32_t actual,Color wanted){
    const std::array<float,4> components{wanted.b,wanted.g,wanted.r,wanted.a};
    for(unsigned i=0;i<4;++i){++checks;
        if(std::abs(int((actual>>(8*i))&255)-int(std::lround(std::clamp(components[i],0.f,1.f)*255)))>1)
            throw std::runtime_error("Course GLM pixel channel "+std::to_string(i)+" actual "+std::to_string((actual>>(8*i))&255)+" expected "+std::to_string(components[i]*255));
    }
}
Mesh panel(){
    Mesh m;m.quad({-20,-20,-10},{20,-20,-10},{20,20,-10},{-20,20,-10},{.2f,.4f,.6f,.6f});
    for(auto& r:m.ranges){r.original=true;r.courseLighting=true;r.pcw=2;r.tsp=0x20900000;r.isp=0xc0000000;}
    return m;
}
float colorByte(float v,float scale){return float(unsigned(v*scale)&255)/255.f;}
}
int main()try{
    constexpr unsigned w=320,h=240;Renderer renderer;check(renderer.initialize(nullptr,w,h,true),renderer.error.c_str());
    const auto path=std::filesystem::temp_directory_path()/("idas3-course-glm-"+std::to_string(GetCurrentProcessId())+".bmp");
    auto capture=[&](const Mesh& mesh,Vec3 eye=Vec3{},const OriginalRearViewFrame* rear=nullptr,const OriginalShowroomLighting* showroom=nullptr,bool night=false){
        check(renderer.draw(mesh,eye,eye+Vec3{0,0,-1},night,false,nullptr,false,showroom,rear),renderer.error.c_str());
        check(renderer.saveBitmap(path.wstring()),renderer.error.c_str());
        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER head{};in.read(reinterpret_cast<char*>(&head),sizeof(head));in.seekg(head.bfOffBits);
        std::vector<std::uint32_t> pixels(w*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Course GLM readback failed");return pixels;
    };
    auto mesh=panel();
    // Isolate all36 original ambient records, including byte quantization.
    // Disabled descriptors must not remain in either original light mask.
    for(unsigned course=0;course<9;++course)for(bool night:{false,true})for(bool wet:{false,true}){
        auto state=original::originalCourseLighting(course,night,wet);for(auto& light:state.lights)light.enabled=false;
        renderer.courseLighting=&state;const auto pixels=capture(mesh,{},nullptr,nullptr,night);
        const Color expected{.2f*colorByte(state.ambient[0],255),.4f*colorByte(state.ambient[1],255),.6f*colorByte(state.ambient[2],255),.6f};
        for(unsigned y:{24u,120u,215u})for(unsigned x:{20u,160u,299u})pixel(pixels[y*w+x],expected);
    }
    original::OriginalCourseLighting state;state.ambient={.2f,.3f,.1f};state.count=1;
    auto& light=state.lights[0];light.kind=original::OriginalCourseLightKind::Parallel;light.incomingDirection={0,0,-1};light.color={.4f,.25f,.1f};
    renderer.courseLighting=&state;
    const Color expected{.2f*(colorByte(.2f,255)+2*colorByte(.4f,127)),.4f*(colorByte(.3f,255)+2*colorByte(.25f,127)),.6f*(colorByte(.1f,255)+2*colorByte(.1f,127)),.6f};
    const auto lit=capture(mesh);pixel(lit[120*w+160],expected);
    check(lit==capture(mesh),"Repaint consumed course light state");
    // Flat lighting must come from the final SOURCE vertex, not D3D's first
    // vertex and not a post-interpolation normal at the pixel.
    auto flat=mesh;flat.ranges[0].pcw=0;
    for(unsigned i=0;i<flat.vertices.size();++i)flat.vertices[i].normal=i%3==2?Vec3{0,0,1}:Vec3{1,0,0};
    const auto flatPixels=capture(flat);for(unsigned y:{30u,120u,210u})for(unsigned x:{30u,160u,290u})pixel(flatPixels[y*w+x],expected);
    flat.ranges[0].pcw=2;check(capture(flat)!=flatPixels,"Gouraud lighting incorrectly flattened");
    auto unlit=mesh;unlit.ranges[0].gmp=512;
    for(auto& v:unlit.vertices)v.offsetColor={.05f,.03f,.01f,.1f};
    pixel(capture(unlit,{},nullptr,nullptr,true)[120*w+160],{.25f,.43f,.61f,.7f});
    // Material alpha-ignore follows vertex addition for untextured forms.
    unlit.ranges[0].tsp&=~(1u<<20);pixel(capture(unlit)[120*w+160],{.25f,.43f,.61f,1});
    auto car=mesh;for(auto& r:car.ranges)r.courseLighting=false;
    const auto carWithCourseState=capture(car);renderer.courseLighting=nullptr;
    check(carWithCourseState==capture(car),"Course ARRAY GLM leaked into independent car scope");
    renderer.courseLighting=&state;OriginalShowroomLighting showroom;
    const auto showroomWithCourseState=capture(mesh,{},nullptr,&showroom);renderer.courseLighting=nullptr;
    check(showroomWithCourseState==capture(mesh,{},nullptr,&showroom),"Course GLM leaked into showroom");
    renderer.courseLighting=&state;OriginalRearViewFrame rear;rear.eye={0,0,1};rear.target={0,0,-1};rear.up={0,1,0};
    pixel(capture(mesh,{},&rear)[32*w+160],expected);
    // Translate eye, geometry and point light together. Its source view-space
    // packet and illuminated surface must remain identical in both views.
    state=original::originalCourseLighting(0,true,false);state.count=1;
    state.lights[0].position={0,0,-8};state.lights[0].incomingDirection={0,0,-1};
    auto pointPanel=panel();for(auto& v:pointPanel.vertices){v.position.x*=.02f;v.position.y*=.02f;}
    const auto point=capture(pointPanel);const Vec3 shift{137,-28,51};
    for(auto& v:pointPanel.vertices)v.position+=shift;
    for(unsigned i=0;i<3;++i)state.lights[0].position[i]+=i==0?shift.x:i==1?shift.y:shift.z;
    const auto translated=capture(pointPanel,shift);
    for(unsigned y=115;y<125;++y)for(unsigned x=155;x<165;++x){
        const auto a=point[y*w+x],b=translated[y*w+x];
        for(unsigned channel=0;channel<4;++channel)check(std::abs(int((a>>(8*channel))&255)-int((b>>(8*channel))&255))<=1,"Point light view-space translation mismatch");
    }
    // Identical adjacent materials cannot coalesce across light ownership.
    Mesh joined;joined.append(mesh);joined.append(car);check(joined.ranges.size()==2,"Append merged course/car light scopes");
    check(joined.ranges[0].courseLighting&&!joined.ranges[1].courseLighting,"Append lost course light ownership");
    // Source course GLM cannot inherit native beams or street/ambient light.
    // Original headlights are a separate projected-geometry submission.
    state={};state.ambient={.1f,.1f,.1f};renderer.courseLighting=&state;
    renderer.vehiclePosition={0,0,0};renderer.vehicleForward={0,0,-1};renderer.vehicleLights=false;
    renderer.opponentLights=false;
    const auto noBeam=capture(mesh,{},nullptr,nullptr,true);
    renderer.vehicleLights=true;
    const auto playerBeam=capture(mesh,{},nullptr,nullptr,true);
    check(playerBeam==noBeam,"Native player beam double-lit source course GLM");
    renderer.vehicleLights=false;renderer.opponentLights=true;renderer.opponentPosition={};renderer.opponentForward={0,0,-1};
    check(capture(mesh,{},nullptr,nullptr,true)==noBeam,"Native opponent beam double-lit source course GLM");
    renderer.opponentLights=false;renderer.courseLampPositions={{0,3,-10}};
    check(capture(mesh,{},nullptr,nullptr,true)==noBeam,"Native street pools double-lit source course GLM");
    renderer.vehicleLights=true;
    const auto noLightBeam=capture(unlit,{},nullptr,nullptr,true);renderer.vehicleLights=false;
    check(capture(unlit,{},nullptr,nullptr,true)==noLightBeam,"Native beam bypassed source noLight material");
    auto fog=original::originalCourseFog(3,false,false);renderer.courseFog=&fog;
    auto replaced=mesh;replaced.ranges[0].tsp=(replaced.ranges[0].tsp&~(3u<<22))|(3u<<22);
    const auto fogBase=capture(replaced,{},nullptr,nullptr,true);renderer.vehicleLights=true;
    check(capture(replaced,{},nullptr,nullptr,true)==fogBase,"Beam reintroduced fog-mode3 discarded base material");
    renderer.courseFog=nullptr;
    std::filesystem::remove(path);
    std::cout<<"PASS "<<checks<<" course GLM framebuffer checks:36 ambient states,source color127,per-vertex flat/Gouraud,no-light and alpha,car/showroom isolation,rear view,point translation,repaint and range boundaries.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
