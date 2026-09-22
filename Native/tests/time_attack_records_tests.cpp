#include "time_attack_records.h"
#include "imported_course_catalog.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
void require(bool x,const char* message){if(!x)throw std::runtime_error(message);}
int main(){try{
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
