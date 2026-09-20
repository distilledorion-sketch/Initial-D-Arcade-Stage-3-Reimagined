#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "original_audio.h"
#include "original_menu_audio.h"
#include "original_engine_playback.h"
#include "original_sfx_sequence.h"
#include "original_dynamics.h"
#include "original_attract_audio.h"
#include "original_music_playback.h"
#include "original_audio_dsp_runtime.h"
#include "original_oneshot_playback.h"
#include "music_catalog.h"
// Only the PCM cursor is accessed privately. Bounded sample-window checks
// deliberately seek past leading silence; no game or audio device is started.
// Dependencies are included first so this cannot rewrite their private fields.
#define private public
#include "audio.h"
#undef private

using namespace idas3;
namespace {
std::uint64_t checks=0,pcmFrames=0,audibleFrames=0;
void check(bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);}
constexpr std::array<std::string_view,13> originalPaths{
    "01_gamble_rumble.bin","02_speedy_speed_boy.bin","03_remember_me.bin","04_save_me.bin",
    "05_over_the_rainbow.bin","06_stop_your_self_control.bin","07_crazy_for_love.bin",
    "08_express_love.bin","09_blackout.bin","10_fall_in_the_web.bin","11_pamela.bin",
    "12_fight_for_love_tonight.bin","13_dancin_in_my_dreams.bin"};
constexpr std::array<std::string_view,13> originalTitles{
    "Gamble Rumble","Speedy Speed Boy","Remember Me","Save Me","Over the Rainbow",
    "Stop Your Self Control","Crazy for Love","Express Love","Black Out",
    "Fall in the Web of Desire","Pamela","Fight for Love Tonight","Dancin' in My Dreams"};
constexpr std::array<std::string_view,17> additionalPaths{
    "stage1/EZ001.bin","stage1/MD001.bin","stage1/HR001.bin","stage1/VH002.bin","stage1/VH003.bin","stage1/EZ002.bin",
    "stage2/EZ001.bin","stage2/EZ002.bin","stage2/NM001.bin","stage2/NM002.bin","stage2/HD001.bin","stage2/HD002.bin",
    "stage2/DF001.bin","stage2/DF002.bin","stage2/VH001.bin","stage2/UH001.bin","stage2/UH002.bin"};
constexpr std::array<std::string_view,14> stage4Paths{
    "stage4/avex_02_letsgocomeon.adx","stage4/avex_03_gobeatcrazy.adx",
    "stage4/avex_04_speedcar.adx","stage4/avex_05_flytometothemoon.adx",
    "stage4/avex_06_revolution.adx","stage4/avex_07_wellseeheaven.adx",
    "stage4/avex_08_allaround.adx","stage4/avex_09_eldorado.adx",
    "stage4/avex_10_raisinhell.adx","stage4/avex_11_spacelove.adx",
    "stage4/avex_12_nocontrol.adx","stage4/avex_13_foreveryoung.adx",
    "stage4/avex_14_riderofthesky.adx","stage4/avex_15_thefiresonme.adx"};
constexpr std::array<std::string_view,14> stage5Paths{
    "stage5/avex_02_sunintherain.adx","stage5/avex_03_lookabomba.adx",
    "stage5/avex_04_sweetsixteengirl.adx","stage5/avex_05_loveisanameoflove.adx",
    "stage5/avex_06_adrenaline.adx","stage5/avex_07_blackufo.adx",
    "stage5/avex_08_discofire.adx","stage5/avex_09_midnightlove.adx",
    "stage5/avex_10_gasgasgas.adx","stage5/avex_11_chemicallove.adx",
    "stage5/avex_12_rockinhardcore.adx","stage5/avex_13_speedman.adx",
    "stage5/avex_14_fighting.adx","stage5/avex_15_rightnow.adx"};
constexpr std::array<std::string_view,14> stage6Paths{
    "stage6/avex_01_super_rider.wav","stage6/avex_02_rock_beaten_wild.wav",
    "stage6/avex_03_once_upon_a_time.wav","stage6/avex_04_set_me_free.wav",
    "stage6/avex_05_king_of_eurobeat.wav","stage6/avex_06_the_love_bite.wav",
    "stage6/avex_07_euro_night.wav","stage6/avex_08_queen_of_meam.wav",
    "stage6/avex_09_mad_desire.wav","stage6/avex_10_burn_into_the_beat.wav",
    "stage6/avex_11_forever_sad.wav","stage6/avex_12_hurricane_man.wav",
    "stage6/avex_13_dont_turn_it_off.wav","stage6/avex_14_you_are_my_wonder.wav"};
