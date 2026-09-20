#include "local_driver_setup.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace idas3;
namespace {
unsigned checks=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const std::filesystem::path&p,const std::vector<std::uint8_t>&b){std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(b.data()),b.size());if(!out)throw std::runtime_error("Test fixture write");}
void put(std::vector<std::uint8_t>&b,unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)b[offset+i]=std::uint8_t(value>>(i*8));}
void rehash(std::vector<std::uint8_t>&b){std::uint32_t value=2166136261;for(unsigned i=0;i<28;++i){value^=b[i];value*=16777619;}put(b,28,value);}
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("isolated scratch parent required");
    const auto parent=std::filesystem::weakly_canonical(argv[1]);std::filesystem::create_directories(parent);
    const auto directory=parent/("driver-setup-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    LocalDriverSetup store(directory);using Status=LocalDriverSetup::Status;
    require(store.load(0).status==Status::Missing,"Missing setup became complete");require(!std::filesystem::exists(directory),"Read created storage");
    std::filesystem::create_directories(directory);const std::vector<std::uint8_t> oldProfile{'o','r','i','g','i','n','a','l',0,0xff};write(directory/"car_00.profile",oldProfile);write(directory/"car_00.profile.previous",oldProfile);
    for(unsigned car=0;car<35;++car){
        require(store.load(car).status==Status::Missing,"Another car conferred completion");require(store.markComplete(car),"First completion failed");
        LocalDriverSetup restarted(directory);require(restarted.load(car).status==Status::Complete&&!restarted.load(car).recoveredFromBackup,"Completion did not survive restart");
        const auto bytes=read(store.path(car));require(bytes.size()==32,"Marker size/version format changed");require(store.markComplete(car),"Repeated completion failed");require(read(store.path(car))==bytes,"Repeated completion changed marker identity");
        auto damaged=bytes;damaged.push_back('!');write(store.path(car),damaged);const auto recovered=store.load(car);require(recovered.status==Status::Complete&&recovered.recoveredFromBackup,"Valid previous marker not recovered");
        require(store.markComplete(car),"Backup repair failed");require(store.load(car).status==Status::Complete&&!store.load(car).recoveredFromBackup,"Repair did not restore primary");
    }
    require(read(directory/"car_00.profile")==oldProfile&&read(directory/"car_00.profile.previous")==oldProfile,"Setup modified original profile files");
    const auto stableMarker=read(store.path(1));write(directory/"car_01.profile",{'r','a','c','e','-','p','r','o','g','r','e','s','s'});require(store.load(1).status==Status::Complete&&read(store.path(1))==stableMarker,"Ordinary profile changes invalidated setup");
    const auto marker=store.path(0),backup=std::filesystem::path(marker.wstring()+L".previous");const auto original=read(marker);std::filesystem::remove(backup);
    for(unsigned offset=0;offset<32;++offset){auto b=original;b[offset]^=0x55;write(marker,b);require(store.load(0).status==Status::Unreadable,"Damaged marker became ready");}
    for(unsigned length=0;length<32;++length){write(marker,std::vector<std::uint8_t>(original.begin(),original.begin()+length));require(store.load(0).status==Status::Unreadable,"Truncated marker became ready");}
    for(auto[offset,value]:std::array<std::pair<unsigned,unsigned>,6>{{{8,2},{12,1},{16,0},{16,2},{20,1},{24,1}}}){auto b=original;put(b,offset,value);rehash(b);write(marker,b);require(store.load(0).status==Status::Unreadable,"Unsupported marker with valid checksum accepted");}
    auto trailing=original;trailing.push_back(0);write(marker,trailing);require(store.load(0).status==Status::Unreadable,"Trailing marker bytes accepted");
    write(marker,read(store.path(1)));require(store.load(0).status==Status::Unreadable,"Other car marker accepted");
    write(backup,std::vector<std::uint8_t>{'x'});require(store.load(0).status==Status::Unreadable,"Two damaged markers accepted");
    std::filesystem::remove(marker);std::filesystem::remove(backup);write(std::filesystem::path(marker.wstring()+L".new-abandoned"),original);require(store.load(0).status==Status::Missing,"Uncommitted temporary marker accepted");
    write(backup,original);require(store.load(0).status==Status::Complete&&store.load(0).recoveredFromBackup,"Missing primary did not recover committed backup");
    require(store.markComplete(0),"Missing-primary repair failed");require(store.load(0).status==Status::Complete&&!store.load(0).recoveredFromBackup,"Repaired primary absent");
    write(directory/"not-a-directory",{'x'});LocalDriverSetup blocked(directory/"not-a-directory");require(!blocked.markComplete(0),"Invalid storage parent reported commit success");require(blocked.load(0).status!=Status::Complete,"Failed save conferred completion");
    LocalDriverSetup occupied(directory/"occupied-target");std::filesystem::create_directories(occupied.path(0));write(occupied.path(0)/"preserve",{'x'});require(!occupied.markComplete(0),"Occupied marker destination reported success");require(occupied.load(0).status==Status::Unreadable,"Occupied marker path accepted");require(read(occupied.path(0)/"preserve")==std::vector<std::uint8_t>{'x'},"Failed replacement damaged destination");unsigned pending=0;for(const auto&entry:std::filesystem::directory_iterator(directory/"occupied-target"))if(entry.path().filename().string().find(".new-")!=std::string::npos)++pending;require(pending==0,"Failed replacement left temporary markers");
    for(unsigned car:{35u,0xffffffffu}){bool bad=false;try{(void)store.load(car);}catch(const std::out_of_range&){bad=true;}require(bad,"Invalid car path accepted");bad=false;try{(void)store.markComplete(car);}catch(const std::out_of_range&){bad=true;}require(bad,"Invalid car marker accepted");}
    unsigned preserved=0;for(const auto&entry:std::filesystem::directory_iterator(directory))if(entry.path().filename().string().find(".unreadable.new-")!=std::string::npos)++preserved;require(preserved==35,"Damaged primaries not preserved");
    require(read(directory/"car_00.profile")==oldProfile,"Later metadata checks changed profile bytes");
    // The generated test directory is verified before recursive removal.
    const auto resolved=std::filesystem::weakly_canonical(directory);require(resolved.parent_path()==parent&&resolved.filename().string().starts_with("driver-setup-test-"),"Unsafe test cleanup path");std::filesystem::remove_all(resolved);
    std::cout<<"PASS "<<checks<<" checks:35 independent setup markers, strict32-byte/version/car/checksum validation, committed backup recovery, atomic replacements, failed-write isolation and unchanged profile bytes.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
