#include "original_car_lighting.h"
#include "original_car_lighting_reference.h"
#include <random>
#include <sstream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image and FSCA table required");
 unsigned checks=0,cases=0;std::size_t instructions=0;
 auto eq=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" original="+hex(a)+" native="+hex(b));};
 auto bits=[](float v){return std::bit_cast<unsigned>(v);};
 for(unsigned id=0;id<35;++id){CarLightReference r(argv[1],argv[2]);r.carConstructor(r.car,id);const auto native=originalCarLighting();const auto set=r.m.read32(r.car+2540);
  eq(r.m.read32(set+68),0,"ctor empty ARRAY");eq(r.m.read32(set+88),bits(native.gain),"ctor gain");
  for(unsigned j=0;j<3;++j){eq(r.m.read32(set+72+4*j),bits(native.ambient[j]),"ctor ambient");eq(r.m.read32(r.car+2544+32+4*j),bits(native.ownSpot.color[j]),"ctor RGB");eq(r.m.read32(r.car+2544+68+4*j),bits(native.ownSpot.incomingDirection[j]),"ctor direction");}
  eq(r.m.read8(r.car+2568),native.ownSpot.enabled,"ctor disabled");eq(r.m.read32(r.car+2544+60),bits(native.ownSpot.distance0),"ctor near");eq(r.m.read32(r.car+2544+64),bits(native.ownSpot.distance1),"ctor far");
  eq(r.m.read32(r.car+2544+84),native.ownSpot.angle0,"ctor inner");eq(r.m.read32(r.car+2544+88),native.ownSpot.angle1,"ctor outer");instructions+=r.instructions;
 }
 std::mt19937 rng(0x033a00);std::uniform_real_distribution<float> number(-1.f,1.f);
 for(unsigned row=0;row<36;++row)for(unsigned direction=0;direction<2;++direction)for(unsigned mode=0;mode<4;++mode){
  CarLightReference r(argv[1],argv[2]);r.carConstructor(r.car,row%35);r.carConstructor(r.rival,(row+17)%35);r.courseConstructor(row/4,row&2,row&1);r.m.write32(r.course+48,direction);
  r.borrow(r.car,row/4,row&2,row&1);if(mode!=2&&mode!=3)r.borrow(r.rival,row/4,row&2,row&1);
  const auto base=originalCourseLighting(row/4,row&2,row&1);auto player=originalCarLighting(),rival=originalCarLighting();
  // Initial generic borrowing copies group ambient; each actual pose parent
  // replaces it with the separate course header before the first draw.
  for(unsigned p:{r.car,r.rival}){r.c.r[4]=p;for(unsigned j=0;j<3;++j)r.c.fr[4+j]=r.m.read32(r.course+12+4*j);r.run(0x0c0354e0);}
  OriginalCarAmbientInputs inputs;for(unsigned j=0;j<3;++j)inputs.courseAmbient[j]=r.m.readFloat(r.course+12+4*j);updateOriginalCarAmbient(player,inputs);updateOriginalCarAmbient(rival,inputs);
  const unsigned race=r.car+0xc000;r.m.write32(race+1036,r.course);r.m.write32(race+1048,r.car);r.m.write32(race+1052,r.rival);r.m.write32(race+1640,mode);
  r.c.callHooks[0x0c035160]=[](auto&){}; // Separate projector activation/query binding only.
  if(row&2){r.c.r[4]=race;r.run(0x0c0633c0);r.c.r[4]=race;r.run(0x0c063b60);setOriginalCarLightEnabled(player,true);setOriginalCarLightEnabled(rival,mode!=2&&mode!=3);}
  OriginalCarLightingSetup setup{row/4,mode,bool(row&2),bool(row&1)};
  const auto headerAmbient=originalCourseCarAmbient(setup);for(unsigned j=0;j<3;++j)eq(r.m.read32(r.course+12+4*j),bits(headerAmbient[j]),"separate course header ambient");
  auto compareArray=[&](unsigned set,const OriginalCourseLighting&native){
   eq(r.m.read32(set+68),native.count,"registered count");
   for(unsigned j=0;j<3;++j)eq(r.m.read32(set+72+4*j),bits(native.ambient[j]),"array ambient");eq(r.m.read32(set+88),bits(native.gain),"array gain");
   for(unsigned i=0;i<native.count;++i){const auto p=r.m.read32(set+4+4*i);const auto&l=native.lights[i];
    eq(r.m.read32(p+20),l.kind==OriginalCourseLightKind::Spot?2:l.kind==OriginalCourseLightKind::Point?1:0,"registered kind");eq(r.m.read8(p+24),l.enabled,"shared enabled");
    for(unsigned j=0;j<3;++j){eq(r.m.read32(p+32+4*j),bits(l.color[j]),"shared RGB");if(l.kind!=OriginalCourseLightKind::Point)eq(r.m.read32(p+(l.kind==OriginalCourseLightKind::Spot?68:44)+4*j),bits(l.incomingDirection[j]),"shared direction");}
    if(l.kind==OriginalCourseLightKind::Spot||l.kind==OriginalCourseLightKind::Point){for(unsigned j=0;j<3;++j)eq(r.m.read32(p+44+4*j),bits(l.position[j]),"shared position");}
   }
  };
  for(unsigned sample=0;sample<4;++sample){
   for(auto pair:{std::pair(r.car,&player),std::pair(r.rival,&rival)}){
    auto world=originalLightIdentityMatrix;for(unsigned j=0;j<16;++j)if(j%4!=3)world[j]=number(rng)*(j>=12?500.f:1.f);
    for(unsigned j=0;j<16;++j)r.m.writeFloat(pair.first+2404+4*j,world[j]);r.c.r[13]=pair.first;r.c.r[14]=r.stack-0x8000;r.c.r[15]=r.stack;
    r.run(0x0c034b26,0x0c034b8e);publishOriginalCarLight(*pair.second,world);
    const bool enabled=sample!=2&&(sample==0||pair.first==r.car);r.c.r[4]=pair.first;r.run(enabled?0x0c035120:0x0c035200);setOriginalCarLightEnabled(*pair.second,enabled);
    const float gain=sample==0?1.f:sample==1?.57f:sample==2?0.f:1.3f;r.c.r[4]=pair.first;r.c.fr[4]=bits(gain);r.run(0x0c035500);pair.second->gain=gain;
   }
   const auto scopes=composeOriginalRaceLighting(base,player,rival,setup);eq(scopes.hasRival,mode!=2&&mode!=3,"numeric rival gate");
   compareArray(r.m.read32(r.course+60),scopes.course);compareArray(r.m.read32(r.car+2540),scopes.player);if(scopes.hasRival)compareArray(r.m.read32(r.rival+2540),scopes.rival);
   auto view=originalLightIdentityMatrix;for(unsigned j=0;j<16;++j)if(j%4!=3)view[j]=number(rng)*(j>=12?700.f:1.f);
   for(auto pair:{std::pair(r.m.read32(r.course+60),&scopes.course),std::pair(r.m.read32(r.car+2540),&scopes.player),std::pair(r.m.read32(r.rival+2540),&scopes.rival)}){
    if(!scopes.hasRival&&pair.second==&scopes.rival)continue;
    const auto actual=r.packets(pair.first,view);const auto expected=originalCourseLightingPacket(*pair.second,view);eq(unsigned(actual.size()),expected.count,"packed active count");
    for(unsigned i=0;i<expected.count;++i)for(unsigned j=0;j<8;++j){if(actual[i][j]!=expected.lights[i][j])std::cerr<<"row "<<row<<" direction "<<direction<<" mode "<<mode<<" sample "<<sample<<" set "<<hex(pair.first)<<" light "<<i<<" word "<<j<<'\n';eq(actual[i][j],expected.lights[i][j],"exact transformed light packet");}
   }
  }
  instructions+=r.instructions;++cases;
 }
 // Actual Happo update changes nearest spots, never its borrowed point1068.
 // Query position is a declared path boundary; full041320 executes.
 for(unsigned wet=0;wet<2;++wet){CarLightReference r(argv[1],argv[2]);r.courseConstructor(4,true,wet);
  const unsigned point=r.m.read32(r.course+1068),query=r.car+0x30000,queryTable=query+128,matrix=query+256;
  std::array<unsigned,17> initial;for(unsigned j=0;j<17;++j)initial[j]=r.m.read32(point+4*j);
  r.m.write32(r.course+24,query);r.m.write32(query,queryTable);r.m.write32(queryTable+44,0x0f000040);r.m.write32(r.course+80,matrix);
  r.c.callHooks[0x0f000040]=[&](auto&q){for(unsigned j=0;j<3;++j)r.m.writeFloat(q.r[6]+4*j,number(rng)*2000.f);};
  for(unsigned sample=0;sample<64;++sample){for(unsigned j=0;j<16;++j)r.m.writeFloat(matrix+4*j,originalLightIdentityMatrix[j]);r.c.r[4]=r.course;r.run(0x0c041320);
   for(unsigned j=0;j<17;++j)eq(r.m.read32(point+4*j),initial[j],"Happo point preserved by actual update");}
  instructions+=r.instructions;
 }
 // Native ambient helper against the full actual parents, not a copied
 // arithmetic expectation. Publication and unrelated lamp requests are
 // boundaries; ambient branches and0354E0 execute unchanged.
 {
  CarLightReference r(argv[1],argv[2]);r.carConstructor(r.car,0);r.carConstructor(r.rival,29);
  const unsigned race=r.car+0xc000,hud=r.car+0x18000,actor=r.car+0x19000;
  r.m.write32(race+1036,r.course);r.m.write32(race+1048,r.car);r.m.write32(race+1052,r.rival);r.m.write32(race+1404,hud);r.m.write32(race+1640,0);
  for(unsigned p:{r.car,r.rival})r.m.write32(p+2396,actor);
  r.c.callHooks[0x0c034840]=[](auto&){};r.c.callHooks[0x0c035220]=[](auto&){};r.c.callHooks[0x0c035120]=[](auto&){};r.c.callHooks[0x0c035200]=[](auto&){};
  for(unsigned courseId=0;courseId<9;++courseId)for(unsigned flags=0;flags<8;++flags)for(float gap:{-5.01f,-5.f,-4.99999f,-2.5f,0.f,1.f}){
   const bool wet=flags&1,night=flags&2,priorLamp=flags&4;
   r.m.write32(0x0c31c99c,1);r.m.write32(0x0c31c99c+4,courseId);r.m.write32(0x0c31c99c+8,flags&1);r.m.write32(0x0c31c99c+32,wet);r.m.write32(race+1664,night);r.m.writeFloat(hud+100,gap);r.m.write8(r.rival+81,priorLamp);
   OriginalCarAmbientInputs input;input.course=courseId;input.wet=wet;input.night=night;input.rivalLightBeforeRequest=priorLamp;input.signedAdvantage=gap;
   for(unsigned j=0;j<3;++j){input.courseAmbient[j]=number(rng);r.m.writeFloat(r.course+12+4*j,input.courseAmbient[j]);}
   for(unsigned p:{r.car,r.rival}){input.rival=p==r.rival;auto native=originalCarLighting();updateOriginalCarAmbient(native,input);r.c.r[4]=race;r.run(input.rival?0x0c0638c0:0x0c063280);
    const auto set=r.m.read32(p+2540);for(unsigned j=0;j<3;++j)eq(r.m.read32(set+72+4*j),bits(native.ambient[j]),"full parent native ambient helper");}
  }
  instructions+=r.instructions;
 }
 std::cout<<"PASS35 full033A00 constructors, "<<cases<<" course/time/weather/direction/mode registration cases with four current-pose/enable/gain submissions each; "<<checks<<" exact checks / "<<instructions<<" original instructions.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
