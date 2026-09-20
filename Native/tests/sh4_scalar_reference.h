#pragma once
// DEVELOPMENT-ONLY CPU oracle for bounded scalar blocks from the verified
// user-supplied image. Not linked into the game. Unsupported instructions and
// uninitialized external memory fail closed. No devices, timing or GPU paths.
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sstream>
#include <span>

namespace idas3::reference {
inline std::int32_t signed32(std::uint32_t x){return std::bit_cast<std::int32_t>(x);}
inline std::uint32_t sext8(std::uint32_t x){return std::uint32_t(std::int32_t(std::int8_t(x)));}
inline std::uint32_t sext16(std::uint32_t x){return std::uint32_t(std::int32_t(std::int16_t(x)));}
inline std::string hex(std::uint32_t x){std::ostringstream s;s<<std::hex<<x;return s.str();}
class RefMemory {
public:
    static constexpr std::uint32_t imageBase=0x0c020000;
    std::vector<std::uint8_t> image;
    std::unordered_map<std::uint32_t,std::uint8_t> writes;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> zeroRegions;
    explicit RefMemory(const std::filesystem::path& path){
        std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Original image unavailable");
        image.assign(std::istreambuf_iterator<char>(f),{});
        if(image.size()!=4194304)throw std::runtime_error("Original reference requires canonical 4MiB image");
        if(read32(0x0c15d0f4)!=0x42a00000||read32(0x0c15d0f8)!=0x42d60000)throw std::runtime_error("Original input literal identity mismatch");
    }
    void clear(){writes.clear();zeroRegions.clear();}
    void zeroRegion(std::uint32_t address,std::uint32_t size){if(size>0x2000000||address+size<address)throw std::runtime_error("Invalid reference region");zeroRegions.push_back({address,size});}
    std::uint8_t read8(std::uint32_t address)const{
        auto it=writes.find(address);if(it!=writes.end())return it->second;
        for(auto [base,size]:zeroRegions)if(address>=base&&address-base<size)return 0;
        if(address>=imageBase&&std::size_t(address-imageBase)<image.size())return image[address-imageBase];
        throw std::runtime_error("Uninitialized original reference read 0x"+hex(address));
    }
    std::uint16_t read16(std::uint32_t a)const{return std::uint16_t(read8(a))|(std::uint16_t(read8(a+1))<<8);}
    std::uint32_t read32(std::uint32_t a)const{return std::uint32_t(read16(a))|(std::uint32_t(read16(a+2))<<16);}
    float readFloat(std::uint32_t a)const{return std::bit_cast<float>(read32(a));}
    void write8(std::uint32_t a,std::uint8_t v){writes[a]=v;}
    void write16(std::uint32_t a,std::uint16_t v){write8(a,std::uint8_t(v));write8(a+1,std::uint8_t(v>>8));}
    void write32(std::uint32_t a,std::uint32_t v){write16(a,std::uint16_t(v));write16(a+2,std::uint16_t(v>>16));}
    void writeFloat(std::uint32_t a,float v){write32(a,std::bit_cast<std::uint32_t>(v));}
};

class RefCpu {
public:
    RefMemory& memory;
    std::array<std::uint32_t,16> r{},fr{},xf{};
    bool fpscrSz=false;
    // Verified local primary numerical FSCA half-wave data. No std::sin
    // substitute: tests must provide all32768 words before executing FSCA.
    std::span<const std::uint32_t> fscaHalfWave;
    std::uint32_t pc=0,pr=0,fpul=0,mach=0,macl=0;
    bool t=false;
    // Optional explicit dependency boundary. Tests must identify every hook in
    // their report; a math hook does not prove the original math routine itself.
    std::unordered_map<std::uint32_t,std::function<void(RefCpu&)>> callHooks;
    explicit RefCpu(RefMemory& m):memory(m){}
    float getFloat(int n)const{return std::bit_cast<float>(fr[n]);}
    void setFloat(int n,float v){fr[n]=std::bit_cast<std::uint32_t>(v);}
    std::size_t run(std::uint32_t start,std::uint32_t stop,std::size_t maxSteps=10000){
        pc=start;std::size_t count=0;
        while(pc!=stop){
            if(count++>=maxSteps)throw std::runtime_error("Reference instruction budget exceeded at 0x"+hex(pc));
            if(auto hook=callHooks.find(pc);hook!=callHooks.end()){hook->second(*this);pc=pr;continue;}
            auto here=pc;auto op=memory.read16(here);pc=here+2;
            const int n=(op>>8)&15;const auto top=op>>12;
            const auto branch8=[&](){return here+4+std::uint32_t(std::int32_t(std::int8_t(op&255))*2);};
            bool transfer=false,delay=false;std::uint32_t target=pc;
            if((op&0xff00)==0x8900){if(t){transfer=true;target=branch8();}}
            else if((op&0xff00)==0x8b00){if(!t){transfer=true;target=branch8();}}
            else if((op&0xff00)==0x8d00||(op&0xff00)==0x8f00){delay=true;transfer=true;bool taken=((op&0xff00)==0x8d00)?t:!t;target=taken?branch8():here+4;}
            else if(top==0xa||top==0xb){int d=op&0xfff;if(d&0x800)d-=0x1000;target=here+4+std::uint32_t(d*2);delay=transfer=true;if(top==0xb)pr=here+4;}
            else if((op&0xf0ff)==0x400b||(op&0xf0ff)==0x402b){target=r[n];delay=transfer=true;if((op&0xf0ff)==0x400b)pr=here+4;}
            else if((op&0xf0ff)==0x0003||(op&0xf0ff)==0x0023){target=here+4+r[n];delay=transfer=true;if((op&0xf0ff)==0x0003)pr=here+4;}
            else if(op==0x000b){target=pr;delay=transfer=true;}
            else scalar(op,here);
            if(delay){if(count++>=maxSteps)throw std::runtime_error("Reference delay-slot budget");scalar(memory.read16(here+2),here+2);}
            if(transfer)pc=target;
        }
        return count;
    }
private:
    void scalar(std::uint16_t op,std::uint32_t here){
        int n=(op>>8)&15,m=(op>>4)&15,low=op&15,top=op>>12;
        auto imm=sext8(op&255);auto disp=op&15;
        if(op==0x0009)return;
        //1F6610 prefetches a normal RAM matrix. Cache prefetch has no
        // scalar-visible effect; store-queue/device PREF is not supported.
        if((op&0xf0ff)==0x0083){if(r[n]<0x0c000000u||r[n]>=0x0e000000u)throw std::runtime_error("Reference PREF outside ordinary RAM");return;}
        if(top==0xe){r[n]=imm;return;}
        if(top==0x7){r[n]+=imm;return;}
        if(top==0xd){r[n]=memory.read32(((here+4)&~3u)+(op&255)*4);return;}
        if(top==0x9){r[n]=sext16(memory.read16(here+4+(op&255)*2));return;}
        if(top==0x1){memory.write32(r[n]+disp*4,r[m]);return;}
        if(top==0x5){r[n]=memory.read32(r[m]+disp*4);return;}
        if(top==0x6){switch(low){
            case 0:r[n]=sext8(memory.read8(r[m]));return;
            case 1:r[n]=sext16(memory.read16(r[m]));return;
            case 2:r[n]=memory.read32(r[m]);return;
            case 3:r[n]=r[m];return;
            case 4:{auto address=r[m];auto value=sext8(memory.read8(address));if(n!=m)r[m]++;r[n]=value;return;}
            case 5:{auto address=r[m];auto value=sext16(memory.read16(address));if(n!=m)r[m]+=2;r[n]=value;return;}
            case 6:{auto address=r[m];auto value=memory.read32(address);if(n!=m)r[m]+=4;r[n]=value;return;}
            case 7:r[n]=~r[m];return;
            case 8:r[n]=(r[m]&0xffff0000)|((r[m]&255)<<8)|((r[m]>>8)&255);return;
            case 9:r[n]=(r[m]<<16)|(r[m]>>16);return;
            case 10:{auto original=r[m];r[n]=0-original-std::uint32_t(t);t=original!=0||t;return;}
            case 11:r[n]=0-r[m];return;
            case 12:r[n]=r[m]&255;return;
            case 13:r[n]=r[m]&65535;return;
            case 14:r[n]=sext8(r[m]);return;
            case 15:r[n]=sext16(r[m]);return;
        }}
        if(top==0x2){switch(low){
            case 0:memory.write8(r[n],std::uint8_t(r[m]));return;
            case 1:memory.write16(r[n],std::uint16_t(r[m]));return;
            case 2:memory.write32(r[n],r[m]);return;
            case 4:{auto v=r[m];--r[n];memory.write8(r[n],std::uint8_t(v));return;}
            case 5:{auto v=r[m];r[n]-=2;memory.write16(r[n],std::uint16_t(v));return;}
            case 6:{auto v=r[m];r[n]-=4;memory.write32(r[n],v);return;}
            case 8:t=(r[n]&r[m])==0;return;
            case 9:r[n]&=r[m];return;
            case 10:r[n]^=r[m];return;
            case 11:r[n]|=r[m];return;
            case 12:{ // CMP/STR: any equal corresponding byte sets T.
                const auto different=r[n]^r[m];
                t=(different&0xffu)==0||(different&0xff00u)==0||(different&0xff0000u)==0||(different&0xff000000u)==0;return;
            }
            case 13:r[n]=(r[n]>>16)|(r[m]<<16);return;
            case 14:macl=(r[n]&65535)*(r[m]&65535);return;
            case 15:macl=std::uint32_t(std::int32_t(std::int16_t(r[n]))*std::int32_t(std::int16_t(r[m])));return;
        }}
        if(top==0x3){switch(low){
            case 0:t=r[n]==r[m];return;
            case 2:t=r[n]>=r[m];return;
            case 3:t=signed32(r[n])>=signed32(r[m]);return;
            case 5:{const std::uint64_t product=std::uint64_t(r[n])*std::uint64_t(r[m]);mach=std::uint32_t(product>>32);macl=std::uint32_t(product);return;}
            case 6:t=r[n]>r[m];return;
            case 7:t=signed32(r[n])>signed32(r[m]);return;
            case 8:r[n]-=r[m];return;
            case 10:{std::uint64_t sub=std::uint64_t(r[m])+t;bool borrow=std::uint64_t(r[n])<sub;r[n]-=std::uint32_t(sub);t=borrow;return;}
            case 12:r[n]+=r[m];return;
            case 14:{std::uint64_t sum=std::uint64_t(r[n])+r[m]+t;r[n]=std::uint32_t(sum);t=(sum>>32)!=0;return;}
        }}
        if(top==0x0){switch(low){
            case 4:memory.write8(r[0]+r[n],std::uint8_t(r[m]));return;
            case 5:memory.write16(r[0]+r[n],std::uint16_t(r[m]));return;
            case 6:memory.write32(r[0]+r[n],r[m]);return;
            case 7:macl=r[n]*r[m];return;
            case 12:r[n]=sext8(memory.read8(r[0]+r[m]));return;
            case 13:r[n]=sext16(memory.read16(r[0]+r[m]));return;
            case 14:r[n]=memory.read32(r[0]+r[m]);return;
        }
        switch(op&0xf0ff){case 0x0029:r[n]=t;return;case 0x002a:r[n]=pr;return;case 0x005a:r[n]=fpul;return;case 0x001a:r[n]=macl;return;case 0x000a:r[n]=mach;return;}
        }
        if(top==0x4){
            switch(op&0xff){
                case 0x00:case 0x20:t=(r[n]>>31)!=0;r[n]<<=1;return;
                case 0x01:t=(r[n]&1)!=0;r[n]>>=1;return;
                case 0x21:t=(r[n]&1)!=0;r[n]=std::uint32_t(signed32(r[n])>>1);return;
                case 0x25:{const bool outgoing=(r[n]&1)!=0;r[n]=(r[n]>>1)|(std::uint32_t(t)<<31);t=outgoing;return;} // ROTCR
                case 0x24:{const bool outgoing=(r[n]>>31)!=0;r[n]=(r[n]<<1)|std::uint32_t(t);t=outgoing;return;} // ROTCL
                case 0x08:r[n]<<=2;return;case 0x18:r[n]<<=8;return;case 0x28:r[n]<<=16;return;
                case 0x09:r[n]>>=2;return;case 0x19:r[n]>>=8;return;case 0x29:r[n]>>=16;return;
                case 0x10:--r[n];t=r[n]==0;return;
                case 0x11:t=signed32(r[n])>=0;return;case 0x15:t=signed32(r[n])>0;return;
                case 0x22:r[n]-=4;memory.write32(r[n],pr);return;
                case 0x26:pr=memory.read32(r[n]);r[n]+=4;return;
                case 0x5a:fpul=r[n];return;
                case 0x1a:macl=r[n];return; // LDS Rn,MACL (e.g.023D10)
                case 0x2a:pr=r[n];return;
            }
            if(low==0xc||low==0xd){auto count=signed32(r[m]);if(count>=0)r[n]<<=(count&31);else if((count&31)==0)r[n]=low==0xc?std::uint32_t(signed32(r[n])>>31):0;else r[n]=low==0xc?std::uint32_t(signed32(r[n])>>(-count&31)):r[n]>>(-count&31);return;}
        }
        if((op&0xff00)==0x8800){t=r[0]==imm;return;}
        if(top==0x8){switch(n){case 0:memory.write8(r[m]+disp,std::uint8_t(r[0]));return;case 1:memory.write16(r[m]+disp*2,std::uint16_t(r[0]));return;case 4:r[0]=sext8(memory.read8(r[m]+disp));return;case 5:r[0]=sext16(memory.read16(r[m]+disp*2));return;}}
        if(top==0xc){switch(n){case 7:r[0]=((here+4)&~3u)+(op&255)*4;return;case 8:t=(r[0]&(op&255))==0;return;case 9:r[0]&=op&255;return;case 10:r[0]^=op&255;return;case 11:r[0]|=op&255;return;}}
        if(top==0xf){
            if(op==0xf3fd){fpscrSz=!fpscrSz;return;}
            if(op==0xfbfd){std::swap(fr,xf);return;}
            if((op&0xf3ff)==0xf1fd){
                // Primary Flycast sh4_fpu.cpp:569-611, finite PR0 FTRV.
                const int first=n&12;std::array<float,4> value{},result{};
                for(int i=0;i<4;++i)value[i]=getFloat(first+i);
                for(int row=0;row<4;++row){
                    double sum=double(std::bit_cast<float>(xf[row]))*double(value[0]);
                    sum+=double(std::bit_cast<float>(xf[row+4]))*double(value[1]);
                    sum+=double(std::bit_cast<float>(xf[row+8]))*double(value[2]);
                    sum+=double(std::bit_cast<float>(xf[row+12]))*double(value[3]);
                    result[row]=float(sum);
                }
                for(int i=0;i<4;++i)setFloat(first+i,result[i]);return;
            }
            if((op&0xf1ff)==0xf0fd){
                if(fscaHalfWave.size()!=32768)throw std::runtime_error("Original FSCA reference table is required");
                const auto phase=std::uint16_t(fpul);
                const auto sine=[&](std::uint16_t index){return fscaHalfWave[index&0x7FFFu]^((index&0x8000u)?0x80000000u:0u);};
                fr[n&14]=sine(phase);fr[(n&14)+1]=sine(std::uint16_t(phase+0x4000u));return;
            }
            if((op&0xf0ff)==0xf0ed){
                // PR=0 finite FIPR contract from local Flycast primary
                // sh4_fpu.cpp:400-415. Four double products accumulated in
                // order, followed by one float rounding. Not a silicon claim.
                const int vn=n&12,vm=(n&3)<<2;
                double sum=double(getFloat(vn))*double(getFloat(vm));
                sum+=double(getFloat(vn+1))*double(getFloat(vm+1));
                sum+=double(getFloat(vn+2))*double(getFloat(vm+2));
                sum+=double(getFloat(vn+3))*double(getFloat(vm+3));
                setFloat(vn+3,float(sum));return;
            }
            if(fpscrSz&&low>=6&&low<=12){
                const auto readPair=[&](int encoded){const auto& bank=(encoded&1)?xf:fr;const int first=encoded&14;return std::array<std::uint32_t,2>{bank[first],bank[first+1]};};
                const auto writePair=[&](int encoded,const std::array<std::uint32_t,2>& value){auto& bank=(encoded&1)?xf:fr;const int first=encoded&14;bank[first]=value[0];bank[first+1]=value[1];};
                const auto loadPair=[&](std::uint32_t address){return std::array<std::uint32_t,2>{memory.read32(address),memory.read32(address+4)};};
                const auto storePair=[&](std::uint32_t address,const std::array<std::uint32_t,2>& value){memory.write32(address,value[0]);memory.write32(address+4,value[1]);};
                switch(low){
                    case 6:writePair(n,loadPair(r[0]+r[m]));return;
                    case 7:storePair(r[0]+r[n],readPair(m));return;
                    case 8:writePair(n,loadPair(r[m]));return;
                    case 9:writePair(n,loadPair(r[m]));r[m]+=8;return;
                    case 10:storePair(r[n],readPair(m));return;
                    case 11:{const auto value=readPair(m);r[n]-=8;storePair(r[n],value);return;}
                    case 12:writePair(n,readPair(m));return;
                }
            }
            switch(low){
                case 0:setFloat(n,getFloat(n)+getFloat(m));return;
                case 1:setFloat(n,getFloat(n)-getFloat(m));return;
                case 2:setFloat(n,getFloat(n)*getFloat(m));return;
                case 3:setFloat(n,getFloat(n)/getFloat(m));return;
                case 4:t=getFloat(n)==getFloat(m);return;
                case 5:t=getFloat(n)>getFloat(m);return;
                case 6:fr[n]=memory.read32(r[0]+r[m]);return;
                case 7:memory.write32(r[0]+r[n],fr[m]);return;
                case 8:fr[n]=memory.read32(r[m]);return;
                case 9:fr[n]=memory.read32(r[m]);r[m]+=4;return;
                case 10:memory.write32(r[n],fr[m]);return;
                case 11:r[n]-=4;memory.write32(r[n],fr[m]);return;
                case 12:fr[n]=fr[m];return;
                // Finite F32 FMAC contract matches the local Flycast primary
                // reference core/hw/sh4/interpr/sh4_fpu.cpp:559: std::fma.
                // Exceptional NaNs/FPSCR modes are outside this scalar oracle.
                case 14:setFloat(n,std::fma(getFloat(0),getFloat(m),getFloat(n)));return;
            }
            if(low==13)switch(m){
                case 0:fr[n]=fpul;return;
                case 1:fpul=fr[n];return;
                case 2:setFloat(n,float(signed32(fpul)));return;
                case 3:{float value=getFloat(n);if(std::isnan(value)||value<=-2147483648.f)fpul=0x80000000;else if(value>=2147483648.f)fpul=0x7fffffff;else fpul=std::uint32_t(std::int32_t(value));return;}
                case 4:fr[n]^=0x80000000;return;
                case 5:fr[n]&=0x7fffffff;return;
                case 6:setFloat(n,std::sqrt(getFloat(n)));return;
                // Local Flycast sh4_fpu.cpp:357-368, PR0 finite-positive only.
                case 7:setFloat(n,1.0f/std::sqrt(getFloat(n)));return;
                case 8:setFloat(n,0);return;case 9:setFloat(n,1);return;
            }
        }
        if(op==0x0008){t=false;return;}if(op==0x0018){t=true;return;}
        throw std::runtime_error("Unsupported original reference opcode 0x"+hex(op)+" at 0x"+hex(here));
    }
};
} // namespace idas3::reference
