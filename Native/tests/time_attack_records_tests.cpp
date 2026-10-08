#include "time_attack_records.h"
#include "imported_course_catalog.h"
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace idas3;
void require(bool x,const char* message){if(!x)throw std::runtime_error(message);}
std::string read(const std::filesystem::path& file){std::ifstream in(file,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void idZeroMigration(){
    const auto root=std::filesystem::temp_directory_path()/("idas3-record-migration-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const std::string base="condition,weather,car,finish_ticks6000",name=",name0,name1,name2,name3,name4,manual,night",splits=",split1,split2,split3";
    // Each historical format in a different save, including a Unicode path.
    for(unsigned format=0;format<3;++format){
        const auto file=root/std::to_string(format)/std::filesystem::path(u8"記録.csv");
        std::filesystem::create_directory(file.parent_path());
        {std::ofstream out(file);out<<base<<(format?name:"")<<(format==2?splits:"")<<'\n';
            for(unsigned condition=0;condition<supportedConditionCount;++condition)for(unsigned wet=0;wet<2;++wet)for(unsigned car=0;car<35;++car)
                out<<condition<<','<<wet<<','<<car<<','<<1000000+car<<(format?",1,2,3,4,221,1,1":"")<<(format==2?",200000,400000,600000":"")<<'\n';
        }
        const auto original=read(file);TimeAttackRecords records;
        require(records.load(file)&&records.needsMigration(),"Historical Gunsai/Odawara rows need migration");
        require(read(file)==original,"Read-only imports must never rewrite save files");
        for(unsigned condition=0;condition<supportedConditionCount;++condition)for(unsigned wet=0;wet<2;++wet)for(unsigned car=0;car<35;++car){
            const auto e=records.personalBest(condition,wet,car);
            require(e.ticks6000==(condition>=32?0:1000000+car),"Reset only Gunsai/Odawara, across directions/weather/all models");
            if(condition<32&&format)require(e.manual&&e.night&&e.nameGlyphs[0]==1,"Preserve unaffected record metadata");
            if(condition<32&&format==2)require(e.intermediate6000==std::array<std::uint32_t,3>{200000,400000,600000},"Preserve unrelated checkpoints");
        }
        require(records.save(file),"Archive and migrate local times");auto archive=file;archive+=".before-idzero-revision-1.bak";
        require(read(archive)==original,"Legacy CSV backup must be byte-exact");
        for(unsigned condition=32;condition<36;++condition)for(unsigned wet=0;wet<2;++wet)for(unsigned car=0;car<35;++car)
            records.record({condition,wet,car,1400000+condition*1000+wet*100+car});
        require(records.save(file),"New slower post-reset times must save");
        TimeAttackRecords loaded;require(loaded.load(file)&&!loaded.needsMigration(),"Migration is one-time");
        for(unsigned condition=32;condition<36;++condition)for(unsigned wet=0;wet<2;++wet)for(unsigned car=0;car<35;++car)
            require(loaded.best(condition,wet,car).model==1400000+condition*1000+wet*100+car,"New Gunsai/Odawara times survive restart");
        require(loaded.save(file)&&read(archive)==original,"Later saves must retain the first recovery archive");
        {std::ofstream out(file);out<<base<<"\n34,0,0,900000\ninvalid\n";}
        require(!loaded.load(file)&&!loaded.needsMigration()&&loaded.best(34,0,0).model==1434000,"Malformed migration must not partially replace live data");
        {std::ofstream out(file);out<<base<<name<<splits<<",course_revision\n34,0,0,900000,1,2,3,4,221,1,1,0,0,0,2\n";}
        require(!loaded.load(file)&&loaded.best(34,0,0).model==1434000,"Future revision must fail without discarding current state");
    }
    const auto blocked=root/"blocked.csv";{std::ofstream out(blocked);out<<base<<"\n34,0,0,900000\n";}
    auto archive=blocked;archive+=".before-idzero-revision-1.bak";std::filesystem::create_directory(archive);
    const auto original=read(blocked);TimeAttackRecords records;
    require(records.load(blocked)&&!records.save(blocked)&&read(blocked)==original,"Archive failure must leave original data intact");
    // Upgrading the Odawara-only fix must retain its new records and backup.
    const auto mixed=root/"already-migrated-odawara.csv";
    const std::string mixedCsv=base+name+splits+",course_revision\n"
        "32,0,0,900000,1,2,3,4,221,1,1,0,0,0,0\n"
        "33,1,34,950000,1,2,3,4,221,1,1,0,0,0,0\n"
        "34,0,0,1200000,1,2,3,4,221,1,1,200000,400000,600000,1\n"
        "35,1,34,1300000,1,2,3,4,221,0,0,0,0,0,1\n";
    {std::ofstream out(mixed,std::ios::binary);out<<mixedCsv;}
    auto oldArchive=mixed;oldArchive+=".before-odawara-revision-1.bak";
    {std::ofstream out(oldArchive);out<<"Earlier Odawara backup";}
    require(records.load(mixed)&&records.needsMigration()&&records.best(32,0,0).model==0&&records.best(33,1,34).model==0,"Old Gunsai rows were not removed after the earlier Odawara migration");
    require(records.best(34,0,0).model==1200000&&records.best(35,1,34).model==1300000,"Gunsai migration reset valid Odawara times");
    require(records.personalBest(34,0,0).intermediate6000==std::array<std::uint32_t,3>{200000,400000,600000},"Gunsai migration changed Odawara splits");
    require(records.save(mixed),"Separate backup for follow-up Gunsai migration");
    auto newArchive=mixed;newArchive+=".before-idzero-revision-1.bak";
    require(read(newArchive)==mixedCsv&&read(oldArchive)=="Earlier Odawara backup","Follow-up migration overwrote the earlier archive or omitted its new backup");
    records.record({32,0,0,1500000});records.record({33,1,34,1600000});
    require(records.save(mixed)&&records.load(mixed)&&!records.needsMigration()&&records.best(32,0,0).model==1500000&&records.best(33,1,34).model==1600000,"New Gunsai times did not survive follow-up migration");
    require(records.best(34,0,0).model==1200000&&records.best(35,1,34).model==1300000,"New Gunsai saves lost valid Odawara records");
    std::cout<<"PASS Gunsai/Odawara migration: all cars, both directions/weather, three legacy formats, Unicode paths, exact backups, restart, failure safety and Odawara-only upgrade\n";
}
int main(){try{
    idZeroMigration();
    TimeAttackRecords records;records.record({6,0,0,1000124});records.record({6,0,1,999996});
    records.record({6,1,0,700000});records.record({7,0,0,600000});
    auto best=records.best(6,0,0);require(best.course==999996&&best.model==1000124,"source course/weather partition and shared course/model records");
    for(unsigned i=0;i<100;++i)records.record({6,0,2,1200000+i*4});
    require(records.entries().size()<=14&&records.best(6,0,2).model==1200000,"retain top ten and every model best");
    const auto file=std::filesystem::temp_directory_path()/"idas3_native_records_test.csv";
    require(records.save(file),"save records");TimeAttackRecords loaded;require(loaded.load(file),"load records");
    require(loaded.best(6,0,0).model==1000124,"retain exact 6000Hz finish value");require(loaded.save(file),"replace records safely");
    {std::ofstream out(file);out<<"condition,weather,car,finish_ticks6000\n6,0,0,invalid\n";}
    require(!loaded.load(file)&&loaded.best(6,0,0).model==1000124,"reject malformed data without replacing valid in-memory records");
    TimeAttackEntry hakone{18,0,0,1800000};hakone.intermediate6000={400000,800000,1300000};hakone.nameGlyphs={1,2,3,4,221};hakone.night=true;
    records.record(hakone);records.record({19,1,0,1900000});
    require(records.save(file)&&loaded.load(file),"Extended records persist alongside legacy courses");
    require(loaded.best(6,0,0).model==1000124&&loaded.best(18,0,0).model==1800000&&loaded.best(19,1,0).model==1900000,"Hakone direction/weather separate from Akina");
    require(loaded.personalBest(18,0,0).intermediate6000==hakone.intermediate6000,"Personal checkpoint splits survive restart");
    records.record({20,0,0,1700000});records.record({21,1,0,1600000});
    require(records.save(file)&&loaded.load(file),"Sadamine records survive save/reload");
    require(loaded.best(20,0,0).model==1700000&&loaded.best(21,1,0).model==1600000&&loaded.best(18,0,0).model==1800000&&loaded.best(6,0,0).model==1000124,"Sadamine, Hakone and original course partitions remain independent");
    require(loaded.best(20,1,0).model==0&&loaded.best(21,0,0).model==0,"Sadamine direction/weather records do not leak");
    records.record({22,0,0,1500000});records.record({23,0,0,1550000});
    require(records.save(file)&&loaded.load(file),"Enna records survive save/reload");
    require(loaded.best(22,0,0).model==1500000&&loaded.best(23,0,0).model==1550000&&loaded.best(6,0,0).model==1000124&&loaded.best(20,0,0).model==1700000,"Enna directions do not overwrite Akina or Sadamine records");
    for(unsigned c=24;c<30;++c)for(unsigned w=0;w<2;++w)records.record({c,w,0,1800000+c*1000+w*500});
    require(records.save(file)&&loaded.load(file),"New course records persist");
    for(unsigned c=24;c<30;++c)for(unsigned w=0;w<2;++w)require(loaded.best(c,w,0).model==1800000+c*1000+w*500,"Special Stage course/direction/weather remains separate");
    require(loaded.best(6,0,0).model==1000124&&loaded.best(22,0,0).model==1500000,"Existing records survive the new maps");
    bool rejected=false;try{records.record({supportedConditionCount,0,0,1});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Unknown course condition rejected");
    TimeAttackRecords full;
    for(unsigned condition=0;condition<supportedConditionCount;++condition)for(unsigned weather=0;weather<2;++weather){
        for(unsigned place=0;place<10;++place)full.record({condition,weather,0,100000+place});
        for(unsigned car=1;car<35;++car)full.record({condition,weather,car,200000+car});
    }
    require(full.entries().size()==supportedConditionCount*2*44&&full.save(file)&&loaded.load(file)&&loaded.entries().size()==supportedConditionCount*2*44,"Full retention across all supported courses survives reload");
    std::filesystem::remove(file);std::cout<<"Native records: exact timestamps, source partitions, bounded retention and persistence pass.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
