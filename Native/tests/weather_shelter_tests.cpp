#include "weather_shelter.h"
#include "course_scene_catalog.h"
#include "course.h"
#include "wet_weather.h"
#include <chrono>
#include <iostream>
#include <fstream>
using namespace idas3;
int main(int argc,char** argv)try{
 if(argc!=3)throw std::runtime_error("native-root output-csv");
 std::ofstream out(argv[2]);out<<"night,reverse,index,x,y,z,covered\n";
 for(bool night:{false,true})for(bool reverse:{false,true}){
  auto scene=OriginalCourseScene::load(argv[1],"k_tu",night,reverse,true);WeatherShelter s;s.build(scene.model,scene.assemblies());
  auto course=Course::load(std::filesystem::path(argv[1])/"data/courses","k_tu");unsigned covered=0;
  for(unsigned i=0;i<course.points.size();++i){auto p=course.points[i]+Vec3{0,1,0};bool c=s.covered(p);covered+=c;out<<night<<','<<reverse<<','<<i<<','<<p.x<<','<<p.y<<','<<p.z<<','<<c<<'\n';}
  std::cout<<night<<','<<reverse<<": ceiling triangles="<<s.size()<<" covered route points="<<covered<<'/'<<course.points.size()<<'\n';

  if(covered!=145||!s.covered(course.points[2488]+Vec3{0,1,0})||s.covered(course.points[2380]+Vec3{0,1,0})||s.covered(course.points[2600]+Vec3{0,1,0}))throw std::runtime_error("Tunnel interior/portal coverage changed");
  WetWeather weather;const auto position=course.points[2488];std::array<WetWeather::Car,2> cars{{{position,0,30,true},{}}};weather.advance(.2,true,false,cars);
  for(unsigned stride:{1u,4u}){
    weather.build(position+Vec3{0,1,0},position+Vec3{0,1,10},true,stride,&s);unsigned spray=0;
    for(unsigned i=0;i<weather.count;++i){const auto& q=weather.quads[i];if(q.waterTrail)++spray;else if(s.covered(q.center-Vec3{0,.9f,0}))throw std::runtime_error("Rain emitted under tunnel roof");}
    if(!spray)throw std::runtime_error("Tunnel shelter disabled tire spray");
    const auto outside=course.points[2380];weather.build(outside+Vec3{0,1,0},outside+Vec3{0,1,10},true,stride,&s);unsigned drops=0;
    for(unsigned i=0;i<weather.count;++i)drops+=!weather.quads[i].waterTrail;if(drops<20)throw std::runtime_error("Exterior rain disappeared");
  }
  const auto begin=std::chrono::steady_clock::now();
  for(unsigned i=0;i<1000;++i)weather.build(position+Vec3{0,1,0},position+Vec3{0,1,10},true,1,&s);
  std::cout<<"Weather build with shelter ms/frame="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/1000<<'\n';
  s.clear();if(s.size()||s.covered(position))throw std::runtime_error("Shelter reset retained previous course");

 }
 std::cout<<"PASS four Tsuchisaka wet variants: both portals, interior rain blocked, exterior rain and tire spray preserved at both detail settings.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
