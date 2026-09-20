#pragma once
#include <filesystem>

namespace idas3 {
// Native saved-driver setup metadata. This does not encode source card flags,
// modify original profiles, or infer completion from a saved file/name.
class LocalDriverSetup {
public:
    enum class Status {Missing,Complete,Unreadable};
    struct Loaded {Status status=Status::Missing;bool recoveredFromBackup=false;};
    explicit LocalDriverSetup(std::filesystem::path directory):directory_(std::move(directory)){}
    Loaded load(unsigned car)const;
    // Caller commits this only AFTER name confirmation and a successful
    // original-profile save. Failed/unfinished marker writes never imply ready.
    bool markComplete(unsigned car)const;
    std::filesystem::path path(unsigned car)const;
private:
    std::filesystem::path directory_;
};
}
