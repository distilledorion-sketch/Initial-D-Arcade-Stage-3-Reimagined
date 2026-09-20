#include "original_audio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>

namespace idas3 {
OriginalAudioClip decodeOriginalAdx(std::span<const std::uint8_t> data){
    // Format/codec references: vgmstream src/meta/adx.c and
    // src/coding/adx_decoder.c. Supports unencrypted encoding3 v3 (PS2
    // Special Stage) and v4; other codecs/encryption fail explicitly.
    if(data.size()<38||data[0]!=0x80||data[1]!=0)
        throw std::runtime_error("Invalid original ADX header");
    const auto be16=[&](std::size_t at){return (unsigned(data[at])<<8)|unsigned(data[at+1]);};
    const auto be32=[&](std::size_t at){return (std::uint32_t(be16(at))<<16)|be16(at+2);};
    const auto signed16=[&](std::size_t at){const auto value=be16(at);return value<32768?int(value):int(value)-65536;};
    if(data[4]!=3||data[5]!=18||data[6]!=4||(data[18]!=3&&data[18]!=4)||data[19]!=0)
        throw std::runtime_error("Unsupported ADX codec/version/encryption");
    OriginalAudioClip out;out.channels=data[7];out.sampleRate=be32(8);
    const std::size_t frames=be32(12),start=std::size_t(be16(2))+4;
    const unsigned cutoff=be16(16);
    if((out.channels!=1&&out.channels!=2)||out.sampleRate<8000||out.sampleRate>48000||
        !frames||frames>64*1024*1024/out.channels||!cutoff||cutoff>=out.sampleRate/2)
        throw std::runtime_error("Invalid ADX sample metadata");
    const bool version3=data[18]==3;
    const std::size_t historyEnd=version3?0x14:0x18+std::max(out.channels,2u)*4;
    if(start<historyEnd+6||start>data.size()||
        !std::equal(data.begin()+start-6,data.begin()+start,"(c)CRI"))
        throw std::runtime_error("Invalid ADX data offset/signature");
    const std::size_t blocks=(frames+31)/32;
    if(blocks>(data.size()-start)/(18*out.channels))
        throw std::runtime_error("Truncated ADX sample blocks");
    out.loopEnd=frames;
    // V4 stores two initial predictor samples per channel before optional
    // loop data. CRI AINF can occupy that extension instead of loop records.
    std::size_t ainfSize=0;
    if(!version3&&start-6>=historyEnd+12&&be32(historyEnd+4)==0x41494e46u){
        ainfSize=be32(historyEnd+8);
        if(ainfSize<8||ainfSize>start-6-historyEnd)
            throw std::runtime_error("Invalid ADX AINF extent");
    }
    if(start-6-ainfSize>=historyEnd+24){
        out.looping=be32(historyEnd+4)!=0;
        if(out.looping){
            out.loopStart=be32(historyEnd+8);out.loopEnd=be32(historyEnd+16);
            if(out.loopStart>=out.loopEnd||out.loopEnd>frames)
                throw std::runtime_error("Invalid ADX loop bounds");
        }
    }
    const float cosine=std::cos(float(2.0*std::numbers::pi*double(cutoff)/double(out.sampleRate)));
    const float a=float(std::numbers::sqrt2-double(cosine));
    const float b=float(std::numbers::sqrt2-1.0);
    const float predictor=(a-std::sqrt((a+b)*(a-b)))/b;
    const int coefficient1=int(predictor*8192.f),coefficient2=int(predictor*predictor*-4096.f);
    out.samples.resize(frames*out.channels);
    for(unsigned channel=0;channel<out.channels;++channel){
        int previous=version3?0:signed16(0x18+channel*4),older=version3?0:signed16(0x1a+channel*4);
        for(std::size_t block=0;block<blocks;++block){
            const std::size_t at=start+(block*out.channels+channel)*18;
            const unsigned storedScale=be16(at);
            // A high-bit scale is an end/encryption marker, not a valid
            // encoded block inside the header's declared sample count.
            if(storedScale&0x8000u)throw std::runtime_error("Invalid ADX block scale before sample end");
            const int scale=int(storedScale)+1;
            const std::size_t count=std::min<std::size_t>(32,frames-block*32);
            for(std::size_t sample=0;sample<count;++sample){
                const unsigned packed=data[at+2+sample/2];
                const int nibble=int((sample&1)?packed&15:packed>>4);
                // Early v3 rounds the two predictor products separately.
                // Preserve v4's combined rounding for the existing songs.
                const auto prediction=version3?
                    ((std::int64_t(coefficient1)*previous)>>12)+((std::int64_t(coefficient2)*older)>>12):
                    (std::int64_t(coefficient1)*previous+std::int64_t(coefficient2)*older)>>12;
                const auto decoded=std::clamp<std::int64_t>((nibble<8?nibble:nibble-16)*scale+prediction,-32768,32767);
                older=previous;previous=int(decoded);
                out.samples[(block*32+sample)*out.channels+channel]=std::int16_t(decoded);
            }
        }
    }
    return out;
}
OriginalAudioClip decodeOriginalMsAdpcmWave(std::span<const std::uint8_t> data){
    // XACT's MS ADPCM prediction uses arithmetic >>8, including negative
    // predictions. Reference: vgmstream/src/coding/msadpcm_decoder.c.
    const auto u16=[&](std::size_t at){return unsigned(data[at])|(unsigned(data[at+1])<<8);};
    const auto u32=[&](std::size_t at){return std::uint32_t(u16(at))|(std::uint32_t(u16(at+2))<<16);};
    const auto tag=[&](std::size_t at,const char* value){return std::equal(data.begin()+at,data.begin()+at+4,value);};
    if(data.size()<12||!tag(0,"RIFF")||!tag(8,"WAVE")||std::uint64_t(u32(4))+8!=data.size())
        throw std::runtime_error("Invalid MS ADPCM RIFF extent");
    std::size_t fmt=0,fmtSize=0,fact=0,payload=0,payloadSize=0,loop=0;
    for(std::size_t at=12;at<data.size();){
        if(data.size()-at<8)throw std::runtime_error("Truncated WAVE chunk header");
        const std::size_t size=u32(at+4),body=at+8;
        if(size>data.size()-body||(size&1)>data.size()-body-size)
            throw std::runtime_error("Truncated WAVE chunk");
        if(tag(at,"fmt ")){if(fmt)throw std::runtime_error("Duplicate WAVE format");fmt=body;fmtSize=size;}
        else if(tag(at,"fact")){if(fact||size!=4)throw std::runtime_error("Invalid WAVE fact");fact=body;}
        else if(tag(at,"data")){if(payload)throw std::runtime_error("Duplicate WAVE data");payload=body;payloadSize=size;}
        else if(tag(at,"smpl")){
            if(loop||size!=60||u32(body+28)!=1||u32(body+32)!=0||u32(body+40)!=0||u32(body+52)!=0||u32(body+56)!=0)
                throw std::runtime_error("Unsupported WAVE sample loop");
            loop=body;
        }
        at=body+size+(size&1);
    }
    if(!fmt||fmtSize!=50||!fact||!payload||u16(fmt)!=2||u16(fmt+14)!=4||u16(fmt+16)!=32||u16(fmt+20)!=7)
        throw std::runtime_error("Unsupported MS ADPCM WAVE format");
    OriginalAudioClip out;out.channels=u16(fmt+2);out.sampleRate=u32(fmt+4);
    const std::size_t blockBytes=u16(fmt+12),blockFrames=u16(fmt+18),frames=u32(fact);
    if((out.channels!=1&&out.channels!=2)||out.sampleRate<8000||out.sampleRate>48000
       ||blockBytes<7*out.channels||blockBytes>8192||blockBytes%out.channels
       ||blockFrames!=(blockBytes-7*out.channels)*2/out.channels+2
       ||u32(fmt+8)!=out.sampleRate*blockBytes/blockFrames
       ||!frames||frames>std::size_t(out.sampleRate)*3600||!payloadSize||payloadSize>64*1024*1024
       ||payloadSize%blockBytes||frames>payloadSize/blockBytes*blockFrames
       ||frames<=(payloadSize/blockBytes-1)*blockFrames)
        throw std::runtime_error("Invalid MS ADPCM sample bounds");
    constexpr int coefficients[7][2]={{256,0},{512,-256},{0,0},{192,64},{240,0},{460,-208},{392,-232}};
    for(unsigned i=0;i<7;++i)for(unsigned j=0;j<2;++j)
        if(std::int16_t(u16(fmt+22+i*4+j*2))!=coefficients[i][j])throw std::runtime_error("Unsupported MS ADPCM coefficients");
    if(loop){
        out.looping=true;out.loopStart=u32(loop+44);out.loopEnd=std::size_t(u32(loop+48))+1;
        if(out.loopStart>=out.loopEnd||out.loopEnd>frames)throw std::runtime_error("Invalid MS ADPCM loop bounds");
    }
    out.samples.resize(frames*out.channels);
    constexpr int adaptation[16]={230,230,230,230,307,409,512,614,768,614,512,409,307,230,230,230};
    for(std::size_t frame=0,at=payload;frame<frames;frame+=blockFrames,at+=blockBytes){
        int predictor[2]{},history1[2]{},history2[2]{};std::int64_t delta[2]{};
        for(unsigned channel=0;channel<out.channels;++channel){
            predictor[channel]=data[at+channel];delta[channel]=std::int16_t(u16(at+out.channels+channel*2));
            history1[channel]=std::int16_t(u16(at+3*out.channels+channel*2));
            history2[channel]=std::int16_t(u16(at+5*out.channels+channel*2));
            if(predictor[channel]>=7||delta[channel]<0)throw std::runtime_error("Invalid MS ADPCM block header");
        }
        for(std::size_t sample=0;sample<std::min(blockFrames,frames-frame);++sample){
            for(unsigned channel=0;channel<out.channels;++channel){
                int decoded;
                if(sample<2)decoded=sample?history1[channel]:history2[channel];
                else{
                    const auto nibbleIndex=(sample-2)*out.channels+channel;
                    const unsigned nibble=(data[at+7*out.channels+nibbleIndex/2]>>((nibbleIndex&1)?0:4))&15;
                    const auto& coeff=coefficients[predictor[channel]];
                    const std::int64_t prediction=(std::int64_t(history1[channel])*coeff[0]+std::int64_t(history2[channel])*coeff[1])>>8;
                    decoded=int(std::clamp<std::int64_t>(prediction+(int(nibble)<8?int(nibble):int(nibble)-16)*delta[channel],-32768,32767));
                    history2[channel]=history1[channel];history1[channel]=decoded;
                    delta[channel]=std::clamp<std::int64_t>((delta[channel]*adaptation[nibble])>>8,16,2147483647);
                }
                out.samples[(frame+sample)*out.channels+channel]=std::int16_t(decoded);
            }
        }
    }
    return out;
}
OriginalAudioClip decodeOriginalSpsd(std::span<const std::uint8_t> data){
    if(data.size()>=2&&data[0]==0x80&&data[1]==0)return decodeOriginalAdx(data);
    if(data.size()>=4&&data[0]=='R'&&data[1]=='I'&&data[2]=='F'&&data[3]=='F')return decodeOriginalMsAdpcmWave(data);
    // SPSD header/layout and Yamaha expansion are documented by vgmstream:
    // https://github.com/vgmstream/vgmstream/blob/master/src/meta/spsd.c
    // https://github.com/vgmstream/vgmstream/blob/master/src/coding/yamaha_decoder.c
    if(data.size()<64||data[0]!='S'||data[1]!='P'||data[2]!='S'||data[3]!='D')throw std::runtime_error("Invalid original SPSD header");
    const auto u16=[&](std::size_t at){return unsigned(data[at])|(unsigned(data[at+1])<<8);};
    const auto u32=[&](std::size_t at){return std::uint32_t(u16(at))|(std::uint32_t(u16(at+2))<<16);};
    if((data[4]!=0&&data[4]!=1)||data[5]!=1||data[6]!=0||data[7]!=4)throw std::runtime_error("Unsupported SPSD version");
    const unsigned codec=data[8],index=u16(10);const std::size_t size=u32(12);
    OriginalAudioClip out;out.channels=(data[9]&3)?2:1;out.sampleRate=u16(42);out.looping=(data[9]&128)!=0;
    if(out.sampleRate<8000||out.sampleRate>48000||size==0||size>64*1024*1024||size>data.size()-64||size%out.channels)throw std::runtime_error("Invalid SPSD sample bounds");
    if(codec!=0&&codec!=1&&codec!=3)throw std::runtime_error("Unsupported SPSD codec");
    if(index!=255&&index!=13&&!(index==0&&out.channels==1))throw std::runtime_error("Unsupported SPSD interleave");
    const std::size_t perChannel=size/out.channels;
    if(codec==0&&perChannel%2)throw std::runtime_error("Odd PCM16 SPSD sample bounds");
    const std::size_t frames=codec==3?perChannel*2:codec==0?perChannel/2:perChannel;
    const std::size_t loopAdjustment=codec==3?16384:codec==0?4096:8192;
    out.loopStart=std::size_t(u32(44))+loopAdjustment;
    if(out.looping&&out.loopStart>=frames)throw std::runtime_error("Invalid SPSD loop start");
    out.samples.resize(frames*out.channels);
    const auto channelByte=[&](unsigned channel,std::size_t byte){
        std::size_t offset;
        if(index==13){const std::size_t group=byte/8192,within=byte%8192,block=std::min<std::size_t>(8192,perChannel-group*8192);offset=group*8192*out.channels+channel*block+within;}
        else offset=channel*perChannel+byte;
        return data[64+offset];
    };
    constexpr std::array<int,8> scales{230,230,230,230,307,409,512,614};
    for(unsigned channel=0;channel<out.channels;++channel){int history=0,step=127;
        for(std::size_t frame=0;frame<frames;++frame){int sample;
            if(codec==0){const auto word=unsigned(channelByte(channel,frame*2))|(unsigned(channelByte(channel,frame*2+1))<<8);sample=std::int16_t(word);}
            else if(codec==1)sample=int(std::int8_t(channelByte(channel,frame)))*256;
            else{
                const unsigned nibble=(channelByte(channel,frame/2)>>((frame&1)*4))&15;
                const int delta=std::min(32767,((int(nibble&7)*2+1)*step)/8);
                history=std::clamp(history*254/256+((nibble&8)?-delta:delta),-32768,32767);
                step=std::clamp(step*scales[nibble&7]/256,127,24576);sample=history;
            }
            out.samples[frame*out.channels+channel]=std::int16_t(sample);
        }
    }
    return out;
}
OriginalAudioClip loadOriginalSpsd(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Missing original audio stream: "+path.string());
    const auto length=input.tellg();if(length<38||length>64*1024*1024+64)throw std::runtime_error("Invalid original audio file length");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));input.seekg(0);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),length))throw std::runtime_error("Truncated original audio file");
    return decodeOriginalSpsd(bytes);
}
}
