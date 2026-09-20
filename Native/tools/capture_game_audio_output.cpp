// Process-isolated WASAPI loopback diagnostic. API sequence follows Microsoft's
// Windows-classic-samples/Samples/ApplicationLoopback example (MIT license).
// Records only the explicitly identified InitialDRemake.exe process tree.
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include <wrl/implements.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <sstream>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
void require(HRESULT hr,const char* text){if(FAILED(hr)){std::ostringstream out;out<<text<<" HRESULT0x"<<std::hex<<unsigned(hr);throw std::runtime_error(out.str());}}
class Activation final:public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,Microsoft::WRL::FtmBase,IActivateAudioInterfaceCompletionHandler>{
public:
    HANDLE done=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HRESULT result=E_PENDING;
    ComPtr<IAudioClient> client;
    ~Activation(){if(done)CloseHandle(done);}
    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation)override{
        ComPtr<IUnknown> unknown;HRESULT activated=E_PENDING;
        result=operation->GetActivateResult(&activated,&unknown);
        if(SUCCEEDED(result))result=activated;
        if(SUCCEEDED(result))result=unknown.As(&client);
        SetEvent(done);return S_OK;
    }
};
int wmain(int argc,wchar_t** argv)try{
    if(argc!=3)throw std::runtime_error("game-PID output-directory required");
    const auto pid=DWORD(std::stoul(argv[1]));
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)throw std::runtime_error("Target game process is not running");
    std::array<wchar_t,32768> exe{};DWORD length=DWORD(exe.size());const bool identified=QueryFullProcessImageNameW(process,0,exe.data(),&length)!=FALSE;CloseHandle(process);
    if(!identified||fs::path(exe.data()).filename()!=L"InitialDRemake.exe")throw std::runtime_error("Capture target must be InitialDRemake.exe");
    const fs::path output=argv[2];fs::create_directories(output);
    require(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM initialization");
    auto completion=Microsoft::WRL::Make<Activation>();
    AUDIOCLIENT_ACTIVATION_PARAMS parameters{};parameters.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    parameters.ProcessLoopbackParams.TargetProcessId=pid;parameters.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT argument{};argument.vt=VT_BLOB;argument.blob.cbSize=sizeof(parameters);argument.blob.pBlobData=reinterpret_cast<BYTE*>(&parameters);
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    require(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),&argument,completion.Get(),&operation),"Process loopback activation");
    if(WaitForSingleObject(completion->done,10000)!=WAIT_OBJECT_0)throw std::runtime_error("Loopback activation timed out");
    require(completion->result,"Loopback activation result");
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=176400;
    auto client=completion->client;
    require(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,0,0,&format,nullptr),"Loopback format");
    HANDLE ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ready)throw std::runtime_error("Capture event failed");
    require(client->SetEventHandle(ready),"Capture event binding");
    ComPtr<IAudioCaptureClient> capture;require(client->GetService(IID_PPV_ARGS(&capture)),"Capture service");
    std::vector<std::int16_t> pcm;pcm.reserve(44100*40*2);
    std::ofstream packets(output/"packets.csv");packets<<"wall_seconds,device_frame,frames,flags,peak\n";
    require(client->Start(),"Start loopback capture");
    std::cout<<"READY process-isolated game capture; waiting for menu sound, up to60seconds.\n"<<std::flush;
    const auto began=std::chrono::steady_clock::now();bool heard=false;double firstSound=0;std::uint64_t discontinuities=0,silentFrames=0;
    for(;;){
        const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
        if(elapsed>=60||(heard&&elapsed-firstSound>=20))break;
        WaitForSingleObject(ready,100);
        UINT32 pending=0;require(capture->GetNextPacketSize(&pending),"Packet size");
        while(pending){
            BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;UINT64 device=0,qpc=0;
            require(capture->GetBuffer(&data,&frames,&flags,&device,&qpc),"Capture packet");
            const bool silent=(flags&AUDCLNT_BUFFERFLAGS_SILENT)!=0;
            int peak=0;if(!silent)for(unsigned i=0;i<frames*2;++i)peak=std::max(peak,std::abs(int(reinterpret_cast<const std::int16_t*>(data)[i])));
            if(!heard&&peak>8){heard=true;firstSound=elapsed;std::cout<<"Detected game audio; recording20seconds.\n"<<std::flush;}
            if(heard){
                const auto at=pcm.size();pcm.resize(at+frames*2);
                if(!silent)std::copy_n(reinterpret_cast<const std::int16_t*>(data),frames*2,pcm.begin()+at);
                if(silent)silentFrames+=frames;
                discontinuities+=(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
                packets<<elapsed<<','<<device<<','<<frames<<','<<flags<<','<<peak<<'\n';
            }
            require(capture->ReleaseBuffer(frames),"Release packet");require(capture->GetNextPacketSize(&pending),"Next packet");
        }
    }
    require(client->Stop(),"Stop capture");CloseHandle(ready);
    if(!heard)throw std::runtime_error("No audible game output observed; the game may be unfocused or muted");
    std::ofstream wave(output/"game-output.wav",std::ios::binary);
    auto u32=[&](std::uint32_t value){wave.write(reinterpret_cast<const char*>(&value),4);};
    auto u16=[&](std::uint16_t value){wave.write(reinterpret_cast<const char*>(&value),2);};
    wave.write("RIFF",4);u32(36+std::uint32_t(pcm.size()*2));wave.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(44100);u32(176400);u16(4);u16(16);wave.write("data",4);u32(std::uint32_t(pcm.size()*2));wave.write(reinterpret_cast<const char*>(pcm.data()),pcm.size()*2);
    double energy=0;for(auto sample:pcm)energy+=double(sample)*sample;
    std::ofstream report(output/"capture.txt");report<<"Process="<<pid<<"\nFrames="<<pcm.size()/2<<"\nSilentFlagFrames="<<silentFrames<<"\nDiscontinuityPackets="<<discontinuities<<"\nRMS="<<std::sqrt(energy/pcm.size())<<'\n';
    std::cout<<"Captured "<<pcm.size()/2<<" stereo frames; silenceflags="<<silentFrames<<" discontinuities="<<discontinuities<<"\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
