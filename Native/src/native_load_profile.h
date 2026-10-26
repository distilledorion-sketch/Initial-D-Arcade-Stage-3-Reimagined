#pragma once
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>

namespace idas3 {
// Opt-in investigation only. Buffer markers in memory and write once at the
// outermost load boundary, never inside mesh/texture loops or ordinary frames.
class NativeLoadProfile {
    using Clock=std::chrono::steady_clock;
    struct Entry {const char* label=nullptr;double value=0;unsigned depth=0;bool bytes=false;};
    struct State {std::array<Entry,512> entries{};unsigned count=0,depth=0,batch=0;};
    static State& state(){static thread_local State value;return value;}
    static const std::filesystem::path& output(){
        static const auto path=[] {
            try{const char* value=std::getenv("IDAS3_NATIVE_LOAD_PROFILE");
                if(value&&*value){auto p=std::filesystem::u8path(value);if(p.is_absolute())return p;}}
            catch(...){}
            return std::filesystem::path{};
        }();return path;
    }
    bool active_=false;unsigned depth_=0;const char* label_;Clock::time_point start_,previous_;
    void record(const char* label,double value,bool bytes=false)noexcept{
        auto& s=state();if(s.count<s.entries.size())s.entries[s.count++]={label,value,depth_,bytes};
    }
public:
    static bool legacySceneryLifecycle(){
        static const bool value=[] {const char* p=std::getenv("IDAS3_NATIVE_SCENERY_BASELINE");return !output().empty()&&p&&p[0]=='1'&&p[1]=='\0';}();
        return value;
    }
    explicit NativeLoadProfile(const char* label):label_(label){
        active_=!output().empty();if(!active_)return;
        auto& s=state();depth_=s.depth++;if(!depth_)s.count=0;
        start_=previous_=Clock::now();
    }
    bool enabled()const noexcept{return active_;}
    void mark(const char* label)noexcept{
        if(!active_)return;auto now=Clock::now();record(label,std::chrono::duration<double,std::milli>(now-previous_).count());previous_=now;
    }
    void bytes(const char* label,std::size_t value)noexcept{if(active_)record(label,double(value),true);}
    ~NativeLoadProfile()noexcept{
        if(!active_)return;
        record(label_,std::chrono::duration<double,std::milli>(Clock::now()-start_).count());
        auto& s=state();if(--s.depth)return;
        try{
            std::ofstream stream(output(),std::ios::app);stream.imbue(std::locale::classic());stream<<std::setprecision(12);
            for(unsigned i=0;i<s.count;++i){const auto& e=s.entries[i];stream<<s.batch<<','<<e.depth<<','<<e.label<<','<<(e.bytes?"bytes":"ms")<<','<<e.value<<'\n';}
            ++s.batch;
        }catch(...){}
    }
    NativeLoadProfile(const NativeLoadProfile&)=delete;
    NativeLoadProfile& operator=(const NativeLoadProfile&)=delete;
};
}
