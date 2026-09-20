#include "original_ranking_board.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;using namespace idas3::original;using namespace idas3::reference;
constexpr unsigned obj=0x0d000000,stack=0x0d080000,stop=0x00ff0000,recordAt=obj+0x2000,rankBank=obj+0x3000,commonBank=obj+0x4000,chunkList=obj+0x5000,commonList=obj+0x6000,vt=obj+0x7000;
void require(bool value,const char*m){if(!value)throw std::runtime_error(m);}
void bmp(const std::filesystem::path&p,const std::vector<unsigned>&pixels){std::ofstream f(p,std::ios::binary);auto u16=[&](unsigned x){f.put(char(x));f.put(char(x>>8));};auto u32=[&](unsigned x){u16(x);u16(x>>16);};f.write("BM",2);u32(54+640*480*4);u32(0);u32(54);u32(40);u32(640);u32(unsigned(-480));u16(1);u16(32);u32(0);u32(640*480*4);u32(0);u32(0);u32(0);u32(0);f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);}
int main(int argc,char**argv){try{
 require(argc==4,"canonicalImage gameRoot outputDir");RefMemory m(argv[1]);auto root=std::filesystem::path(argv[2]);auto records=OriginalRankingRecords::load(root/"data/original_assets/attract/ranking/factory_records.idasrank");unsigned long long instructions=0,comparisons=0;
 for(unsigned sample=0;sample<120;++sample){
  m.clear();m.zeroRegion(obj,0x100000);RefCpu c(m);c.r[15]=stack;c.pr=stop;
  auto record=records.record(sample%9,sample%2,(sample/2)%2,sample%10);if(sample%5==0)record.bytes[4]=210;
  for(unsigned i=0;i<16;++i)m.write8(recordAt+i,record.bytes[i]);
  m.write32(obj+12,rankBank);m.write32(obj+16,commonBank);m.write32(rankBank,vt);m.write32(commonBank,vt);m.write32(rankBank+16,chunkList);m.write32(commonBank+16,commonList);m.write32(vt+60,0x00ff0010);
  for(unsigned i=0;i<51;++i)m.write32(chunkList+i*4,obj+0x10000+i*256);
  for(unsigned i=0;i<35;++i)m.write32(commonList+i*4,obj+0x20000+i*256);
  const int age=int(sample%15)-2;const float y=-1.455f-float(sample%10)*.365f;auto expected=originalRankingRowDraws(record,y,age);unsigned next=0;float slide=0;bool uv=false;std::array<float,4>us{},vs{};
  c.callHooks[0xc1bdd40]=[&](auto&cpu){slide=cpu.getFloat(4);uv=cpu.r[5]&&cpu.r[6];if(uv)for(unsigned i=0;i<4;++i){us[i]=m.readFloat(cpu.r[5]+4*i);vs[i]=m.readFloat(cpu.r[6]+4*i);}cpu.r[0]=(slide!=0||uv)?1:0;};
  c.callHooks[0x00ff0010]=[&](auto&cpu){require(next<expected.size(),"extra row draw");const auto&d=expected[next++];require(d.chunk==cpu.r[5],"row chunk");require((d.bank==OriginalRankingBoardDraw::Bank::common)==(cpu.r[4]==commonBank),"row bank");
   for(auto[index,v]:std::initializer_list<std::pair<unsigned,float>>{{0,d.position.x},{1,d.position.y},{2,d.position.z}}){++comparisons;if(std::bit_cast<unsigned>(v)!=m.read32(cpu.r[6]+index*4))throw std::runtime_error("Ranking row coordinate differs sample "+std::to_string(sample)+" draw "+std::to_string(next-1)+" component "+std::to_string(index));}
   require(d.overrideMaterial==bool(cpu.r[7]),"material override");require(d.overrideUv==uv,"UV flag");if(uv)for(unsigned i=0;i<4;++i){require(std::bit_cast<unsigned>(d.u[i])==std::bit_cast<unsigned>(us[i]),"U override");require(std::bit_cast<unsigned>(d.v[i])==std::bit_cast<unsigned>(vs[i]),"V override");comparisons+=2;}
  };
  c.callHooks[0xc2223e0]=[](auto&cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
  c.r[4]=obj;c.r[5]=recordAt;c.r[6]=unsigned(-1);c.r[7]=unsigned(age);c.setFloat(4,y);instructions+=c.run(0xc1bde80,stop,100000);require(next==expected.size(),"missing row draw");
 }
 // Whole total-board callback, including original032680 pointer arithmetic,
 // header selection, row reveal scheduling and alternate stripe placement.
 std::ifstream recordFile(root/"data/original_assets/attract/ranking/factory_records.idasrank",std::ios::binary);std::vector<unsigned char>raw{std::istreambuf_iterator<char>(recordFile),{}};
 for(unsigned sample=0;sample<360;++sample){
  m.clear();m.zeroRegion(obj,0x100000);RefCpu c(m);c.r[15]=stack;c.pr=stop;
  for(unsigned i=16;i<raw.size();++i)m.write8(obj+0x40000+i-16,raw[i]);
  const unsigned course=sample%9,direction=(sample/9)%2,wet=(sample/18)%2,frame=(sample%4)*30+1;
  const unsigned mode=sample/72;
  auto expected=mode?originalModelRankingBoardDraws(records,course,direction,wet,frame,mode-1):originalRankingBoardDraws(records,course,direction,wet,frame);unsigned next=0;bool uv=false;std::array<float,4>us{},vs{};
  m.write32(obj+4,frame-1);m.write32(obj+8,obj+0x40000);m.write32(obj+12,rankBank);m.write32(obj+16,commonBank);m.write32(rankBank,vt);m.write32(commonBank,vt);m.write32(rankBank+16,chunkList);m.write32(commonBank+16,commonList);m.write32(vt+44,0x00ff0020);m.write32(vt+60,0x00ff0010);
  for(unsigned i=0;i<51;++i)m.write32(chunkList+i*4,obj+0x10000+i*256);for(unsigned i=0;i<35;++i)m.write32(commonList+i*4,obj+0x20000+i*256);
  auto check=[&](auto&cpu,bool transient){require(next<expected.size(),"extra total-board draw");const auto&d=expected[next++];if(d.chunk!=cpu.r[5])throw std::runtime_error("Total board chunk differs sample "+std::to_string(sample)+" draw "+std::to_string(next-1));require((d.bank==OriginalRankingBoardDraw::Bank::common)==(cpu.r[4]==commonBank),"total board bank");if(transient){for(auto[i,v]:std::initializer_list<std::pair<unsigned,float>>{{0,d.position.x},{1,d.position.y},{2,d.position.z}}){++comparisons;if(std::bit_cast<unsigned>(v)!=m.read32(cpu.r[6]+4*i))throw std::runtime_error("Total board coordinate sample "+std::to_string(sample)+" draw "+std::to_string(next-1)+" component "+std::to_string(i));}require(d.overrideMaterial==bool(cpu.r[7]),"total board material override");if(d.overrideUv)for(unsigned i=0;i<4;++i){require(std::bit_cast<unsigned>(d.u[i])==std::bit_cast<unsigned>(us[i]),"total board U");require(std::bit_cast<unsigned>(d.v[i])==std::bit_cast<unsigned>(vs[i]),"total board V");comparisons+=2;}}};
  c.callHooks[0x00ff0020]=[&](auto&cpu){check(cpu,false);};c.callHooks[0x00ff0010]=[&](auto&cpu){check(cpu,true);};c.callHooks[0xc1d0880]=[](auto&){};
  c.callHooks[0xc1bdd40]=[&](auto&cpu){uv=cpu.r[5]&&cpu.r[6];if(uv)for(unsigned i=0;i<4;++i){us[i]=m.readFloat(cpu.r[5]+4*i);vs[i]=m.readFloat(cpu.r[6]+4*i);}cpu.r[0]=(cpu.getFloat(4)!=0||uv)?1:0;};c.callHooks[0xc2223e0]=[](auto&cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
  if(mode){c.r[4]=obj;c.r[5]=mode-1;c.r[6]=1;instructions+=c.run(0xc1bd9a0,stop,100000);c.pr=stop;m.write32(stack,mode-1);}
  else{m.write32(stack,unsigned(-1));m.write32(stack+4,0);}
  c.r[4]=obj;c.r[5]=course;c.r[6]=direction;c.r[7]=wet;instructions+=c.run(mode?0xc1bdc00:0xc1bda20,stop,100000);require(next==expected.size(),"missing total/model board draw");require(m.read32(obj+4)==frame,"source board frame increment");
 }
 // Execute1BDD40's exact material/color and per-vertex UV mutation. Synthetic
 // parser events expose one GMP and four VUR records; parser/allocation are
 // explicit boundaries, while all mutation arithmetic executes source bytes.
 for(int age=0;age<12;++age){
  auto record=records.record(3,0,0,0);record.bytes[4]=210;auto d=originalRankingRowDraws(record,-1.45f,age).front();
  m.clear();m.zeroRegion(obj,0x100000);RefCpu c(m);c.r[15]=stack;c.pr=stop;c.r[4]=obj+0x10000;c.r[5]=obj+0x11000;c.r[6]=obj+0x12000;
  const float slide=age<=10?std::bit_cast<float>(0x40cccccdU)-float(age)*std::bit_cast<float>(0x3f23d70aU):0.f;c.setFloat(4,slide);
  for(unsigned i=0;i<4;++i){m.writeFloat(c.r[5]+4*i,d.u[i]);m.writeFloat(c.r[6]+4*i,d.v[i]);}
  c.callHooks[0xc1d5400]=[&](auto&cpu){cpu.r[0]=obj+0x10000;};c.callHooks[0xc1d4be0]=[](auto&){};unsigned event=0;
  c.callHooks[0xc1d4c40]=[&](auto&cpu){unsigned type=event==0?1:event<=4?3:0x80000000;unsigned ptr=obj+0x13000+event*64;m.write32(cpu.r[5],type);cpu.r[0]=ptr;++event;};
  instructions+=c.run(0xc1bdd40,stop,10000);require(m.read32(obj+0x13000+12)==d.color,"source ranking material color");require(m.read32(obj+0x13000+20)==d.color,"source ranking specular color");comparisons+=2;
  for(unsigned i=0;i<4;++i){require(m.read32(obj+0x13000+(i+1)*64+16)==std::bit_cast<unsigned>(d.u[i]),"source material U");require(m.read32(obj+0x13000+(i+1)*64+20)==std::bit_cast<unsigned>(d.v[i]),"source material V");comparisons+=2;}
 }
 OriginalRankingBoard board;board.load(root);std::filesystem::path dest(argv[3]);std::filesystem::create_directories(dest);for(unsigned frame:{1u,10u,40u,100u}){std::vector<unsigned>pixels(640*480,0xff101820);board.paint(pixels,640,480,records,3,0,0,frame);bmp(dest/("ranking-"+std::to_string(frame)+".bmp"),pixels);}
 for(unsigned page=0;page<4;++page){std::vector<unsigned>pixels(640*480,0xff101820);board.paint(pixels,640,480,records,3,0,0,83,true,page);bmp(dest/("model-ranking-"+std::to_string(page)+".bmp"),pixels);}
 std::cout<<"PASS Ranking rows:120 cases,360 total/model boards, "<<comparisons<<" coordinate/UV comparisons, "<<instructions<<" original instructions; eight CPU previews\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
