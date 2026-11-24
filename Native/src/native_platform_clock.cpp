#include "native_platform_clock.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace idas3 {
namespace {
using Bytes=std::array<unsigned char,16>;
constexpr std::array<unsigned char,8> magic{'I','D','3','C','L','K','1',0};
void put(Bytes& bytes,unsigned offset,std::uint32_t word){
    for(unsigned i=0;i<4;++i)bytes[offset+i]=static_cast<unsigned char>(word>>(8*i));
}
std::uint32_t get(const Bytes& bytes,unsigned offset){
    std::uint32_t word=0;for(unsigned i=0;i<4;++i)word|=std::uint32_t(bytes[offset+i])<<(8*i);return word;
}
bool read(const std::filesystem::path& file,std::uint32_t& value){
    std::ifstream in(file,std::ios::binary);Bytes bytes{};
    if(!in.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||in.peek()!=EOF)return false;
    if(!std::equal(magic.begin(),magic.end(),bytes.begin()))return false;
    value=get(bytes,8);return get(bytes,12)==(value^0xa3c59e71u);
}
bool write(const std::filesystem::path& file,std::uint32_t value)noexcept{
    try{
        auto temporary=file;temporary+=".new";
        Bytes bytes{};std::copy(magic.begin(),magic.end(),bytes.begin());
        put(bytes,8,value);put(bytes,12,value^0xa3c59e71u);
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out)return false;
        out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();
        if(!out)return false;
#ifdef _WIN32
        return MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        std::error_code error;std::filesystem::rename(temporary,file,error);return !error;
#endif
    }catch(...){return false;}
}
}
//One bounded slot coalesces newer published seconds while the filesystem is
//busy. Failed writes wait for a changed snapshot or an explicit shutdown drain;
//render frames neither retry them nor wait on filesystem operations.
struct NativePlatformClock::Writer {
    explicit Writer(std::function<bool(std::uint32_t)> sink):sink_(std::move(sink)),thread_([this]{run();}){}
    ~Writer(){
        {std::lock_guard lock(mutex_);stopping_=true;}
        changed_.notify_one();thread_.join();
    }
    void request(std::uint32_t value){
        {std::lock_guard lock(mutex_);value_=value;++requested_;}
        changed_.notify_one();
    }
    bool healthy(){std::lock_guard lock(mutex_);return healthy_;}
    bool flush(){
        std::unique_lock lock(mutex_);
        if(completed_==requested_&&!healthy_){++requested_;changed_.notify_one();}
        const auto target=requested_;
        finished_.wait(lock,[&]{return completed_>=target;});
        return healthy_;
    }
private:
    void run(){
        std::unique_lock lock(mutex_);
        for(;;){
            changed_.wait(lock,[&]{return stopping_||completed_!=requested_;});
            if(completed_==requested_&&stopping_)return;
            const auto version=requested_;const auto value=value_;
            lock.unlock();bool result=false;
            try{result=sink_(value);}catch(...){result=false;}
            lock.lock();completed_=version;healthy_=result;finished_.notify_all();
        }
    }
    std::function<bool(std::uint32_t)> sink_;
    std::mutex mutex_;
    std::condition_variable changed_,finished_;
    std::uint64_t requested_=0,completed_=0;
    std::uint32_t value_=0;
    bool healthy_=true,stopping_=false;
    std::thread thread_;
};
NativePlatformClock::NativePlatformClock()=default;
std::uint64_t NativePlatformClock::hostMilliseconds(){
    return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
void NativePlatformClock::begin(std::uint32_t retained,std::uint64_t milliseconds){
    clock_.begin(retained,std::uint32_t(milliseconds/1000));
    bootMilliseconds_=lastMilliseconds_=milliseconds;ticks_=0;active_=true;
}
std::uint32_t NativePlatformClock::activate(const std::filesystem::path& userdata,std::uint64_t milliseconds){
    const auto root=std::filesystem::absolute(userdata).lexically_normal();
    if(active_){
        if(persistent_&&root!=root_)throw std::logic_error("An active platform clock cannot change game-data folders");
        return current();
    }
    root_=root;file_=root/"platform-clock-v1.bin";
    std::uint32_t retained=0;bool valid=false;
    try{
    std::filesystem::create_directories(root);
    const auto lockFile=root/"platform-clock-v1.lock";
#ifdef _WIN32
    const auto handle=CreateFileW(lockFile.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Platform clock is already in use or unavailable");
    lock_=reinterpret_cast<std::intptr_t>(handle);
#else
    const int fd=open(lockFile.c_str(),O_CREAT|O_RDWR,0600);
    if(fd<0)throw std::runtime_error("The game-data folder's platform clock is unavailable");
    if(flock(fd,LOCK_EX|LOCK_NB)!=0){close(fd);throw std::runtime_error("The game-data folder's platform clock is already in use");}
    lock_=fd;
#endif
    valid=read(file_,retained);
    writer_=std::make_unique<Writer>([file=file_](std::uint32_t value){return write(file,value);});
    persistent_=true;
    }catch(...){
        //A second game instance or read-only folder must not crash the game or
        //overwrite another owner's clock. Run in memory from committed history.
        writer_.reset();releaseLock();degraded_=true;
        try{valid=read(file_,retained);}catch(...){valid=false;}
    }
    //Missing/invalid storage represents cleared original SRAM, whose clock0/4
    //are zero. Never recover from an uncommitted temporary file.
    begin(valid?retained:0,milliseconds);
    if(writer_&&!valid)writer_->request(current());
    return current();
}
std::uint32_t NativePlatformClock::activateInjected(std::uint32_t retained,std::uint64_t milliseconds,
    std::function<bool(std::uint32_t)> persist){
    if(active_)return current();
    if(persist)writer_=std::make_unique<Writer>(std::move(persist));
    begin(retained,milliseconds);return current();
}
bool NativePlatformClock::advanceTo(std::uint64_t milliseconds){
    if(!active_||milliseconds<lastMilliseconds_)return true;
    lastMilliseconds_=milliseconds;
    const auto elapsed=milliseconds-bootMilliseconds_;
    //852*530/27MHz =3763/225 milliseconds per source frame. Integer rational
    //timestamps retain RTC boundary phase and are independent of display FPS.
    const auto total=(elapsed/3763)*225+(elapsed%3763)*225/3763;
    const auto count=total-ticks_;
    if(!count)return !degraded_&&(!writer_||writer_->healthy());
    const auto publication=total&~std::uint64_t(15);
    const auto publicationElapsed=(publication/225)*3763+(publication%225)*3763/225;
    const auto rtc=std::uint32_t((bootMilliseconds_+publicationElapsed)/1000);
    const bool changed=clock_.advanceReadyTicks(count,rtc);ticks_=total;
    if(changed&&writer_)writer_->request(current());
    return !degraded_&&(!writer_||writer_->healthy());
}
bool NativePlatformClock::flush()noexcept{
    try{return !degraded_&&(!writer_||writer_->flush());}catch(...){return false;}
}
void NativePlatformClock::releaseLock()noexcept{
    if(lock_==-1)return;
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(lock_));
#else
    flock(int(lock_),LOCK_UN);close(int(lock_));
#endif
    lock_=-1;
}
NativePlatformClock::~NativePlatformClock(){flush();writer_.reset();releaseLock();}
}
