#include "ui.h"
#include "frontend.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace idas3 {
const std::vector<std::uint32_t>& Frontend::paint(int,int){throw std::logic_error("Unexpected frontend in the isolated map test");}
struct CourseMapTestAccess {
    // The geometry the race HUD actually uses, on the source 640x480 canvas.
    static constexpr float x=14,y=352,w=112,h=112;
    static inline int size=0,zoom=2;
    static std::vector<std::uint32_t> render(Hud& hud,const Course& course,int width,int height,
            const VehicleState& car,const VehicleState* rival){
        hud.resize(width,height);hud.setMapSize(size);hud.setMapZoom(zoom);GdiFlush();
        // A nonzero background makes any write outside the frame visible.
        for(std::size_t i=0;i<std::size_t(width)*height;++i)hud.pixels[i]=0xff000000u|unsigned((i*2654435761u)&0xffffffu);
        hud.map(course,x,y,w,h,car,rival);
        GdiFlush();return {hud.pixels,hud.pixels+std::size_t(width)*height};
    }
    // The frame in device pixels, for the containment check.
    // The map anchors to the bottom left of the source canvas, like the rest
    // of the original HUD, not to the development canvas.
    static float fit(const Hud& hud){return std::min(float(hud.width)/640.f,float(hud.height)/480.f);}
    static void frame(const Hud& hud,int& x0,int& y0,int& x1,int& y1){
        const float f=fit(hud),base=float(hud.height)-480.f*f;
        const float edge=112.f*(1.f+.25f*size);
        x0=int(14.f*f);y0=int(base+(464.f-edge)*f);x1=int((14.f+edge)*f)+1;y1=int(base+464.f*f)+1;
    }
    static std::pair<int,int> center(const Hud& hud){
        const float f=fit(hud),base=float(hud.height)-480.f*f;
        const float edge=112.f*(1.f+.25f*size);
        return {int((14.f+edge*.5f)*f),int(base+(464.f-edge*.5f)*f)};
    }
};
}
int main(int argc,char** argv)try{
    using namespace idas3;
    if(argc!=2)throw std::invalid_argument("game-root required");
    const std::filesystem::path root=argv[1];
    Hud hud;hud.loadOriginal(root);
    using A=CourseMapTestAccess;
    unsigned cases=0;
    const auto course=Course::load(root/"data/courses","s_nm","s_nm",false);
    if(course.points.size()<64)throw std::runtime_error("The map test needs a real course");

    const auto carAt=[&](float distance,float yawOffset){
        const auto here=course.sample(distance);
        VehicleState car{};car.position=here.center;
        car.yaw=std::atan2(here.tangent.x,here.tangent.z)+yawOffset;
        return car;
    };
    for(A::zoom=0;A::zoom<3;++A::zoom)
    for(A::size=0;A::size<3;++A::size)
    for(auto [width,height]:{std::pair{640,480},std::pair{1280,720},std::pair{2560,1080},std::pair{960,540}}){
        const auto car=carAt(course.length*.4f,0);
        const auto image=A::render(hud,course,width,height,car,nullptr);
        int x0=0,y0=0,x1=0,y1=0;A::frame(hud,x0,y0,x1,y1);
        // Nothing outside the frame: the map shows a window, not the route.
        for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
            if(xx>=x0&&xx<x1&&yy>=y0&&yy<y1)continue;
            const std::size_t i=std::size_t(yy)*width+xx;
            if(image[i]!=(0xff000000u|unsigned((i*2654435761u)&0xffffffu)))
                throw std::runtime_error("The course map drew outside its frame");
        }
        ++cases;
        // The car is fixed inside the frame, so its indicator is always painted
        // at the same place whatever the world position or heading.
        const auto [cx,cy]=A::center(hud);
        const auto elsewhere=A::render(hud,course,width,height,carAt(course.length*.75f,1.f),nullptr);
        const std::size_t middle=std::size_t(cy)*width+cx;
        // The indicator has to actually be there: a background pixel would
        // make this comparison pass for the wrong reason.
        const auto green=image[middle];
        if(((green>>8)&255)<110||((green>>16)&255)>((green>>8)&255)-30)
            throw std::runtime_error("The player indicator is not at the centre of the frame");
        if(green!=elsewhere[middle])
            throw std::runtime_error("The player indicator moved inside the frame");
        ++cases;
        // Turning the car turns the map.
        const auto turned=A::render(hud,course,width,height,carAt(course.length*.4f,1.2f),nullptr);
        if(turned==image)throw std::runtime_error("The course map did not turn with the car");
        ++cases;
        // Driving on changes what the window shows.
        const auto moved=A::render(hud,course,width,height,carAt(course.length*.4f+90.f,0),nullptr);
        if(moved==image)throw std::runtime_error("The course map did not follow the car");
        ++cases;
        // An opponent inside the window is drawn; one a long way off is not.
        VehicleState close{},distant{};
        close.position=course.sample(course.length*.4f+18.f).center;
        distant.position=course.sample(course.length*.4f+2200.f).center;
        const auto withNear=A::render(hud,course,width,height,car,&close);
        const auto withFar=A::render(hud,course,width,height,car,&distant);
        if(withNear==image)throw std::runtime_error("A nearby opponent left no indicator");
        // An opponent past the window is held at the frame edge rather than
        // vanishing, so it must still be drawn and must land near the border.
        if(withFar==image)throw std::runtime_error("A distant opponent left no indicator");
        int fx0=0,fy0=0,fx1=0,fy1=0;A::frame(hud,fx0,fy0,fx1,fy1);
        int rx=-1,ry=-1;
        for(int yy=fy0;yy<fy1&&rx<0;++yy)for(int xx=fx0;xx<fx1;++xx){
            const auto p=withFar[std::size_t(yy)*width+xx];
            if(((p>>16)&255)>150&&((p>>8)&255)<90&&(p&255)<90){rx=xx;ry=yy;break;}
        }
        if(rx<0)throw std::runtime_error("The held opponent indicator is not inside the frame");
        const int marginX=std::min(rx-fx0,fx1-rx),marginY=std::min(ry-fy0,fy1-ry);
        if(std::min(marginX,marginY)>(fx1-fx0)/4)
            throw std::runtime_error("A far opponent was not held at the frame edge");
        cases+=2;
        // Handedness. The rendered view is right-handed, so what is to the
        // right of the car on screen lies along cross(forward,up), and that is
        // where its indicator has to land. This pins the axis that made the
        // first version come out mirrored.
        {
            const auto ahead=forward(car.yaw);
            const Vec3 screenRight{-std::cos(car.yaw),0,std::sin(car.yaw)};
            VehicleState beside{};beside.position=car.position+screenRight*26.f;
            const auto sideways=A::render(hud,course,width,height,car,&beside);
            int fx0=0,fy0=0,fx1=0,fy1=0;A::frame(hud,fx0,fy0,fx1,fy1);
            long long sum=0,count=0;
            for(int yy=fy0;yy<fy1;++yy)for(int xx=fx0;xx<fx1;++xx){
                const auto p=sideways[std::size_t(yy)*width+xx];
                if(((p>>16)&255)>150&&((p>>8)&255)<90&&(p&255)<90){sum+=xx;++count;}
            }
            if(!count)throw std::runtime_error("The opponent beside the car left no indicator");
            if(sum/count<=(fx0+fx1)/2)
                throw std::runtime_error("The course map is mirrored: an opponent on the right drew on the left");
            (void)ahead;++cases;
        }
    }
    A::size=2;A::zoom=0;
    const auto zoomCar=carAt(course.length*.4f,0);
    const auto wide=A::render(hud,course,1280,720,zoomCar,nullptr);
    A::zoom=2;const auto close=A::render(hud,course,1280,720,zoomCar,nullptr);
    if(wide==close)throw std::runtime_error("Zoom did not change the visible road");
    const auto [cx,cy]=A::center(hud);
    if(wide[std::size_t(cy)*1280+cx]!=close[std::size_t(cy)*1280+cx])
        throw std::runtime_error("Zoom changed the fixed player marker");
    cases+=2;
    std::cout<<"PASS "<<cases<<" course map cases across three sizes, three zoom-out levels and four viewports: framed window, "
             <<"fixed player indicator, heading-up rotation, following, opponent in and out of range.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
