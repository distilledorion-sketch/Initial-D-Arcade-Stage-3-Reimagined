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
 unsigned car=argc>4?std::stoul(argv[4]):0,chunkCount=argc>5?std::stoul(argv[5]):106;if(car>=35||chunkCount>10000)throw std::runtime_error("car/chunk bound");
 RefMemory mem(argv[1]);const unsigned map=mem.read32(0xc33b250+car*4);constexpr unsigned obj=0x0d000000,parts=0x0d010000,stack=0x0d020000,stop=0x00ff0000;
 mem.zeroRegion(obj,0x2000);mem.zeroRegion(parts,0x4000);mem.zeroRegion(stack,0x10000);
 std::ifstream f(argv[2],std::ios::binary);std::vector<unsigned char>b((std::istreambuf_iterator<char>(f)),{});if(b.size()!=828)throw std::runtime_error("parts size");for(unsigned i=0;i<b.size();++i)mem.write8(parts+i,b[i]);
 mem.write32(obj+0x2f0,parts);mem.write32(obj+0x354,map);mem.write32(obj+0x34c,car);for(unsigned i=0;i<=211;++i)mem.write32(obj+0x358+i*4,i);for(unsigned i=0;i<6;++i)mem.write32(obj+0x2d8+i*4,i);for(unsigned i=0;i<16;++i)mem.writeFloat(obj+84+i*4,(i%5==0)?1.0f:0.0f);
 RefCpu cpu(mem);cpu.r[15]=stack+0xf000;cpu.pr=stop;
 // Exact constructor car22 variant remap at0C0264AA..0C0264BA.
 if(car==22){mem.write32(obj+0x2e0,4);mem.write32(obj+0x2e4,2);mem.write32(obj+0x2e8,3);}
 // Execute the complete original display preset setter, including all12 bytes.
 cpu.r[4]=obj;cpu.r[5]=car;cpu.pr=stop;auto presetInstructions=cpu.run(0xc035f00,stop,10000);
 unsigned config=mem.read32(obj+0x2d4);
 std::vector<Op> ops;std::vector<std::vector<Op>> matrices;std::ofstream out(argv[3]);out<<std::setprecision(9)<<"{\"car_index\":"<<car<<",\"slot_map_address\":"<<map<<",\"preset_instructions\":"<<presetInstructions<<",\"config_word\":"<<config<<",\"draws\":[";bool first=true;unsigned calls=0,draws=0,external=0;
 auto emit=[&](unsigned slot,const char*pass,unsigned pc){unsigned semantic=mem.read32(obj+0x358+slot*4);int chunk=semantic<140?int(mem.read32(map+semantic*4)):-1;if(semantic>=140&&semantic<=186)chunk=int(mem.read32(map+semantic*4));if(semantic>211)throw std::runtime_error("semantic bound");if(chunk<0)return;if(chunk>=int(chunkCount))throw std::runtime_error("chunk bound");if(!first)out<<",";first=false;++draws;out<<"{\"slot\":"<<slot<<",\"semantic\":"<<semantic<<",\"chunk\":"<<chunk<<",\"pass\":\""<<pass<<"\",\"return_pc\":"<<pc<<",\"operations\":[";bool of=true;for(auto&o:ops){if(!of)out<<",";of=false;out<<"{\"type\":\""<<o.type<<"\",\"source\":"<<o.source<<",\"values\":[";for(unsigned j=0;j<o.values.size();++j){if(j)out<<",";out<<o.values[j];}out<<"]}";}out<<"]}";};
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
 cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;auto instructions=cpu.run(0xc026d80,stop,100000);
 out<<"],\"original_instructions\":"<<instructions<<",\"draw_attempts\":"<<calls<<",\"draw_count\":"<<draws<<",\"external_draw_helpers\":"<<external<<",\"final_matrix_depth\":"<<matrices.size()<<"}\n";
 std::cout<<"Captured "<<draws<<" draws, "<<instructions<<" instructions, config="<<std::hex<<config<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
