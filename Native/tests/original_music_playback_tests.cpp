#include "original_music_playback.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace idas3;
namespace {
using Op=original::OriginalSelectionMusicOperation;
std::uint64_t checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void start(OriginalMusicPlayback& p,unsigned cue){p.apply({Op::Start,cue,cue?0xa8u:0x1a8u});p.apply({Op::Control,cue,0x4a0,cue?103u:109u});}
bool zero(const OriginalIcsMixFrame& f){return std::all_of(f.dry.begin(),f.dry.end(),[](auto n){return n==0;})&&std::all_of(f.effects.begin(),f.effects.end(),[](auto n){return n==0;});}
bool equal(const OriginalIcsMixFrame&a,const OriginalIcsMixFrame&b){return a.dry==b.dry&&a.effects==b.effects;}
struct Metrics {
    std::uint64_t frames=0,nonzero=0,stereoDifferent=0,desktopClips=0,hash=1469598103934665603ull;
    std::int64_t peak=0;long double square=0;
    void add(const OriginalIcsMixFrame& f){++frames;nonzero+=f.dry[0]!=0||f.dry[1]!=0;stereoDifferent+=f.dry[0]!=f.dry[1];for(auto v:f.dry){peak=std::max(peak,std::abs(std::int64_t(v)));square+=static_cast<long double>(v)*v;desktopClips+=std::abs(double(v)*.38)>32767.;hash^=std::uint32_t(v);hash*=1099511628211ull;}}
    double rms()const{return std::sqrt(double(square/(2*frames)));}
};
std::vector<std::uint8_t> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing score fixture");return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
unsigned u32(const std::vector<std::uint8_t>&b,unsigned p){return unsigned(b.at(p))|(unsigned(b.at(p+1))<<8)|(unsigned(b.at(p+2))<<16)|(unsigned(b.at(p+3))<<24);}
void put(std::vector<std::uint8_t>& b,unsigned p,unsigned value){for(unsigned i=0;i<4;++i)b.at(p+i)=std::uint8_t(value>>(i*8));}
using Record=std::vector<std::uint8_t>;
Record sourceNote(const std::filesystem::path& root,bool choke){
    const auto score=read(root/"data/original_audio/selection/type.idms");
    for(unsigned at=32;at<score.size();at+=112)if(u32(score,at)==1&&
        (choke?((u32(score,at+28)>>8)&0x83)==0x81:!(u32(score,at+28)&8)))
        return {score.begin()+at,score.begin()+at+112};
    throw std::runtime_error("Required original voice fixture missing");
}
void writeFixture(const std::filesystem::path& root,const std::filesystem::path& dest,std::vector<Record> events){
    const auto source=root/"data/original_audio/selection",out=dest/"data/original_audio/selection";std::filesystem::create_directories(out);
    for(const auto file:{"TYPE.dtpk","SELECT.dtpk","RESULT.dtpk","builtin_samples.bin","select.idms","result.idms"})std::filesystem::copy_file(source/file,out/file,std::filesystem::copy_options::overwrite_existing);
    Record anchor(112);put(anchor,4,10000);events.push_back(anchor);
    auto score=read(source/"type.idms");Record b(score.begin(),score.begin()+32);put(b,20,10000);put(b,24,10001);put(b,28,unsigned(events.size()));
    for(const auto& event:events)b.insert(b.end(),event.begin(),event.end());
    std::ofstream f(out/"type.idms",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(f),"Unable to write isolated controller score");
}
void controllerFixture(const std::filesystem::path& root,const std::filesystem::path& dest,unsigned voices,bool pitch,bool release){
    // Keep an original sample and context; a legal fixed envelope holds these
    // isolated voices while the source traversal's 39/40 allocation gate runs.
    auto note=sourceNote(root,false);put(note,4,0);put(note,16,40);put(note,28,0x80);
    put(note,48,31);put(note,52,0);put(note,56,0);put(note,60,0);
    std::vector<Record> records;
    for(unsigned i=0;i<voices;++i){auto n=note;put(n,16,40+i);records.push_back(n);}
    if(release){auto off=note;put(off,0,2);put(off,4,1);records.push_back(off);}
    auto change=note;put(change,0,3);put(change,4,2);put(change,8,pitch?0xe0000000:0xb0000000);put(change,28,pitch?1:4);put(change,pitch?56:68,pitch?0x7800:0xf1f);records.push_back(change);
    writeFixture(root,dest,std::move(records));
}
// Keep one actual source NoteOn and manufacture only the test's note-off time.
// Two scratch scores differ solely in the original percussion flag, proving
// that integration does not release percussion as ordinary sustained notes.
void fixture(const std::filesystem::path& root,const std::filesystem::path& dest,bool percussion){
    const auto source=root/"data/original_audio/selection",out=dest/"data/original_audio/selection";std::filesystem::create_directories(out);
    for(const auto file:{"TYPE.dtpk","SELECT.dtpk","RESULT.dtpk","builtin_samples.bin","select.idms","result.idms"})std::filesystem::copy_file(source/file,out/file,std::filesystem::copy_options::overwrite_existing);
    auto score=read(source/"type.idms");unsigned chosen=0;
    for(unsigned at=32;at<score.size();at+=112)if(u32(score,at)==1&&(u32(score,at+28)&8)){chosen=at;break;}
    check(chosen!=0,"Final original percussion flags are absent from score");
    std::vector<std::uint8_t> b(score.begin(),score.begin()+32);b.resize(32+112*3);put(b,20,10000);put(b,24,10001);put(b,28,3);
    std::copy_n(score.begin()+chosen,112,b.begin()+32);put(b,36,0);if(!percussion)put(b,60,u32(b,60)&~8u);
    put(b,144,2);put(b,148,1);put(b,156,u32(b,44));put(b,160,u32(b,48));
    put(b,256,0);put(b,260,10000); // inert loop anchor outside test duration
    std::ofstream f(out/"type.idms",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(f),"Unable to write isolated test score");
}
}
int main(int argc,char** argv){try{
    if(argc<3)throw std::runtime_error("Expected project root and scratch directory");const auto root=std::filesystem::path(argv[1]),scratch=std::filesystem::path(argv[2]);
    const auto began=std::chrono::steady_clock::now();
    for(unsigned cue=0;cue<2;++cue){
        OriginalMusicSequence sequence;sequence.load(root,cue);unsigned percussion=0,pitch=0,lfo=0,pan=0,filter=0;std::uint64_t expectedNotes=0,expectedCommands=0;
        for(const auto&e:sequence.events()){
            if(e.kind==OriginalMusicSequenceEventKind::NoteOn){check(e.voiceFlags0!=255&&e.voiceFlags1!=255,"Stale score flags interpreted as percussion");percussion+=(e.voiceFlags0&8)!=0;}
            if(e.kind==OriginalMusicSequenceEventKind::Configure){pitch+=(e.parameterMask&OriginalMusicPitch)!=0;lfo+=(e.parameterMask&OriginalMusicLfo)!=0;pan+=(e.parameterMask&OriginalMusicPan)!=0;filter+=(e.parameterMask&248)!=0;}
        }
        check(percussion>0&&pitch>0&&lfo>0&&(!cue||pan>0),"Source score omitted percussion/controller metadata");
        const std::uint64_t length=std::uint64_t(sequence.loopEndTick()+sequence.loopEndTick()-sequence.loopStartTick())*44+1;
        OriginalMusicPlayback playback(root);start(playback,cue);sequence.start();Metrics metrics;unsigned minBoundaryVoices=64;
        for(std::uint64_t frame=0;frame<length;++frame){
            sequence.advanceSample([&](const auto&e){expectedNotes+=e.kind==OriginalMusicSequenceEventKind::NoteOn;expectedCommands+=e.kind==OriginalMusicSequenceEventKind::Configure;});
            metrics.add(playback.renderFrame());
            if(frame%44100==0)check(playback.playing()&&playback.activeVoices()<=64,"Song stopped or exhausted its physical voice pool");
            if(frame==std::uint64_t(sequence.loopEndTick())*44||frame==length-1)minBoundaryVoices=std::min(minBoundaryVoices,playback.activeVoices());
        }
        const auto stats=playback.statistics();check(stats.songsStarted==1&&stats.notesStarted==expectedNotes,"Full-loop event count or song restart differs");check(stats.sourceLevel==(cue?103:109),"Wrong original source level");check(stats.parameterChanges>0&&expectedCommands>0,"Source controllers failed to reach playing voices");
        check(stats.percussionChokes>0&&stats.legatoRetunes>0,"Source percussion choke or legato retune was never serviced");
        check(metrics.nonzero>length/2&&metrics.stereoDifferent>length/4&&metrics.rms()>20,"Original music is silent or lacks stereo content");check(metrics.desktopClips==0,"Original music clips at desktop headroom0.38");check(minBoundaryVoices>0,"Music disappears at source loop boundary");
        std::cout<<(cue?"SELECT":"TYPE")<<" full intro +2 score cycles: frames="<<length<<" seconds="<<double(length)/44100<<" notes="<<stats.notesStarted<<" noteoffs="<<stats.noteOffs<<" controllers="<<stats.parameterChanges<<" chokes="<<stats.percussionChokes<<" retunes="<<stats.legatoRetunes<<" peakvoices="<<stats.peakVoices<<" peak="<<metrics.peak<<" rms="<<metrics.rms()<<" desktopPeak="<<metrics.peak*.38<<" clips="<<metrics.desktopClips<<" hash="<<metrics.hash<<" sourcePatches(pitch,lfo,pan,filter)="<<pitch<<","<<lfo<<","<<pan<<","<<filter<<"\n";
        playback.apply({Op::Control,cue,0x1200a0});check(!playback.playing()&&!playback.activeVoices(),"Immediate stop left music/release tails");for(unsigned i=0;i<100;++i)check(zero(playback.renderFrame()),"Immediate stop leaked samples");
    }
    {
        OriginalMusicPlayback a(root),b(root);start(a,0);start(b,0);Metrics initial;
        for(unsigned i=0;i<44100*2;++i){if(i%735==0)a.apply({Op::Control,0,0x4a0,109});auto av=a.renderFrame(),bv=b.renderFrame();check(equal(av,bv),"Repeated source level write restarted/changed music");initial.add(av);}
        check(a.statistics().songsStarted==1&&a.samplePosition()==b.samplePosition(),"Same-level writes reset song clock");
        for(unsigned cycle=0;cycle<3;++cycle){a.apply({Op::Control,0,0x1200a0});a.apply({Op::Unload,0});start(a,0);Metrics restarted;for(unsigned i=0;i<44100*2;++i)restarted.add(a.renderFrame());check(restarted.hash==initial.hash,"Returning/restarting changed original music output");}
        a.reset();check(a.statistics().samples==0&&!a.playing()&&!a.activeVoices(),"Playback reset retains state");
    }
    for(unsigned cue=0;cue<2;++cue){
        OriginalMusicPlayback p(root);start(p,cue);p.apply({Op::Control,cue,0xaa0,8});
        // Start occurs at sample0; first TimerB service is sample44. Source
        // AA0(8) polls integer level0 and force-stops at its985th timer tick.
        for(unsigned i=0;i<44*985;++i)p.renderFrame();check(p.playing(),"Source fade stopped before985 TimerB ticks");
        check(zero(p.renderFrame())&&!p.playing()&&!p.activeVoices(),"Source fade did not force-stop exactly on tick985");check(p.statistics().fades==1&&p.statistics().stops==1&&p.statistics().fadeLevel==0,"Fade control/state incorrect");
        for(unsigned i=0;i<1000;++i)check(zero(p.renderFrame()),"Fade completion leaked release tails");
    }
    {
        fixture(root,scratch/"percussion",true);fixture(root,scratch/"sustained",false);OriginalMusicPlayback percussion(scratch/"percussion"),sustained(scratch/"sustained");start(percussion,0);start(sustained,0);unsigned differences=0;
        for(unsigned i=0;i<44100;++i){auto a=percussion.renderFrame(),b=sustained.renderFrame();differences+=!equal(a,b);}
        check(percussion.statistics().notesStarted==1&&percussion.statistics().noteOffs==0,"Original percussion must ignore ordinary note-off");check(sustained.statistics().noteOffs==1&&differences>0,"Ordinary note-off test never altered a sustained voice");
        percussion.stop();check(!percussion.activeVoices()&&zero(percussion.renderFrame()),"Forced stop failed to kill percussion");
    }
    for(unsigned voices:{1u,39u,40u})for(bool pitch:{false,true})for(bool released:{false,true}){
        const auto dest=scratch/("controller_"+std::to_string(voices)+"_"+std::to_string(pitch)+"_"+std::to_string(released));controllerFixture(root,dest,voices,pitch,released);
        OriginalMusicPlayback p(dest);start(p,0);for(unsigned i=0;i<89;++i)p.renderFrame();
        check(p.activeVoices()==voices,"Traversal fixture lost its controlled voice count");
        check(p.statistics().parameterChanges==unsigned(!released||pitch||voices>=40),"Source held/released controller traversal differs at 40-voice gate");
    }
    for(bool sameChannel:{false,true}){
        auto first=sourceNote(root,true),second=first;put(first,4,0);put(second,4,1);if(!sameChannel)put(second,12,(u32(second,12)+1)%16);
        const auto dest=scratch/(sameChannel?"choke_same":"choke_other");writeFixture(root,dest,{first,second});OriginalMusicPlayback p(dest);start(p,0);for(unsigned i=0;i<45;++i)p.renderFrame();
        check(p.statistics().percussionChokes==unsigned(sameChannel),"Source percussion choke crossed a logical channel");
    }
    for(bool released:{false,true}){
        auto first=sourceNote(root,false);put(first,4,0);put(first,16,40);put(first,24,1);put(first,28,0x80);
        put(first,48,31);put(first,52,0);put(first,56,0);put(first,60,0);
        std::vector<Record> records{first};auto off=first;put(off,0,2);put(off,4,1);if(released)records.push_back(off);
        auto retune=first;put(retune,0,4);put(retune,4,2);put(retune,16,41);put(retune,28,257);put(retune,56,0x7800);retune[104]/=2;records.push_back(retune);
        put(off,4,3);records.push_back(off);put(off,4,4);put(off,16,41);records.push_back(off);
        const auto dest=scratch/(released?"retune_tail":"retune_held");writeFixture(root,dest,std::move(records));
        OriginalMusicPlayback playback(dest);start(playback,0);OriginalMusicSequence fixtureSequence;fixtureSequence.load(dest,0);
        const auto& initial=fixtureSequence.events().front();const auto& change=*std::find_if(fixtureSequence.events().begin(),fixtureSequence.events().end(),[](const auto&e){return e.kind==OriginalMusicSequenceEventKind::Retune;});
        const auto parameters=[&](const auto& event){auto p=event.parameters;OriginalMusicVolumeContext v{event.velocityTableValue,event.layerGain8,event.channelVolume0A,event.channelGain10,event.master05,event.bankFade06,event.channelFlags0};v.channelGain10=originalMusicChannelGain(109,64,127,64,v.channelFlags0);v.bankFade06=127;p.totalLevel=originalMusicTotalLevel(v);return p;};
        const auto bank=loadOriginalMusicBank(root/"data/original_audio/selection/TYPE.dtpk");const auto& sample=bank.builtinSamples.front();
        OriginalMusicVoice expected;auto p=parameters(initial);expected.start({sample.pcm,sample.loopStart,sample.loopEnd,sample.looping},p);
        for(unsigned frame=0;frame<220;++frame){
            if(frame==44&&released)expected.release();
            if(frame==88){p.pitch=change.parameters.pitch;p.totalLevel=parameters(change).totalLevel;expected.configure(p);}
            if(frame==176&&!released)expected.release();
            check(equal(playback.renderFrame(),expected.renderFrame()),"Source legato retune restarted sample/envelope or changed unrelated parameters");
        }
        check(playback.statistics().notesStarted==1&&playback.statistics().legatoRetunes==1&&playback.statistics().peakVoices==1,"Source legato allocated another voice");
        check(playback.statistics().noteOffs==1,"Retune destination key or release state was not preserved");
    }
    {
        auto retune=sourceNote(root,false);put(retune,0,4);put(retune,4,0);put(retune,28,257);
        const auto dest=scratch/"retune_without_target";writeFixture(root,dest,{retune});OriginalMusicPlayback p(dest);start(p,0);
        for(unsigned frame=0;frame<100;++frame)check(zero(p.renderFrame()),"Source targetless mono retune invented a fallback note");
        check(!p.activeVoices()&&p.statistics().notesStarted==0&&p.statistics().legatoRetunes==0,"Targetless retune changed allocation state");
    }
    {
        const auto bank=loadOriginalMusicBank(root/"data/original_audio/selection/TYPE.dtpk");const auto& pcm=bank.builtinSamples.front().pcm;
        for(bool oneShot:{false,true}){
            OriginalMusicVoiceParameters p;p.envelope1=31;p.envelope2=10;
            OriginalMusicVoicePool driver,hardware;OriginalMusicVoice reference;const OriginalMusicPcm sample{pcm,64,100,true};
            driver.start(1,sample,p,true,oneShot?0x88:0x80);hardware.start(1,sample,p);reference.start(sample,p);bool keyedOff=false,retired=false,saw512=false;
            for(unsigned frame=0;frame<500000&&!retired;++frame){
                bool firstKeyoff=false;
                if(!keyedOff&&reference.position()!=0&&reference.position()>=64){
                    reference.release();hardware.keyOffHardware(1);if(!oneShot)driver.release(1);keyedOff=true;firstKeyoff=true;
                }
                const unsigned attenuation=reference.envelope()>>16;saw512|=keyedOff&&attenuation==512;
                const bool shouldRetire=keyedOff&&!firstKeyoff&&reference.phase()!=0&&attenuation>512;
                driver.pollDriver();check(driver.active(1)==!shouldRetire,"Source driver strict attenuation retirement differs from hardware monitor");
                if(shouldRetire){check(hardware.active(1),"Pure hardware voice accidentally used the source retirement threshold");retired=true;break;}
                check(driver.releasing(1)==(keyedOff&&!oneShot),"Hardware-only percussion cutoff entered the source release list");
                const auto actual=driver.renderFrame(),expected=reference.renderFrame();hardware.renderFrame();check(equal(actual,expected),"Source driver poll changed voice rendering before retirement");
            }
            check(retired&&saw512,"Driver retirement fixture did not cross the strict 512 boundary");
        }
    }
    const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();std::cout<<"Original music playback passed:"<<checks<<" checks; elapsed="<<elapsed<<"s; no audio device or WAV\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
