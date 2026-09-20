#include "wheel_feedback_backend.h"
#include "original_ffb.h"
#include "original_ffb_owner.h"
#ifndef _WIN32
#error The wheel feedback backend requires Windows DirectInput 8.
#endif
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dinput.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <mutex>
#include <vector>

namespace {
constexpr DWORD effectDurationMicroseconds=100000;
constexpr std::size_t maximumDevices=256;

std::size_t copyUtf8(char* destination,std::size_t capacity,const char* source) noexcept {
    if(!destination||capacity==0)return 0;
    const auto length=std::strlen(source);
    auto count=std::min(length,capacity-1);
    // source[count] is the first omitted byte. Do not retain a partial UTF-8
    // code point when the next byte is a continuation of the preceding one.
    while(count>0&&count<length&&(static_cast<unsigned char>(source[count])&0xC0u)==0x80u)--count;
    std::memcpy(destination,source,count);destination[count]=0;return count;
}
void nameUtf8(char* destination,std::size_t capacity,const wchar_t* source) noexcept {
    char converted[MAX_PATH*4+1]{};
    const int result=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,source,-1,converted,int(sizeof(converted)),nullptr,nullptr);
    copyUtf8(destination,capacity,result>1?converted:"Unnamed force-feedback device");
}
void formatGuid(const GUID& guid,char (&text)[40]) noexcept {
    std::snprintf(text,sizeof(text),"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        guid.Data1,unsigned(guid.Data2),unsigned(guid.Data3),unsigned(guid.Data4[0]),unsigned(guid.Data4[1]),
        unsigned(guid.Data4[2]),unsigned(guid.Data4[3]),unsigned(guid.Data4[4]),unsigned(guid.Data4[5]),
        unsigned(guid.Data4[6]),unsigned(guid.Data4[7]));
}
int hexDigit(char value) noexcept {
    if(value>='0'&&value<='9')return value-'0';
    if(value>='a'&&value<='f')return value-'a'+10;
    if(value>='A'&&value<='F')return value-'A'+10;
    return -1;
}
bool parseGuid(const char* text,GUID& result) noexcept {
    if(!text)return false;
    // Bounded read; only canonical GUID forms are accepted, never a device
    // index, VID/PID alias, partial name, or an automatic first-device choice.
    std::size_t length=0;while(length<40&&text[length])++length;
    const char* first=text;
    if(length==38&&text[0]=='{'&&text[37]=='}')++first;
    else if(length!=36)return false;
    constexpr int hyphens[]{8,13,18,23};
    for(int at:hyphens)if(first[at]!='-')return false;
    unsigned char bytes[16]{};int byte=0;
    for(int at=0;at<36;){
        if(at==8||at==13||at==18||at==23){++at;continue;}
        const int high=hexDigit(first[at]),low=hexDigit(first[at+1]);
        if(high<0||low<0||byte>=16)return false;
        bytes[byte++]=static_cast<unsigned char>((high<<4)|low);at+=2;
    }
    if(byte!=16)return false;
    GUID parsed{};
    parsed.Data1=(DWORD(bytes[0])<<24)|(DWORD(bytes[1])<<16)|(DWORD(bytes[2])<<8)|bytes[3];
    parsed.Data2=WORD((unsigned(bytes[4])<<8)|bytes[5]);
    parsed.Data3=WORD((unsigned(bytes[6])<<8)|bytes[7]);
    std::copy_n(bytes+8,8,parsed.Data4);
    if(IsEqualGUID(parsed,GUID_NULL))return false;
    result=parsed;return true;
}
DIPROPDWORD deviceProperty(DWORD value=0) noexcept {
    DIPROPDWORD property{};property.diph.dwSize=sizeof(property);
    property.diph.dwHeaderSize=sizeof(property.diph);property.diph.dwHow=DIPH_DEVICE;
    property.dwData=value;return property;
}
bool actuatorAxis(const DIDEVICEOBJECTINSTANCEW& object) noexcept {
    if((object.dwFlags&DIDOI_FFACTUATOR)==0||(DIDFT_GETTYPE(object.dwType)&DIDFT_AXIS)==0)return false;
    if(IsEqualGUID(object.guidType,GUID_Slider))return false;
    // HID Simulation Controls accelerator, brake, clutch and throttle are
    // never steering output targets, even if a driver marks one as an actuator.
    if(object.wUsagePage==0x02&&(object.wUsage==0xC4||object.wUsage==0xC5||object.wUsage==0xC6||object.wUsage==0xBB))return false;
    return true;
}
BOOL CALLBACK findActuator(const DIDEVICEOBJECTINSTANCEW* object,void* context) noexcept {
    if(object&&actuatorAxis(*object)){
        *static_cast<DWORD*>(context)=object->dwType;return DIENUM_STOP;
    }
    return DIENUM_CONTINUE;
}
HWND foregroundUnityWindow() noexcept {
    const HWND foreground=GetForegroundWindow();if(!foreground)return nullptr;
    DWORD process=0;GetWindowThreadProcessId(foreground,&process);
    if(process!=GetCurrentProcessId())return nullptr;
    const HWND window=GetAncestor(foreground,GA_ROOTOWNER);
    if(!window||!IsWindowVisible(window)||IsIconic(window))return nullptr;
    GetWindowThreadProcessId(window,&process);if(process!=GetCurrentProcessId())return nullptr;
    wchar_t className[64]{};
    if(GetClassNameW(window,className,int(std::size(className)))==0||std::wcscmp(className,L"UnityWndClass")!=0)return nullptr;
    return window;
}

