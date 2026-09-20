// This translation unit intentionally includes the backend for pure GUID,
// UTF-8 and actuator-selection checks without adding a shipping test API.
// Hardware calls below are restricted to read-only device discovery. It never
// calls Idas3WheelSetForce, Acquire, CreateEffect, Start or SetParameters.
#include "../src/wheel_feedback_backend.cpp"
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks=0;
void require(bool condition,const char* reason){++checks;if(!condition)throw std::runtime_error(reason);}
void pureChecks(){
    require(sizeof(Idas3WheelDevice)==180,"Wheel device ABI size changed");
    GUID guid{};
    require(parseGuid("{01234567-89ab-cdef-0123-456789abcdef}",guid),"Canonical GUID rejected");
    require(guid.Data1==0x01234567u&&guid.Data2==0x89abu&&guid.Data3==0xcdefu&&guid.Data4[0]==1&&guid.Data4[7]==0xef,"GUID byte order differs");
    char formatted[40]{};formatGuid(guid,formatted);
    require(std::strcmp(formatted,"{01234567-89ab-cdef-0123-456789abcdef}")==0,"GUID did not round trip");
    GUID plain{};require(parseGuid("01234567-89AB-CDEF-0123-456789ABCDEF",plain)&&IsEqualGUID(guid,plain),"Uppercase or brace-free GUID rejected");
    const char* invalid[]{nullptr,"","0","first","01234567-89ab-cdef-0123-456789abcdeg","{01234567-89ab-cdef-0123-456789abcdef","{01234567-89ab-cdef-0123-456789abcdef}}","{00000000-0000-0000-0000-000000000000}","0123456789ab-cdef-0123-456789abcdef"};
    for(auto text:invalid)require(!parseGuid(text,plain),"Invalid explicit GUID accepted");
    const char* utf8="Wheel \xe2\x86\x92 \xf0\x9f\x8f\x8e";
    for(std::size_t capacity=1;capacity<24;++capacity){
        char output[32];std::memset(output,0x7f,sizeof(output));
        const auto count=copyUtf8(output,capacity,utf8);
        require(count<capacity&&output[count]==0,"UTF-8 copy overflow/termination");
        require(count==0||MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,output,int(count),nullptr,0)>0,"UTF-8 truncation split a code point");
        require(static_cast<unsigned char>(output[capacity])==0x7f,"UTF-8 copy wrote beyond capacity");
    }
    DIDEVICEOBJECTINSTANCEW object{};object.dwSize=sizeof(object);object.dwType=DIDFT_ABSAXIS;object.guidType=GUID_XAxis;
    require(!actuatorAxis(object),"Input-only steering axis accepted as force actuator");
    object.dwFlags=DIDOI_FFACTUATOR;require(actuatorAxis(object),"Force-capable X axis rejected");
    object.guidType=GUID_Slider;require(!actuatorAxis(object),"Slider accepted as steering force axis");
    object.guidType=GUID_XAxis;object.wUsagePage=2;
    for(WORD usage:{WORD(0xc4),WORD(0xc5),WORD(0xc6),WORD(0xbb)}){object.wUsage=usage;require(!actuatorAxis(object),"Pedal/throttle accepted as steering force axis");}
    object.wUsage=0xc8;require(actuatorAxis(object),"HID steering actuator rejected");
    object.dwType=DIDFT_BUTTON;require(!actuatorAxis(object),"Button accepted as steering force axis");
    require(effectDurationMicroseconds==100000,"Effect duration is not bounded to 100 ms");
}
}
int main()try{
    pureChecks();
    const int count=Idas3WheelRefreshDevices();
    char status[512]{};const auto statusLength=Idas3WheelCopyStatus(status,int(sizeof(status)));
    require(statusLength>0&&status[statusLength]==0,"Discovery status missing or unterminated");
    require(count>=0,"Read-only DirectInput discovery failed");
    Idas3WheelDevice invalid{};require(Idas3WheelGetDevice(0,&invalid)==0,"Invalid device record size accepted");
    invalid.size=sizeof(invalid);require(Idas3WheelGetDevice(-1,&invalid)==0&&Idas3WheelGetDevice(count,&invalid)==0,"Out-of-range discovery index accepted");
    for(int index=0;index<count;++index){
        Idas3WheelDevice found{};found.size=sizeof(found);
        require(Idas3WheelGetDevice(index,&found)==1&&found.size==180,"Enumerated device record missing");
        GUID parsed{};require(parseGuid(found.id,parsed),"Discovery GUID is invalid");
        require(found.name[0]&&std::memchr(found.name,0,sizeof(found.name)),"Device name not terminated");
        require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,found.name,-1,nullptr,0)>0,"Device name is not UTF-8");
        std::cout<<"DEVICE "<<found.id<<" "<<found.name<<" VID="<<found.vendorId<<" PID="<<found.productId<<'\n';
    }
    Idas3WheelShutdown();
    require(backend.input==nullptr&&backend.device==nullptr&&backend.effect==nullptr,"Discovery shutdown retained a device");
    std::cout<<"PASS "<<checks<<" GUID/UTF8/axis/ABI/discovery checks; "<<count<<" devices. No acquisition or motor commands. "<<status<<'\n';
    return 0;
}catch(const std::exception& error){Idas3WheelShutdown();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