constexpr std::array<std::string_view,14> stage7Paths{
    "stage7/avex_01_disconnected.wav","stage7/avex_02_remember_me.wav",
    "stage7/avex_03_night_of_fire.wav","stage7/avex_04_i_need_a_revolution.wav",
    "stage7/avex_05_power_two.wav","stage7/avex_06_crazy_for_love.wav",
    "stage7/avex_07_burning_up_the_night(total_fire).wav","stage7/avex_08_freedom_ride.wav",
    "stage7/avex_09_ministry_of_power.wav","stage7/avex_10_speed_of_light.wav",
    "stage7/avex_11_the_top.wav","stage7/avex_12_up_and_dance_up_and_go.wav",
    "stage7/avex_13_pamela.wav","stage7/avex_14_limousine.wav"};
constexpr std::array<std::string_view,16> stage8Paths{
    "stage8/avex_01_breakin_out.wav","stage8/avex_02_notings_gonna_stop_us_tonight.wav",
    "stage8/avex_03_come_on_baby.wav","stage8/avex_04_sunlight.wav",
    "stage8/avex_05_prayer.wav","stage8/avex_06_your_love_is_like_a_medicine.wav",
    "stage8/avex_07_when_the_sun_goes_down.wav","stage8/avex_08_super_driver.wav",
    "stage8/avex_09_kiss.wav","stage8/avex_10_far_from_the_light.wav",
    "stage8/avex_11_the_race_of_the_night.wav","stage8/avex_12_nonsense_sensation.wav",
    "stage8/avex_13_hearts_on_fire.wav","stage8/avex_14_adrenaline.wav",
    "stage8/avex_15_never_say_never.wav","stage8/avex_16_i_just_wanna_stay_with_you.wav"};

void catalog(){
    check(raceMusicCatalog.size()==117,"Race catalog must retain 102 tracks and append 15 Special Stage tracks");
    std::set<std::string> ids,paths;
    for(std::size_t i=0;i<raceMusicCatalog.size();++i){
        const auto& track=raceMusicCatalog[i];
        check(track.id&&*track.id&&track.title&&*track.title&&track.relativePath&&*track.relativePath,"Empty race track metadata");
        check(ids.insert(track.id).second,"Duplicate stable race music ID");
        check(paths.insert(track.relativePath).second,"Duplicate race stream path");
        check(findMusicTrack(track.id)==int(i),"Stable track ID lookup changed");
        const std::filesystem::path path(track.relativePath);
        check(!path.is_absolute(),"Catalog path must remain relative to stream root");
        for(const auto& part:path)check(part!="..","Catalog path leaves stream root");
        if(i<13){
            check(track.stage==3,"Existing index no longer identifies Stage 3");
            check(track.relativePath==originalPaths[i],"Existing Stage 3 stream order changed");
            check(track.title==originalTitles[i],"Existing Stage 3 title changed");
        }else if(i<30){
            check(track.stage==(i<19?1:2),"Appended track stage/order changed");
            check(track.relativePath==additionalPaths[i-13],"Appended source track order changed");
        }else if(i<44){
            check(track.stage==4,"Stage 4 track source is incorrect");
            check(track.relativePath==stage4Paths[i-30],"Stage 4 track order/path is incorrect");
            check(track.artist&&*track.artist,"Stage 4 artist is missing");
        }else if(i<58){
            check(track.stage==5,"Stage 5 track source is incorrect");
            check(track.relativePath==stage5Paths[i-44],"Stage 5 track order/path is incorrect");
            check(track.artist&&*track.artist,"Stage 5 artist is missing");
        }else if(i<72){
            check(track.stage==6,"Stage 6 track source is incorrect");
            check(track.relativePath==stage6Paths[i-58],"Stage 6 track order/path is incorrect");
            check(track.artist&&*track.artist,"Stage 6 artist is missing");
        }else if(i<86){
            check(track.stage==7,"Stage 7 track source is incorrect");
            check(track.relativePath==stage7Paths[i-72],"Stage 7 track order/path is incorrect");
            check(track.artist&&*track.artist,"Stage 7 artist is missing");
        }else if(i<102){
            check(track.stage==8,"Stage 8 track source is incorrect");
            check(track.relativePath==stage8Paths[i-86],"Stage 8 track order/path is incorrect");
            check(track.artist&&*track.artist,"Stage 8 artist is missing");
        }else{
            check(track.stage==10,"Special Stage category is incorrect");
            check(std::string_view(track.id).starts_with("specialstage."),"Special Stage ID is incorrect");
            check(path.extension()==".ADX","Special Stage source extension changed");
            for(std::size_t old=0;old<102;++old)check(std::string_view(track.title)!=raceMusicCatalog[old].title,"Duplicate Special Stage title");
        }
    }
    check(findMusicTrack("no-such-stage-track")==-1,"Unknown stable ID accepted");
    check(clampMusicTrack(-1)==0&&clampMusicTrack(0)==0,"Lower saved-index clamp changed");
    check(clampMusicTrack(12)==12&&clampMusicTrack(13)==13&&clampMusicTrack(29)==29,"Valid saved index changed");
    check(clampMusicTrack(30)==30&&clampMusicTrack(43)==43,"New valid saved index changed");
    check(clampMusicTrack(44)==44&&clampMusicTrack(57)==57,"New Stage 5 saved index changed");
    check(clampMusicTrack(58)==58&&clampMusicTrack(71)==71,"New Stage 6 saved index changed");
    check(clampMusicTrack(72)==72&&clampMusicTrack(85)==85,"New Stage 7 saved index changed");
    check(clampMusicTrack(86)==86&&clampMusicTrack(101)==101,"New Stage 8 saved index changed");
    check(clampMusicTrack(102)==102&&clampMusicTrack(116)==116&&clampMusicTrack(117)==116&&clampMusicTrack(std::numeric_limits<int>::max())==116,"Upper saved-index clamp is stale");
}