struct DeviceEntry { GUID guid{};Idas3WheelDevice description{}; };
struct Backend {
    std::mutex mutex;
    IDirectInput8W* input=nullptr;
    IDirectInputDevice8W* device=nullptr;
    IDirectInputEffect* effect=nullptr;
    // The rest of the cabinet's board. Each is optional: a wheel that cannot
    // play one leaves it null and still gets everything else.
    IDirectInputEffect* springEffect=nullptr;
    IDirectInputEffect* damperEffect=nullptr;
    IDirectInputEffect* rumbleEffect=nullptr;
    // Private dependency used by CPU-only ownership tests. Shipping code
    // always uses the actual Windows foreground/Unity-window query above.
    HWND (*foregroundWindow)() noexcept=foregroundUnityWindow;
    GUID selected{};
    HWND window=nullptr;
    DWORD axis=0,previousAutocenter=0;
    bool autocenterChanged=false;
    std::vector<DeviceEntry> devices;
    char status[512]="Wheel feedback stopped. Refresh devices to discover compatible hardware.";

    void message(const char* text) noexcept {copyUtf8(status,sizeof(status),text);}
    void failure(const char* operation,HRESULT result) noexcept {
        const char* detail="driver rejected the operation";
        if(result==DIERR_INPUTLOST||result==DIERR_NOTACQUIRED)detail="device or focus was lost";
        else if(result==DIERR_OTHERAPPHASPRIO||result==DIERR_NOTEXCLUSIVEACQUIRED)detail="another application owns the device, or exclusive access is unavailable";
        else if(result==DIERR_DEVICENOTREG||result==DIERR_NOTFOUND||result==DIERR_UNPLUGGED)detail="selected device is disconnected or unavailable";
        else if(result==DIERR_UNSUPPORTED)detail="wheel driver does not support this feature";
        std::snprintf(status,sizeof(status),"%s: %s (0x%08lx). Force feedback stopped.",operation,detail,static_cast<unsigned long>(result));
    }
    // Called under mutex, including every partial-initialization failure.
    // No acquisition is attempted here, even after focus/device loss.
    HRESULT releaseDevice() noexcept {
        HRESULT restoration=DI_OK;
        const auto drop=[](IDirectInputEffect*& slot)noexcept{
            if(!slot)return;
            auto* released=slot;slot=nullptr;
            try{released->Stop();}catch(...){}
            try{released->Unload();}catch(...){}
            try{released->Release();}catch(...){}
        };
        drop(effect);drop(springEffect);drop(damperEffect);drop(rumbleEffect);
        if(device){
            auto* released=device;device=nullptr;
            try{released->Unacquire();}catch(...){}
            if(autocenterChanged){auto property=deviceProperty(previousAutocenter);
                try{restoration=released->SetProperty(DIPROP_AUTOCENTER,&property.diph);}catch(...){restoration=E_UNEXPECTED;}}
            try{released->Release();}catch(...){}
        }
        selected={};window=nullptr;axis=0;previousAutocenter=0;autocenterChanged=false;
        return restoration;
    }
    void releaseInput() noexcept {
        auto* released=input;input=nullptr;
        if(released)try{released->Release();}catch(...){}
    }
    void stopWithMessage(const char* reason) noexcept {
        const auto restored=releaseDevice();message(reason);
        if(FAILED(restored)){
            const auto count=std::strlen(status);
            std::snprintf(status+count,sizeof(status)-count," Autocenter restoration failed (0x%08lx); device released.",static_cast<unsigned long>(restored));
        }
    }
    bool stopForFailure(const char* operation,HRESULT result) noexcept {
        failure(operation,result);char reason[sizeof(status)];std::memcpy(reason,status,sizeof(reason));
        stopWithMessage(reason);return false;
    }
    bool initialize() {
        if(input)return true;
        const auto result=DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&input),nullptr);
        if(FAILED(result)){releaseInput();failure("DirectInput initialization",result);return false;}
        return true;
    }
    bool open(const GUID& guid,HWND target) {
        if(!initialize())return false;
        auto result=input->CreateDevice(guid,&device,nullptr);
        if(FAILED(result))return stopForFailure("Open selected wheel",result);
        selected=guid;window=target;
        DIDEVCAPS caps{};caps.dwSize=sizeof(caps);result=device->GetCapabilities(&caps);
        if(FAILED(result))return stopForFailure("Read wheel capabilities",result);
        if((caps.dwFlags&DIDC_FORCEFEEDBACK)==0){stopWithMessage("Selected controller has no DirectInput force-feedback actuators.");return false;}
        DIEFFECTINFOW info{};info.dwSize=sizeof(info);result=device->GetEffectInfo(&info,GUID_ConstantForce);
        if(FAILED(result))return stopForFailure("Constant-force support",result);
        if(DIEFT_GETTYPE(info.dwEffType)!=DIEFT_CONSTANTFORCE){stopWithMessage("Selected wheel does not support constant force.");return false;}
        axis=0;result=device->EnumObjects(findActuator,&axis,DIDFT_AXIS);
        if(FAILED(result))return stopForFailure("Read wheel actuator axes",result);
        if(axis==0){stopWithMessage("Selected device has no force-capable steering axis; pedals and sliders are excluded.");return false;}
        result=device->SetDataFormat(&c_dfDIJoystick2);
        if(FAILED(result))return stopForFailure("Set wheel data format",result);
        result=device->SetCooperativeLevel(target,DISCL_EXCLUSIVE|DISCL_FOREGROUND);
        if(FAILED(result))return stopForFailure("Set foreground exclusive wheel access",result);
        // DirectInput properties must be changed while unacquired. The saved
        // value is restored before release, including when Acquire fails.
        auto autocenter=deviceProperty();result=device->GetProperty(DIPROP_AUTOCENTER,&autocenter.diph);
        if(FAILED(result))return stopForFailure("Read original autocenter setting",result);
        previousAutocenter=autocenter.dwData;
        if(previousAutocenter!=DIPROPAUTOCENTER_OFF){
            autocenter.dwData=DIPROPAUTOCENTER_OFF;
            // Remember the obligation before SetProperty: even a failed driver
            // call can have changed state partially, so cleanup restores it.
            autocenterChanged=true;result=device->SetProperty(DIPROP_AUTOCENTER,&autocenter.diph);
            if(FAILED(result))return stopForFailure("Disable wheel autocenter",result);
        }
        if(foregroundWindow()!=target){stopWithMessage("Unity game window lost foreground; wheel feedback stopped.");return false;}
        result=device->Acquire();
        if(FAILED(result))return stopForFailure("Acquire selected wheel",result);
        LONG direction=DI_FFNOMINALMAX;DICONSTANTFORCE constant{};
        DIEFFECT parameters{};parameters.dwSize=sizeof(parameters);
        parameters.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTIDS;
        parameters.dwDuration=effectDurationMicroseconds;parameters.dwGain=DI_FFNOMINALMAX;
        parameters.dwTriggerButton=DIEB_NOTRIGGER;parameters.cAxes=1;
        parameters.rgdwAxes=&axis;parameters.rglDirection=&direction;
        parameters.cbTypeSpecificParams=sizeof(constant);parameters.lpvTypeSpecificParams=&constant;
        result=device->CreateEffect(GUID_ConstantForce,&parameters,&effect,nullptr);
        if(FAILED(result))return stopForFailure("Create finite constant-force effect",result);
        // The remaining board effects are best-effort. A wheel without them is
        // still usable, so a missing effect must not stop the whole feedback.
        createOptional(GUID_Spring,DIEFT_CONDITION,springEffect);
        createOptional(GUID_Damper,DIEFT_CONDITION,damperEffect);
        createOptional(GUID_Sine,DIEFT_PERIODIC,rumbleEffect);
        return true;
    }
    // Conditions and the periodic both need one axis and a direction, exactly
    // like the constant force; only the type-specific payload differs.
    void createOptional(const GUID& type,DWORD expected,IDirectInputEffect*& slot) noexcept {
        slot=nullptr;
        DIEFFECTINFOW info{};info.dwSize=sizeof(info);
        if(FAILED(device->GetEffectInfo(&info,type)))return;
        if(DIEFT_GETTYPE(info.dwEffType)!=expected)return;
        LONG direction=DI_FFNOMINALMAX;
        DIEFFECT parameters{};parameters.dwSize=sizeof(parameters);
        parameters.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTIDS;
        parameters.dwDuration=effectDurationMicroseconds;parameters.dwGain=DI_FFNOMINALMAX;
        parameters.dwTriggerButton=DIEB_NOTRIGGER;parameters.cAxes=1;
        parameters.rgdwAxes=&axis;parameters.rglDirection=&direction;
        DICONDITION condition{};DIPERIODIC periodic{};
        if(expected==DIEFT_CONDITION){
            parameters.cbTypeSpecificParams=sizeof(condition);
            parameters.lpvTypeSpecificParams=&condition;
        }else{
            periodic.dwPeriod=20000;
            parameters.cbTypeSpecificParams=sizeof(periodic);
            parameters.lpvTypeSpecificParams=&periodic;
        }
        if(FAILED(device->CreateEffect(type,&parameters,&slot,nullptr)))slot=nullptr;
    }
    static LONG scaled(float value) noexcept {
        const double bounded=double(value)<-1.0?-1.0:(double(value)>1.0?1.0:double(value));
        return static_cast<LONG>(std::lround(bounded*DI_FFNOMINALMAX));
    }
    // A condition centred on zero with symmetric coefficients is how the board's
    // spring and damper read: no offset, no dead band, full saturation.
    bool applyCondition(IDirectInputEffect* target,float magnitude) noexcept {
        if(!target)return true;
        if(magnitude<=0.f){try{target->Stop();}catch(...){}return true;}
        DICONDITION condition{};
        condition.lOffset=0;
        condition.lPositiveCoefficient=scaled(magnitude);
        condition.lNegativeCoefficient=scaled(magnitude);
        condition.dwPositiveSaturation=DWORD(DI_FFNOMINALMAX);
        condition.dwNegativeSaturation=DWORD(DI_FFNOMINALMAX);
        condition.lDeadBand=0;
        DIEFFECT parameters{};parameters.dwSize=sizeof(parameters);
        parameters.dwDuration=effectDurationMicroseconds;
        parameters.cbTypeSpecificParams=sizeof(condition);
        parameters.lpvTypeSpecificParams=&condition;
        return SUCCEEDED(target->SetParameters(&parameters,DIEP_DURATION|DIEP_TYPESPECIFICPARAMS|DIEP_START));
    }
    bool applyRumble(float intensity,float hertz) noexcept {
        if(!rumbleEffect)return true;
        if(intensity<=0.f||hertz<=0.f){try{rumbleEffect->Stop();}catch(...){}return true;}
        const double period=1000000.0/double(hertz);
        DIPERIODIC periodic{};
        periodic.dwMagnitude=DWORD(scaled(intensity<0.f?0.f:intensity));
        periodic.lOffset=0;periodic.dwPhase=0;
        periodic.dwPeriod=DWORD(period<1000.0?1000.0:(period>1000000.0?1000000.0:period));
        DIEFFECT parameters{};parameters.dwSize=sizeof(parameters);
        parameters.dwDuration=effectDurationMicroseconds;
        parameters.cbTypeSpecificParams=sizeof(periodic);
        parameters.lpvTypeSpecificParams=&periodic;
        return SUCCEEDED(rumbleEffect->SetParameters(&parameters,DIEP_DURATION|DIEP_TYPESPECIFICPARAMS|DIEP_START));
    }
    // The whole board in one call. Ownership, foreground and lease rules are
    // setForce's; only the payload is richer. Power scales every effect, as
    // the board's command 3 does.
    bool setCabinet(const char* text,const Idas3WheelCabinetState& state) {
        const auto finite=[](float v){return std::isfinite(v);};
        if(!finite(state.power)||!finite(state.torque)||!finite(state.spring)||
           !finite(state.damperStrength)||!finite(state.rumbleIntensity)||!finite(state.rumbleFrequencyHz)){
            stopWithMessage("Invalid wheel cabinet state; expected finite values. Feedback stopped.");return false;}
        if(state.power<0.f||state.power>1.f||state.torque<-1.f||state.torque>1.f||
           state.spring<0.f||state.spring>1.f||state.damperStrength<0.f||state.damperStrength>1.f||
           state.rumbleIntensity<0.f||state.rumbleIntensity>1.f){
            stopWithMessage("Wheel cabinet state out of range. Feedback stopped.");return false;}
        GUID guid{};
        if(!parseGuid(text,guid)){stopWithMessage("Select a valid explicit wheel GUID. Feedback stopped.");return false;}
        const HWND target=foregroundWindow();
        if(!target){stopWithMessage("The Unity game window must be foreground for wheel feedback.");return false;}
        if(device&&(!IsEqualGUID(guid,selected)||window!=target)){
            const auto restored=releaseDevice();
            if(FAILED(restored)){failure("Restore previous wheel autocenter",restored);return false;}
        }
        // Board disabled, or nothing to play: never acquire on an idle state.
        const float power=state.active?state.power:0.f;
        const bool silent=power<=0.f||(state.torque==0.f&&state.spring<=0.f&&
            state.damperStrength<=0.f&&state.rumbleIntensity<=0.f);
        if(!device&&silent){message("Idle cabinet state; wheel remains unacquired.");return true;}
        if(!device&&!open(guid,target))return false;
        auto result=device->Poll();
        if(FAILED(result))return stopForFailure("Poll selected wheel",result);
        if(foregroundWindow()!=target){stopWithMessage("Unity game window lost foreground; wheel feedback stopped.");return false;}
        if(silent){
            try{effect->Stop();}catch(...){}
            applyCondition(springEffect,0.f);applyCondition(damperEffect,0.f);applyRumble(0.f,0.f);
            message("Wheel ready; board idle.");return true;
        }
        DICONSTANTFORCE constant{scaled(state.torque*power)};
        DIEFFECT parameters{};parameters.dwSize=sizeof(parameters);
        parameters.dwDuration=effectDurationMicroseconds;
        parameters.cbTypeSpecificParams=sizeof(constant);parameters.lpvTypeSpecificParams=&constant;
        result=effect->SetParameters(&parameters,DIEP_DURATION|DIEP_TYPESPECIFICPARAMS|DIEP_START);
        if(FAILED(result)||result==DI_DOWNLOADSKIPPED)return stopForFailure("Refresh cabinet torque",result);
        // Optional effects never fail the call; a wheel without them keeps the
        // torque it can play.
        applyCondition(springEffect,state.spring*power);
        applyCondition(damperEffect,state.damperStrength*power);
        applyRumble(state.rumbleIntensity*power,state.rumbleFrequencyHz);
        if(foregroundWindow()!=target){stopWithMessage("Unity game window lost foreground; wheel feedback stopped.");return false;}
        std::snprintf(status,sizeof(status),
            "Wheel feedback active; cabinet board (torque %.2f, spring %.2f, damper %.2f, rumble %.2f @ %.0f Hz, power %.2f).",
            double(state.torque),double(state.spring),double(state.damperStrength),
            double(state.rumbleIntensity),double(state.rumbleFrequencyHz),double(state.power));
        return true;
    }
    bool setForce(const char* text,float force) {
        GUID guid{};
        if(!std::isfinite(force)||force< -1.f||force>1.f){stopWithMessage("Invalid wheel force; expected a finite value from -1 to 1. Feedback stopped.");return false;}
        if(!parseGuid(text,guid)){stopWithMessage("Select a valid explicit wheel GUID. Feedback stopped.");return false;}
        const HWND target=foregroundWindow();
        if(!target){stopWithMessage("The Unity game window must be foreground for wheel feedback.");return false;}
        if(device&&(!IsEqualGUID(guid,selected)||window!=target)){
            const auto restored=releaseDevice();
            if(FAILED(restored)){failure("Restore previous wheel autocenter",restored);return false;}
        }
        if(!device&&force==0.f){message("Zero force; wheel remains unacquired.");return true;}
        if(!device&&!open(guid,target))return false;
        // Do not repeatedly reacquire after loss: fail closed, release, and let
        // the managed owner decide when a fresh, foreground request is allowed.
        auto result=device->Poll();
        if(FAILED(result))return stopForFailure("Poll selected wheel",result);
        if(foregroundWindow()!=target){stopWithMessage("Unity game window lost foreground; wheel feedback stopped.");return false;}
        if(force==0.f){
            result=effect->Stop();
            if(FAILED(result))return stopForFailure("Stop zero-force effect",result);
            message("Wheel ready; zero force.");return true;
        }
        DICONSTANTFORCE constant{static_cast<LONG>(std::lround(double(force)*DI_FFNOMINALMAX))};
        DIEFFECT parameters{};parameters.dwSize=sizeof(parameters);
        parameters.dwDuration=effectDurationMicroseconds;
        parameters.cbTypeSpecificParams=sizeof(constant);parameters.lpvTypeSpecificParams=&constant;
        // DIEP_START restarts the bounded 100 ms lease on every fresh command.
        // No infinite effects, autonomous refresh thread, RESET or STOPALL.
        result=effect->SetParameters(&parameters,DIEP_DURATION|DIEP_TYPESPECIFICPARAMS|DIEP_START);
        if(FAILED(result)||result==DI_DOWNLOADSKIPPED)return stopForFailure("Refresh finite wheel force",result);
        if(foregroundWindow()!=target){stopWithMessage("Unity game window lost foreground; wheel feedback stopped.");return false;}
        message("Wheel feedback active; finite 100 ms constant force.");return true;
    }
};
Backend backend;

