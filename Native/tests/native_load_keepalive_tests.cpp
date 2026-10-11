#include "native_load_keepalive.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
unsigned checks=0;
std::atomic<unsigned> calls=0;
idas3::LoadKeepAliveState* recursiveState=nullptr;
void require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
void count(){++calls;}
void recurse(){++calls;recursiveState->service(idas3::LoadKeepAliveState::Clock::time_point{}+std::chrono::seconds(10));}
void throwing(){++calls;throw std::runtime_error("fixture exception");}
}
int main()try{
    using State=idas3::LoadKeepAliveState;
    const State::Clock::time_point start{};
    State state;
    state.service(start);require(calls==0,"No callback was registered");
    state.callback=count;
    state.service(start);require(calls==1,"First keepalive did not run");
    state.service(start+std::chrono::milliseconds(99));require(calls==1,"Keepalive was not throttled");
    state.service(start+std::chrono::milliseconds(100));require(calls==2,"Keepalive did not resume after throttle");
    state.callback=recurse;recursiveState=&state;
    state.service(start+std::chrono::seconds(1));require(calls==3&&!state.pumping,"Keepalive re-entered itself");
    state.callback=throwing;
    state.service(start+std::chrono::seconds(2));require(calls==4&&!state.pumping,"Exception escaped or retained recursion guard");
    state.callback=count;
    state.service(start+std::chrono::seconds(3));require(calls==5,"Keepalive failed after a callback exception");
    idas3::setLoadKeepAlive(count);
    std::thread other([]{idas3::serviceLoadKeepAlive();});other.join();
    require(calls==5,"Another thread invoked the Unity callback");
    idas3::serviceLoadKeepAlive();require(calls==6,"Registering thread lost its callback");
    idas3::setLoadKeepAlive(nullptr);idas3::serviceLoadKeepAlive();
    require(calls==6,"Cleared callback was invoked");
    std::cout<<"PASS "<<checks<<" native load keepalive checks\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
