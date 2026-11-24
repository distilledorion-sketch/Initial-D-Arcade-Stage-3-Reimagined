#include "native_platform_clock.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <condition_variable>
#include <mutex>
using idas3::NativePlatformClock;
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("Private test directory required");
    const auto root=std::filesystem::absolute(argv[1])/std::to_string(NativePlatformClock::hostMilliseconds());
    std::filesystem::create_directories(root);
    unsigned checks=0;
    const auto require=[&](bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);};
    const auto bytes=[](const auto& path){std::ifstream in(path,std::ios::binary);return std::vector<char>{std::istreambuf_iterator<char>(in),{}};};
    std::uint32_t expected=0;std::uint64_t expectedTicks=0;
    for(unsigned fps:{30u,60u,144u,240u}){
        NativePlatformClock clock;require(clock.activateInjected(11,950)==11,"Injected activation seed");
        for(unsigned frame=0;frame<=fps*5;++frame)require(clock.advanceTo(950+std::uint64_t(frame)*1000/fps),"Injected clock performs no IO");
        require(clock.current()==16&&clock.ticks()==298,"RTC phase or original platform cadence changed");
        if(fps==30){expected=clock.current();expectedTicks=clock.ticks();}
        else require(clock.current()==expected&&clock.ticks()==expectedTicks,"Host FPS changes clock");
        require(clock.activateInjected(999,999999)==16,"Repeated activation rewound clock");
        clock.advanceTo(949);require(clock.current()==16&&clock.ticks()==298,"Clock accepted backwards host time");
    }
    //Ready batching avoids unbounded work after a long host suspension.
    NativePlatformClock longRun;longRun.activateInjected(0xfffffffeu,500);
    require(longRun.advanceTo(86400500)&&longRun.current()==86398u,"Long suspension or uint32 retained wrap");
    const auto folder=root/"persistent",file=folder/"platform-clock-v1.bin";
    std::vector<char> committed;
    {
        NativePlatformClock clock;require(clock.activate(folder,0)==0,"New platform is cleared SRAM");
        require(clock.advanceTo(5000)&&clock.current()==4,"Published persisted powered-on seconds");
        require(clock.flush(),"Drain first published value");
        committed=bytes(file);require(committed.size()==16,"Clock storage is bounded");
        require(clock.activate(folder,999999)==4,"Repeated settings activation reset clock");
        {
            NativePlatformClock second;require(second.activate(folder,0)==4,"Second owner lost committed baseline");
            require(!second.advanceTo(9000)&&second.current()==12,"Locked clock must continue in memory and report degraded persistence");
            require(!second.flush()&&bytes(file)==committed,"Second owner overwrote the active cabinet clock");
        }
        bool changedRoot=false;try{clock.activate(root/"different",0);}catch(const std::logic_error&){changedRoot=true;}
        require(changedRoot,"Active clock silently changed storage roots");
    }
    {
        NativePlatformClock clock;require(clock.activate(folder,100000)==4,"Reopen lost retained powered-on time");
        require(clock.advanceTo(103000)&&clock.current()==6,"Time while application was closed was counted");
        require(clock.flush(),"Published value flush");
        committed=bytes(file);
        const auto temporary=std::filesystem::path(file.string()+".new");
        std::filesystem::create_directory(temporary);
        clock.advanceTo(105000);
        require(!clock.flush()&&clock.current()==8,"Failed persistence lost or rewound in-memory clock");
        require(bytes(file)==committed,"Failed atomic write damaged committed clock");
        require(std::filesystem::remove(temporary),"Remove only this fixture's empty obstruction");
        require(clock.flush()&&bytes(file)!=committed,"Retry failed to commit published value");
        committed=bytes(file);
    }
    //A crash-created temporary record is not a committed SRAM image.
    {std::ofstream out(file.string()+".new",std::ios::binary);out<<"uncommitted";}
    {NativePlatformClock clock;require(clock.activate(folder,200000)==8,"Uncommitted temporary replaced valid storage");}
    {std::ofstream out(file,std::ios::binary|std::ios::trunc);out<<"bad";}
    {NativePlatformClock clock;require(clock.activate(folder,300000)==0,"Invalid storage did not clear counter");require(clock.flush(),"Clear invalid storage atomically");}
    require(bytes(file).size()==16,"Invalid storage recovery size");
    const auto unavailable=root/"unavailable";
    {std::ofstream out(unavailable);out<<"This is a file, not a directory";}
    {NativePlatformClock clock;require(clock.activate(unavailable,0)==0,"Unavailable storage must start cleared in memory");
     require(!clock.advanceTo(5000)&&clock.current()==4&&!clock.flush(),"Unavailable storage must not stop the platform clock");}
    const auto shutdown=root/"shutdown";
    {NativePlatformClock clock;clock.activate(shutdown,1000);}
    require(std::filesystem::is_regular_file(shutdown/"platform-clock-v1.bin"),"Destruction omitted pending platform storage");
    //Hold the persistence sink after its first request. The same thread can
    //advance thousands of frames and release it only if advanceTo never waits
    //for IO. No timing threshold or filesystem speed assumption is involved.
    std::mutex mutex;std::condition_variable condition;bool entered=false,released=false;
    std::vector<std::uint32_t> written;
    {
        NativePlatformClock clock;clock.activateInjected(0,0,[&](std::uint32_t value){
            std::unique_lock lock(mutex);entered=true;condition.notify_one();
            condition.wait(lock,[&]{return released;});written.push_back(value);return true;
        });
        //Release the deliberately blocked writer before clock destruction even
        //when an assertion fails, so a regression reports failure, not a hang.
        struct ReleaseWriter {
            std::mutex& mutex;std::condition_variable& condition;bool& released;
            ~ReleaseWriter(){{std::lock_guard lock(mutex);released=true;}condition.notify_all();}
        } releaseWriter{mutex,condition,released};
        clock.advanceTo(2000);
        {std::unique_lock lock(mutex);require(condition.wait_for(lock,std::chrono::seconds(10),[&]{return entered;}),"Writer did not receive the first snapshot");}
        for(unsigned ms=2001;ms<=120000;++ms)require(clock.advanceTo(ms),"Blocked sink stalled or failed platform publication");
        require(clock.current()==119,"Blocked sink changed source clock");
        {std::lock_guard lock(mutex);released=true;}condition.notify_one();
        require(clock.flush(),"Coalesced latest snapshot did not drain");
        require(written.size()==2&&written.front()==1&&written.back()==119,"Slow sink did not coalesce to the latest snapshot");
    }
    unsigned failedWrites=0;
    {NativePlatformClock clock;clock.activateInjected(0,0,[&](std::uint32_t){++failedWrites;return false;});
     clock.advanceTo(2000);require(!clock.flush(),"Failed sink should report failure");
     const auto failures=failedWrites;
     for(unsigned i=0;i<1000;++i)require(!clock.advanceTo(2000),"Failed sink status was lost");
     require(failedWrites==failures,"Unchanged render frames retried failed IO");}
    std::cout<<"PASS "<<checks<<" platform host checks; deterministic RTC injection, original-rate ticks, FPS independence, nonblocking coalesced persistence, bounded retries, second-instance isolation, atomic failure/retry, crash temporary isolation and cleared-storage recovery. Private artifacts: "<<root.string()<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
