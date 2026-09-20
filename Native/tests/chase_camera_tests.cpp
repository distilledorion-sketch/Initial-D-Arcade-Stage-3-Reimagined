#include "chase_camera.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()try{
    ChaseCamera camera;
    const auto first=camera.update({0,0,0},0,0);
    const Vec3 translation{500,70,-800};
    const auto moved=camera.update(translation,0,1./60);
    require(length((moved.eye-first.eye)-translation)<.001f,"Camera translation lag changes distance under acceleration");
    require(length((moved.target-first.target)-translation)<.001f,"Look target separates from car");
    auto atRate=[](int hz){ChaseCamera c;c.update({},pi-.03f,0);ChaseView result{};for(int i=0;i<hz;++i)result=c.update({},-pi+.03f,1.0/hz);return result;};
    const auto low=atRate(30),high=atRate(144);
    require(length(low.eye-high.eye)<.001f,"Chase orbit depends on render rate");
    require(low.eye.z>0&&high.eye.z>0,"Camera takes long turn across angle wrap");
    camera.reset();const auto restart=camera.update(translation,pi/2,0);
    require(std::abs(restart.eye.z-translation.z)<.001f&&restart.eye.x<translation.x,"Restart retains old chase direction");
    std::cout<<"PASS chase framing remains attached through translation, frame rates, heading wrap and restart\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
