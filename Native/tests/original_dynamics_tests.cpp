#include "original_dynamics.h"
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace idas3::original;
namespace {
int checked=0;
void check(bool ok,const char* m) { ++checked; if(!ok){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);} }
bool close(float a,float b,float e=1e-6f){return std::abs(a-b)<e;}
float sine(float x){return std::sin(x);}
float cosine(float x){return std::cos(x);}
float controlledDot(const std::array<float,3>& a,const std::array<float,3>& b){return (a[0]*b[0]+a[1]*b[1])+a[2]*b[2];}
}
int main(){
  OriginalDriveState s;
  OriginalLossState loss;
  OriginalLossParameters lp;
  lp.normalizedBrake0CAA98A0=1;
  updateOriginalLongitudinalLoss(s,loss,lp);
  check(std::bit_cast<unsigned>(loss.speedLoss0CAA9880)==0x3E6B851Fu,"original brake coefficient preserves exact literal bits");
  check(loss.persistentPenalty0CAA9884==0,"zero persistent state remains zero");

  s={};loss={};lp.normalizedBrake0CAA98A0=0;
  s.setu(0x1A8,1);
  updateOriginalLongitudinalLoss(s,loss,lp);
  check(std::bit_cast<unsigned>(loss.speedLoss0CAA9880)==0x3EB851ECu,"original flagged .36 loss is preserved");
  check(s.u(0x1AC)==1,"zero state248 with flag sets original secondary flag");

  s={};loss={};s.setf(0x248,1);loss.persistentPenalty0CAA9884=700;
  lp.condition0C9015CC=2;lp.cap0C284F80=2000;lp.growth0C284F84=1;
  updateOriginalLongitudinalLoss(s,loss,lp);
  check(close(loss.speedLoss0CAA9880,1.001f),"penalty contributes before decay");
  check(loss.persistentPenalty0CAA9884==699,"decay is exactly previous penalty /700");
  check(s.f(0x1F4)==70,"published penalty snapshot precedes decay");

  s={};loss={};s.setf(0x248,1);s.setu(0x43C,1);loss.persistentPenalty0CAA9884=42;
  updateOriginalLongitudinalLoss(s,loss,lp);
  check(loss.persistentPenalty0CAA9884==0,"original clear flag clears persistent state");
  check(close(s.f(0x1F4),4.2f),"published snapshot intentionally precedes clear flag");
  check(close(loss.speedLoss0CAA9880,0.001f),"cleared penalty does not add later loss");

  s={};loss={};s.setf(0x248,1);s.setf(0x258,10);lp.condition0C9015CC=0;
  updateOriginalLongitudinalLoss(s,loss,lp);
  check(loss.persistentPenalty0CAA9884<1.2f,"condition0/1 caps persistent penalty at1.2 before decay");

  OriginalAngularParameters p;
  p.carRecord0C283F18_08=1;p.carRecord0C283F18_0C=1;
  p.table0C285124=1;p.global0C900E40=1;
  s={};s.setf(0x22C,3);s.setf(0x230,4);s.setf(0x1BC,1);
  check(computeOriginalMotionScale(s,p)==2.5f,"3/4 displacement norm divided by original axle sum");
  s.setf(0x248,1);
  check(computeOriginalMotionScale(s,p)<0.003f,"original state248 strongly changes primary response");

  s={};s.setf(0x1CC,1);
  const auto result=updateOriginalAngular(s,p,{sine,cosine,controlledDot},1);
  check(std::bit_cast<unsigned>(s.f(0xD8))==0x3F060A92u,"full steering primary contribution uses exact pi/6 literal");
  check(s.f(0x110)==s.f(0xD8),"primary heading integrates pre-retention rate");
  check(s.f(0xDC)==0,"rate retention occurs after integration");
  check(close(s.f(0x274),2*s.f(0xD8)),"secondary error couples primary heading and steering");
  check(result.motionScale==1,"retained original scalar is not recomputed from later state");

  s={};s.setf(0x284,110);s.setf(0x1CC,-0.5f);s.setf(0x1BC,1);
  const auto turning=updateOriginalAngular(s,p,{sine,cosine,controlledDot},0);
  check(s.f(0x28C)==0.5f,"countersteering updates original opposing-direction state");
  check(close(turning.steeringCorrection,2.0f),"correction uses UNCLAMPED memory quotient");
  check(close(s.f(0x284),110.0f-1.04719758f),"memory drain uses separately clamped quotient");

  std::cout<<"PASS "<<checked<<" original-dynamics structural tests; opcode differential parity remains required.\n";
}
