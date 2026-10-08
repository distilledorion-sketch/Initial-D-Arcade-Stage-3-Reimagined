#include "frontend.h"
#include "imported_course.h"
#include "original_time_attack_visit.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void picture(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels){
    std::ofstream out(path,std::ios::binary);out<<"P6\n640 480\n255\n";
    for(auto p:pixels){char rgb[]{char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
}
int main(int argc,char** argv)try{
    check(argc==4,"native_root odawara_pack private_output");
    const auto course=ImportedCourse::load(argv[2]);check(course.id==17,"Odawara fixture identity");
    std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
    Frontend menu,reference;menu.initialize(argv[1]);reference.initialize(argv[1]);menu.enableHakoneCourse(argv[2],17);
    OriginalTimeAttackVisit visit;visit.load(argv[1]);
    for(bool reverse:{false,true}){
        double area=0;const auto period=int(course.centerFor(reverse).size())-1;
        for(int i=0;i<period;++i){auto a=course.routePoint(reverse,i),b=course.routePoint(reverse,i+1);area+=double(a[0])*b[2]-double(b[0])*a[2];}
        // HUD/map projection (-x,-z) preserves signed area. Positive screen
        // area is clockwise; check the actual ordered road, not copied labels.
        check(reverse?area<0:area>0,"Odawara route winding disagrees with corrected label");
        menu.course=17;menu.reverse=reverse;menu.stage=FrontendStage::Route;menu.advance(1);
        reference.course=0;reference.reverse=!reverse;reference.stage=FrontendStage::Route;reference.advance(1);
        const auto actual=menu.paint(640,480),expected=reference.paint(640,480);
        for(int y=64;y<145;++y)for(int x=100;x<550;++x)
            check(actual[y*640+x]==expected[y*640+x],"Odawara selection must highlight the same original artwork as the physical direction");
        picture(out/(reverse?"route-ccw.ppm":"route-cw.ppm"),actual);
        menu.change(1);menu.advance(.2);check(menu.reverse!=reverse,"Route selection input failed");
        menu.change(-1);menu.advance(.2);check(menu.reverse==reverse,"Returning route input changed stable identity");
        menu.confirm();menu.advance(1);check(menu.stage==FrontendStage::Weather&&menu.course==17&&menu.reverse==reverse,"Confirm swapped physical route");
        for(bool wet:{false,true}){
            OriginalTimeAttackVisit::Setup setup;setup.condition=34+unsigned(reverse);setup.weather=unsigned(wet);setup.car=0;
            setup.customCourseName="ODAWARA";setup.customMaps.resize(1);setup.ticks6000=1200000;setup.courseRankingQualified=true;
            setup.localRecords.push_back({setup.condition,setup.weather,0,setup.ticks6000});
            visit.beginAfterResults(setup);for(unsigned i=0;i<110;++i)visit.advance({});
            check(visit.stage()==OriginalTimeAttackVisit::Stage::Ranking,"Odawara result missing ranking");
            std::vector<std::uint32_t> pixels(640*480);visit.paint(pixels,640,480);
            picture(out/(std::string(reverse?"ranking-ccw":"ranking-cw")+(wet?"-wet":"-dry")+".ppm"),pixels);
        }
    }
    for(unsigned condition=0;condition<18;++condition)check(originalCoursePresentationCondition(condition)==condition,"Original course label changed");
    check(originalCoursePresentationCondition(34)==1&&originalCoursePresentationCondition(35)==0,"Odawara presentation mapping");
    check(originalCoursePresentationCondition(32)==8&&originalCoursePresentationCondition(33)==9,"Gunsai rankings use outbound/inbound");
    std::cout<<"PASS actual Odawara route winding, rendered selection labels, input/confirmation, both weather rankings and stable original directions\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