struct Enumeration { IDirectInput8W* input;std::vector<DeviceEntry> entries;bool failed=false; };
BOOL CALLBACK enumerateDevice(const DIDEVICEINSTANCEW* instance,void* context) noexcept {
    auto& enumeration=*static_cast<Enumeration*>(context);
    if(!instance||enumeration.entries.size()>=maximumDevices)return DIENUM_STOP;
    IDirectInputDevice8W* device=nullptr;
    try{
        // CreateDevice/GetCapabilities/GetProperty are read-only discovery.
        // No SetCooperativeLevel, Acquire, effect creation or property writes.
        if(FAILED(enumeration.input->CreateDevice(instance->guidInstance,&device,nullptr))){if(device)device->Release();return DIENUM_CONTINUE;}
        DIDEVCAPS capabilities{};capabilities.dwSize=sizeof(capabilities);
        const auto result=device->GetCapabilities(&capabilities);
        if(FAILED(result)||(capabilities.dwFlags&DIDC_FORCEFEEDBACK)==0){device->Release();return DIENUM_CONTINUE;}
        DeviceEntry entry{};entry.guid=instance->guidInstance;entry.description.size=sizeof(entry.description);
        formatGuid(instance->guidInstance,entry.description.id);
        nameUtf8(entry.description.name,sizeof(entry.description.name),instance->tszInstanceName[0]?instance->tszInstanceName:instance->tszProductName);
        auto ids=deviceProperty();
        if(SUCCEEDED(device->GetProperty(DIPROP_VIDPID,&ids.diph))){entry.description.vendorId=LOWORD(ids.dwData);entry.description.productId=HIWORD(ids.dwData);}
        device->Release();device=nullptr;
        enumeration.entries.push_back(entry);return DIENUM_CONTINUE;
    }catch(...){if(device)device->Release();enumeration.failed=true;return DIENUM_STOP;}
}
// Do not run DirectInput cleanup from DllMain or static destruction under the
// loader lock. The managed owner calls Shutdown; finite effects expire even
// when the process terminates without a final managed update.
void unexpectedFailure() noexcept {
    try{std::lock_guard lock(backend.mutex);backend.stopWithMessage("Unexpected wheel backend failure; feedback stopped.");backend.releaseInput();}catch(...){}
}
}

