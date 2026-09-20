#include "original_music_bank.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace idas3;
namespace {
std::uint64_t checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::vector<std::uint8_t> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Missing test fixture");return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void reject(std::vector<std::uint8_t> b,std::size_t at,std::uint32_t value){for(unsigned k=0;k<4;++k)b.at(at+k)=std::uint8_t(value>>(8*k));bool threw=false;try{decodeOriginalMusicBank(b);}catch(const std::exception&){threw=true;}check(threw,"Invalid bank accepted");}
}
int main(int argc,char** argv){try{
    const auto root=std::filesystem::path(argc>1?argv[1]:".")/"data/original_audio/selection";
    // The shipped banks store their samples expanded to 16-bit. Everything
    // below describes the game's packed layout -- addresses recorded from the
    // ARM driver, and the 4-bit ADPCM loop history -- so it reads the packed
    // copies kept beside this reference instead.
    const auto packed=std::filesystem::path(argc>1?argv[1]:".")/"verification/music-bank-reference";
    auto type=loadOriginalMusicBank(packed/"TYPE.dtpk"),select=loadOriginalMusicBank(packed/"SELECT.dtpk"),result=loadOriginalMusicBank(packed/"RESULT.dtpk");
    std::size_t frames=0,layers=0;unsigned encodings[3]{};std::set<unsigned> lfo,filterMode,sampleOffset,env1,env2;
    for(const auto name:{"TYPE","SELECT","RESULT"}){
        const auto& b=std::string(name)=="TYPE"?type:std::string(name)=="SELECT"?select:result;
        check(b.samples.size()==(std::string(name)=="TYPE"?21:std::string(name)=="SELECT"?19:9),"Sample count");check(b.programs.size()==(std::string(name)=="RESULT"?10:11),"Program count");check(b.songs.size()==1,"Song count");check(b.songs[0].command==(std::string(name)=="SELECT"?0xa8:0x1a8),"Song command");
        check(b.builtinSamples.size()==1&&b.builtinSamples[0].sourceId==1&&b.builtinSamples[0].pcm.size()==100&&b.builtinSamples[0].loopEnd==100,"Driver sample namespace and exclusive loop end");
        for(unsigned i=0;i<b.samples.size();++i){
            const auto& s=b.samples[i];++encodings[s.encoding];std::ostringstream fn;fn<<(i<10?"0":"")<<i<<".pcm16";const auto reference=read(root/name/fn.str());
            check(reference.size()==s.pcm.size()*2,"PCM length");
            for(std::size_t j=0;j<s.pcm.size();++j){const auto expected=std::uint16_t(reference[j*2])|(std::uint16_t(reference[j*2+1])<<8);check(std::bit_cast<std::uint16_t>(s.pcm[j])==expected,"Decoded PCM differs");}frames+=s.pcm.size();
            // Independent streaming PCMS2 replay with saved history at LSA,
            // validating three repeated loops against the decoded PCM carrier.
            if(s.encoding==2){
                int quant=127,prev=0,savedQuant=0,savedPrev=0;bool saved=false;unsigned at=0;constexpr int rate[8]={230,230,230,230,307,409,512,614};
                for(unsigned j=0;j<unsigned(s.loopEnd)+3*unsigned(s.loopEnd-s.loopStart);++j){
                    if(at==s.loopStart){if(!saved){savedQuant=quant;savedPrev=prev;saved=true;}else{quant=savedQuant;prev=savedPrev;}}
                    const unsigned n=(s.encodedBytes[at>>1]>>((at&1)*4))&15;
                    int delta=quant*(int(n&7)*2+1)/8;delta=std::min(delta,32767);prev=std::clamp(prev+((n&8)?-delta:delta),-32768,32767);quant=std::clamp(quant*rate[n&7]/256,127,24576);
                    check(prev==s.pcm[at],"ADPCM repeated loop history differs");if(++at==s.loopEnd)at=s.loopStart;
                }
            }
        }
        for(const auto& p:b.programs)for(const auto& g:p.groups)for(const auto& l:g.layers){++layers;lfo.insert(l.lfo);const unsigned flv0=l.rawBytes[16]|(unsigned(l.rawBytes[17])<<8);filterMode.insert((flv0&0x7ff)?l.rawBytes[25]>>3:0x20);sampleOffset.insert(l.sampleOffset);env1.insert(l.envelope1);env2.insert(l.envelope2);}
        reject(b.originalBytes,8,1);reject(b.originalBytes,0x30,64);reject(b.originalBytes,b.sampleTableOffset,999);reject(b.originalBytes,b.sampleTableOffset+4,b.samples[0].descriptor[0]|0x1800000);reject(b.originalBytes,b.sampleTableOffset+12,1);reject(b.originalBytes,b.programTableOffset,0x00040001);reject(b.originalBytes,b.sequenceTableOffset,2);
        auto truncated=b.originalBytes;truncated.resize(32);bool threw=false;try{decodeOriginalMusicBank(truncated);}catch(const std::exception&){threw=true;}check(threw,"Truncated bank accepted");
    }
    std::ifstream fixture(root/"source_lookup.txt");check(bool(fixture),"Missing original ARM reference");std::string row;unsigned originalCalls=0;
    while(std::getline(fixture,row)){std::istringstream in(row);char kind;std::string name;unsigned id;in>>kind>>name>>id;const auto& bank=name=="TYPE"?type:name=="SELECT"?select:result;
        if(kind=='P'){unsigned offset;in>>offset;check(bank.programs.at(id).bankOffset==offset,"Original ARM program lookup mismatch");}
        else if(kind=='L'){
            unsigned group,key,velocity,offset;in>>group>>key>>velocity>>offset;const auto& p=bank.programs.at(id);const auto& g=p.groups.at(group);const int index=g.noteToLayer.at(key);unsigned native=0;
            if(index>=0){const auto& layer=g.layers.at(index);const auto first=p.rawBytes[0]?layer.rawBytes[32]:g.header[2],last=p.rawBytes[0]?layer.rawBytes[33]:g.header[3];if(velocity>=first&&velocity<=last)native=layer.bankOffset;}
            check(native==offset,"Original ARM key/velocity layer selection mismatch");
        }else if(kind=='B'){unsigned a,b,c,d;in>>a>>b>>c>>d;const auto& s=bank.builtinSamples.at(0);check(s.sourceId==id&&(s.bankOffset&65535)==a&&((s.descriptor[0]>>16)&0x7ff)==b&&s.loopStart==c&&s.loopEnd==d,"Original built-in sample lookup mismatch");}
        else{unsigned offset,a,b,c,d;in>>offset>>a>>b>>c>>d;const auto& s=bank.samples.at(id);const auto address=s.descriptor[0]+0x100000+(s.encoding==0?offset*2:s.encoding==1?offset:offset/2);check((address&65535)==a&&((address>>16)&0x7ff)==b,"Original sample address/encoding mismatch");check(((s.looping&&s.loopStart?s.loopStart-offset:s.loopStart)&65535)==c&&((s.loopEnd-offset)&65535)==d,"Original offset loop bounds mismatch");}++originalCalls;
    }
    check(encodings[0]==7&&encodings[1]==5&&encodings[2]==37,"Unexpected encoding inventory");
    std::cout<<"Music bank reference passed: "<<originalCalls<<" original ARM calls,49 samples,"<<frames<<" decoded frames,"<<layers<<" layers,"<<checks<<" comparisons\n";
    for(const auto& [label,values]:{std::pair{"LFO",lfo},{"filter mode",filterMode},{"sample offset",sampleOffset},{"envelope1",env1},{"envelope2",env2}}){std::cout<<label<<":";for(auto v:values)std::cout<<" "<<std::hex<<v;std::cout<<std::dec<<"\n";}
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
