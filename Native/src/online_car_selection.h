#pragma once
#include <stdexcept>
namespace idas3 {
// Keep the existing primary-slot and legacy identifiers stable. Every
// additional profile within a slot has a distinct, unambiguous identifier.
inline constexpr int onlineCarSelectionCount=40+5*35;
struct OnlineCarSelection { int slot=-1,car=-1; };
inline OnlineCarSelection decodeOnlineCarSelection(int selection){
    if(selection<0||selection>=onlineCarSelectionCount)throw std::invalid_argument("Invalid online car selection");
    if(selection<5)return {selection,-1};
    if(selection<40)return {-1,selection-5};
    return {(selection-40)/35,(selection-40)%35};
}
inline int onlineSlotCarSelection(int slot,int car){
    if(slot<0||slot>=5||car<0||car>=35)throw std::invalid_argument("Invalid online slot/car");
    return 40+slot*35+car;
}
}
