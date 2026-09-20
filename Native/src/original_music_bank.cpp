#include "original_music_bank.h"
#include "original_music_band_limit.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace idas3 {
namespace {
struct Reader {
    std::span<const std::uint8_t> b;
    void check(std::size_t p,std::size_t n)const{if(p>b.size()||n>b.size()-p)throw std::runtime_error("Music bank record out of bounds");}
    std::uint8_t u8(std::size_t p)const{check(p,1);return b[p];}
    std::uint16_t u16(std::size_t p)const{return u8(p)|(std::uint16_t(u8(p+1))<<8);}
    std::uint32_t u32(std::size_t p)const{return u16(p)|(std::uint32_t(u16(p+2))<<16);}
    std::vector<std::uint8_t> bytes(std::size_t p,std::size_t n)const{check(p,n);return {b.begin()+p,b.begin()+p+n};}
};
void require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
OriginalMusicLayer layerAt(const Reader& r,std::uint32_t at,std::size_t limit,std::size_t samples){
    require(at<=limit&&64<=limit-at,"Music layer outside instrument");
    OriginalMusicLayer out;out.bankOffset=at;r.check(at,64);
    std::copy_n(r.b.begin()+at,64,out.rawBytes.begin());
    out.sampleId=r.u16(at+2);out.sampleOffset=r.u16(at+4);
    // Program0 in both banks refers to the driver's built-in sample1. Preserve
    // that namespace; it must never be mistaken for local bank sample1.
    require(out.sampleId==1||(out.sampleId>=256&&out.sampleId-256<samples),"Unverified music layer sample bank");
    out.coarsePitch=std::bit_cast<std::int8_t>(r.u8(at+10));out.finePitch=std::bit_cast<std::int8_t>(r.u8(at+11));
    out.envelope1=r.u16(at+12);out.envelope2=r.u16(at+14);out.lfo=r.u16(at+48);
    return out;
}
}
OriginalMusicBank decodeOriginalMusicBank(std::span<const std::uint8_t> bytes){
    Reader r{bytes};r.check(0,64);
    require(r.u32(0)==0x4b505444&&r.u32(8)==bytes.size(),"Invalid music DTPK header/size");
    require(r.u32(0x30)==0&&r.u32(0x34)==0,"Expected A8 instrument bank without A9/ICS");
    OriginalMusicBank out;out.originalBytes.assign(bytes.begin(),bytes.end());
    out.combinationTableOffset=r.u32(0x20);out.programTableOffset=r.u32(0x24);
    out.volumeTableOffset=r.u32(0x28);out.sequenceTableOffset=r.u32(0x2c);out.sampleTableOffset=r.u32(0x3c);
    const auto effect=r.u32(0x38);
    require(64<=out.combinationTableOffset&&out.combinationTableOffset<out.programTableOffset&&out.programTableOffset<out.volumeTableOffset&&out.volumeTableOffset<out.sequenceTableOffset&&out.sequenceTableOffset<effect&&effect<out.sampleTableOffset&&out.sampleTableOffset<bytes.size(),"Unverified music table ordering");
    const auto sampleCount=r.u32(out.sampleTableOffset)+1;
    require(sampleCount&&sampleCount<=256,"Invalid music sample count");
    r.check(out.sampleTableOffset+4,std::size_t(sampleCount)*16);
    // The ADPCM history and quantizer restart at the block, exactly as the
    // hardware does; a stereo pair decodes its second block the same way.
    const auto decode=[&](const OriginalMusicSample& s,std::uint32_t offset,std::size_t frames){
        std::vector<std::int16_t> pcm(frames);
        if(s.encoding==0){for(std::size_t k=0;k<frames;++k)pcm[k]=std::bit_cast<std::int16_t>(r.u16(offset+2*k));}
        else if(s.encoding==1){const auto encoded=r.bytes(offset,frames);
            for(std::size_t k=0;k<frames;++k)pcm[k]=std::int16_t(std::bit_cast<std::int8_t>(encoded[k]))*256;}
        else {
            // AICA PCMS2: low nibble first, saturation of delta before addition,
            // no SPSD stream leakage. At LSA hardware saves predecode history and
            // quantizer and restores it on every loop, so this PCM loop repeats.
            constexpr int scale[8]={230,230,230,230,307,409,512,614};
            const auto encoded=r.bytes(offset,(frames+1)/2);
            int previous=0,quantizer=127;
            for(std::size_t k=0;k<frames;++k){
                const unsigned nibble=(encoded[k/2]>>((k&1)*4))&15,index=nibble&7;
                const int delta=std::min(32767,(quantizer*int(index*2+1))>>3);
                previous=std::clamp(previous+((nibble&8)?-delta:delta),-32768,32767);
                quantizer=std::clamp((quantizer*scale[index])>>8,127,24576);
                pcm[k]=std::int16_t(previous);
            }
        }
        return pcm;
    };
    for(unsigned i=0;i<sampleCount;++i){
        OriginalMusicSample s;s.sourceId=std::uint16_t(256+i);const auto at=out.sampleTableOffset+4+16*i;
        for(unsigned j=0;j<4;++j)s.descriptor[j]=r.u32(at+4*j);
        const auto location=s.descriptor[0],length=s.descriptor[3];
        // Bit 0x80 of the third word is a stereo pair; no other flag appears in
        // any preserved bank, so anything else still fails closed.
        require(!(location&0xfc000000u)&&(s.descriptor[2]==0||s.descriptor[2]==0x80),"Unverified music sample flags/channels");
        s.stereo=s.descriptor[2]==0x80;
        s.encoding=std::uint8_t((location>>23)&3);s.looping=(location&0x02000000)!=0;
        require(s.encoding!=3,"ADPCM stream needs continuous history and is not supported");
        s.bankOffset=location&0x7fffff;s.loopStart=r.u16(at+4);s.loopEnd=r.u16(at+6);
        require(s.bankOffset>=out.sampleTableOffset+4+16*sampleCount&&length>0,"Invalid music sample location");
        s.encodedBytes=r.bytes(s.bankOffset,length);
        require(s.encoding!=0||length%2==0,"Unaligned PCM16 sample");
        const std::size_t frames=s.encoding==0?length/2:s.encoding==1?length:std::size_t(length)*2;
        require(s.loopStart<s.loopEnd&&s.loopEnd<frames,"Invalid music sample LSA/LEA");
        s.pcm=decode(s,s.bankOffset,frames);
        // The right channel is the block immediately after the left one, which
        // is where the driver points the sibling voice it allocates.
        if(s.stereo)s.pcmRight=decode(s,s.bankOffset+length,frames);
        out.samples.push_back(std::move(s));
    }
    // ARM3084..3110: bank+24 -> subbank u16 relative -> program u16 relative.
    const auto table=out.programTableOffset;
    require(r.u16(table)==0&&r.u16(table+2)==4,"Unverified music instrument subbank layout");
    const auto sub=table+4;const unsigned count=r.u16(sub)+1;
    require(count&&count<=128,"Invalid music program count");
    std::vector<std::uint32_t> offsets;
    for(unsigned i=0;i<count;++i){const auto at=sub+r.u16(sub+2+2*i);require(at>=sub+2+2*count&&at<out.volumeTableOffset&&(!i||at>offsets.back()),"Invalid music program offset");offsets.push_back(at);}
    for(unsigned i=0;i<count;++i){
        OriginalMusicProgram p;p.bankOffset=offsets[i];const auto end=i+1<count?offsets[i+1]:out.volumeTableOffset;
        require(end-p.bankOffset>=16,"Truncated music program");p.rawBytes=r.bytes(p.bankOffset,end-p.bankOffset);
        const auto kind=r.u8(p.bankOffset);require(kind<=1,"Unverified music program kind");
        for(unsigned slot=0;slot<4;++slot){
            const auto relative=r.u16(p.bankOffset+8+2*slot);if(!relative)continue;
            OriginalMusicGroup g;g.bankOffset=p.bankOffset+relative;g.noteToLayer.fill(-1);
            require(g.bankOffset>=p.bankOffset+16&&g.bankOffset<=end&&64<=end-g.bankOffset,"Invalid music group offset");
            std::copy_n(bytes.begin()+g.bankOffset,64,g.header.begin());
            if(kind==0){
                require(g.header[2]<=g.header[3]&&g.header[3]<=127,"Invalid music group velocity range");
                const unsigned layers=unsigned(g.header[0])+1;unsigned first=0;
                for(unsigned j=0;j<layers;++j){
                    auto layer=layerAt(r,g.bankOffset+64+64*j,end,sampleCount);const unsigned last=layer.rawBytes[0];
                    require(first<=last&&last<=127,"Invalid music key split");
                    for(unsigned key=first;key<=last;++key)g.noteToLayer[key]=std::int16_t(j);
                    first=last+1;g.layers.push_back(std::move(layer));
                }
            }else{
                // ARM3A78..3AB4: drum key range then a relative u16 layer pointer.
                const auto keyTable=g.bankOffset+64;const unsigned first=r.u8(keyTable),last=r.u8(keyTable+1);
                require(first<=last&&last<=127,"Invalid drum note range");
                for(unsigned key=first;key<=last;++key){
                    const auto rel=r.u16(keyTable+2+2*(key-first));if(!rel)continue;
                    const auto layerOffset=g.bankOffset+rel;
                    auto found=std::find_if(g.layers.begin(),g.layers.end(),[&](const auto& l){return l.bankOffset==layerOffset;});
                    if(found==g.layers.end()){auto layer=layerAt(r,layerOffset,end,sampleCount);require(layer.rawBytes[32]<=layer.rawBytes[33]&&layer.rawBytes[33]<=127,"Invalid drum velocity range");g.layers.push_back(std::move(layer));g.noteToLayer[key]=std::int16_t(g.layers.size()-1);}
                    else g.noteToLayer[key]=std::int16_t(found-g.layers.begin());
                }
            }
            p.groups.push_back(std::move(g));
        }
        require(!p.groups.empty(),"Empty music program");out.programs.push_back(std::move(p));
    }
    const unsigned volumeCount=r.u32(out.volumeTableOffset)+1;
    require(volumeCount&&volumeCount<=16&&out.volumeTableOffset+4+128*volumeCount<=out.sequenceTableOffset,"Invalid music volume tables");
    for(unsigned i=0;i<volumeCount;++i){std::array<std::uint8_t,128> row;std::copy_n(bytes.begin()+out.volumeTableOffset+4+128*i,128,row.begin());out.volumeTables.push_back(row);}
    const auto seq=out.sequenceTableOffset;
    require(r.u32(seq)==0&&r.u16(seq+4)==8&&r.u8(seq+7)==0xa8,"Unverified A8 sequence group layout");
    const auto group=seq+8;
    require(r.u32(group)==0&&r.u32(group+4)==16,"Unverified A8 song table layout");
    OriginalMusicSong song;song.command=0xa8|(std::uint32_t(r.u8(seq+6))<<8);song.bankOffset=seq+16;
    song.rawBytes=r.bytes(song.bankOffset,effect-song.bankOffset);out.songs.push_back(std::move(song));
    return out;
}
OriginalMusicBank loadOriginalMusicBank(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Original selection music bank unavailable");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
    auto result=decodeOriginalMusicBank(bytes);
    std::ifstream builtinFile(path.parent_path()/"builtin_samples.bin",std::ios::binary);
    if(!builtinFile)throw std::runtime_error("Original music built-in sample unavailable");
    const std::vector<std::uint8_t> builtin{std::istreambuf_iterator<char>(builtinFile),std::istreambuf_iterator<char>()};Reader r{builtin};
    require(builtin.size()==232&&std::equal(builtin.begin(),builtin.begin()+8,"MUSB0001")&&r.u32(8)==1&&r.u32(12)==1,"Invalid original music built-in sample asset");
    OriginalMusicSample s;s.sourceId=1;s.bankOffset=0x916c;s.encoding=0;s.loopStart=0;s.loopEnd=100;s.looping=true;
    const std::array<std::uint32_t,4> expected{0x0200916c,0x00640000,0,200};
    for(unsigned i=0;i<4;++i){s.descriptor[i]=r.u32(16+4*i);require(s.descriptor[i]==expected[i],"Unverified built-in sample descriptor");}
    s.encodedBytes=r.bytes(32,200);s.pcm.resize(100);
    for(unsigned i=0;i<100;++i)s.pcm[i]=std::bit_cast<std::int16_t>(r.u16(32+2*i));
    result.builtinSamples.push_back(std::move(s));return result;
}
void buildOriginalMusicBandLimitedLevels(OriginalMusicBank& bank,unsigned levels){
    if(!levels)return;
    const auto build=[&](OriginalMusicSample& s){
        s.bandLimited.clear();s.bandLimitedSpans.clear();
        s.bandLimited.reserve(levels);
        for(unsigned level=0;level<levels;++level)
            s.bandLimited.push_back(bandLimitOriginalMusicSample(s.pcm,s.loopStart,s.loopEnd,
                                                                 s.looping,level));
        for(const auto& one:s.bandLimited)s.bandLimitedSpans.push_back(one);
    };
    for(auto& s:bank.samples)build(s);
    for(auto& s:bank.builtinSamples)build(s);
}
}