std::array<short,2> referenceSample(const OriginalAudioClip& clip,double cursor){
    std::array<short,2> result{};
    const auto frame=std::size_t(cursor);const float fraction=float(cursor-frame);
    for(unsigned channel=0;channel<2;++channel){
        const auto a=frame*clip.channels+std::min(channel,clip.channels-1);
        const auto next=clip.looping&&clip.loopEnd&&frame+1>=clip.loopEnd?clip.loopStart:std::min(frame+1,clip.frames()-1);
        const auto b=next*clip.channels+std::min(channel,clip.channels-1);
        const float song=(clip.samples[a]+(clip.samples[b]-clip.samples[a])*fraction)/32768.f;
        result[channel]=short(std::clamp(song*.38f*.60f,-1.f,1.f)*32767);
    }
    return result;
}

void compareWindow(EngineAudio& audio,const OriginalAudioClip& clip,std::size_t start,unsigned count,const std::string& label){
    // This is a test-only seek, not a user-facing seek API. Starting at an
    // independently chosen PCM window makes leading silence immaterial.
    audio.musicFrame=double(start);double expectedCursor=double(start);std::uint64_t audible=0;
    for(unsigned i=0;i<count;++i){
        const auto actual=audio.renderStereo(800,0,0,0,false);
        check(actual==referenceSample(clip,expectedCursor),label+": race mixer PCM/rate/channel/headroom mismatch");
        if(actual[0]||actual[1])++audible;
        expectedCursor+=double(clip.sampleRate)/44100;
        const auto end=clip.looping&&clip.loopEnd?clip.loopEnd:clip.frames();
        if(expectedCursor>=end)expectedCursor=clip.looping?
            clip.loopStart+std::fmod(expectedCursor-end,double(end-clip.loopStart)):
            std::fmod(expectedCursor,double(clip.frames()));
    }
    check(audible>0,label+": sampled race output is silent");
    check(std::abs(audio.attractStatistics().frame-expectedCursor)<1e-7,label+": stream cursor differs");
    pcmFrames+=count;audibleFrames+=audible;
}

