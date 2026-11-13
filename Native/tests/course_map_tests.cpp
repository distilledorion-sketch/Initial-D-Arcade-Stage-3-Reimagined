#include "ui.h"
#include "frontend.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <stdexcept>
namespace idas3 {
const std::vector<std::uint32_t>& Frontend::paint(int,int){throw std::logic_error("Unexpected frontend in the isolated map test");}
struct CourseMapTestAccess {
    // The geometry the race HUD actually uses, on the source 640x480 canvas.
    static constexpr float x=12,y=370,w=96,h=96;
    static inline int size=0,zoom=2;
    static std::vector<std::uint32_t> render(Hud& hud,const Course& course,int width,int height,
            const VehicleState& car,const VehicleState* rival,bool night=false){
        hud.resize(width,height);hud.setMapSize(size);hud.setMapZoom(zoom);GdiFlush();
        // A nonzero background makes any write outside the frame visible.
        for(std::size_t i=0;i<std::size_t(width)*height;++i)hud.pixels[i]=0xff000000u|unsigned((i*2654435761u)&0xffffffu);
        hud.map(course,x,y,w,h,car,rival,0,night);
        GdiFlush();return {hud.pixels,hud.pixels+std::size_t(width)*height};
    }
    // The frame in device pixels, for the containment check.
    // The map anchors to the bottom left of the source canvas, like the rest
    // of the original HUD, not to the development canvas.
    static float fit(const Hud& hud){return std::min(float(hud.width)/640.f,float(hud.height)/480.f);}
    static void frame(const Hud& hud,int& x0,int& y0,int& x1,int& y1){
        const float f=fit(hud),base=float(hud.height)-480.f*f;
        const float edge=w*(1.f+.25f*size);
        x0=int(x*f);y0=int(base+(y+h-edge)*f);x1=int((x+edge)*f)+1;y1=int(base+(y+h)*f)+1;
    }
    static std::pair<int,int> center(const Hud& hud){
        const float f=fit(hud),base=float(hud.height)-480.f*f;
        const float edge=w*(1.f+.25f*size);
        return {int((x+edge*.5f)*f),int(base+(y+h-edge*.5f)*f)};
    }
};
}
int main(int argc,char** argv)try{
    using namespace idas3;
    if(argc!=2&&argc!=3)throw std::invalid_argument("game-root [capture-directory] required");
    const std::filesystem::path root=argv[1];
    Hud hud;hud.loadOriginal(root);
    using A=CourseMapTestAccess;
    unsigned cases=0;
    // Compare rebuilt edges against an analytic straight road at several
    // output sizes. Magnifying old texels can pass movement tests but cannot
    // keep this subpixel edge and its original field/road alpha intact.
    {
        NativeImage source{512,512,std::vector<unsigned>(512*512,0x77ffffff)};
        source.argb[0]=0xff000000;
        const std::array<std::array<float,5>,2> path{{{.1f,.2f,0,0,0},{.9f,.8f,0,0,0}}};
        const auto artwork=navigationArtwork(source,path,2.25f);
        if(!artwork.navigationDistanceField||artwork.argb.size()>1024*1024)
            throw std::runtime_error("Navigation artwork lost its scalable format or memory bound");
        const std::array<std::array<float,5>,2> edgePath{{{.9f,-.5f,0,0,0},{1.1f,-.5f,0,0,0}}};
        const auto clipped=navigationArtwork(source,edgePath,4.f);
        for(unsigned x=0;x<800;++x)if(((clipped.argb[512*1024+x]>>16)&255)!=255)
            throw std::runtime_error("Clamped navigation path wrapped across the map");
        ++cases;
        for(int size:{96,216,432,864}){
            std::vector<unsigned> pixels(std::size_t(size)*size);
            compositeImage(pixels,size,size,artwork,0,0,float(size),float(size));
            unsigned edgePixels=0;
            for(int y=size/4;y<size*3/4;++y)for(int x=size/4;x<size*3/4;++x){
                const float distance=std::abs(.6f*(x+.5f-.1f*size)-.8f*(y+.5f-.2f*size));
                const float radius=2.25f*size/1024.f;
                const auto color=pixels[std::size_t(y)*size+x];const float coverage=1.f-float(color&255)/255.f;
                if(distance>radius+1.5f&&(color!=0x77ffffff))throw std::runtime_error("Rebuilt artwork altered the map field colour or alpha");
                if(distance<radius-1.5f&&color!=0xff000000)throw std::runtime_error("Rebuilt artwork altered the road colour or alpha");
                if(coverage>.05f&&coverage<.95f){++edgePixels;if(std::abs(distance-radius)>1.5f)throw std::runtime_error("Navigation edge wandered off its authored path");}
            }
            if(edgePixels<unsigned(size/4))throw std::runtime_error("Magnified navigation has aliased edges");
            ++cases;
        }
    }
    // UV coordinates were exported as world +X/-Z. For a car travelling
    // towards -Z (UV +V), world +X is on its screen-right. Test the actual
    // textured quad, independently of the opponent's projection formula.
    {
        OriginalNavigation navigation;navigation.setRoot(root);auto& bank=*navigation.find("k_df");
        std::array<std::uint32_t,96*96> image{};
        const OriginalNavigation::Point p{.5f,.5f,0,1};
        bank.paint(image,96,96,0,0,96,1,false,p);
        for(const auto& batch:bank.window.batches)for(const auto& vertex:batch.vertices){
            if(vertex.position.x>48&&vertex.u<=p.u)
                throw std::runtime_error("Original navigation is mirrored: screen-right samples world-left");
            if(vertex.position.y<48&&vertex.v<=p.v)
                throw std::runtime_error("Original navigation points behind instead of ahead");
            ++cases;
        }
    }
    for(const auto* circuit:{"k_ez","s_nm"})for(bool reverse:{false,true}){
        OriginalNavigation navigation;navigation.setRoot(root);auto& bank=*navigation.find(circuit);
        auto first=bank.sample(.17f,reverse),later=bank.sample(2.17f,reverse);
        if(std::abs(first.u-later.u)>1e-5f||std::abs(first.v-later.v)>1e-5f||
           std::abs(first.du-later.du)>1e-5f||std::abs(first.dv-later.dv)>1e-5f)
            throw std::runtime_error("Circuit navigation pinned a later-lap opponent at the finish");
        ++cases;
    }
    {
        OriginalNavigation navigation;navigation.setRoot(root);
        constexpr const char* ids[]{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu"};
        constexpr const char* codes[]{"ez","nm","hd","df","vh","uh","sy","tu"};
        for(unsigned i=0;i<8;++i){
            auto& bank=*navigation.find(ids[i]);const auto texture=bank.model.chunks.at(bank.day).batches.at(0).material[9];
            const auto source=NativeTextureBank::load(root/"data/original_assets/navigation"/codes[i]/"textures.idastex");
            const auto& old=source.at(texture);const auto& fresh=bank.textures.at(texture);
            const auto dark=[](const auto& im){return std::count_if(im.argb.begin(),im.argb.end(),[](auto p){return ((p>>16)&255)<128;});};
            const double ratio=double(dark(fresh))*old.width*old.height/(double(dark(old))*fresh.width*fresh.height);
            if(ratio<.8||ratio>1.2)throw std::runtime_error(std::string("Rebuilt road weight differs from source: ")+codes[i]);
            ++cases;
        }
    }
    // Ground the map in the actual road records, in both race directions.
    // These checks catch wrong-lap/direction mapping as well as a map that
    // moves smoothly but is following a different part of the road.
    for(const auto* id:{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu"})for(bool reverse:{false,true}){
        auto c=Course::load(root/"data/courses",id,id,reverse);
        OriginalNavigation navigation;navigation.setRoot(root);auto& bank=*navigation.find(id);
        if(bank.path.size()!=c.points.size())throw std::runtime_error("Navigation records do not match road records");
        for(std::size_t i=1;i+1<c.points.size();++i){
            const auto p=bank.sampleCourse(c,c.cumulative[i]);
            const auto& record=bank.path[reverse?bank.path.size()-1-i:i];
            if(std::abs(p.u-record[0])>1e-6f||std::abs(p.v-record[1]-2.f)>1e-6f)
                throw std::runtime_error("Navigation does not follow the actual road segment");
            const auto before=bank.sampleCourse(c,c.cumulative[i]-.001f);
            const auto after=bank.sampleCourse(c,c.cumulative[i]+.001f);
            if(std::hypot(after.du-before.du,after.dv-before.dv)>.003f)
                throw std::runtime_error("Navigation rotation snaps at a road segment boundary");
            cases+=2;
        }
        for(unsigned i=1;i<200;++i){
            const float distance=c.length*float(i)/200;
            const auto road=c.sample(distance),ahead=c.sample(std::min(distance+25.f,c.length));
            const auto delta=ahead.center-road.center;
            const float expectedRight=-road.tangent.z*delta.x+road.tangent.x*delta.z;
            const auto p=bank.sampleCourse(c,distance),other=bank.sampleCourse(c,ahead.distance);
            const auto offset=p.offset(other);
            if(std::abs(expectedRight)>2&&offset[0]*expectedRight<=0)
                throw std::runtime_error("Navigation turn/opponent lies on the opposite side to the actual road");
            ++cases;
        }
        if(c.closed){
            const auto first=bank.sampleCourse(c,0),last=bank.sampleCourse(c,c.length);
            if(std::hypot(first.u-last.u,first.v-last.v)>1e-6f||std::hypot(first.du-last.du,first.dv-last.dv)>1e-6f)
                throw std::runtime_error("Circuit navigation jumps at the lap seam");
            ++cases;
        }
    }
    // Sample a real navigation texture at the exact junction of four texels.
    // Its filtered value must be their average, with the unfiltered path still
    // selecting one texel. This catches accidental changes to ordinary UI.
    {
        OriginalNavigation navigation;navigation.setRoot(root);auto& bank=*navigation.find("k_df");
        auto chunk=bank.model.chunks.at(bank.day);auto& batch=chunk.batches.at(0);
        const auto originalTextures=NativeTextureBank::load(root/"data/original_assets/navigation/df/textures.idastex");
        const auto& image=originalTextures.at(batch.material[9]);unsigned sx=0,sy=0;bool found=false;
        for(unsigned y=0;y+1<image.height&&!found;++y)for(unsigned x=0;x+1<image.width;++x){
            if(image.argb[y*image.width+x]!=image.argb[y*image.width+x+1]){sx=x;sy=y;found=true;break;}
        }
        if(!found)throw std::runtime_error("Navigation filter fixture has no contrasting texels");
        const unsigned samples[]={image.argb[sy*image.width+sx],image.argb[sy*image.width+sx+1],image.argb[(sy+1)*image.width+sx],image.argb[(sy+1)*image.width+sx+1]};
        unsigned expected=0;
        for(unsigned shift:{0,8,16,24}){unsigned sum=0;for(auto sample:samples)sum+=(sample>>shift)&255;expected|=((sum+2)/4)<<shift;}
        batch.ich[0]=8;batch.ich[2]=(1u<<29)|(1u<<20)|(1u<<16)|(1u<<15)|(1u<<13)|(3u<<6);
        batch.material[2]=1;batch.material[3]=0xffffffffu;
        for(auto& vertex:batch.vertices){vertex.u=float(sx+1)/image.width;vertex.v=float(sy+1)/image.height;}
        SpritePlacement placement;placement.invertY=true;placement.authoredHeight=0;placement.sourceTextureFiltering=true;
        std::array<unsigned,1> filtered{},nearest{};
        compositeOriginalMenuChunk(filtered,1,1,originalTextures,chunk,placement);
        placement.sourceTextureFiltering=false;compositeOriginalMenuChunk(nearest,1,1,originalTextures,chunk,placement);
        if(filtered[0]!=expected||nearest[0]!=samples[3]||filtered==nearest)throw std::runtime_error("Source navigation filtering changed texel colour or ignored the opt-in");
        ++cases;
    }
    auto course=Course::load(root/"data/courses","s_nm","s_nm",false);
    // The generated fallback still rotates with the physical car. Arcade
    // banks below instead follow their original navigation-path tangent.
    course.id="imported-map-test";
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
    // The old tests only asserted movement. Pin the actual arcade defaults:
    // coverage is per-course, artwork/tint come from the recovered bank, and
    // widening road edges or steering in place cannot zoom/rotate that map.
    const auto save=[&](const std::filesystem::path& path,const auto& pixels,int w,int h){
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+DWORD(pixels.size()*4);
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-h;info.biPlanes=1;info.biBitCount=32;
        std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(&file),sizeof(file));f.write(reinterpret_cast<const char*>(&info),sizeof(info));f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
    };
    if(argc==3){
        std::filesystem::create_directories(argv[2]);
        OriginalNavigation navigation;navigation.setRoot(root);auto& bank=*navigation.find("s_uh");
        const auto c=Course::load(root/"data/courses","s_uh","s_uh",false);
        const auto point=bank.sampleCourse(c,c.length*.17f);
        std::vector<unsigned> preview(512*512,0xff708078u);
        bank.paint(preview,512,512,0,0,512,1,false,point);
        save(std::filesystem::path(argv[2])/"iroha-rebuilt.bmp",preview,512,512);
        const auto texture=bank.model.chunks.at(bank.day).batches.at(0).material[9];
        const auto original=NativeTextureBank::load(root/"data/original_assets/navigation/uh/textures.idastex");
        bank.textures.replaceImage(texture,original.at(texture));
        std::fill(preview.begin(),preview.end(),0xff708078u);
        bank.paint(preview,512,512,0,0,512,1,false,point);
        save(std::filesystem::path(argv[2])/"iroha-source.bmp",preview,512,512);
    }
    for(const auto* id:{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu"})for(bool reverse:{false,true}){
        auto c=Course::load(root/"data/courses",id,id,reverse);
        const auto p=c.sample(c.length*.17f);VehicleState car;car.position=p.center;car.yaw=std::atan2(p.tangent.x,p.tangent.z);
        A::size=0;A::zoom=2;
        for(auto [w,h]:{std::pair{640,480},std::pair{1280,720}})for(bool night:{false,true}){
            const auto image=A::render(hud,c,w,h,car,nullptr,night);
            auto wideRoad=c;for(auto& l:wideRoad.left)l=l*4;for(auto& r:wideRoad.right)r=r*4;
            if(image!=A::render(hud,wideRoad,w,h,car,nullptr,night))throw std::runtime_error("Road width changed original navigation");
            auto turned=car;turned.yaw+=1.2f;
            if(image!=A::render(hud,c,w,h,turned,nullptr,night))throw std::runtime_error("Car yaw replaced the navigation-path heading");
            int x0,y0,x1,y1;A::frame(hud,x0,y0,x1,y1);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(x<x0||x>=x1||y<y0||y>=y1){
                const auto i=std::size_t(y)*w+x;
                if(image[i]!=(0xff000000u|unsigned((i*2654435761u)&0xffffffu)))throw std::runtime_error("Original navigation escaped its frame");
            }
            A::zoom=0;const auto zoomOut=A::render(hud,c,w,h,car,nullptr,night);A::zoom=2;
            if(image==zoomOut)throw std::runtime_error(std::string("Original navigation zoom-out has no effect: ")+id+(reverse?" reverse":" forward")+(night?" night":" day"));
            if(argc==3&&std::string_view(id)=="k_df"){
                std::filesystem::create_directories(argv[2]);
                save(std::filesystem::path(argv[2])/(std::string("akina-")+(reverse?"up-":"down-")+(night?"night-":"day-")+std::to_string(w)+".bmp"),image,w,h);
            }
            cases+=4;
        }
    }
    std::cout<<"PASS "<<cases<<" course map cases across three sizes, three zoom-out levels and four viewports: framed window, "
             <<"fixed player indicator, heading-up rotation, following, opponent in and out of range.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
