#include "time_attack_season.h"
#include "time_attack_records.h"
#include "time_attack_ghost.h"
#include "local_driver_profiles.h"
#include "online_result_reward.h"
#include "imported_course_catalog.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace idas3;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
std::vector<unsigned char> bytes(const std::filesystem::path& file){
    std::ifstream in(file,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
void write(const std::filesystem::path& file,const std::vector<unsigned char>& content){
    std::filesystem::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);
    out.write(reinterpret_cast<const char*>(content.data()),content.size());check(bool(out),"fixture write");
}
void text(const std::filesystem::path& file,const std::string& content){write(file,{content.begin(),content.end()});}
void put(std::vector<unsigned char>& b,unsigned at,unsigned value){for(unsigned i=0;i<4;++i)b[at+i]=static_cast<unsigned char>(value>>(i*8));}
void checksum(std::vector<unsigned char>& b){unsigned hash=2166136261u;for(unsigned i=0;i<b.size();++i)if(i<20||i>=24){hash^=b[i];hash*=16777619u;}put(b,20,hash);}
std::vector<unsigned char> oldProfile(unsigned car,const original::OriginalBattleProfile& profile){
    std::vector<unsigned char> b(24+1228);const unsigned char magic[]{'I','D','3','P','R','F','1',0};
    std::copy(std::begin(magic),std::end(magic),b.begin());put(b,8,1);put(b,12,car);put(b,16,307);
    for(unsigned i=0;i<307;++i)put(b,24+i*4,profile.words[i]);checksum(b);return b;
}
original::OriginalBattleProfile fixture(unsigned car){
    auto p=original::makeOriginalFreshBattleProfile();
    // Sentinel every original word, then restore only the format's documented
    // structural constraints. All non-TA words must survive byte for byte.
    for(unsigned i=0;i<p.words.size();++i)p.words[i]=0x87123456u+i+car;
    p.setu(0,1);p.setu(4,3);p.setu(8,1);p.setu(12,0);p.setu(16,car);p.setu(20,13);p.setu(24,2);p.setu(28,7);p.setu(32,0);
    for(unsigned i=0;i<9;++i)p.setu(80+i*4,i%7);
    for(unsigned i=0;i<8;++i)p.setu(1080+i*4,i+1);
    for(unsigned i=0;i<36;++i){p.setu(176+i*4,1000000+i);p.setu(320+i*4,i%2);for(unsigned j=0;j<3;++j)p.setu(500+(i*3+j)*4,200000*(j+1));}
    p.setu(72,86421);p.setu(1140,9);p.setByte(1187,7);return p;
}
bool timeWord(unsigned offset){return (offset>=176&&offset<464)||(offset>=500&&offset<932);}
void profilePreserved(const original::OriginalBattleProfile& before,const original::OriginalBattleProfile& after){
    for(unsigned offset=0;offset<1228;offset+=4)check(after.u(offset)==(timeWord(offset)?0:before.u(offset)),"non-TA profile changed or TA record remained");
}
std::string oldRecords(){
    std::ostringstream out;out<<"condition,weather,car,finish_ticks6000,name0,name1,name2,name3,name4,manual,night,split1,split2,split3,course_revision\n";
    for(unsigned condition=0;condition<supportedConditionCount;++condition)for(unsigned weather=0;weather<2;++weather)for(unsigned car=0;car<35;++car)
        out<<condition<<','<<weather<<','<<car<<','<<1000000+car<<",1,2,3,4,221,1,1,200000,400000,600000,"<<localTimeAttackRevision(condition/2)<<'\n';
    return out.str();
}
std::filesystem::path retiredGhost(const std::filesystem::path& profile,unsigned course,bool reverse,bool wet){
    auto current=TimeAttackGhost::path(profile,course,reverse,wet).string();const auto at=current.rfind("_s2.idghost");
    check(at!=std::string::npos,"seasoned ghost name");current.erase(at,3);return current;
}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("scratch parent directory required");
    const auto root=std::filesystem::path(argv[1])/("season-reset-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    std::vector<std::filesystem::path> profiles,records,oldGhosts,newGhosts;
    records.push_back(root/"time_attack_records_v1.csv");
    for(int slot=-1;slot<5;++slot){
        const auto directory=slot<0?root/"driver_profiles_v1":root/"saves"/("slot_"+std::to_string(slot+1))/"driver_profiles_v1";
        profiles.push_back(directory);records.push_back(directory/"hakone_personal_v1.csv");
        LocalDriverProfiles store(directory);
        for(unsigned car=0;car<35;++car)write(store.path(car),oldProfile(car,fixture(car)));
        for(unsigned course=0;course<supportedCourseCount;++course)for(bool reverse:{false,true})for(bool wet:{false,true}){
            const auto old=retiredGhost(directory,course,reverse,wet);
            for(const char* suffix:{"",".tmp",".previous"}){auto path=old;path+=suffix;text(path,"old ghost");oldGhosts.push_back(path);}
            const auto current=TimeAttackGhost::path(directory,course,reverse,wet);text(current,"current ghost sentinel");newGhosts.push_back(current);
        }
    }
    const auto legacyCsv=oldRecords();for(const auto& file:records)text(file,legacyCsv);
    std::vector<std::filesystem::path> keep{root/"settings.txt",root/"last_run.csv",root/"replays"/"saved.idreplay",root/"saves"/"slot_1"/"save.txt",root/"identity.json"};
    for(const auto& file:keep)text(file,"preserved unrelated data");
    const std::array<const char*,18> scenes{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3","hakone","sadamine","enna","myogi_special","usui_special","momiji","tsubaki","gunsai","odawara"};
    for(unsigned course=0;course<scenes.size();++course)for(const char* owner:{"original_race_v3_","development_v01_"}){
        const std::string key=std::string(owner)+scenes[course]+"_rev_wet_car34_mt";
        for(const char* suffix:{"",".tmp",".previous"}){const auto file=root/(key+(course==17?"_r2":"")+".csv"+suffix);text(file,"old telemetry");oldGhosts.push_back(file);}
        const auto file=TimeAttackGhost::telemetryPath(root,key,course);text(file,"new telemetry");newGhosts.push_back(file);
    }
    const auto futureGhost=root/"driver_profiles_v1"/"ghosts_v1"/"course_3_forward_dry_s3.idghost";text(futureGhost,"future ghost");
    const auto migrated=migrateTimeAttackSeason(root);
    check(migrated.complete&&migrated.profilesMigrated==210&&migrated.recordFilesMigrated==7,"all six profile directories and all record stores migrate");
    for(const auto& directory:profiles){LocalDriverProfiles store(directory);for(unsigned car=0;car<35;++car){
        const auto value=store.load(car);check(value.origin==LocalDriverProfiles::Origin::Saved&&!value.needsTimeAttackSeasonMigration,"migrated profile tagged current");
        profilePreserved(fixture(car),value.profile);
        auto archive=store.path(car);archive+=".before-time-attack-season-2.bak";check(bytes(archive)==oldProfile(car,fixture(car)),"exact profile archive");
    }}
    for(const auto& file:records){TimeAttackRecords store;check(store.load(file)&&!store.needsMigration()&&store.entries().empty(),"old course/model rankings empty");
        auto archive=file;archive+=".before-time-attack-season-2.bak";check(bytes(archive)==std::vector<unsigned char>(legacyCsv.begin(),legacyCsv.end()),"exact record archive");}
    for(const auto& file:oldGhosts)check(!std::filesystem::exists(file),"retired ghost remains");
    for(const auto& file:newGhosts)check(std::filesystem::is_regular_file(file),"current ghost deleted");
    for(const auto& file:keep)check(bytes(file)==std::vector<unsigned char>({'p','r','e','s','e','r','v','e','d',' ','u','n','r','e','l','a','t','e','d',' ','d','a','t','a'}),"non-TA file changed");
    check(std::filesystem::is_regular_file(futureGhost),"future season ghost removed");
    // New, slower season records must survive repeated startup. A reset cannot
    // depend on a global marker or accidentally delete subsequent progress.
    LocalDriverProfiles current(profiles.front());auto fresh=current.load(0).profile;fresh.setu(176,1600000);fresh.setu(500,400000);
    check(current.save(0,fresh),"new personal record save");TimeAttackRecords board;board.record({6,0,0,1600000});check(board.save(records.front()),"new cabinet record save");
    const auto second=migrateTimeAttackSeason(root);check(second.complete&&second.profilesMigrated==0&&second.recordFilesMigrated==0,"repeat startup idempotent");
    check(current.load(0).profile.words==fresh.words&&board.load(records.front())&&board.best(6,0,0).model==1600000,"new slower times erased on restart");
    // Restored old copies are filtered even before the startup pass runs again.
    write(current.path(1),oldProfile(1,fixture(1)));const auto restored=current.load(1);
    check(restored.needsTimeAttackSeasonMigration,"restored old card recognized");profilePreserved(fixture(1),restored.profile);
    text(records.back(),legacyCsv);TimeAttackRecords restoredBoard;check(restoredBoard.load(records.back())&&restoredBoard.entries().empty(),"restored CSV resurrected old rankings");
    text(oldGhosts.front(),"restored old ghost");check(migrateTimeAttackSeason(root).complete&&!std::filesystem::exists(oldGhosts.front()),"restored old data retry");
    // A valid historical fallback card also receives the migration, preserving
    // progress if the main card is missing; future-season data is never rewritten.
    const auto recovery=root/"recovery";LocalDriverProfiles recoveryStore(recovery/"driver_profiles_v1");
    auto backup=recoveryStore.path(2);backup+=".previous";write(backup,oldProfile(2,fixture(2)));
    check(migrateTimeAttackSeason(recovery).complete,"backup-only migration failed");profilePreserved(fixture(2),recoveryStore.load(2).profile);
    auto future=bytes(current.path(0));put(future,24,timeAttackSeason+1);checksum(future);write(current.path(0),future);
    check(current.load(0).origin==LocalDriverProfiles::Origin::Unreadable&&!current.save(0,fresh),"future season fell back or was overwritten");
    check(!migrateTimeAttackSeason(root).complete&&bytes(current.path(0))==future,"future data modified by startup");
    // Failure preserves the original and retries independently of other files.
    const auto failure=root/"failure";LocalDriverProfiles blocked(failure/"driver_profiles_v1");const auto old=oldProfile(3,fixture(3));write(blocked.path(3),old);
    auto blockedArchive=blocked.path(3);blockedArchive+=".before-time-attack-season-2.bak";std::filesystem::create_directory(blockedArchive);
    check(!migrateTimeAttackSeason(failure).complete&&bytes(blocked.path(3))==old,"failed archive damaged original");
    std::filesystem::remove(blockedArchive);check(migrateTimeAttackSeason(failure).complete,"partial migration retry failed");
    auto unexpected=retiredGhost(failure/"driver_profiles_v1",0,false,false);std::filesystem::create_directories(unexpected);text(unexpected/"sentinel.txt","do not traverse");
    check(!migrateTimeAttackSeason(failure).complete&&std::filesystem::is_regular_file(unexpected/"sentinel.txt"),"unexpected ghost directory traversed");
    check(!migrateTimeAttackSeason({}).complete,"empty migration root accepted");
    // Startup result recovery runs before the global migration. Legacy receipts
    // must award once without reviving old TA data or erasing new-season times.
    const auto receiptRoot=std::filesystem::absolute(root/"receipts");
    LocalDriverProfiles receiptProfiles(receiptRoot/"driver_profiles_v1");OnlineResultReward rewards(receiptRoot);
    OnlineResultReward::Plan pending;pending.selection=5;pending.points=2000;pending.recordSeason=1;
    pending.before=fixture(0);pending.after=pending.before;pending.after.setu(72,pending.before.u(72)+2000);
    write(receiptProfiles.path(0),oldProfile(0,pending.before));
    rewards.apply(pending);auto paid=receiptProfiles.load(0).profile;
    auto expected=pending.after;clearPreviousSeasonTimeAttack(expected);
    check(paid.words==expected.words,"legacy receipt before migration lost reward or restored old TA");
    const auto paidBytes=bytes(receiptProfiles.path(0));rewards.apply(pending);
    check(bytes(receiptProfiles.path(0))==paidBytes,"legacy receipt replay awarded twice");
    // A restored receipt after the migration cannot erase a new personal best.
    auto newTimes=pending.before;clearPreviousSeasonTimeAttack(newTimes);newTimes.setu(176,1700000);newTimes.setu(500,500000);
    check(receiptProfiles.save(0,newTimes),"new-season receipt baseline");rewards.apply(pending);
    newTimes.setu(72,pending.after.u(72));check(receiptProfiles.load(0).profile.words==newTimes.words,"legacy receipt overwrote new-season records");
    const auto beforeRetry=bytes(receiptProfiles.path(0));rewards.apply(pending);check(bytes(receiptProfiles.path(0))==beforeRetry,"new-season retry not idempotent");
    auto conflict=newTimes;conflict.setu(72,conflict.u(72)+1000);check(receiptProfiles.save(0,conflict),"reward conflict fixture");
    bool rejected=false;try{rewards.apply(pending);}catch(const std::exception&){rejected=true;}
    check(rejected&&receiptProfiles.load(0).profile.words==conflict.words,"old receipt overwrote newer non-TA progress");
    pending.recordSeason=timeAttackSeason+1;rejected=false;try{rewards.apply(pending);}catch(const std::exception&){rejected=true;}
    check(rejected,"future-season reward accepted");
    std::cout<<"PASS "<<checks<<" season migration checks; all 210 cars preserved outside TA fields, seven boards reset, old ghosts removed, backups and retries verified. Scratch: "<<root<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
