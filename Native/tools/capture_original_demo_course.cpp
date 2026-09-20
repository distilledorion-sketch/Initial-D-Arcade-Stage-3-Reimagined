// Dedicated scene20 capture, preserving the original03FCE0 dispatch.
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
using namespace idas3::reference;
using Matrix=std::array<float,16>;
Matrix identity(){Matrix m{};for(unsigned i=0;i<16;i+=5)m[i]=1;return m;}
Matrix multiply(const Matrix&a,const Matrix&b){Matrix m{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double n=0;for(unsigned k=0;k<4;++k)n+=double(a[r*4+k])*b[k*4+c];m[r*4+c]=float(n);}return m;}
struct Draw{unsigned chunk;Matrix matrix;};
struct LampDraw{unsigned before,chunk;std::array<float,3> position;bool operator==(const LampDraw&)const=default;};
int main(int argc,char**argv){try{
 if(argc!=7&&argc!=8&&argc!=10)throw std::runtime_error("image scene-index reverse path-count chunk-count output.json [weather [lamp-path lamp-chunk]] required");
 const unsigned scene=std::stoul(argv[2]),reverse=std::stoul(argv[3]),pathCount=std::stoul(argv[4]),chunkCount=std::stoul(argv[5]);
 // Snow rows16/17 both enter043600 and call19DDC0 via literal043710.
 constexpr unsigned tables[]={0xc38de4c,0xc38df24,0xc38e1ac,0xc38e284,0xc38dffc,0xc38e0d4,0xc38dc9c,0xc38dd74,0xc38213c,0xc38213c,0xc38e50c,0xc38e5e4,0xc38e35c,0xc38e434,0xc38e6bc,0xc38e794,0xc38dd74,0xc38dd74};
 if((scene>=std::size(tables)&&scene!=20)||reverse>1||pathCount<2||pathCount>20000||chunkCount>4096)throw std::runtime_error("Course capture bound");
 RefMemory mem(argv[1]);constexpr unsigned obj=0xd000000,stack=0xd020000,path=0xd040000,pathVtable=0xd040100,pathCountHook=0xd040200,stop=0xff0000;
 mem.zeroRegion(obj,0x10000);mem.zeroRegion(stack,0x10000);mem.zeroRegion(path,0x1000);
 const unsigned weather=argc>=8?std::stoul(argv[7]):0;
 const bool captureLamps=argc==10;constexpr unsigned lampData=0xd060000;
 unsigned lampCount=0,lampChunk=0;
 if(captureLamps){
  lampChunk=std::stoul(argv[9]);std::ifstream in(argv[8],std::ios::binary);
  std::vector<unsigned char> data{std::istreambuf_iterator<char>(in),{}};
  if(data.size()<8||data.size()>0x10000)throw std::runtime_error("Lamp source size bound");
  for(unsigned i=0;i<data.size();++i)mem.write8(lampData+i,data[i]);
  lampCount=mem.read32(lampData);
  if(mem.read32(lampData+4)!=3||data.size()!=8+12*lampCount||lampCount>256||lampChunk>=chunkCount)throw std::runtime_error("Lamp path header/chunk bound");
  unsigned offset=scene/2==1?0x4fc:scene/2==5?0x568:scene/2==4?0x4a8:0;
  if(!offset)throw std::runtime_error("Course has no recovered lamp list field");
  mem.write32(obj+offset,lampData);
 }
 // Original0427DC..27F0 uses the night constructor for daytime rain, except Happo scene8.
 const unsigned constructorScene=scene+((scene!=8&&!(scene&1)&&weather==1)?1:0);
 const auto vt=scene==20?0xc381ddcu:tables[constructorScene];mem.write32(obj,vt);mem.write32(obj+4,vt-40);mem.write32(obj+8,vt-64);
 mem.write32(obj+24,path);mem.write32(path,pathVtable);mem.write32(pathVtable+36,pathCountHook);
 mem.write32(obj+48,reverse);mem.write32(obj+52,scene&1);mem.write32(obj+56,weather);
 const unsigned boundaryAddress=mem.read32(mem.read32(0xc191cfc)+(scene==20?3:scene/2)*4);std::vector<unsigned> boundaries;int sentinel=0;
 for(unsigned i=0;i<128;++i){const int n=signed32(mem.read32(boundaryAddress+i*4));if(n<0){sentinel=n;break;}boundaries.push_back(unsigned(n));}
 if(boundaries.empty()||!std::is_sorted(boundaries.begin(),boundaries.end())||(sentinel!=-1&&sentinel!=-2))throw std::runtime_error("Original sector table bound");
 mem.write32(obj+36,boundaryAddress);
 RefCpu cpu(mem);Matrix current=identity();std::vector<Matrix> matrices;std::vector<Draw> draws;std::vector<LampDraw> lamps;bool lampTranslation=false,billboard=false;unsigned backgrounds=0,inversions=0,excluded=0;
 if(scene>=4&&scene!=20){for(unsigned i=0;i<((scene==12||scene==13)?6u:((scene==14||scene==15)?2u:4u));++i){const unsigned dynamic=obj+0x8000+i*0x200;mem.write32(obj+0x4e4+i*4,dynamic);mem.write32(dynamic,obj+0x9000);}mem.write32(obj+0x9000+36,pathCountHook+4);cpu.callHooks[pathCountHook+4]=[&](RefCpu&){++excluded;};}
 cpu.callHooks[pathCountHook]=[&](RefCpu&c){c.r[0]=pathCount;};
 auto draw=[&](RefCpu&c){if(c.r[5]>=chunkCount)throw std::runtime_error("Original world chunk outside bank at"+std::to_string(c.pr)+" index"+std::to_string(c.r[5]));
  if(billboard){if(c.r[5]!=lampChunk)throw std::runtime_error("Unexpected original lamp chunk");lamps.push_back({unsigned(draws.size()),c.r[5],{current[3],current[7],current[11]}});}
  else draws.push_back({c.r[5],current});};
 for(auto addr:{0xc19cec0u,0xc19cf40u,0xc19d000u,0xc19d0c0u,0xc03d740u,0xc03d7e0u})cpu.callHooks[addr]=draw;
 cpu.callHooks[0xc05a8e0]=[](RefCpu&c){if(c.r[5]!=0)throw std::runtime_error("Background selected nonzero index");c.r[0]=0xe000000;};
 cpu.callHooks[0xc1d7120]=[&](RefCpu&c){if(c.r[4]!=0xe000000)throw std::runtime_error("Unexpected direct scene draw");++backgrounds;};
 cpu.callHooks[0xc2223b8]=[](RefCpu&c){if(!c.r[5])throw std::runtime_error("Division by zero");c.fpul=std::uint32_t(signed32(c.r[4])/signed32(c.r[5]));};
 cpu.callHooks[0xc1f6610]=[&](RefCpu&c){matrices.push_back(current);if(c.r[4])for(unsigned r=0;r<4;++r)for(unsigned col=0;col<4;++col)current[r*4+col]=mem.readFloat(c.r[4]+(col*4+r)*4);};
 cpu.callHooks[0xc1f65c0]=[&](RefCpu&c){for(unsigned i=0;i<std::max(1u,c.r[4]);++i){if(matrices.empty())throw std::runtime_error("Matrix stack underflow");current=matrices.back();matrices.pop_back();}};
 cpu.callHooks[0xc1fbd60]=[&](RefCpu&c){for(unsigned i=0;i<16;++i)mem.writeFloat(c.r[4]+4*i,(i%5==0)?1.f:0.f);};
 cpu.callHooks[0xc1fc0a0]=[&](RefCpu&){++inversions;};
 cpu.callHooks[0xc1fc5a0]=[&](RefCpu&c){for(unsigned r=0;r<4;++r)for(unsigned col=0;col<4;++col)current[r*4+col]=mem.readFloat(c.r[4]+(col*4+r)*4);};
 auto translate=[&](float x,float y,float z){auto m=identity();m[3]=x;m[7]=y;m[11]=z;current=multiply(current,m);};
 cpu.callHooks[0xc1fd060]=[&](RefCpu&c){lampTranslation=captureLamps&&c.r[4]>=lampData+8&&c.r[4]<lampData+8+lampCount*12;translate(mem.readFloat(c.r[4]),mem.readFloat(c.r[4]+4),mem.readFloat(c.r[4]+8));};
 cpu.callHooks[0xc1f6ac0]=[&](RefCpu&c){translate(c.getFloat(4),c.getFloat(5),c.getFloat(6));};
 for(auto [address,axis]:std::array<std::pair<unsigned,unsigned>,3>{{{0xc1f67e0,0},{0xc1f68a0,1},{0xc1f6950,2}}})cpu.callHooks[address]=[&,axis](RefCpu&c){const float angle=float(c.r[4]&65535)*6.283185307179586f/65536.f,s=std::sin(angle),co=std::cos(angle);auto m=identity();if(axis==0){m[5]=co;m[6]=-s;m[9]=s;m[10]=co;}else if(axis==1){m[0]=co;m[2]=s;m[8]=-s;m[10]=co;}else{m[0]=co;m[1]=-s;m[4]=s;m[5]=co;}current=multiply(current,m);};
 for(auto addr:{0xc03a320u,0xc03aa40u,0xc1fb360u,0xc1f6af0u,0xc1fbf80u})cpu.callHooks[addr]=[](RefCpu&){};
 if(captureLamps){
  // Capture all lamp submissions before downstream frustum admission. D3D
  // clips the recovered quads in the native renderer; this is not a port of
  // the original culling routine.
  for(auto addr:{0xc19ce80u,0xc19cfa0u,0xc03d6e0u})cpu.callHooks[addr]=[](RefCpu&c){c.r[0]=1;};
  cpu.callHooks[0xc1f6af0]=[&](RefCpu&){if(lampTranslation){billboard=true;for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)current[row*4+col]=row==col?1.f:0.f;}};
  cpu.callHooks[0xc1fbf80]=[&](RefCpu&c){if(billboard){if(c.r[4]!=0||matrices.empty())throw std::runtime_error("Lamp matrix reset boundary");current=matrices.back();billboard=lampTranslation=false;}};
 }
 for(auto addr:{0xc042520u,0xc085220u})cpu.callHooks[addr]=[&](RefCpu&){++excluded;};
 std::map<std::string,unsigned> known;std::vector<std::string> choices;std::vector<std::vector<LampDraw>> lampChoices;std::vector<std::pair<unsigned,unsigned>> changes;unsigned long long instructions=0;unsigned lastChoice=~0u;
 const auto primary=mem.read32(vt+52),statics=mem.read32(vt+68);
 const auto initialWrites=mem.writes;
 for(unsigned index=0;index<pathCount;++index){mem.writes=initialWrites;cpu.r.fill(0);cpu.fr.fill(0);cpu.xf.fill(0);cpu.fpscrSz=false;cpu.t=false;cpu.fpul=cpu.mach=cpu.macl=0;
  unsigned sector=unsigned(std::upper_bound(boundaries.begin(),boundaries.end(),index)-boundaries.begin());if(sentinel==-2&&sector==boundaries.size())sector=0;
  mem.write32(obj+64,reverse?pathCount-1-index:index);mem.write32(obj+68,index);mem.write32(obj+72,sector);
  draws.clear();lamps.clear();lampTranslation=billboard=false;matrices.clear();current=identity();backgrounds=inversions=excluded=0;
  for(auto entry:{primary,statics}){cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;try{instructions+=cpu.run(entry,stop,30000);}catch(const std::exception&){std::cerr<<"entry="<<std::hex<<entry<<" pc="<<cpu.pc<<" pr="<<cpu.pr<<" r4="<<cpu.r[4]<<" r0="<<cpu.r[0]<<" r1="<<cpu.r[1]<<" r2="<<cpu.r[2]<<" r3="<<cpu.r[3]<<"\n";throw;}if(!matrices.empty())throw std::runtime_error("Original scene unbalanced matrix stack");}
  if(backgrounds!=1||inversions!=2)throw std::runtime_error("Original background anchoring sequence changed");
  std::ostringstream key;key<<std::setprecision(9)<<'[';for(unsigned i=0;i<draws.size();++i){if(i)key<<',';key<<"{\"chunk\":"<<draws[i].chunk<<",\"matrix\":[";for(unsigned j=0;j<16;++j){if(j)key<<',';key<<draws[i].matrix[j];}key<<"]}";}key<<']';
  auto text=key.str();auto found=known.find(text);unsigned choice;if(found==known.end()){choice=unsigned(choices.size());known.emplace(text,choice);choices.push_back(std::move(text));lampChoices.push_back(lamps);}else{choice=found->second;if(lampChoices[choice]!=lamps)throw std::runtime_error("Lamp insertion varies within a static assembly");}
  if(choice!=lastChoice){changes.emplace_back(index,choice);lastChoice=choice;}
 }
 std::ofstream out(argv[6]);out<<"{\"scene_index\":"<<scene<<",\"reverse\":"<<reverse<<",\"path_count\":"<<pathCount<<",\"vtable\":"<<vt<<",\"primary\":"<<primary<<",\"static\":"<<statics<<",\"boundary_address\":"<<boundaryAddress<<",\"sentinel\":"<<sentinel<<",\"boundaries\":[";
 for(unsigned i=0;i<boundaries.size();++i){if(i)out<<',';out<<boundaries[i];}out<<"],\"assemblies\":[";for(unsigned i=0;i<choices.size();++i){if(i)out<<',';out<<choices[i];}out<<"],\"changes\":[";for(unsigned i=0;i<changes.size();++i){if(i)out<<',';out<<'['<<changes[i].first<<','<<changes[i].second<<']';}out<<"],\"original_instructions\":"<<instructions<<",\"background_chunk\":0,\"background_anchor\":\"camera_xz\",\"excluded_dynamic_helpers_last_sample\":"<<excluded;
 if(captureLamps){out<<",\"lamp_assemblies\":["<<std::setprecision(9);for(unsigned i=0;i<lampChoices.size();++i){if(i)out<<',';out<<'[';for(unsigned j=0;j<lampChoices[i].size();++j){if(j)out<<',';const auto& l=lampChoices[i][j];out<<"{\"before\":"<<l.before<<",\"chunk\":"<<l.chunk<<",\"position\":["<<l.position[0]<<','<<l.position[1]<<','<<l.position[2]<<"]}";}out<<']';}out<<']';}
 out<<"}\n";
 std::cout<<"Captured scene"<<scene<<" reverse"<<reverse<<": "<<choices.size()<<" original assemblies, "<<changes.size()<<" exact path-index transitions, "<<instructions<<" instructions\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}







