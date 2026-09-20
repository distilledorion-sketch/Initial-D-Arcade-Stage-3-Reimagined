#include "sh4_scalar_reference.h"
// Development-only original-opcode selection capture. No renderer or device emulation.
// Matrix helper inputs are captured at explicit call boundaries; emitted matrices
// are derived separately and do not claim hardware FSCA/FTRV bit parity.
#include <iostream>
#include <iomanip>
using namespace idas3::reference;
struct Op { std::string type; std::vector<float> values; unsigned source=0; };
int main(int argc,char**argv){try{
 if(argc<4)throw std::runtime_error("image parts output required");
 unsigned car=argc>4?std::stoul(argv[4]):0,chunkCount=argc>5?std::stoul(argv[5]):106;int enemy=argc>6?std::stoi(argv[6]):-1;const bool lights=argc>7&&std::stoi(argv[7])!=0;if(car>=35||chunkCount>10000||enemy < -1||enemy>30)throw std::runtime_error("car/chunk/enemy bound");
 const unsigned color=argc>8?std::stoul(argv[8]):0;if(color>=8||(enemy>=0&&color))throw std::runtime_error("Player paint selection bounds");
 RefMemory mem(argv[1]);const unsigned map=mem.read32(0xc33b250+car*4);constexpr unsigned obj=0x0d000000,parts=0x0d010000,stack=0x0d020000,stop=0x00ff0000;
 mem.zeroRegion(obj,0x2000);mem.zeroRegion(parts,0x4000);mem.zeroRegion(stack,0x10000);
 std::ifstream f(argv[2],std::ios::binary);std::vector<unsigned char>b((std::istreambuf_iterator<char>(f)),{});if(b.size()!=828)throw std::runtime_error("parts size");for(unsigned i=0;i<b.size();++i)mem.write8(parts+i,b[i]);
 mem.write32(obj+0x2f0,parts);mem.write32(obj+0x354,map);mem.write32(obj+0x34c,car);for(unsigned i=0;i<=211;++i)mem.write32(obj+0x358+i*4,i);for(unsigned i=0;i<6;++i)mem.write32(obj+0x2d8+i*4,i);for(unsigned i=0;i<16;++i)mem.writeFloat(obj+84+i*4,(i%5==0)?1.0f:0.0f);
 RefCpu cpu(mem);cpu.r[15]=stack+0xf000;cpu.pr=stop;
 // Compiler unsigned division is an explicit arithmetic-service boundary.
 cpu.callHooks[0xc2223b8]=[](auto&c){if(!c.r[5])throw std::runtime_error("Appearance divisor zero return="+std::to_string(c.pr)+" numerator="+std::to_string(c.r[4]));c.fpul=c.r[4]/c.r[5];};
 // Exact constructor car22 variant remap at0C0264AA..0C0264BA.
 if(car==22){mem.write32(obj+0x2e0,4);mem.write32(obj+0x2e4,2);mem.write32(obj+0x2e8,3);}
 // Original constructor config clear and car identity/slot-map binding.
 cpu.r[4]=obj+0x2d4;cpu.pr=stop;auto presetInstructions=cpu.run(0xc228dc0,stop,10000);
 constexpr unsigned constructorFrame=0xd050000;mem.zeroRegion(constructorFrame,0x1000);
 mem.write32(constructorFrame+48,obj);mem.write32(constructorFrame+52,car);cpu.r[14]=constructorFrame;
 presetInstructions+=cpu.run(0xc026436,0xc0264bc,10000);
 if(enemy>=0){
   //063720 creates the model by profile+20, then063738 passes ENEMY+24.
   cpu.r[4]=obj;cpu.r[5]=unsigned(enemy);cpu.pr=stop;presetInstructions+=cpu.run(0xc035f00,stop,10000);
 }else{
   cpu.pr=stop;presetInstructions+=cpu.run(0xc134a60,stop,100000);
   mem.write32(0xc31c99c+16,car);
   mem.write32(0xc31c99c+64,color);
   constexpr unsigned scene=0xd040000;mem.zeroRegion(scene,0x3000);mem.write32(scene+1048,obj);
   cpu.r[10]=scene+1020;cpu.r[13]=scene;cpu.r[9]=scene;cpu.r[15]=stack+0xf000;
   // Actual player call block, through all tuning setters, before029040.
   presetInstructions+=cpu.run(0xc0630b4,0xc06316e,100000);
   const unsigned digits[]={2,2,9,3,6};for(unsigned i=0;i<5;++i)mem.write8(obj+0x6b4+i,digits[i]);
 }
 //029040 first rebuilds the semantic slot array and applies car/appearance
 //visibility. Stop before02988E's material-alpha/paint preparation boundary.
 cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;
 presetInstructions+=cpu.run(0xc029040,0xc02988e,10000);
 unsigned config=mem.read32(obj+0x2d4);
 std::vector<Op> ops;std::vector<std::vector<Op>> matrices;std::ofstream out(argv[3]);out<<std::setprecision(9)<<"{\"car_index\":"<<car<<",\"enemy_id\":"<<enemy<<",\"slot_map_address\":"<<map<<",\"preset_instructions\":"<<presetInstructions<<",\"config_word\":"<<config<<",\"plate_digits\":[";for(unsigned i=0;i<5;++i){if(i)out<<",";out<<unsigned(mem.read8(obj+0x6b4+i));}out<<"],\"draws\":[";bool first=true;unsigned calls=0,draws=0,external=0;
 //026100 resolves every semantic slot0..211;140..186 use material copies of the same geometry.
 auto emit=[&](unsigned slot,const char*pass,unsigned pc){unsigned semantic=mem.read32(obj+0x358+slot*4);if(semantic==0xffffffffu)return;if(semantic>211)throw std::runtime_error("semantic bound");int chunk=int(mem.read32(map+semantic*4));if(chunk<0)return;if(chunk>=int(chunkCount))throw std::runtime_error("chunk bound");if(!first)out<<",";first=false;++draws;out<<"{\"slot\":"<<slot<<",\"semantic\":"<<semantic<<",\"chunk\":"<<chunk<<",\"pass\":\""<<pass<<"\",\"return_pc\":"<<pc<<",\"operations\":[";bool of=true;for(auto&o:ops){if(!of)out<<",";of=false;out<<"{\"type\":\""<<o.type<<"\",\"source\":"<<o.source<<",\"values\":[";for(unsigned j=0;j<o.values.size();++j){if(j)out<<",";out<<o.values[j];}out<<"]}";}out<<"]}";};
 cpu.callHooks[0xc025fe0]=[&](RefCpu&c){++calls;emit(c.r[5],"material",c.pr);};cpu.callHooks[0xc026040]=[&](RefCpu&c){++calls;emit(c.r[5],"direct",c.pr);};
 cpu.callHooks[0xc1f6610]=[&](RefCpu&c){matrices.push_back(ops);if(c.r[4]){std::vector<float>m;for(unsigned i=0;i<16;++i)m.push_back(mem.readFloat(c.r[4]+i*4));ops={{"load_matrix",m,c.r[4]}};}};
 cpu.callHooks[0xc1f65c0]=[&](RefCpu&c){unsigned n=std::max(1u,c.r[4]);for(unsigned i=0;i<n;++i){if(matrices.empty())throw std::runtime_error("matrix stack underflow");ops=matrices.back();matrices.pop_back();}};
 cpu.callHooks[0xc1fd060]=[&](RefCpu&c){ops.push_back({"translate",{mem.readFloat(c.r[4]),mem.readFloat(c.r[4]+4),mem.readFloat(c.r[4]+8)},c.r[4]});};
 cpu.callHooks[0xc1f6ac0]=[&](RefCpu&c){ops.push_back({"translate",{c.getFloat(4),c.getFloat(5),c.getFloat(6)},c.pr});};
 cpu.callHooks[0xc1f69d0]=[&](RefCpu&c){ops.push_back({"scale",{c.getFloat(4),c.getFloat(5),c.getFloat(6)},c.pr});};
 for(auto [addr,type]:std::vector<std::pair<unsigned,std::string>>{{0xc1f67e0,"rotate_x_u16"},{0xc1f68a0,"rotate_y_u16"},{0xc1f6950,"rotate_z_u16"}})cpu.callHooks[addr]=[&,type](RefCpu&c){ops.push_back({type,{float(c.r[4])},c.pr});};
 for(auto [addr,type]:std::vector<std::pair<unsigned,std::string>>{{0xc1f6770,"rotate_x_float"},{0xc1f6780,"rotate_y_float"}})cpu.callHooks[addr]=[&,type](RefCpu&c){ops.push_back({type,{c.getFloat(4)},c.pr});};
 for(auto addr:{0xc1d77e0u,0xc1d78a0u,0xc1d8380u,0xc1db280u})cpu.callHooks[addr]=[](RefCpu&){};
 cpu.callHooks[0xc026160]=[&](RefCpu&){++external;}; // separately loaded number plate model; excluded from car bank
 cpu.callHooks[0xc029ee0]=[&](RefCpu&){++external;}; // motion blur tire pass, neutral intensity0
 mem.write8(obj+80,argc>9&&std::stoi(argv[9])!=0?1:0);mem.write8(obj+81,lights?1:0);cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;auto instructions=cpu.run(0xc026d80,stop,100000);
 out<<"],\"original_instructions\":"<<instructions<<",\"draw_attempts\":"<<calls<<",\"draw_count\":"<<draws<<",\"external_draw_helpers\":"<<external<<",\"final_matrix_depth\":"<<matrices.size()<<",\"popup_motor\":"<<mem.read32(obj+0x6c4)<<",\"popup_counter\":"<<mem.read32(obj+0x6c8)<<",\"popup_phase\":"<<mem.read32(obj+0x6d0)<<",\"popup_visible\":"<<mem.read32(obj+0x6d4)<<"}\n";
 std::cout<<"Captured "<<draws<<" draws, "<<instructions<<" instructions, config="<<std::hex<<config<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}