void customPlayback(const std::filesystem::path& root){
    EngineAudio audio;audio.configure(root);
    auto clip=std::make_shared<OriginalAudioClip>();clip->sampleRate=44100;clip->channels=2;clip->looping=true;
    clip->samples.resize(44100*2);for(std::size_t i=0;i<clip->samples.size();++i)clip->samples[i]=short(12000*std::sin(double(i)*.07));
    audio.customRaceMusic=clip;audio.musicTrack=-2;
    audio.scene(false,false,false,false,true);
    for(int i=0;i<100;++i)audio.renderStereo(800,0,0,0,false);
    check(audio.musicFrame==0,"Custom music advanced during showcase");
    audio.scene(false,false,false);compareWindow(audio,*clip,0,4096,"custom PCM");
    compareWindow(audio,*clip,clip->frames()-64,256,"custom loop");
    const auto cursor=audio.musicFrame;audio.scene(false,false,true);
    for(int i=0;i<100;++i)check(audio.renderStereo(800,0,0,0,false)==std::array<short,2>{},"Custom music leaked while paused");
    check(audio.musicFrame==cursor,"Custom cursor moved while paused");
    audio.scene(false,false,false);audio.setOutputGains({1,0,1,1});
    for(int i=0;i<100;++i)check(audio.renderStereo(800,0,0,0,false)==std::array<short,2>{},"Custom music ignored music mute");
    audio.scene(false,true,false);check(audio.musicScene==2&&audio.music.samples!=clip->samples,"Custom music replaced finish announcement");
    audio.scene(true,false,false);audio.musicTrack=1;audio.scene(false,false,false);
    check(audio.loadedTrack==1&&audio.music.samples!=clip->samples,"Built-in selection did not restore original race music");
}
void playback(const std::filesystem::path& root){
    EngineAudio audio;audio.configure(root);audio.scene(false,false,false);
    for(std::size_t index=0;index<raceMusicCatalog.size();++index){
        const auto& track=raceMusicCatalog[index];const std::string label=track.id;
        const auto clip=loadOriginalSpsd(root/"data/original_audio/streams"/track.relativePath);
        check(clip.frames()>0&&clip.channels>=1&&clip.channels<=2,label+": invalid decoded clip");
        check(clip.sampleRate>=8000&&clip.sampleRate<=48000,label+": invalid decoded rate");
        const auto bounds=std::minmax_element(clip.samples.begin(),clip.samples.end());
        check(*bounds.first<*bounds.second,label+": decoded stream is constant/silent");
        check(audio.selectMusicTrack(int(index)),label+": catalog track cannot be selected");
        check(audio.musicTrack==int(index)&&audio.musicName()==track.title,label+": selected metadata is wrong");
        // Select an audible source window rather than assuming every track
        // starts immediately; preserve each decoded clip's rate and stereo.
        const auto loud=std::find_if(clip.samples.begin(),clip.samples.end(),[](auto value){return std::abs(int(value))>=256;});
        check(loud!=clip.samples.end(),label+": no usable audible PCM window");
        const auto start=std::size_t(loud-clip.samples.begin())/clip.channels;
        compareWindow(audio,clip,start,4096,label);
        if(track.stage>=4){
            check(clip.looping&&clip.loopEnd>clip.loopStart&&clip.loopEnd<=clip.frames(),label+": original source loop bounds missing");
            compareWindow(audio,clip,clip.loopEnd-64,256,label+" source loop boundary");
        }
        if(index==19||index==58||index==72||index==86||index==102){
            audio.musicFrame=0;
            for(unsigned i=0;i<44100;++i)audio.renderStereo(800,0,0,0,false);
            check(clip.frames()>clip.sampleRate,"One-second rate fixture is shorter than a second");
            check(std::abs(audio.attractStatistics().frame-double(clip.sampleRate))<1e-5,label+": rate does not advance one source second in 44100 output frames");
            pcmFrames+=44100;
        }
        const auto serial=audio.outputResetSerial();const auto cursor=audio.attractStatistics().frame;
        check(audio.selectMusicTrack(int(index)),"Same selection should remain valid");
        check(audio.outputResetSerial()==serial&&audio.attractStatistics().frame==cursor,"Same selection restarted playback");
        for(int invalid:{-1,117,std::numeric_limits<int>::max()}){
            check(!audio.selectMusicTrack(invalid),"Invalid race track was accepted");
            check(audio.musicTrack==int(index)&&audio.outputResetSerial()==serial&&audio.attractStatistics().frame==cursor,"Invalid selection mutated playback");
        }
    }
    audio.selectMusicTrack(12);audio.nextMusic();check(audio.musicTrack==13,"Cycle still wraps at old 13-track bound");
    audio.selectMusicTrack(29);audio.nextMusic();check(audio.musicTrack==30,"Stage 4 tracks were not appended after Stage 2");
    audio.selectMusicTrack(43);audio.nextMusic();check(audio.musicTrack==44,"Stage 5 tracks were not appended after Stage 4");
    audio.selectMusicTrack(57);audio.nextMusic();check(audio.musicTrack==58,"Stage 6 tracks were not appended after Stage 5");
    audio.selectMusicTrack(71);audio.nextMusic();check(audio.musicTrack==72,"Stage 7 tracks were not appended after Stage 6");
    audio.selectMusicTrack(85);audio.nextMusic();check(audio.musicTrack==86,"Stage 8 tracks were not appended after Stage 7");
    audio.selectMusicTrack(101);audio.nextMusic();check(audio.musicTrack==102,"Special Stage append boundary changed");
    audio.selectMusicTrack(116);audio.nextMusic();check(audio.musicTrack==0,"Final added track does not wrap to Stage 3 index 0");
    audio.selectMusicTrack(116);audio.renderStereo(800,0,0,0,false);audio.scene(false,false,true);
    const auto pausedCursor=audio.attractStatistics().frame;
    for(unsigned i=0;i<735;++i)check(audio.renderStereo(800,0,0,0,false)==std::array<short,2>{},"Paused added track is audible");
    check(audio.attractStatistics().frame==pausedCursor,"Paused added track cursor advanced");
    audio.scene(false,false,false);audio.renderStereo(800,0,0,0,false);
    check(audio.attractStatistics().frame>pausedCursor,"Added track did not resume");
    // This covers configure's persisted-index boundary; Main settings-file
    // parsing itself remains an application-harness responsibility.
    EngineAudio restored;restored.musicTrack=116;restored.configure(root);
    check(restored.musicTrack==116&&restored.musicName()==raceMusicCatalog[116].title,"Configure discarded valid persisted added-track index");
}

