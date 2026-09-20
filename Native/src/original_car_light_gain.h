#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace idas3::original {
// Source course+44 shadow-intensity stream, read by03D100. Scene constructors
// may leave it NULL; this is distinct from a loaded stream containing ones.
class OriginalCarLightGain {
public:
    static OriginalCarLightGain load(const std::filesystem::path& root,
        unsigned course,bool night,bool wet,bool reverse);
    float evaluate(std::int32_t index,float fraction)const;
    bool hasTable()const{return !values_.empty();}
    bool reverse()const{return reverse_;}
    std::span<const float> values()const{return values_;}
private:
    std::vector<float> values_;
    bool reverse_{};
};
// Caller069020 invokes the two gain setters only during daytime. This helper
// is table evaluation alone; night callers must retain their current gain.
}
