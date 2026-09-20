#pragma once
#include "original_battle_hud.h"
#include "original_battle_profile.h"
#include <array>
#include <string>

namespace idas3 {
struct OriginalNameGlyph {std::uint32_t texture=0;float x=0,y=0,width=0,height=0;};
struct OriginalOnlineBattleName {
    std::string text,carCode;
    std::vector<OriginalNameGlyph> glyphs;
};
// Settled original normal-layout name objects, including0CF620's conversion
// of the five profile glyph slots into the original double-byte font codes.
class OriginalBattleNames {
public:
    static OriginalBattleNames load(const std::filesystem::path& root);
    std::span<const std::uint8_t> sourceName(std::uint32_t enemy,bool player=false)const;
    std::vector<std::uint8_t> sourcePlayerName(const original::OriginalBattleProfile&)const;
    std::vector<OriginalNameGlyph> glyphs(std::uint32_t enemy,bool player=false,
        const original::OriginalBattleProfile* profile=nullptr)const;
    void paintTimeAttack(std::span<std::uint32_t>,int,int,std::uint32_t,const original::OriginalBattleProfile*)const;
    void paint(std::span<std::uint32_t> argb,int width,int height,
        std::uint32_t enemy,std::uint32_t profileMode,std::uint32_t playerCar,std::uint32_t rivalCar,
        const original::OriginalBattleProfile* profile=nullptr)const;
    // Source profile3 positions and profile font; Unicode conversion is the
    // native network-name adapter. Longer names fit the original name column.
    OriginalOnlineBattleName onlineName(const std::string& utf8,bool player,std::uint32_t car)const;
    std::array<OriginalOnlineBattleName,2> paintOnline(std::span<std::uint32_t> argb,int width,int height,
        const std::string& playerName,const std::string& rivalName,std::uint32_t playerCar,std::uint32_t rivalCar)const;
private:
    std::array<std::array<std::uint8_t,16>,32> names_{};
    std::array<std::array<std::uint8_t,2>,221> profileGlyphs_{};
    std::array<std::uint16_t,9216> indices_{};
    std::array<std::uint32_t,9216> unicode_{};
    void paintGlyphs(std::span<std::uint32_t>,int,int,std::span<const OriginalNameGlyph>)const;
    void paintLabels(std::span<std::uint32_t>,int,int,std::uint32_t,std::uint32_t,float,float,bool playerOnly=false)const;
    NativeTextureBank font_;
    OriginalBattleHudAssets labels_;
};
}
