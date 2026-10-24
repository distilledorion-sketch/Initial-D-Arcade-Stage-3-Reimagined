#include "local_driver_profiles.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#endif

using namespace idas3;
namespace fs=std::filesystem;
using Result=LocalDriverProfiles::ConditionalSave;
namespace {
unsigned checks=0;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);++checks;}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
auto profile(unsigned car,unsigned points){auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(72,points);p.words[150]=0x87123456u;return p;}
void damage(const fs::path& path){std::ofstream out(path,std::ios::binary|std::ios::app);out.put('!');}
class Lock {
#ifdef _WIN32
    HANDLE handle=INVALID_HANDLE_VALUE;
#else
    int handle=-1;
#endif
public:
    explicit Lock(const fs::path& file){
        const auto path=fs::path(file.wstring()+L".lock");
#ifdef _WIN32
        handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(handle!=INVALID_HANDLE_VALUE,"fixture lock failed");
#else
        handle=open(path.c_str(),O_RDWR|O_CREAT,0600);
        require(handle>=0&&flock(handle,LOCK_EX|LOCK_NB)==0,"fixture lock failed");
#endif
    }
    ~Lock(){
#ifdef _WIN32
        if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
#else
        if(handle>=0){flock(handle,LOCK_UN);close(handle);}
#endif
    }
};
class Child {
#ifdef _WIN32
    PROCESS_INFORMATION process{};
#else
    pid_t pid=-1;
#endif
public:
    Child(const fs::path& executable,const fs::path& root,const std::string& mode){
#ifdef _WIN32
        auto command=L"\""+executable.wstring()+L"\" "+std::wstring(mode.begin(),mode.end())+L" \""+root.wstring()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=0,"child start failed");
#else
        pid=fork();require(pid>=0,"child start failed");
        if(pid==0){execl(executable.c_str(),executable.c_str(),mode.c_str(),root.c_str(),static_cast<char*>(nullptr));_exit(98);}
#endif
    }
    void stop(){
#ifdef _WIN32
        if(!process.hProcess)return;
        require(TerminateProcess(process.hProcess,99)!=0,"owned child termination failed");
        require(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0,"owned child did not exit");
        CloseHandle(process.hThread);CloseHandle(process.hProcess);process={};
#else
        if(pid<0)return;
        require(kill(pid,SIGKILL)==0,"owned child termination failed");
        int status;require(waitpid(pid,&status,0)==pid,"owned child did not exit");pid=-1;
#endif
    }
    ~Child(){try{stop();}catch(...){}}
};
void marker(const fs::path& root){std::ofstream(root/"ready")<<"ready\n";}
void waitMarker(const fs::path& root){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(!fs::exists(root/"ready")&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(fs::exists(root/"ready"),"child did not reach the requested crash boundary");
}
void child(const fs::path& root,bool lock){
    LocalDriverProfiles store(root);
    if(lock){Lock held(store.path(0));marker(root);for(;;)std::this_thread::sleep_for(std::chrono::seconds(1));}
    require(store.saveExpected(0,profile(0,321),profile(0,2321))==Result::Saved,"child reward save failed");
    marker(root);for(;;)std::this_thread::sleep_for(std::chrono::seconds(1));
}
}
int main(int argc,char** argv)try{
    if(argc==3){const std::string mode=argv[1];require(mode=="hold"||mode=="apply","invalid child mode");child(argv[2],mode=="hold");return 0;}
    if(argc!=2)throw std::invalid_argument("private scratch parent required");
    const auto parent=fs::absolute(argv[1]);
    const auto root=parent/("conditional-profiles-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(fs::create_directories(root),"fixture directory must be new");
    const auto before=profile(0,321),after=profile(0,2321);
    LocalDriverProfiles store(root/"ordinary");require(store.save(0,before),"baseline save failed");
    require(store.saveExpected(0,before,after)==Result::Saved,"planned reward not applied");
    const auto saved=bytes(store.path(0));const auto stamp=fs::last_write_time(store.path(0));
    LocalDriverProfiles restarted(root/"ordinary");
    require(restarted.saveExpected(0,before,after)==Result::AlreadySaved,"restart repeated an applied reward");
    require(bytes(store.path(0))==saved&&fs::last_write_time(store.path(0))==stamp,"repeated reward rewrote committed data");
    require(store.load(0).profile.words==after.words,"complete expected profile was not retained");
    auto newer=after;newer.setu(72,4000);require(store.save(0,newer),"newer profile save failed");
    const auto primary=bytes(store.path(0)),backup=bytes(fs::path(store.path(0).wstring()+L".previous"));
    require(store.saveExpected(0,before,after)==Result::Conflict,"stale result overwrote later progress");
    auto invalid=after;invalid.setu(16,1);
    require(store.saveExpected(0,before,invalid)==Result::Invalid,"wrong car accepted");
    require(bytes(store.path(0))==primary&&bytes(fs::path(store.path(0).wstring()+L".previous"))==backup,"refused result changed profile files");
    damage(store.path(0)); // Backup contains the already-applied result.
    require(store.saveExpected(0,before,after)==Result::AlreadySaved,"applied backup not recovered");
    require(store.load(0).origin==LocalDriverProfiles::Origin::Saved&&store.load(0).profile.words==after.words,"backup result acknowledged without repairing primary");
    damage(store.path(0));damage(fs::path(store.path(0).wstring()+L".previous"));
    const auto damaged=bytes(store.path(0));
    require(store.saveExpected(0,before,after)==Result::Unreadable,"unreadable save was replaced by recovery");
    require(bytes(store.path(0))==damaged,"unreadable save was not preserved");

    LocalDriverProfiles fresh(root/"fresh");auto original=fresh.load(8).profile,earned=original;earned.setu(72,1000);
    require(fresh.saveExpected(8,original,earned)==Result::Saved,"first stock-car reward failed");
    require(fresh.saveExpected(8,original,earned)==Result::AlreadySaved&&fresh.load(8).profile.u(72)==1000,"stock-car reward repeated");

    LocalDriverProfiles denied(root/"denied");require(denied.save(0,before),"denied baseline failed");
    const auto deniedBytes=bytes(denied.path(0));fs::create_directory(fs::path(denied.path(0).wstring()+L".previous"));
    require(denied.saveExpected(0,before,after)==Result::IoError,"blocked backup publication was accepted");
    require(bytes(denied.path(0))==deniedBytes,"failed publication changed the primary");
    fs::remove(fs::path(denied.path(0).wstring()+L".previous"));
    require(denied.saveExpected(0,before,after)==Result::Saved,"publication did not recover after obstruction was removed");

    const auto lockRoot=root/"locked";LocalDriverProfiles locked(lockRoot);require(locked.save(0,before),"locked baseline failed");
    {Child process(fs::absolute(argv[0]),lockRoot,"hold");waitMarker(lockRoot);
        require(!locked.save(0,after)&&locked.saveExpected(0,before,after)==Result::IoError,"ordinary or conditional writer ignored another process lock");
        require(locked.load(0).profile.words==before.words,"blocked writer changed the profile");process.stop();}
    require(locked.saveExpected(0,before,after)==Result::Saved,"terminated writer kept a stale lock");

    const auto crashRoot=root/"crashed";LocalDriverProfiles crashed(crashRoot);require(crashed.save(0,before),"crash baseline failed");
    {Child process(fs::absolute(argv[0]),crashRoot,"apply");waitMarker(crashRoot);process.stop();}
    const auto crashBytes=bytes(crashed.path(0));
    require(crashed.saveExpected(0,before,after)==Result::AlreadySaved,"crash after profile publication repeated its credit");
    require(bytes(crashed.path(0))==crashBytes&&crashed.load(0).profile.u(72)==2321,"post-crash balance changed");
    std::cout<<"PASS "<<checks<<" conditional profile recovery checks; fixtures retained at "<<root<<"\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
