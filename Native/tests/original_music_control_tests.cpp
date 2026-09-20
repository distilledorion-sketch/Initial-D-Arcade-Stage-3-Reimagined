#include "original_music_control.h"
#include "original_music_sequence.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace idas3;
namespace {unsigned checks=0,rows=0;
void eq(unsigned a,unsigned b){++checks;if(a!=b)throw std::runtime_error("line "+std::to_string(rows)+": "+std::to_string(a)+" != "+std::to_string(b));}
OriginalMusicTrackControl track(std::istream&in){unsigned f,h,b,v,i,l;in>>f>>h>>b>>v>>i>>l;return {std::uint8_t(f),std::uint8_t(h),std::uint8_t(b),v,i,l};}
void compare(const OriginalMusicTrackControl&a,const OriginalMusicTrackControl&b){eq(a.flags0,b.flags0);eq(a.header1,b.header1);eq(a.bank2,b.bank2);eq(a.volume1C,b.volume1C);eq(a.increment20,b.increment20);eq(a.lastVolume24,b.lastVolume24);}
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("ARM control fixture required");std::ifstream file(argv[1]);if(!file)throw std::runtime_error("fixture missing");std::string line;
 while(std::getline(file,line)){++rows;std::istringstream in(line);char type;in>>type;
 if(type=='G'){unsigned a,b,c,d,e,out;in>>a>>b>>c>>d>>e>>out;eq(originalMusicChannelGain(std::uint8_t(a),std::uint8_t(b),std::uint8_t(c),std::uint8_t(d),std::uint8_t(e)),out);}
 else if(type=='V'){unsigned a,b,c,d,e,f,g,out;in>>a>>b>>c>>d>>e>>f>>g>>out;OriginalMusicVolumeContext v{std::uint8_t(a),std::uint8_t(b),std::uint8_t(c),std::uint8_t(d),std::uint8_t(e),std::uint8_t(f),std::uint8_t(g)};eq(originalMusicTotalLevel(v),out);}
 else if(type=='H'){for(unsigned expected:{0u,0u,0u,0u,0x3c1fu,0u,0u,0u,0u,0x5a5aff20u,0x1f1fu,0x1f1fu}){unsigned v;in>>v;eq(v,expected);}}
 else if(type=='K'){unsigned f0,f1,bank,low,out;in>>f0>>f1>>bank>>low>>out;eq(originalMusicStopKillsVoice(std::uint8_t(f0),std::uint8_t(f1),bank,low),out);}
 else if(type=='Z'){unsigned ticks,n,volume;in>>ticks>>n>>volume;eq(ticks,985);eq(n,128);eq(volume,0x7f0000u-985u*8388u);}
 else if(type=='L'){unsigned a,out;in>>a>>out;eq(originalMusicFadeLevel(std::uint8_t(a)),out);}
 else {auto t=track(in);unsigned arg;in>>arg;auto expected=track(in);OriginalMusicControlEvents events;
  if(type=='R')requestOriginalMusicFade(std::span(&t,1),t.bank2,std::uint8_t(arg));
  else if(type=='S')stopOriginalMusicTracks(std::span(&t,1),t.bank2|arg);
  else if(type=='T')tickOriginalMusicFade(t);
  else if(type=='P')events=pollOriginalMusicFade(t);
  else throw std::runtime_error("unknown fixture");compare(t,expected);
  if(type=='P'){unsigned n;in>>n;eq(events.count,n);for(unsigned i=0;i<n;++i){unsigned cmd,dest;in>>cmd>>dest;eq(events.events[i].command,cmd);eq(unsigned(events.events[i].local)+2u*unsigned(events.events[i].external),dest);}}
 }
 if(!in)throw std::runtime_error("fixture parse "+std::to_string(rows));
 }
 unsigned notes=0;const auto root=std::filesystem::path(argv[1]).parent_path().parent_path().parent_path();
 for(unsigned cue:{0u,1u}){OriginalMusicSequence sequence;sequence.load(root,cue);for(const auto&e:sequence.events())if(e.kind==OriginalMusicSequenceEventKind::NoteOn){
  OriginalMusicVolumeContext c{e.velocityTableValue,e.layerGain8,e.channelVolume0A,e.channelGain10,e.master05,e.bankFade06,e.channelFlags0};
  eq(originalMusicTotalLevel(c),e.parameters.totalLevel);++notes;
 }}
 if(notes<100)throw std::runtime_error("incomplete score note coverage");
 eq(originalMusicManagerCommand(0x4a0,109,2),0xa0046d02);eq(originalMusicManagerCommand(0xaa0,8,2),0xa00a0802);eq(originalMusicManagerCommand(0x1200a0,0,2),0xa0001202);
 // Source fade argument8 from full level is985 TimerB ticks, and emits exactly
 //127 decreasing volume values plus bank stop. Repeated poll never duplicates.
 OriginalMusicTrackControl t{0x90,0x40,2,0x7f0000,0,0x7f0000};requestOriginalMusicFade(std::span(&t,1),2,8);unsigned ticks=0,updates=0,stops=0;
 while(t.flags0&&ticks<2000){++ticks;tickOriginalMusicFade(t);auto e=pollOriginalMusicFade(t);for(unsigned i=0;i<e.count;++i){if((e.events[i].command&0xffff0000)==0xa01c0000)++updates;else ++stops;}eq(pollOriginalMusicFade(t).count,0);}
 eq(ticks,985);eq(updates,127);eq(stops,1);
 std::cout<<"Music controls: "<<rows<<" actual-ARM fixture cases, "<<checks<<" comparisons ("<<notes<<" actualscore note TLs); argument8 fade985 ticks/43340 sample clocks\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
