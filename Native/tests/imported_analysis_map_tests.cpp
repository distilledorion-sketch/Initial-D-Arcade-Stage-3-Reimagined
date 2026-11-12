#include "imported_course.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
namespace {
unsigned checks=0,failures=0;
void check(bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
bool close(float a,float b){return std::abs(a-b)<.002f;}
bool same(std::array<float,2> a,std::array<float,2> b){return close(a[0],b[0])&&close(a[1],b[1]);}
// Independent course-card reference axes: Sadamine/Gunsai/Odawara export
// (-X,-Z); Enna/Special Stage/Tsubaki export (+X,+Z). Hakone is unchanged.
constexpr std::array<std::array<int,2>,9> cardAxes{{{1,-1},{-1,-1},{1,1},{1,1},{1,1},{1,1},{1,1},{-1,-1},{-1,-1}}};
}
int main(int argc,char** argv)try{
    if(argc!=3&&argc!=4)throw std::runtime_error("runtime_assets new_private_output [native_root_for_rendering]");
    const std::filesystem::path output=argv[2];
    if(std::filesystem::exists(output))throw std::runtime_error("Preserve previous map evidence");
    std::filesystem::create_directories(output);
    for(unsigned id=9;id<18;++id)for(bool reverse:{false,true}){
        ImportedCourse course;course.id=id;course.checkpoints={0,5,10,15,20};
        for(unsigned i=0;i<=20;++i)course.center.push_back({float(i*i),0,float(i*40)});
        OriginalTimeAttackTelemetrySnapshot trace;
        for(unsigned i=0;i<=20;++i){const auto p=course.routePoint(reverse,int(i));
            trace.drivingPath.push_back({p,i,0xff00ff00});trace.ditchEvents.push_back({p,i});
            OriginalImpactRecord wall{};wall.position=p;wall.tick=i;wall.magnitude=1000;trace.wallEvents.push_back(wall);
        }
        const auto pages=course.analysisMaps(trace,reverse);
        check(pages.size()==5,"All five pages remain available");
        for(unsigned page=0;page<pages.size();++page){const auto& p=pages[page];
            const std::string label="course "+std::to_string(id)+" reverse "+std::to_string(reverse)+" page "+std::to_string(page);
            check(!p.road.empty()&&!p.driving.empty(),label+" road and trace exist");
            const auto first=p.road.front().from,last=p.road.back().to;
            const int direction=reverse?-1:1;
            check((last[0]-first[0])*cardAxes[id-9][0]*direction>0,label+" horizontal orientation differs from course card");
            check((last[1]-first[1])*cardAxes[id-9][1]*direction>0,label+" vertical orientation differs from course card");
            for(const auto& road:p.road){bool aligned=false;for(const auto& driven:p.driving)aligned|=same(road.from,driven.from)&&same(road.to,driven.to);check(aligned,label+" driven line follows road");}
            check(p.walls.size()==p.road.size()+1&&p.ditches.size()==p.road.size()+1,label+" section filters retain the correct marker count");
            for(unsigned i=0;i<p.walls.size();++i){const auto expected=i<p.road.size()?p.road[i].from:p.road.back().to;
                check(same(p.walls[i],expected)&&same(p.ditches[i],expected),label+" event markers stay on the projected road");}
            for(const auto& road:p.road)for(auto point:{road.from,road.to})check(point[0]>=195&&point[0]<=435&&point[1]>=195&&point[1]<=435,label+" map stays inside frame");
        }
    }
    // Real pack geometry for independent comparison with the existing cards,
    // including the two directions of Odawara's split route.
    constexpr std::array<const char*,9> packs{"HAKONE","SADAMINE","ENNA","MYOGI_SPECIAL","USUI_SPECIAL","MOMIJI","TSUBAKI","GUNSAI","ODAWARA"};
    OriginalTimeAttackVisit visit;if(argc==4)visit.load(argv[3]);
    for(auto pack:packs){auto course=ImportedCourse::load(std::filesystem::path(argv[1])/pack);
        for(bool reverse:{false,true}){const auto pages=course.analysisMaps({},reverse);
            std::ofstream csv(output/(std::string(pack)+(reverse?"-reverse.csv":"-forward.csv")));
            csv<<"page,x1,y1,x2,y2\n";csv.precision(9);
            for(unsigned page=0;page<pages.size();++page)for(const auto& line:pages[page].road)csv<<page<<','<<line.from[0]<<','<<line.from[1]<<','<<line.to[0]<<','<<line.to[1]<<'\n';
            check(bool(csv),"Write real-map comparison data");
            if(argc==4){
                OriginalTimeAttackVisit::Setup setup;setup.condition=course.id*2+unsigned(reverse);setup.customCourseName=course.name;
                setup.customMaps=pages;setup.ticks6000=1200000;visit.beginLecture(setup);
                for(unsigned frame=0;frame<16;++frame)visit.advance({});
                const unsigned capturePages=course.id==10||course.id==11?5:1;
                for(unsigned page=0;page<capturePages;++page){
                    check(visit.stage()==OriginalTimeAttackVisit::Stage::Lecture&&visit.mapIndex()==page,"Real analysis page selection");
                    std::vector<std::uint32_t> pixels(640*480);visit.paint(pixels,640,480);
                    check(std::count_if(pixels.begin(),pixels.end(),[](auto color){return (color&0xffffff)>0;})>20000,"Rendered analysis has visible artwork");
                    std::ofstream picture(output/(std::string(pack)+(reverse?"-reverse-":"-forward-")+std::to_string(page)+".ppm"),std::ios::binary);
                    picture<<"P6\n640 480\n255\n";
                    for(auto color:pixels){const char rgb[]{char(color>>16),char(color>>8),char(color)};picture.write(rgb,3);}
                    check(bool(picture),"Write real analysis render");visit.advance({.nextMap=true});visit.advance({});
                }
            }
        }
    }
    std::cout<<(failures?"FAIL ":"PASS ")<<checks<<" checks; "<<failures<<" failures; 9 courses, 2 directions, 5 pages, aligned telemetry and real route exports\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