void menuContinuity(const std::filesystem::path& root){
    using namespace idas3::original;
    EngineAudio audio;audio.configure(root);audio.scene(true,false,false);
    OriginalSelectionMusicState manager;
    const auto feed=[&](const OriginalSelectionMusicCommands& commands){for(const auto& c:commands)audio.selection(c);};
    feed(requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Type));
    for(unsigned tick=0;tick<8;++tick){feed(tickOriginalSelectionMusic(manager));for(unsigned i=0;i<735;++i)audio.renderStereo(800,0,0,0,false);}
    check(audio.selectionPlaying()&&audio.selectionSamplePosition()>0,"Menu fixture did not start TYPE");
    const auto cursor=audio.selectionSamplePosition(),started=audio.selectionStatistics().songsStarted,serial=audio.outputResetSerial();
    check(audio.selectMusicTrack(116),"Cannot choose added race music from menu");
    check(audio.selectionPlaying()&&audio.selectionSamplePosition()==cursor&&audio.selectionStatistics().songsStarted==started,"Race selection restarted/stopped TYPE");
    check(audio.outputResetSerial()==serial,"Menu race selection unnecessarily reset output queue");
    audio.nextMusic();check(audio.musicTrack==0,"Menu cycle failed to wrap new catalog");
    for(unsigned i=0;i<735;++i)audio.renderStereo(800,0,0,0,false);
    check(audio.selectionSamplePosition()==cursor+735&&audio.selectionStatistics().songsStarted==started,"Menu TYPE continuity lost after cycling");
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: race_music_catalog_tests <native-root>");
    catalog();playback(argv[1]);menuContinuity(argv[1]);customPlayback(argv[1]);
    std::cout<<"PASS race music catalog: "<<checks<<" checks, 117 decoded tracks, "<<pcmFrames<<" bounded mixer frames, "<<audibleFrames<<" audible sampled frames; no audio device or save writes\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
