#pragma once
#include "local_driver_profiles.h"

namespace idas3 {
// A result journal retains the exact disk before-image and intended after-image.
// The latter comes from the selected garage profile, not its race physics copy.
class OnlineResultReward {
public:
    struct Plan {
        int selection=-1;
        std::uint32_t points=0;
        original::OriginalBattleProfile before{},after{};
    };
    explicit OnlineResultReward(std::filesystem::path saveRoot):root_(std::move(saveRoot)){}
    Plan prepare(int selection,const original::OriginalBattleProfile& selected,bool won)const;
    void apply(const Plan& plan)const;
private:
    std::filesystem::path directory(int selection)const;
    std::filesystem::path root_;
};
}
