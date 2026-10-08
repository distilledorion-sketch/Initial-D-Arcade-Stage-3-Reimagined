#include "native_asset_packing.h"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace idas3;
namespace fs=std::filesystem;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void write(const fs::path& file,std::span<const char> bytes){std::ofstream out(file,std::ios::binary);out.write(bytes.data(),bytes.size());check(bool(out),"fixture write failed");}
int main(int argc,char** argv){try{
    check(argc==2,"Pass an isolated fixture output directory");fs::path root=argv[1];fs::create_directories(root);
    std::mt19937 random(31);int checks=0;
    for(unsigned size:{1u,7u,8u,17u,255u,nativeAssetBlockSize-3,nativeAssetBlockSize,nativeAssetBlockSize*2+73})for(int mode=0;mode<3;++mode){
        std::vector<char> original(size);
        for(unsigned i=0;i<size;++i)original[i]=mode==0?0:mode==1?char(i%37):char(random());
        auto packed=packNativeAsset(original);auto file=root/"roundtrip.bin";write(file,packed);
        NativeAssetReader reader(file);std::vector<char> decoded(size);unsigned offset=0;
        while(offset<size){unsigned count=std::min(size-offset,1u+random()%317u);reader.bytes(decoded.data()+offset,count);offset+=count;}
        check(decoded==original&&reader.end(),"roundtrip or exact EOF failed");++checks;
        bool failed=false;try{reader.u32();}catch(const std::exception&){failed=true;}check(failed,"read past EOF accepted");++checks;
        write(file,original);NativeAssetReader raw(file);raw.bytes(decoded.data(),decoded.size());check(decoded==original&&raw.end(),"legacy raw failed");++checks;
        if(size>=4){write(file,packed);NativeAssetReader words(file);for(unsigned i=0;i+4<=size;i+=4){auto v=words.u32();for(int k=0;k<4;++k)check(char(v>>(k*8))==original[i+k],"word decoding failed");}++checks;}
    }
    std::vector<char> zero(nativeAssetBlockSize*2+13);auto packed=packNativeAsset(zero);check(packed.size()<zero.size(),"compression ineffective");
    for(int mutation=0;mutation<9;++mutation){
        auto bad=packed;
        if(mutation==0)bad.resize(12); // truncated header
        if(mutation==1)bad[11]=0x7f; // inflated decoded size
        if(mutation==2)bad[12]=1; // unsupported block size
        if(mutation==3)for(int i=16;i<20;++i)bad[i]=char(255); // stored size bomb
        if(mutation==4)bad[20]^=1; // checksum
        if(mutation==5)bad.resize(bad.size()-1);
        if(mutation==6)bad.push_back(0); // trailing garbage
        if(mutation==7)for(int i=8;i<12;++i)bad[i]=0; // zero size
        if(mutation==8)bad[24]^=char(0x80); // corrupt compressed bytes
        auto file=root/"malformed.bin";write(file,bad);bool rejected=false;
        try{NativeAssetReader reader(file);std::vector<char> out(zero.size());reader.bytes(out.data(),out.size());rejected=!reader.end();}catch(const std::exception&){rejected=true;}
        check(rejected,"malformed block was accepted");++checks;
    }
    // Independent known CRC32/ISO-HDLC vector.
    check(nativeAssetCrc(std::span<const char>("123456789",9))==0xcbf43926u,"CRC mismatch");++checks;
    std::cout<<"PASS native asset storage: "<<checks<<" checks (legacy, multiblock, random, truncation, corruption and budgets)\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
