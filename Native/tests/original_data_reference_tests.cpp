#include "original_data.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("Usage: original_data_reference_tests canonical_image HOSTFS original_physics_data_root");
    RefMemory image{std::filesystem::path(argv[1])};const std::filesystem::path root=argv[3],hostfs=argv[2];
    auto data=OriginalPhysicsData::load(root/"tables.bin");std::size_t comparisons=0;
    const auto check=[&](std::uint32_t actual,std::uint32_t expected){++comparisons;if(actual!=expected)throw std::runtime_error("Exported original data bit mismatch");};
    const auto scalar=[&](float actual,std::uint32_t address){check(std::bit_cast<std::uint32_t>(actual),image.read32(address));};
    std::size_t profileInstructions=0;
    // Actual fresh-profile initializer, including its memset and four complete
    // original copy calls. Seed nonzero storage so unwritten zeros cannot pass.
    constexpr std::uint32_t profile=0x0C31C99C,profileStack=0x0CFFF000,profileStop=0x0F000000;
    image.zeroRegion(profileStack-0x2000,0x2000);
    for(unsigned i=0;i<1228;++i)image.write8(profile+i,0xa5);
    RefCpu fresh(image);fresh.r[15]=profileStack;fresh.pr=profileStop;
    profileInstructions+=fresh.run(0x0C134A60,profileStop,100000);
    for(const auto offset:{0u,16u,24u,32u,68u})check(image.read32(profile+offset),0);
    check(image.read8(profile+152),0);check(image.read8(profile+164),0);
    // Manufacturer lists in31D88C contain all35 cars; execute the original
    // selector instead of merely assigning its expected car output.
    std::array<bool,35> freshCars{};
    constexpr std::array<unsigned,7> manufacturerCounts{7,9,4,5,6,3,1};
    for(unsigned maker=0;maker<7;++maker)for(unsigned model=0;model<manufacturerCounts[maker];++model){
        RefCpu carSelector(image);carSelector.r[4]=maker;carSelector.r[5]=model;carSelector.r[15]=profileStack;carSelector.pr=profileStop;
        profileInstructions+=carSelector.run(0x0C133A60,profileStop,100);
        const auto car=image.read32(profile+16);if(car>=35||freshCars[car])throw std::runtime_error("Original fresh-car selector roster changed");freshCars[car]=true;
        check(image.read32(profile+24),0);check(image.read8(profile+164),0);
        // Mode2 is selected independently of the immutable fresh tuning bytes.
        image.write32(profile,2);
        for(unsigned condition=0;condition<18;++condition)for(unsigned weather=0;weather<2;++weather){
            image.write32(profile+4,condition/2);image.write32(profile+12,condition&1);image.write32(profile+28,condition);image.write32(profile+32,weather);
            const auto native=makeOriginalFreshTimeAttackSelection(car,condition,weather?OriginalWeather::Wet:OriginalWeather::Dry);
            RefCpu setup(image);setup.r[0]=profile;setup.r[8]=condition;
            profileInstructions+=setup.run(0x0C15973E,0x0C1597CE,300);
            check(native.vehicleIndex,image.read32(0x0C901654));check(native.conditionCode,image.read32(0x0C9015CC));
            check(native.vehicleMode0C9015E0,image.read32(0x0C9015E0));check(native.upgradeIndex0C9015F0,image.read32(0x0C9015F0));
            check(native.overrideMode0C9015F4,image.read32(0x0C9015F4));check(native.mode0C9015FC,image.read32(0x0C9015FC));
            check(native.mode0C9015C0,image.read32(0x0C9015C0));check(image.read32(0x0C9015C4),1); // fresh profile defaults to AT
            setup.r[9]=0xffffffffu; // actual mode2 solo opponent argument
            profileInstructions+=setup.run(0x0C159840,0x0C15984A,20);
            check(native.progressEnabled0C9015E4,image.read32(0x0C9015E4));
            profileInstructions+=setup.run(0x0C061B38,0x0C061B3E,10);
            profileInstructions+=setup.run(0x0C159862,0x0C15986A,10);
            check(native.progressMode0C9015D4,image.read32(0x0C9015D4));
        }
    }
    // Independent source polarity: actual16775C..167764 selects literal
    // string DRY for0 and RAIN for1.15973E..159754 propagates profile weather
    // to9015FC and derives the separate snow flag from the condition.
    for(std::uint32_t weather=0;weather<2;++weather){
        RefCpu label(image);label.r[12]=weather;label.run(0x0C16775C,0x0C167764,10);
        std::string name;for(unsigned i=0;image.read8(label.r[5]+i)!=0;++i)name+=char(image.read8(label.r[5]+i));
        if(name!=(weather==0?"DRY":"RAIN"))throw std::runtime_error("Original weather polarity changed");
        for(std::uint32_t condition=0;condition<18;++condition){
            OriginalPhysicsSelection selection;selection.conditionCode=condition;
            selectOriginalWeather(selection,weather?OriginalWeather::Wet:OriginalWeather::Dry);
            image.write32(0x0C31C99C+32,weather);
            RefCpu setup(image);setup.r[0]=0x0C31C99C;setup.r[8]=condition;
            setup.run(0x0C15973E,0x0C159754,30);
            const auto in=data.initialization(selection,{},{0,0,0});
            check(in.mode0C9015FC,image.read32(0x0C9015FC));
            check(in.mode0C9015C0,image.read32(0x0C9015C0));
        }
    }
    //042700 saves incoming r7(time) at local2504 and entry stack[0](weather)
    // at local2732. Execute its real tail, which writes these distinct object
    // fields; scene capture must not put weather in the time slot.
    for(std::uint32_t time=0;time<2;++time)for(std::uint32_t weather=0;weather<2;++weather){
        constexpr std::uint32_t stack=0x0CFD0000,object=0x0CFC0000;
        image.zeroRegion(stack,3000);image.zeroRegion(object,128);
        image.write32(stack+2504,time);image.write32(stack+2516,object);image.write32(stack+2732,weather);
        image.write32(object+48,1);
        RefCpu tail(image);tail.r[14]=stack;tail.run(0x0C0437BC,0x0C044560,32);
        check(image.read32(object+52),time);check(image.read32(object+56),weather);check(image.read32(object+48),1);
    }
    // The contact routine indexes2700F4, while dynamics indexes283F18.
    // All35 records are identical in this verified image; establish both
    // identities before sharing the exported table between those stages.
    for(std::uint32_t vehicle=0;vehicle<35;++vehicle){
        const auto record=data.carRecord(vehicle);
        for(std::size_t word=0;word<record.size();++word)
            check(record[word],image.read32(0x0C2700F4+vehicle*44+std::uint32_t(word*4)));
    }
    for(std::uint32_t condition=0;condition<18;++condition){
        auto path=data.loadPath(root,condition);const auto last=image.read32(0x0C283E88+condition*8);
        check(path.inclusiveLastIndex,last);check(std::uint32_t(path.points.size()),last+1);
        const auto originalString=[&](std::uint32_t address){std::string out;for(unsigned i=0;i<64;++i){const auto c=image.read8(address+i);if(c==0)return out;out+=char(c);}throw std::runtime_error("Unterminated original course filename");};
        const auto setupRecord=0x0C2EFF40+condition*1024;
        auto sourceStem=std::filesystem::path(originalString(setupRecord+448)).filename().string();
        const auto sourcePath=hostfs/"binary"/(sourceStem+(condition&1?"o_0.bin":"i_0.bin"));
        for(std::uint32_t weather=0;weather<2;++weather)for(std::uint32_t selector=0;selector<2;++selector){
            auto stem=std::filesystem::path(originalString(setupRecord+weather*512+320+selector*64)).filename().string();
            const auto sourceFile=hostfs/"binary"/(stem+".bin.nz");
            stem.erase(stem.find("_colli"),6);
            if(originalCollisionFile(condition,selector).string()!="collision_"+stem+".rcl")throw std::runtime_error("Collision filename disagrees with source setup table");
            std::ifstream originalFile(sourceFile,std::ios::binary),exportedFile(root/originalCollisionFile(condition,selector),std::ios::binary);
            if(!originalFile||!exportedFile)throw std::runtime_error("Course collision source/export missing");
            const std::vector<char> originalBytes{std::istreambuf_iterator<char>(originalFile),{}},exportedBytes{std::istreambuf_iterator<char>(exportedFile),{}};
            if(originalBytes!=exportedBytes)throw std::runtime_error("Course collision export changed source bytes");
            ++comparisons;
        }
        std::ifstream input(sourcePath,std::ios::binary);if(!input)throw std::runtime_error("Original path unavailable");
        for(const auto& point:path.points)for(float coordinate:point){std::uint32_t word=0;input.read(reinterpret_cast<char*>(&word),4);if(!input)throw std::runtime_error("Short original path");check(std::bit_cast<std::uint32_t>(coordinate),word);}
        for(std::uint32_t vehicle=0;vehicle<35;++vehicle)for(std::uint32_t overrideMode=0;overrideMode<2;++overrideMode){
            OriginalPhysicsSelection s;s.vehicleIndex=vehicle;s.conditionCode=condition;s.overrideMode0C9015F4=overrideMode;
            s.vehicleMode0C9015E0=(vehicle+condition)%32;s.upgradeIndex0C9015F0=(vehicle*7+condition)%76;
            selectOriginalWeather(s,overrideMode?OriginalWeather::Wet:OriginalWeather::Dry);
            auto p=data.parameters(s,path);const auto index=(condition/2)*140+vehicle*4;
            scalar(p.frame.table0C28451C,0x0C28451C+condition*140+vehicle*4);scalar(p.frame.table0C28866C,0x0C28866C+s.upgradeIndex0C9015F0*4);
            scalar(p.angular.carRecord0C283F18_08,0x0C283F18+vehicle*44+8);scalar(p.angular.carRecord0C283F18_0C,0x0C283F18+vehicle*44+12);
            scalar(p.angular.table0C285124,0x0C285124+index);scalar(p.angular.table0C285610,0x0C285610+index);scalar(p.angular.table0C285AFC,0x0C285AFC+index);
            scalar(p.angular.table0C285FE8,0x0C285FE8+index);scalar(p.angular.table0C287398,0x0C287398+index);
            scalar(p.steeringMemory.table0C2864D4,0x0C2864D4+index);scalar(p.steeringMemory.table0C2869C0,0x0C2869C0+index);scalar(p.steeringMemory.table0C286EAC,0x0C286EAC+index);
            scalar(p.steeringMemory.table0C287884,0x0C287884+index);scalar(p.steeringMemory.table0C287D70,0x0C287D70+index);
            scalar(p.loss.cap0C284F80,0x0C284F80+vehicle*8);scalar(p.loss.growth0C284F84,0x0C284F84+vehicle*8);
            const auto rowAddress=overrideMode?0x0C288654:0x0C28830C+vehicle*24;
            check(p.transmission.profileIndex,image.read32(rowAddress));check(p.transmission.maximumGear,image.read32(rowAddress+4));
            check(std::bit_cast<std::uint32_t>(p.transmission.workingBase),std::bit_cast<std::uint32_t>(image.readFloat(rowAddress+8)-(overrideMode?0.0f:500.0f)));
            scalar(p.transmission.lower,rowAddress+12);scalar(p.transmission.upper,rowAddress+16);scalar(p.transmission.divisor,rowAddress+20);
            for(std::size_t i=0;i<22;++i)check(p.profile.words[i],image.read32(0x0C28825C+p.transmission.profileIndex*88+std::uint32_t(i*4)));
            auto in=data.initialization(s,{12,23,34},{0,.2f,0});
            check(in.mode0C9015C0,condition>15?1u:0u);
            check(in.throttleHistoryCount0C285098,image.read32(0x0C285098+vehicle*4));check(in.vehicleType0C284EF4,image.read32(0x0C284EF4+vehicle*4));
            check(in.modeMask0C283E08,image.read32(0x0C283E08+s.vehicleMode0C9015E0*4));
            auto record=data.carRecord(vehicle);for(std::size_t i=0;i<11;++i)check(record[i],image.read32(0x0C283F18+vehicle*44+std::uint32_t(i*4)));
            OriginalVehicleState state;OriginalInitializationSideState side;
            initializeOriginalVehicle(state,p,side,in);
            check(state.drive.u(0x434),overrideMode);check(state.drive.u(0x438),condition>15?1u:0u);
            check(std::bit_cast<std::uint32_t>(p.angular.global0C900E40),0x3F800000);
            check(state.drive.u(0x1C0),in.throttleHistoryCount0C285098);
        }
    }
    auto path=data.loadPath(root,6);OriginalPhysicsSelection bad;bad.conditionCode=6;bad.vehicleIndex=35;
    bool rejected=false;try{data.parameters(bad,path);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("Unsupported selectable car accepted");
    bad.vehicleIndex=0;bad.upgradeIndex0C9015F0=76;rejected=false;try{data.parameters(bad,path);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("Unsupported upgrade index accepted");
    std::cout<<"PASS 1,260 parameter/initializer selections and1,260 fresh Time Attack profile selections, all35 cars/all18 source paths, "<<comparisons<<" original-data bit comparisons; "<<profileInstructions<<" actual fresh-profile/setup instructions, zero hooks. No original opcodes in exported pack.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
