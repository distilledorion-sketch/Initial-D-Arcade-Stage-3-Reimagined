#pragma once
#include "original_contact.h"
#include <filesystem>
#include <vector>

namespace idas3::original {
struct OriginalRivalState {
    static constexpr std::size_t byteSize=716;
    std::array<std::uint32_t,byteSize/4> words{};
    float f(std::size_t offset)const{return std::bit_cast<float>(words.at(offset/4));}
    std::uint32_t u(std::size_t offset)const{return words.at(offset/4);}
    void setf(std::size_t offset,float value){words.at(offset/4)=std::bit_cast<std::uint32_t>(value);}
    void setu(std::size_t offset,std::uint32_t value){words.at(offset/4)=value;}
};
struct OriginalRivalPath {
    std::uint32_t condition{},inclusiveLastIndex{};
    // Includes the source capacity after the valid prefix: pace reads i+11.
    std::vector<std::array<float,3>> points;
};
class OriginalRivalData {
public:
    // dataRoot is the new data/original_rival directory.
    static OriginalRivalData load(const std::filesystem::path& dataRoot);
    // 046F40 loads i_0/i_1 or o_0/o_1 for the chosen direction;
    // 15AE00 selects the second path only for profile26. An unavailable
    // alternate is an error, never a substitution of the opposite direction.
    OriginalRivalPath loadPath(const std::filesystem::path& dataRoot,std::uint32_t condition,bool alternate=false)const;
    std::uint32_t word(std::uint32_t sourceAddress)const;
    float scalar(std::uint32_t sourceAddress)const{return std::bit_cast<float>(word(sourceAddress));}
private:
    std::vector<std::byte> bytes_;
};
struct OriginalRivalPaceInputs {
    std::uint32_t condition0C9015CC=0,profile0CAA9868=0;
    std::uint32_t level0C9015D0=0,opponentProgress0C901644=0;
    std::array<std::uint32_t,8> progress0C901604{};
};
// Numerical/state effects of15B0A0 entry through15B7A6 inclusive, for one
// caller-selected actor and its168-byte public record. Inactive actors only
// increment the original shared counter. Returns false for that early return.
// Source argument-to-slot dispatch is caller-owned; no hidden enemy defaults.
// Does not yet advance rival XYZ, query road contacts or run battle logic.
bool updateOriginalRivalPace(OriginalRivalState& rival,OriginalActorState& publicActor,
    std::uint32_t& frameCounter0CAA986C,const OriginalRivalData& data,
    const OriginalRivalPath& path,const OriginalRivalPaceInputs& inputs,
    const OriginalDriveState& playerDrive,const OriginalActorState& playerActor);
} // namespace idas3::original
