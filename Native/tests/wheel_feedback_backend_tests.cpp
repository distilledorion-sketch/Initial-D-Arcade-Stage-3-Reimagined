// CPU-only fake DirectInput interfaces. This executable never discovers,
// acquires, or sends a force to physical hardware. Tests exercise a private
// Backend instance with fake devices and a fake foreground-window provider.
#include "../src/wheel_feedback_backend.cpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks=0;
bool testForeground=true;
HWND testWindow() noexcept{return testForeground?reinterpret_cast<HWND>(std::uintptr_t(1)):nullptr;}
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
#define UNUSED_METHOD(name,signature) HRESULT STDMETHODCALLTYPE name signature override{return E_NOTIMPL;}
#define UNKNOWN_METHODS \
    UNUSED_METHOD(QueryInterface,(REFIID,void**)) \
    ULONG STDMETHODCALLTYPE AddRef() override{return 2;}

struct FakeEffect final : IDirectInputEffect {
    int releases=0,stops=0,unloads=0,updates=0;int* failure=nullptr;
    DWORD duration=0,flags=0;LONG magnitude=0;bool loseFocus=false;
    UNKNOWN_METHODS
    ULONG STDMETHODCALLTYPE Release() override{++releases;return 0;}
    UNUSED_METHOD(Initialize,(HINSTANCE,DWORD,REFGUID))
    UNUSED_METHOD(GetEffectGuid,(LPGUID))
    UNUSED_METHOD(GetParameters,(LPDIEFFECT,DWORD))
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT parameters,DWORD updateFlags) override{
        ++updates;duration=parameters->dwDuration;flags=updateFlags;
        magnitude=static_cast<DICONSTANTFORCE*>(parameters->lpvTypeSpecificParams)->lMagnitude;
        if(loseFocus)testForeground=false;
        return *failure==11?DIERR_INPUTLOST:DI_OK;
    }
    UNUSED_METHOD(Start,(DWORD,DWORD))
    HRESULT STDMETHODCALLTYPE Stop() override{++stops;return DI_OK;}
    UNUSED_METHOD(GetEffectStatus,(LPDWORD))
    UNUSED_METHOD(Download,())
    HRESULT STDMETHODCALLTYPE Unload() override{++unloads;return DI_OK;}
    UNUSED_METHOD(Escape,(LPDIEFFESCAPE))
};
struct FakeDevice final : IDirectInputDevice8W {
    int failure=-1,releases=0,acquires=0,unacquires=0,effects=0,propertyWrites=0;
    bool acquired=false,propertyWhileAcquired=false,restoreFails=false;
    DWORD autocenter=DIPROPAUTOCENTER_ON,cooperative=0,effectAxis=0,effectDuration=0;
    FakeEffect effect;
    FakeDevice(){effect.failure=&failure;}
    UNKNOWN_METHODS
    ULONG STDMETHODCALLTYPE Release() override{++releases;return 0;}
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS caps) override{
        caps->dwFlags=failure==13?0:DIDC_FORCEFEEDBACK;return failure==1?E_FAIL:DI_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKW callback,void* context,DWORD) override{
        if(failure==3)return E_FAIL;
        DIDEVICEOBJECTINSTANCEW axis{};axis.dwSize=sizeof(axis);axis.dwFlags=DIDOI_FFACTUATOR;
        axis.dwType=DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(3);axis.guidType=GUID_Slider;
        callback(&axis,context); // Must be ignored, despite the actuator flag.
        if(failure!=12){axis.guidType=GUID_XAxis;callback(&axis,context);}
        return DI_OK;
    }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID property,LPDIPROPHEADER header) override{
        if(&property!=&DIPROP_AUTOCENTER)return DIERR_UNSUPPORTED;
        static_cast<DIPROPDWORD*>(static_cast<void*>(header))->dwData=autocenter;
        return failure==6?DIERR_UNSUPPORTED:DI_OK;
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID property,LPCDIPROPHEADER header) override{
        if(&property!=&DIPROP_AUTOCENTER)return DIERR_UNSUPPORTED;
        ++propertyWrites;propertyWhileAcquired|=acquired;
        const auto value=static_cast<const DIPROPDWORD*>(static_cast<const void*>(header))->dwData;
        if(value==DIPROPAUTOCENTER_ON&&restoreFails)return DIERR_UNPLUGGED;
        autocenter=value; // Model a driver that partially changes before failing.
        return failure==7&&value==DIPROPAUTOCENTER_OFF?E_FAIL:DI_OK;
    }
    HRESULT STDMETHODCALLTYPE Acquire() override{++acquires;if(failure==8)return DIERR_OTHERAPPHASPRIO;acquired=true;return DI_OK;}
    HRESULT STDMETHODCALLTYPE Unacquire() override{++unacquires;acquired=false;return DI_OK;}
    UNUSED_METHOD(GetDeviceState,(DWORD,void*))
    UNUSED_METHOD(GetDeviceData,(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD))
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override{return failure==4?E_FAIL:DI_OK;}
    UNUSED_METHOD(SetEventNotification,(HANDLE))
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD flags) override{cooperative=flags;return failure==5?E_FAIL:DI_OK;}
    UNUSED_METHOD(GetObjectInfo,(LPDIDEVICEOBJECTINSTANCEW,DWORD,DWORD))
    UNUSED_METHOD(GetDeviceInfo,(LPDIDEVICEINSTANCEW))
    UNUSED_METHOD(RunControlPanel,(HWND,DWORD))
    UNUSED_METHOD(Initialize,(HINSTANCE,DWORD,REFGUID))
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID,LPCDIEFFECT parameters,LPDIRECTINPUTEFFECT* result,LPUNKNOWN) override{
        ++effects;effectDuration=parameters->dwDuration;effectAxis=parameters->rgdwAxes[0];*result=&effect;
        return failure==9?E_FAIL:DI_OK;
    }
    UNUSED_METHOD(EnumEffects,(LPDIENUMEFFECTSCALLBACKW,void*,DWORD))
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOW info,REFGUID) override{
        info->dwEffType=failure==14?DIEFT_PERIODIC:DIEFT_CONSTANTFORCE;return failure==2?DIERR_UNSUPPORTED:DI_OK;
    }
    UNUSED_METHOD(GetForceFeedbackState,(LPDWORD))
    UNUSED_METHOD(SendForceFeedbackCommand,(DWORD))
    UNUSED_METHOD(EnumCreatedEffectObjects,(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,void*,DWORD))
    UNUSED_METHOD(Escape,(LPDIEFFESCAPE))
    HRESULT STDMETHODCALLTYPE Poll() override{return failure==10?DIERR_INPUTLOST:DI_OK;}
    UNUSED_METHOD(SendDeviceData,(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD))
    UNUSED_METHOD(EnumEffectsInFile,(LPCWSTR,LPDIENUMEFFECTSINFILECALLBACK,void*,DWORD))
    UNUSED_METHOD(WriteEffectToFile,(LPCWSTR,DWORD,LPDIFILEEFFECT,DWORD))
    UNUSED_METHOD(BuildActionMap,(LPDIACTIONFORMATW,LPCWSTR,DWORD))
    UNUSED_METHOD(SetActionMap,(LPDIACTIONFORMATW,LPCWSTR,DWORD))
    UNUSED_METHOD(GetImageInfo,(LPDIDEVICEIMAGEINFOHEADERW))
};
struct FakeInput final : IDirectInput8W {
    FakeDevice wheel;int creates=0,releases=0;GUID requested{};
    UNKNOWN_METHODS
    ULONG STDMETHODCALLTYPE Release() override{++releases;return 0;}
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid,LPDIRECTINPUTDEVICE8W* result,LPUNKNOWN) override{
        ++creates;requested=guid;*result=&wheel;return wheel.failure==0?DIERR_DEVICENOTREG:DI_OK;
    }
    UNUSED_METHOD(EnumDevices,(DWORD,LPDIENUMDEVICESCALLBACKW,void*,DWORD))
    UNUSED_METHOD(GetDeviceStatus,(REFGUID))
    UNUSED_METHOD(RunControlPanel,(HWND,DWORD))
    UNUSED_METHOD(Initialize,(HINSTANCE,DWORD))
    UNUSED_METHOD(FindDevice,(REFGUID,LPCWSTR,LPGUID))
    UNUSED_METHOD(EnumDevicesBySemantics,(LPCWSTR,LPDIACTIONFORMATW,LPDIENUMDEVICESBYSEMANTICSCBW,void*,DWORD))
    UNUSED_METHOD(ConfigureDevices,(LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSW,DWORD,void*))
};
#undef UNUSED_METHOD
#undef UNKNOWN_METHODS
constexpr char firstGuid[]="{01234567-89ab-cdef-0123-456789abcdef}";
constexpr char secondGuid[]="{11234567-89ab-cdef-0123-456789abcdef}";
struct Fixture {
    FakeInput input;Backend subject;
    Fixture(){testForeground=true;subject.input=&input;subject.foregroundWindow=testWindow;}
    ~Fixture(){subject.releaseDevice();subject.releaseInput();}
};
void happyPath(){
    Fixture f;auto& wheel=f.input.wheel;
    require(f.subject.setForce(firstGuid,0)&&f.input.creates==0,"Initial zero acquired a device");
    require(f.subject.setForce(firstGuid,.25f),"Fake wheel initialization failed");
    require(wheel.cooperative==(DISCL_EXCLUSIVE|DISCL_FOREGROUND),"Cooperative level is not exclusive foreground");
    require(wheel.autocenter==DIPROPAUTOCENTER_OFF&&!wheel.propertyWhileAcquired,"Autocenter not disabled while unacquired");
    require(wheel.effectAxis==(DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(3)),"Selected force actuator object ID was replaced by guessed axis offset");
    require(wheel.effectDuration==100000&&wheel.effect.duration==100000,"Effect did not use a finite100ms duration");
    require(wheel.effect.magnitude==2500&&(wheel.effect.flags&DIEP_START)!=0,"Force magnitude/start mismatch");
    require(f.subject.setForce(firstGuid,-.4f)&&wheel.effect.magnitude==-4000,"Negative force was lost");
    require(f.input.creates==1&&wheel.effects==1&&wheel.acquires==1,"Per-frame force recreated the device/effect");
    require(f.subject.setForce(firstGuid,0)&&f.subject.device!=nullptr&&wheel.effect.stops>0,"Zero force did not stop and retain the cache");
    testForeground=false;
    require(!f.subject.setForce(firstGuid,.2f)&&f.subject.device==nullptr&&f.subject.effect==nullptr,"Lost foreground retained output ownership");
    require(wheel.autocenter==DIPROPAUTOCENTER_ON&&!wheel.acquired&&wheel.releases==1&&wheel.effect.releases==1,"Focus loss did not release/restore wheel");
    require(!wheel.propertyWhileAcquired,"Autocenter was restored before unacquiring");
}
void rejectedInputs(){
    for(float invalid:{-1.001f,1.001f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        Fixture f;require(f.subject.setForce(firstGuid,.2f),"Invalid-force fixture initialization failed");
        require(!f.subject.setForce(firstGuid,invalid)&&!f.subject.device&&f.input.wheel.autocenter==1,"Invalid force retained device or changed it to a guessed value");
    }
    Fixture f;require(f.subject.setForce(firstGuid,.2f),"Invalid-GUID fixture initialization failed");
    require(!f.subject.setForce("first",.2f)&&!f.subject.device,"Invalid GUID fell back to a physical device");
    require(f.input.creates==1,"Invalid GUID opened a replacement wheel");
}
void failures(){
    for(int stage=0;stage<=14;++stage){
        Fixture f;f.input.wheel.failure=stage;
        require(!f.subject.setForce(firstGuid,.25f),"Injected driver/capability failure was accepted");
        require(!f.subject.device&&!f.subject.effect&&!f.input.wheel.acquired,"Failure retained acquired resources");
        require(f.input.wheel.autocenter==1&&!f.input.wheel.propertyWhileAcquired,"Failure did not restore autocenter correctly");
        require(f.input.wheel.releases==1,"Partial initialization did not release device");
    }
    Fixture f;require(f.subject.setForce(firstGuid,.2f),"Restore-failure fixture initialization failed");
    f.input.wheel.restoreFails=true;f.subject.stopWithMessage("Stopped.");
    require(!f.subject.device&&!f.subject.effect&&std::strstr(f.subject.status,"restoration failed"),"Autocenter restore failure not reported/released");
}
void selectionAndFocus(){
    Fixture f;require(f.subject.setForce(firstGuid,.2f)&&f.subject.setForce(secondGuid,.3f),"Explicit wheel switch failed");
    GUID expected{};parseGuid(secondGuid,expected);
    require(IsEqualGUID(f.input.requested,expected)&&f.input.creates==2&&f.input.wheel.releases==1,"Wheel GUID switch used fallback or retained previous ownership");
    f.input.wheel.effect.loseFocus=true;
    require(!f.subject.setForce(secondGuid,.2f)&&!f.subject.device,"Focus lost during driver update did not stop output");
    Fixture stopped;testForeground=false;
    require(!stopped.subject.setForce(firstGuid,.5f)&&stopped.input.creates==0,"Background process opened a device");
}
}
int main()try{
    happyPath();rejectedInputs();failures();selectionAndFocus();
    std::cout<<"PASS "<<checks<<" fake-device force/lease/cache/GUID/focus/cleanup/failure checks. No physical device discovery, acquisition or motor calls.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
