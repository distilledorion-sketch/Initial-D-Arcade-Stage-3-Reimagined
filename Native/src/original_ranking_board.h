#pragma once
#include "native_assets.h"
#include <array>
namespace idas3::original {
struct OriginalRankingRecord {
    std::array<std::uint8_t,16> bytes{};
    std::uint32_t word(unsigned offset)const;
    unsigned car()const{return word(8)>>26;}
};
class OriginalRankingRecords {
public:
    static OriginalRankingRecords load(const std::filesystem::path&);
    OriginalRankingRecord record(unsigned course,unsigned direction,unsigned wet,unsigned rank)const;
    OriginalRankingRecord modelRecord(unsigned course,unsigned direction,unsigned wet,unsigned car)const;
    //02F5AA..02F5DA uses this page's separate top-car appearance word.
    std::uint32_t firstPlaceAppearance(unsigned course,unsigned direction,unsigned wet)const;
    std::uint32_t factorySeed()const{return seed_;}
private:
    std::vector<std::uint8_t> data_;
    std::uint32_t seed_=0;
};
struct OriginalRankingBoardDraw {
    enum class Bank {ranking,common} bank=Bank::ranking;
    unsigned chunk=0;Vec3 position{};
    bool overrideMaterial=false,overrideUv=false;
    std::uint32_t color=0xffffffff;
    std::array<float,4> u{},v{};
};
std::vector<OriginalRankingBoardDraw> originalRankingRowDraws(const OriginalRankingRecord&,float y,int age);
std::vector<OriginalRankingBoardDraw> originalRankingBoardDraws(const OriginalRankingRecords&,unsigned course,unsigned direction,unsigned wet,unsigned sourceFrame);
//1BD9A0 +1BDC00: four manufacturer-grouped model-best pages. These belong
// to attract child12; the post-race ARankinTA owner remains course-only.
std::vector<OriginalRankingBoardDraw> originalModelRankingBoardDraws(const OriginalRankingRecords&,unsigned course,unsigned direction,unsigned wet,unsigned sourceFrame,unsigned page);
class OriginalRankingBoard {
public:
    void load(const std::filesystem::path& root);
    void paint(std::span<std::uint32_t>,int width,int height,const OriginalRankingRecords&,unsigned course,unsigned direction,unsigned wet,unsigned sourceFrame,bool modelMode=false,unsigned modelPage=0)const;
private: NativeModel ranking_,common_;NativeTextureBank rankingTextures_,commonTextures_;
};
}
