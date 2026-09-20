#include "original_ranking_board.h"
#include "original_ranking_data.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <stdexcept>
namespace idas3::original {
namespace {float f(unsigned word){return std::bit_cast<float>(word);}unsigned u32(const std::vector<std::uint8_t>&b,unsigned i){return unsigned(b.at(i))|(unsigned(b.at(i+1))<<8)|(unsigned(b.at(i+2))<<16)|(unsigned(b.at(i+3))<<24);}}
std::uint32_t OriginalRankingRecord::word(unsigned i)const{return unsigned(bytes.at(i))|(unsigned(bytes.at(i+1))<<8)|(unsigned(bytes.at(i+2))<<16)|(unsigned(bytes.at(i+3))<<24);}
OriginalRankingRecords OriginalRankingRecords::load(const std::filesystem::path&path){
    std::ifstream file(path,std::ios::binary);std::vector<std::uint8_t>bytes{std::istreambuf_iterator<char>(file),{}};
    if(bytes.size()!=16+53288||std::string(bytes.begin(),bytes.begin()+8)!="ID3RANK1"||u32(bytes,8)!=53288)throw std::runtime_error("Original ranking record format");
    OriginalRankingRecords out;out.seed_=u32(bytes,12);out.data_.assign(bytes.begin()+16,bytes.end());return out;
}
OriginalRankingRecord OriginalRankingRecords::record(unsigned course,unsigned direction,unsigned wet,unsigned rank)const{
    if(course>=9||direction>=2||wet>=2||rank>=10||data_.size()!=53288)throw std::out_of_range("Original ranking record selector");
    const unsigned at=8+course*5920+direction*2960+wet*1480+740+rank*16;
    OriginalRankingRecord out;std::copy_n(data_.begin()+at,16,out.bytes.begin());if(out.car()>=35)throw std::runtime_error("Original ranking car record");return out;
}
std::uint32_t OriginalRankingRecords::firstPlaceAppearance(unsigned course,unsigned direction,unsigned wet)const{
    if(course>=9||direction>=2||wet>=2||data_.size()!=53288)throw std::out_of_range("Original ranking appearance selector");
    return u32(data_,course*5920+direction*2960+wet*1480+1484);
}
OriginalRankingRecord OriginalRankingRecords::modelRecord(unsigned course,unsigned direction,unsigned wet,unsigned car)const{
    if(course>=9||direction>=2||wet>=2||car>=35||data_.size()!=53288)throw std::out_of_range("Original model ranking selector");
    //0326C0: model rows begin at+900, after ten course records at+740.
    const unsigned at=8+course*5920+direction*2960+wet*1480+900+car*16;
    OriginalRankingRecord out;std::copy_n(data_.begin()+at,16,out.bytes.begin());
    if(out.car()!=car)throw std::runtime_error("Original model ranking identity");return out;
}
std::vector<OriginalRankingBoardDraw> originalRankingRowDraws(const OriginalRankingRecord&r,float y,int age){
    using namespace ranking_data;std::vector<OriginalRankingBoardDraw> out;if(age<0)return out;
    float slide=0;if(age<=10){float t=float(age)*f(lit_0C1BE0AC);slide=f(lit_0C1BE0B0)-t;}
    auto material=[&](OriginalRankingBoardDraw&d,bool uv=false){d.overrideMaterial=slide!=0||uv;if(!d.overrideMaterial)return;int progress=10-int(slide/f(lit_0C1BE0AC));unsigned red,green,blue;if(progress<=4){red=153+20*progress;green=76+15*progress;blue=0;}else{red=255;green=230+5*(progress-5);blue=51*(progress-5);}d.color=0xff000000+(red<<16)+(green<<8)+blue;};
    auto emit=[&](unsigned chunk,float x,OriginalRankingBoardDraw::Bank bank=OriginalRankingBoardDraw::Bank::ranking){OriginalRankingBoardDraw d;d.bank=bank;d.chunk=chunk;d.position={x,y,f(lit_0C1BE0B4)};material(d);out.push_back(d);};
    float x=f(lit_0C1BE0AC)+slide;
    for(unsigned i=0;i<5&&r.bytes[4+i]!=221;++i){unsigned code=r.bytes[4+i];float left=float(code&15)*.0625f;float top=float(16-int(float(code)*.0625f))*.0625f;if(code==210)top=f(lit_0C1BE0BC)-f(lit_0C1BE0C0);
        OriginalRankingBoardDraw d;d.chunk=40;d.position={x,y,f(lit_0C1BE0B4)};d.overrideUv=true;d.u={left+.0625f,left+.0625f,left,left};d.v={top,top-.0625f,top,top-.0625f};if(code==210)d.v={top-.0625f,top,top-.0625f,top};material(d,true);out.push_back(d);x+=f(lit_0C1BE0C8);
    }
    emit(29,f(lit_0C1BE0D4)+slide);
    const auto ticks=r.word(0);const unsigned totalMs=ticks/6;const unsigned ms=totalMs%1000,sec=(totalMs/1000)%60,min=(totalMs/60000)%10;
    const std::array<unsigned,6> digits{ms%10,(ms/10)%10,ms/100,sec%10,sec/10,min};
    const std::array<unsigned,6> xs{lit_0C1BE0D8,lit_0C1BE0DC,lit_0C1BE0E0,lit_0C1BE0E4,lit_0C1BE0E8,lit_0C1BE0EC};
    for(unsigned i=0;i<6;++i)emit(digits[i]+18,f(xs[i])+slide);
    emit(carNameChunks.at(r.car()),f(lit_0C1BE0F0)+slide,OriginalRankingBoardDraw::Bank::common);
    emit((r.word(12)&1)?30:2,f(lit_0C1BE1EC)+slide);emit((r.word(12)&2)?31:17,f(lit_0C1BE1F0)+slide);return out;
}
std::vector<OriginalRankingBoardDraw> originalRankingBoardDraws(const OriginalRankingRecords&records,unsigned course,unsigned direction,unsigned wet,unsigned frame){
    using namespace ranking_data;if(course>=9||direction>=2||wet>=2)throw std::out_of_range("Original ranking board selector");
    std::vector<OriginalRankingBoardDraw> out;auto fixed=[&](unsigned chunk,Vec3 p={}){OriginalRankingBoardDraw d;d.chunk=chunk;d.position=p;out.push_back(d);};
    fixed(48);fixed(courseChunks[course]);fixed(routeChunks[course*2+direction]);if(course!=8)fixed(wet?43:42);fixed(47);fixed(3);fixed(50);
    float y=f(lit_0C1BDBC8);for(unsigned i=0;i<10;++i){if(i&1)fixed(0,{0,y+f(lit_0C1BDBCC),f(lit_0C1BDBD4)});
        auto record=records.record(course,direction,wet,i);if(course==8)record.bytes[12]|=2;
        auto row=originalRankingRowDraws(record,y,int(frame)-int(i*8));out.insert(out.end(),row.begin(),row.end());y-=f(lit_0C1BDBE0);
    }return out;
}
void OriginalRankingBoard::load(const std::filesystem::path&root){auto base=root/"data/original_assets/attract/ranking";ranking_=NativeModel::load(base/"v3sT13rankin/v3sT13rankin.idasmesh");common_=NativeModel::load(base/"v3sT00common/v3sT00common.idasmesh");rankingTextures_=NativeTextureBank::load(base/"v3sT13rankin/textures/textures.idastex");commonTextures_=NativeTextureBank::load(base/"v3sT00common/textures/textures.idastex");}
std::vector<OriginalRankingBoardDraw> originalModelRankingBoardDraws(const OriginalRankingRecords&records,unsigned course,unsigned direction,unsigned wet,unsigned frame,unsigned page){
    using namespace ranking_data;if(course>=9||direction>=2||wet>=2||page>=4)throw std::out_of_range("Original model ranking page");
    // Exact source2AADBC car order and inclusive manufacturer bounds2AAE48.
    constexpr std::array<unsigned,35> order{0,1,2,3,4,5,6,7,8,31,9,13,14,10,11,12,16,15,17,18,19,20,32,33,21,22,23,24,34,25,26,27,28,29,30};
    constexpr std::array<std::array<unsigned,2>,7> groups{{{0,6},{7,15},{16,19},{20,24},{25,30},{31,33},{34,34}}};
    std::vector<OriginalRankingBoardDraw> out;auto fixed=[&](unsigned chunk){OriginalRankingBoardDraw d;d.chunk=chunk;out.push_back(d);};
    fixed(46);fixed(page+4);fixed(page==3?49:50); //1BD9A0, before1BDC00
    fixed(48);fixed(courseChunks[course]);fixed(routeChunks[course*2+direction]);if(course!=8)fixed(wet?43:42);
    const auto block=[&](unsigned group,float y){unsigned index=0;for(unsigned slot=groups[group][0];slot<=groups[group][1];++slot,++index){
        auto record=records.modelRecord(course,direction,wet,order[slot]);if(course==8)record.bytes[12]|=2;
        auto row=originalRankingRowDraws(record,y,int(frame)-int(index*8));out.insert(out.end(),row.begin(),row.end());y-=.2f;
    }};
    //1BDC00 emits two independently staggered blocks on pages0,2,3.
    switch(page){case 0:block(0,-1.54f);block(6,-3.7f);break;case 1:block(1,-1.54f);break;
        case 2:block(3,-1.54f);block(2,-3.3f);break;case 3:block(4,-1.54f);block(5,-3.5f);break;}
    return out;
}
void OriginalRankingBoard::paint(std::span<std::uint32_t>target,int width,int height,const OriginalRankingRecords&records,unsigned course,unsigned direction,unsigned wet,unsigned frame,bool modelMode,unsigned modelPage)const{
    if(width!=640||height!=480||target.size()<640*480)throw std::invalid_argument("Original ranking source canvas must be640x480");
    SpritePlacement p;p.scale=100;p.invertY=true;p.authoredHeight=0;
    const auto draws=modelMode?originalModelRankingBoardDraws(records,course,direction,wet,frame,modelPage):originalRankingBoardDraws(records,course,direction,wet,frame);
    for(const auto&d:draws){const auto&model=d.bank==OriginalRankingBoardDraw::Bank::ranking?ranking_:common_;const auto&textures=d.bank==OriginalRankingBoardDraw::Bank::ranking?rankingTextures_:commonTextures_;auto chunk=model.chunks.at(d.chunk);
        // These authored multi-manufacturer headers submit their background
        // after the logos, relying on original Z testing. Our2D overlay has no
        // depth buffer, so preserve that occlusion by painting far to near.
        if(modelMode&&d.bank==OriginalRankingBoardDraw::Bank::ranking&&d.chunk==modelPage+4){
            const auto depth=[](const NativeModelBatch& b){return b.vertices.empty()?0.f:b.vertices.front().position.z;};
            std::stable_sort(chunk.batches.begin(),chunk.batches.end(),[&](const auto&a,const auto&b){return depth(a)<depth(b);});
        }
        unsigned vertex=0;for(auto&batch:chunk.batches){if(d.overrideMaterial){batch.material[3]=batch.material[5]=d.color;}for(auto&v:batch.vertices){v.position+=d.position;if(d.overrideUv){if(vertex>=4)throw std::runtime_error("Ranking font quad bounds");v.u=d.u[vertex];v.v=d.v[vertex];++vertex;}}}
        compositeOriginalMenuChunk(target,width,height,textures,chunk,p);
    }
}
}
