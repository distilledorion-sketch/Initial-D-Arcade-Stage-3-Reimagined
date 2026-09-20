#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace idas3::original {
// Recorded original public actor state. The last bytes remain data, including
// values whose meaning is not established; no guessed physical interpolation.
struct OriginalDemoActor {
    std::array<std::uint32_t,42> words{};
    float f(unsigned byteOffset) const { return std::bit_cast<float>(words.at(byteOffset/4)); }
};
struct OriginalDemoShot {
    std::array<std::uint32_t,6> frames{}; // relocated original o_advcg table
    std::vector<std::uint32_t> camera; // original type word, followed by parameters
    std::array<std::uint32_t,11> descriptor{}; // trailing o_camadv shot metadata
    std::array<unsigned,2> cars{};
    std::array<unsigned,2> enemies{};
};
struct OriginalDemoCursor { unsigned shot=0,frame=0; };
struct OriginalDemoCameraFrame {
    // Original column-major camera world transform: eye=12..14,
    // forward=-(8..10), up=4..6. No aspect correction has been applied.
    std::array<float,16> world{};
    std::uint32_t fovPhase=0;
    float verticalFovRadians() const { return float(fovPhase)*std::bit_cast<float>(0x40c90fdbu)/65536.f; }
};
class OriginalDemoData {
public:
    static OriginalDemoData load(const std::filesystem::path& gameRoot);
    unsigned frameCount() const { return unsigned(actors_.size()/2); }
    std::span<const OriginalDemoShot> shots() const { return shots_; }
    const OriginalDemoActor& actor(unsigned frame,unsigned slot) const;
    bool hasCamera() const { return cameraFrames_.size()==frameCount(); }
    const OriginalDemoCameraFrame& camera(unsigned frame) const { return cameraFrames_.at(frame); }
    // 035120/035200 change headlights; body submissions remain present.
    static bool actorVisible(unsigned,unsigned slot) { return slot<2; }
    static bool headlightsOn(unsigned sourceTimeline,unsigned slot) {
        return slot<2 && sourceTimeline>=500 && sourceTimeline<(slot?5550u:5720u);
    }
    static bool lightEffectsOn(unsigned sourceTimeline,unsigned slot) {
        return slot<2 && sourceTimeline>=535 && sourceTimeline<(slot?5550u:5720u);
    }
    // Original157640 return: 0 ordinary frame,1 next shot,2 end/wrap.
    unsigned step(OriginalDemoCursor&) const;
private:
    std::vector<OriginalDemoActor> actors_;
    std::vector<OriginalDemoShot> shots_;
    std::vector<OriginalDemoCameraFrame> cameraFrames_;
};
}
