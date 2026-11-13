#include "online_result_reward.h"
#include "local_save_slots.h"
#include "online_car_selection.h"
#include <algorithm>
#include <stdexcept>

namespace idas3 {
std::filesystem::path OnlineResultReward::directory(int selection)const{
    if(root_.empty()||!root_.is_absolute())throw std::invalid_argument("An absolute reward save root is required");
    const auto selected=decodeOnlineCarSelection(selection);
    // Primary aliases can change when a driver changes cars. Intent must name
    // the actual slot and car (or the explicitly selected legacy car).
    if(selected.car<0)throw std::invalid_argument("Reward selection must identify an exact car");
    if(selected.slot<0)return root_/"driver_profiles_v1";
    const LocalSaveSlots slots(root_/"saves");
    if(!slots.at(unsigned(selected.slot)).used)throw std::runtime_error("The reward's driver slot no longer exists");
    return slots.profileDirectory(unsigned(selected.slot));
}
OnlineResultReward::Plan OnlineResultReward::prepare(int selection,
        const original::OriginalBattleProfile& selected,bool won,bool awardPoints)const{
    const auto target=decodeOnlineCarSelection(selection);
    const LocalDriverProfiles profiles(directory(selection));
    if(target.car<0||selected.u(16)!=unsigned(target.car)||selected.u(72)>999999999)
        throw std::invalid_argument("Invalid selected reward profile");
    const auto stored=profiles.load(unsigned(target.car));
    if(stored.origin==LocalDriverProfiles::Origin::Unreadable)
        throw std::runtime_error("Online tuning profile could not be read; existing files were preserved");
    Plan plan;plan.selection=selection;plan.before=stored.profile;plan.after=selected;
    const auto balance=selected.u(72);
    const auto earned=std::min(std::uint64_t(balance)+(awardPoints?(won?2000u:1000u):0u),std::uint64_t(999999999));
    plan.after.setu(72,std::uint32_t(earned));plan.points=std::uint32_t(earned-balance);
    // Card entry owns the counter. Results (including rematches and timeouts)
    // retain its persisted value rather than charging another use.
    plan.after.setu(1140,plan.before.u(1140));
    return plan;
}
void OnlineResultReward::apply(const Plan& plan)const{
    const auto selected=decodeOnlineCarSelection(plan.selection);
    const LocalDriverProfiles profiles(directory(plan.selection));
    if(selected.car<0||plan.before.u(16)!=unsigned(selected.car)||plan.after.u(16)!=unsigned(selected.car)||
       plan.points>2000||plan.after.u(72)>999999999||plan.after.u(72)<plan.points||
       plan.recordSeason==0||plan.recordSeason>timeAttackSeason)
        throw std::invalid_argument("Invalid online reward plan");
    auto before=plan.before,after=plan.after;
    if(plan.recordSeason<timeAttackSeason){
        // A pre-season crash receipt still owes its exact tuning reward. Its
        // embedded TA data belongs to the old season, however. Preserve current
        // TA fields (including new Season 2 records) in BOTH comparison images;
        // every other word retains the original conflict/idempotency guard.
        const auto current=profiles.load(unsigned(selected.car));
        if(current.origin==LocalDriverProfiles::Origin::Unreadable)
            throw std::runtime_error("Online tuning profile could not be recovered; existing files were preserved");
        for(unsigned index=0;index<36;++index){
            for(unsigned offset:{176+4*index,320+4*index,500+12*index,504+12*index,508+12*index}){
                before.setu(offset,current.profile.u(offset));after.setu(offset,current.profile.u(offset));
            }
        }
    }
    using Outcome=LocalDriverProfiles::ConditionalSave;
    switch(profiles.saveExpected(unsigned(selected.car),before,after)){
    case Outcome::Saved:case Outcome::AlreadySaved:return;
    case Outcome::Conflict:throw std::runtime_error("Online tuning profile changed after the result; existing progress was preserved");
    case Outcome::Unreadable:throw std::runtime_error("Online tuning profile could not be recovered; existing files were preserved");
    case Outcome::Invalid:throw std::runtime_error("Invalid online tuning profile in result intent");
    default:throw std::runtime_error("Online tuning points could not be saved; retry is safe");
    }
}
}