int IDAS3_WHEEL_CALL Idas3WheelRefreshDevices() noexcept {
    try{
        std::lock_guard lock(backend.mutex);
        if(!backend.initialize())return -1;
        Enumeration enumeration{backend.input};
        const auto result=backend.input->EnumDevices(DI8DEVCLASS_GAMECTRL,enumerateDevice,&enumeration,DIEDFL_ATTACHEDONLY|DIEDFL_FORCEFEEDBACK);
        if(FAILED(result)||enumeration.failed){backend.devices.clear();backend.message("Could not enumerate force-feedback devices. Refresh again after checking the driver.");return -1;}
        backend.devices=std::move(enumeration.entries);
        if(backend.devices.empty())backend.message("No DirectInput force-feedback wheel found. Ordinary gamepad vibration is not wheel force feedback.");
        else std::snprintf(backend.status,sizeof(backend.status),"Found %zu force-feedback device(s). Select the intended wheel explicitly.",backend.devices.size());
        return static_cast<int>(backend.devices.size());
    }catch(...){unexpectedFailure();return -1;}
}
int IDAS3_WHEEL_CALL Idas3WheelGetDevice(int index,Idas3WheelDevice* out) noexcept {
    try{
        std::lock_guard lock(backend.mutex);
        if(!out||out->size!=sizeof(Idas3WheelDevice)||index<0||static_cast<std::size_t>(index)>=backend.devices.size())return 0;
        *out=backend.devices[static_cast<std::size_t>(index)].description;return 1;
    }catch(...){unexpectedFailure();return 0;}
}
int IDAS3_WHEEL_CALL Idas3WheelSetForce(const char* guidUtf8,float force) noexcept {
    try{std::lock_guard lock(backend.mutex);return backend.setForce(guidUtf8,force)?1:0;}
    catch(...){unexpectedFailure();return 0;}
}
int IDAS3_WHEEL_CALL Idas3WheelSetCabinetState(const char* guidUtf8,
        const Idas3WheelCabinetState* state) noexcept {
    try{
        if(!state||state->size!=sizeof(Idas3WheelCabinetState)||state->version!=1u){
            std::lock_guard lock(backend.mutex);
            backend.stopWithMessage("Unsupported wheel cabinet state layout. Feedback stopped.");return 0;}
        std::lock_guard lock(backend.mutex);return backend.setCabinet(guidUtf8,*state)?1:0;
    }catch(...){unexpectedFailure();return 0;}
}
namespace {
// The cabinet's board and its command owner, one instance for the process.
// They are guarded by the same mutex as the device, so a frame update and a
// status read cannot interleave.
idas3ffb::Owner cabinetOwner;

Idas3WheelCabinetState cabinetFromBoard() noexcept {
    const auto board=idas3ffb::snapshot();
    Idas3WheelCabinetState out{};
    out.size=sizeof(out);out.version=1u;
    // An incoherent read means the writer was mid-update; the board is not torn
    // apart for the wheel, it is simply left alone this frame.
    if(!board.coherent){out.active=0u;return out;}
    out.power=board.power;out.torque=board.torque;out.spring=board.spring;
    out.damperStrength=board.damperStrength;out.damperParameter=board.damperParameter;
    out.rumbleIntensity=board.rumbleIntensity;out.rumbleFrequencyHz=board.rumbleFrequencyHz;
    out.active=board.active?1u:0u;
    return out;
}

}  // namespace

