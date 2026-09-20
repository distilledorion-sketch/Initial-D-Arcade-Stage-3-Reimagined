#pragma once
#include <array>
#include <cstdint>
#include "original_matrix.h"

namespace idas3::original {
// One node of the source's car camera chain (12CCarCamChain). The field names
// are the game's own: its debug dumper at 0C0A01C0 prints a live node back out
// as C source, which is where this layout comes from.
struct OriginalStartCameraShot {
    std::uint32_t kind = 0, smoothJoint = 0, isShake = 0;
    std::array<float, 3> position{};
    float angleRadians = 0;   // authored value; kind5 replaces it from subject size/distance
    float wait = 0;           // raw camera wait parameter; not proven to be seconds
    std::array<float, 3> rotation{};
    std::array<float, 3> shake{};      // shake_vel, shake_conv, shake_rand
    std::array<float, 3> offset{};
    float objectSize = 0;
};
// The camera the source frames the two cars with on the start line, before the
// countdown. condition is 2*course+direction in the original course order
// (Myogi, Usui, Akagi, Akina, Happogahara, Irohazaka, Shomaru, Tsuchisaka,
// AkinaSnow) -- the same code originalStartPose takes -- and shot is 0 or 1.
//
// Every node is kind5 without smoothing/shake. Only node0 is near its start;
// node1 may be hundreds of units away. This static chain is constructed by
// 0D6478, and is NOT the moving two-node pre-race showcase made by06455C.
OriginalStartCameraShot originalStartCamera(std::uint32_t condition, std::uint32_t shot);

struct OriginalStartShowcaseFrame {
    std::array<float,3> eye{}, target{}, up{0.f,1.f,0.f};
    float verticalFieldOfView=0.f;
};
// Actual06455C ->0A8C60 ->0A2220/0A23E0 moving showcase. The owner supplies
// the midpoint of the two source start-grid positions and the horizontal
// road heading: atan2(tangent.x,tangent.z), phase-converted to radians, +pi.
// This is a source camera generator, not the unrelated static table above.
class OriginalStartShowcaseCamera {
public:
    static constexpr unsigned sourceActiveTicks=120;
    void reset(const std::array<float,3>& gridMidpoint,float headingRadians,
        unsigned course,bool reverse,const OriginalFscaTable&);
    // Selects one of the two originally constructed nodes. Selection does
    // not advance it. The enclosing owner's switch timing is caller-owned.
    void selectShot(unsigned shot);
    // One original0A23E0 invocation. Debug bit00800000 uses slower .001
    // acceleration; normal source behavior is .01. No render-delta input.
    void advance(bool sourceSlowCameraFlag=false);
    const OriginalStartShowcaseFrame& frame()const{return nodes_[selected_].frame;}
    unsigned selectedShot()const{return selected_;}
    unsigned updates()const{return nodes_[selected_].updates;}
    unsigned activeTicksRemaining()const{return nodes_[selected_].remaining;}
    const std::array<float,3>& velocity()const{return nodes_[selected_].velocity;}
    bool ready()const{return ready_;}
private:
    struct Node {
        OriginalStartShowcaseFrame frame;
        std::array<float,3> authoredVelocity{},velocity{};
        float maximumSpeed=0.f;
        unsigned remaining=sourceActiveTicks,updates=0;
    };
    std::array<Node,2> nodes_{};
    unsigned selected_=0;
    bool ready_=false;
};
}
