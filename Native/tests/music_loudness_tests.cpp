#include "music_loudness.h"
#include "audio.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <numbers>
using namespace idas3;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::vector<float> tone(unsigned rate,unsigned channels,float level,double seconds=2){
    std::vector<float> out(std::size_t(rate*seconds)*channels);
    for(std::size_t frame=0;frame<out.size()/channels;++frame)
        for(unsigned ch=0;ch<channels;++ch)out[frame*channels+ch]=level*float(std::sin(frame*2*std::numbers::pi*997/rate));
    return out;
}
static void unitTests(){
    for(unsigned rate:{8000u,16000u,22050u,44100u,48000u,96000u})for(unsigned channels:{1u,2u}){
        auto loud=tone(rate,channels,.75f),quiet=tone(rate,channels,.025f);
        const auto a=music_loudness::normalize(loud,rate,channels),b=music_loudness::normalize(quiet,rate,channels);
        check(a.gain<1&&b.gain>1,"Loud tracks must attenuate and quiet tracks must boost");
        double difference=0;for(std::size_t i=0;i<loud.size();++i)difference=std::max(difference,double(std::abs(loud[i]-quiet[i])));
        check(difference<1e-6,"Identical songs mastered at different levels did not converge");
        check(std::abs(music_loudness::measure<float>(loud,rate,channels).lufs+16)<1e-4,"Target loudness mismatch");
        const auto again=music_loudness::normalize(loud,rate,channels);check(std::abs(again.gain-1)<1e-6,"Normalization compounds when repeated");
    }
    auto silence=std::vector<float>(96000,0);check(music_loudness::normalize(silence,48000,2).gain==1,"Silence boosted");
    auto noise=tone(48000,2,.00001f);check(music_loudness::normalize(noise,48000,2).gain==1,"Near-silent audio boosted");
    auto dynamic=tone(48000,2,.02f);dynamic[1234]=1;
    const auto limited=music_loudness::normalize(dynamic,48000,2);
    check(limited.gain<=music_loudness::peakCeiling&&dynamic[1234]<=music_loudness::peakCeiling+1e-7,"Transient peak clips");
    auto stereo=tone(48000,2,.5f);for(std::size_t i=1;i<stereo.size();i+=2)stereo[i]*=.25f;
    music_loudness::normalize(stereo,48000,2);
    for(std::size_t i=0;i<stereo.size();i+=2)check(std::abs(stereo[i]*.25f-stereo[i+1])<1e-7,"Stereo image changed");
    auto longFade=tone(48000,2,.25f);const auto active=music_loudness::measure<float>(longFade,48000,2);
    longFade.resize(longFade.size()*6);const auto gated=music_loudness::measure<float>(longFade,48000,2);
    check(std::abs(active.lufs-gated.lufs)<.5,"Trailing silence skews the loudness measurement");
    for(auto bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto invalid=tone(8000,1,.1f);invalid.back()=bad;bool threw=false;
        try{music_loudness::normalize(invalid,8000,1);}catch(const std::invalid_argument&){threw=true;}
        check(threw&&invalid[1]==tone(8000,1,.1f)[1],"Invalid PCM partially changed the buffer");
    }
    auto shortClip=tone(8000,1,.3f,.02);check(std::isfinite(music_loudness::normalize(shortClip,8000,1).gain),"Short clip failed");
}
static void mixerTest(const std::filesystem::path& root){
    auto clip=std::make_shared<OriginalAudioClip>();clip->sampleRate=44100;clip->channels=2;clip->looping=true;
    auto input=tone(44100,2,.04f);for(float sample:input)clip->samples.push_back(std::int16_t(sample*32767));
    const auto before=clip->samples;
    const float gain=float(music_loudness::measure<std::int16_t>(clip->samples,44100,2).gain);
    EngineAudio audio;audio.configure(root);audio.customRaceMusic=clip;audio.musicTrack=-2;
    audio.scene(false,false,false,false,true); // held before countdown
    for(int i=0;i<100;++i)check(audio.renderStereo(0,0,0,0,false)==std::array<short,2>{},"Held music is audible");
    audio.scene(false,false,false);
    std::vector<float> rendered;rendered.reserve(input.size());
    for(std::size_t i=0;i<1000;++i){const auto output=audio.renderStereo(0,0,0,0,false);
        const auto expected=short((clip->samples[i*2]/32768.f)*music_loudness::raceMixGain*gain*.60f*32767);
        check(std::abs(int(output[0])-expected)<=1&&output[0]==output[1],"Race mixer failed to apply measured gain");
    }
    audio.setOutputGains({.5f,.5f,1,1,1});
    const auto quiet=audio.renderStereo(0,0,0,0,false);
    const auto expected=short((clip->samples[2000]/32768.f)*music_loudness::raceMixGain*gain*.5f*.60f*.5f*32767);
    check(std::abs(int(quiet[0])-expected)<=1,"Music/master controls no longer scale normalized songs");
    audio.setOutputGains({2,2,2,2,2});
    const auto boosted=audio.renderStereo(0,0,0,0,false);
    const auto boostedExpected=short((clip->samples[2002]/32768.f)*music_loudness::raceMixGain*gain*2*.60f*2*32767);
    check(std::abs(int(boosted[0])-boostedExpected)<=1,"200% Music/Master boost was capped at 100%");
    check(clip->samples==before,"Normalization modified the custom music cache");
    audio.scene(true,false,false);audio.scene(false,false,false);audio.setOutputGains({});
    for(std::size_t i=0;i<clip->frames();++i){const auto output=audio.renderStereo(0,0,0,0,false);
        const auto value=short((clip->samples[i*2]/32768.f)*music_loudness::raceMixGain*gain*.60f*32767);
        check(std::abs(int(output[0])-value)<=1,"Replay/reload applied normalization twice");
        for(auto sample:output)rendered.push_back(sample/32768.f);
    }
    // Check the complete mixer output, not just the pre-mix normalized clip.
    const auto final=music_loudness::measure<float>(rendered,44100,2);
    check(final.lufs>-15&&final.lufs<-14.7,"Race mixer attenuates normalized songs below their intended output level");
}
int main(int argc,char** argv)try{
    if(argc==5&&std::string(argv[1])=="--capture-race"){
        auto clip=std::make_shared<OriginalAudioClip>();
        if(std::filesystem::path(argv[3]).extension()==".pcm"){
            const auto size=std::filesystem::file_size(argv[3]);check(size>0&&size%4==0&&size<=64*1024*1024,"Invalid 48 kHz stereo fixture");
            clip->channels=2;clip->sampleRate=48000;clip->samples.resize(size/2);
            std::ifstream input(argv[3],std::ios::binary);input.read(reinterpret_cast<char*>(clip->samples.data()),std::streamsize(size));check(bool(input),"Could not read fixture");
        }else *clip=loadOriginalSpsd(argv[3]);
        const auto measured=music_loudness::measure<std::int16_t>(clip->samples,clip->sampleRate,clip->channels);
        EngineAudio audio;audio.configure(argv[2]);audio.customRaceMusic=clip;audio.musicTrack=-2;audio.scene(false,false,false);
        std::ofstream file(argv[4],std::ios::binary);check(bool(file),"Could not create race output fixture");
        const auto frames=std::size_t(double(clip->frames())*44100/clip->sampleRate);
        for(std::size_t i=0;i<frames;++i){const auto pcm=audio.renderStereo(0,0,0,0,false);file.write(reinterpret_cast<const char*>(pcm.data()),sizeof(pcm));}
        check(bool(file),"Could not write race output fixture");
        std::cout<<"Captured actual race output: source LUFS="<<measured.lufs<<", normalization="<<measured.gain<<", output frames="<<frames<<'\n';return 0;
    }
    if(argc==4 && std::string(argv[1])=="--check-race-loop"){
        const auto index=std::stoi(argv[3]);
        const auto root=std::filesystem::path(argv[2]);
        const auto clip=loadOriginalSpsd(root/"data/original_audio/streams"/raceMusicCatalog.at(index).relativePath);
        check(clip.looping && clip.loopStart>0,"Choose a race song with an intro");
        EngineAudio audio;
        audio.configure(root);
        audio.musicTrack=index;
        audio.scene(false,false,false);
        const auto end=clip.loopEnd?clip.loopEnd:clip.frames();
        const auto count=std::uint64_t(end)*44100/clip.sampleRate;
        for(std::uint64_t i=0;i<count+2;++i)audio.renderStereo(0,0,0,0,false);
        const auto position=audio.raceTimingStatistics().streamFrame;
        const auto expected=clip.loopStart+double(count+2)*clip.sampleRate/44100-end;
        check(std::abs(position-expected)<0.1,"Race mixer returned to the wrong loop frame");
        std::cout<<"PASS "<<raceMusicCatalog[index].title<<" looped at "<<end
                 <<" to "<<clip.loopStart<<" (next playback frame "<<position<<")\n";
        return 0;
    }
    if(argc==3&&std::string(argv[1])=="--audit-originals"){
        const auto root=std::filesystem::path(argv[2])/"data/original_audio/streams";
        for(std::size_t i=0;i<raceMusicCatalog.size();++i){
            auto clip=loadOriginalSpsd(root/raceMusicCatalog[i].relativePath);
            const auto start=std::chrono::steady_clock::now();
            const auto m=music_loudness::measure<std::int16_t>(clip.samples,clip.sampleRate,clip.channels);
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            std::cout<<i<<','<<m.lufs<<','<<m.peak<<','<<m.gain<<','<<ms<<'\n';
        }return 0;
    }
    unitTests();if(argc!=2)throw std::invalid_argument("Native root required");mixerTest(argv[1]);
    std::cout<<"PASS music loudness: rates, mono/stereo, gates, silence, peaks, dynamics, repeated loads, race/countdown and output controls\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