int IDAS3_WHEEL_CALL Idas3WheelUpdateCabinet(const char* guidUtf8,
        const Idas3WheelRaceState* state) noexcept {
    try{
        std::lock_guard lock(backend.mutex);
        if(!state||state->size!=sizeof(Idas3WheelRaceState)||state->version!=1u){
            backend.stopWithMessage("Unsupported wheel race state layout. Feedback stopped.");return 0;}
        idas3ffb::OwnerInput input{};
        input.driving=state->driving!=0u;
        input.carIndex=int(state->carIndex);
        input.speedKmh=state->speedKmh;input.steering=state->steering;
        input.headingError=state->headingError;input.wallLateral=state->wallLateral;
        input.impact=state->impact;input.wallContact=state->wallContact!=0u;
        input.strength=state->strength;input.invert=state->invert!=0u;
        cabinetOwner.update(input,state->deltaSeconds);
        const auto cabinet=cabinetFromBoard();
        return backend.setCabinet(guidUtf8,cabinet)?1:0;
    }catch(...){unexpectedFailure();return 0;}
}

int IDAS3_WHEEL_CALL Idas3WheelGetCabinetState(Idas3WheelCabinetState* out) noexcept {
    try{
        if(!out||out->size!=sizeof(Idas3WheelCabinetState)||out->version!=1u)return 0;
        std::lock_guard lock(backend.mutex);
        *out=cabinetFromBoard();
        return 1;
    }catch(...){unexpectedFailure();return 0;}
}

void IDAS3_WHEEL_CALL Idas3WheelStop() noexcept {
    try{std::lock_guard lock(backend.mutex);cabinetOwner.reset();idas3ffb::reset();}catch(...){}
    try{std::lock_guard lock(backend.mutex);backend.stopWithMessage("Wheel feedback stopped.");}
    catch(...){unexpectedFailure();}
}
void IDAS3_WHEEL_CALL Idas3WheelShutdown() noexcept {
    try{
        std::lock_guard lock(backend.mutex);backend.stopWithMessage("Wheel feedback shut down.");
        backend.releaseInput();
        backend.devices.clear();
    }catch(...){unexpectedFailure();}
}
int IDAS3_WHEEL_CALL Idas3WheelCopyStatus(char* dest,int capacity) noexcept {
    try{
        if(!dest||capacity<=0)return 0;
        std::lock_guard lock(backend.mutex);
        return static_cast<int>(copyUtf8(dest,static_cast<std::size_t>(capacity),backend.status));
    }catch(...){if(dest&&capacity>0)dest[0]=0;return 0;}
}
