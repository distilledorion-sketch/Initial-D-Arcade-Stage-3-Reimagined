#include "original_tuning_preview_state.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("Supply original image and project root");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);
    constexpr unsigned profile=0xc31c99c,owner=0xd000000,preview=0xd001000,stack=0xd100000,stop=0xff0000;
    unsigned cases=0;std::uint64_t instructions=0;
    const auto equal=[&](unsigned a,unsigned b,const char* name){if(a!=b)throw std::runtime_error(std::string(name)+": "+hex(a)+" != "+hex(b));};
    m.zeroRegion(preview,256);m.zeroRegion(stack,65536);
    for(unsigned focus=0;focus<12;++focus){
        for(unsigned angle=0;angle<65536;angle+=17){
            OriginalTuningPreviewState state{angle,focus};m.write32(preview+20,angle);m.write32(preview+24,focus);
            RefCpu c(m);c.r[4]=preview;c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0xc078680,stop,150);
            advanceOriginalTuningPreview(state);equal(state.angle,m.read32(preview+20),"Source focus rotation");++cases;
        }
        OriginalTuningPreviewState state{8192,focus};m.write32(preview+20,8192);m.write32(preview+24,focus);
        for(unsigned frame=0;frame<1200;++frame){RefCpu c(m);c.r[4]=preview;c.r[15]=stack+0xf000;c.pr=stop;
            instructions+=c.run(0xc078680,stop,150);advanceOriginalTuningPreview(state);equal(state.angle,m.read32(preview+20),"Complete focus sequence");++cases;}
    }
    for(unsigned car=0;car<35;++car){const auto& d=data.car(car);
        for(unsigned kind=0;kind<3;++kind)for(unsigned package=0;package<d.packages.size();++package){
            const auto rows=kind==0?d.packages[package].steps.size():kind==2?d.optional.size():1;
            for(unsigned row=0;row<rows;++row)for(unsigned oldPart:{0u,1u,2u,3u}){
                auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(152,std::uint8_t(package));
                p.setByte(153,std::uint8_t(kind==0?row:0));p.setByte(154,std::uint8_t(kind==2?row:0));p.setByte(165,std::uint8_t(oldPart));
                m.clear();m.zeroRegion(owner,0x2000);m.zeroRegion(stack,65536);
                for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);
                m.write32(owner+292,kind);m.write32(owner+264,preview);m.write32(preview+24,11);
                RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0xc0717c0,stop,2000);
                const auto native=originalTuningPreviewFocus(p,data,kind==0?OriginalTuningChildKind::basic:kind==2?OriginalTuningChildKind::optionalPart:OriginalTuningChildKind::performance);
                equal(native.value_or(11),m.read32(preview+24),"Source part focus selector");++cases;
            }
        }
    }
    unsigned plateChecks=0;
    for(unsigned length=0;length<=6;++length)for(unsigned seed=0;seed<222;++seed){
        auto p=makeOriginalFreshBattleProfile();p.setu(76,length);
        m.clear();m.zeroRegion(owner,0x2000);m.zeroRegion(stack,65536);
        for(unsigned i=0;i<5;++i){const unsigned code=(seed+i*39)%222;p.setu(44+i*4,code);m.write32(owner+44+i*4,code);}
        RefCpu c(m);c.r[4]=preview;c.r[5]=owner+44;c.r[6]=length;c.r[15]=stack+0xf000;c.pr=stop;
        c.callHooks[0xc2223b8]=[](auto& q){if(!q.r[5])throw std::runtime_error("Original plate divisor");q.fpul=q.r[4]/q.r[5];};
        instructions+=c.run(0xc0365c0,stop,10000);const auto native=originalTuningPreviewPlateDigits(p);
        for(unsigned i=0;i<5;++i){equal(native[i],m.read8(preview+1716+i),"Source preview name-to-plate");++plateChecks;}
    }
    m.clear();m.zeroRegion(owner,0x2000);m.zeroRegion(stack,65536);
    m.write32(owner,0x3ecccccdu);m.write32(owner+4,0xbf800000u);m.write32(owner+8,0xbf800000u);
    RefCpu lightCpu(m);lightCpu.r[4]=owner;lightCpu.r[15]=stack+0xf000;lightCpu.pr=stop;
    instructions+=lightCpu.run(0xc1f6cf0,stop,100);const auto light=originalTuningPreviewIncomingLight();
    for(unsigned i=0;i<3;++i)equal(std::bit_cast<unsigned>(light[i]),m.read32(owner+i*4),"Original preview normalized incoming light");
    unsigned environmentChecks=0;
    m.write32(owner+1712,0xd020000);m.zeroRegion(0xc2f4b94,4);
    RefCpu environmentCpu(m);environmentCpu.r[4]=owner;environmentCpu.r[5]=3;environmentCpu.r[15]=stack+0xf000;environmentCpu.pr=stop;
    environmentCpu.callHooks[0xc04e480]=[&](auto& q){std::string name;for(unsigned i=0;m.read8(q.r[4]+i);++i)name+=char(m.read8(q.r[4]+i));
        if(name!="/driveA/binary/j_env_select128_b.bin.nz")throw std::runtime_error("Original preview environment source path");q.r[0]=0xd030000;++environmentChecks;};
    environmentCpu.callHooks[0xc1f6f80]=[](auto&){};
    environmentCpu.callHooks[0xc1f7020]=[&](auto& q){equal(q.r[4],0xd020000,"Original environment target handle");equal(m.read32(q.r[5]),0x00800080,"Original environment dimensions");
        equal(m.read32(q.r[5]+4),0x101,"Original environment format/flags");equal(m.read32(q.r[5]+8),0xd030000,"Original environment pixels");++environmentChecks;};
    environmentCpu.callHooks[0xc021980]=[](auto&){};
    // Interrupt-level SR manipulation has no data effect on this isolated
    // descriptor. Execute its pixel-pointer store and both sides separately.
    instructions+=environmentCpu.run(0xc029da0,0xc029dea,1000);
    instructions+=environmentCpu.run(0xc029df6,0xc029df8,10);
    instructions+=environmentCpu.run(0xc029dfc,0xc029e08,100);
    instructions+=environmentCpu.run(0xc029e16,stop,100);equal(environmentChecks,2,"Original environment read/upload count");
    const auto trigPath=std::filesystem::path(argv[2])/"data/original_physics/fsca_table.bin";
    const auto trig=OriginalFscaTable::load(trigPath);std::vector<unsigned> fsca(32768);
    std::ifstream f(trigPath,std::ios::binary);f.seekg(16);f.read(reinterpret_cast<char*>(fsca.data()),131072);
    unsigned matrixChecks=0;
    for(unsigned scene=0;scene<3;++scene)for(unsigned car=0;car<35;++car)for(unsigned yaw:{0u,8192u,32768u,65535u,128000u}){
        m.clear();m.zeroRegion(owner,0x20000);m.zeroRegion(stack,65536);
        m.zeroRegion(0xc92ece0,0x100);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,0x10000);
        m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
        const auto expected=originalTuningPreviewScene(car,yaw,trig,scene);const auto supplied=owner+0xa000;
        // Execute the actual parent's construction too: the expected native
        // matrix must never be seeded into its own source oracle.
        RefCpu parent(m);parent.r[14]=supplied;parent.r[15]=stack+0xf000;parent.pr=stop;parent.fscaHalfWave=fsca;
        for(unsigned i=0;i<16;++i)parent.xf[i]=(i%5==0)?0x3f800000u:0u;
        if(scene==0){parent.r[1]=0xc1f6ac0;instructions+=parent.run(0xc070e4a,0xc070e5c,500);}
        else if(scene==1)instructions+=parent.run(0xc07f3d4,0xc07f3e8,500);
        else instructions+=parent.run(0xc081530,0xc08155e,1000);
        for(unsigned i=0;i<16;++i){equal(parent.xf[i],std::bit_cast<unsigned>(expected.background.elements[i]),"Original parent preview matrix");++matrixChecks;m.write32(supplied+i*4,parent.xf[i]);}
        m.write32(profile+16,car);m.write32(preview+20,yaw);m.write32(preview+8,owner+0x3000);
        m.write32(preview+16,owner+0x4000);m.write32(owner+0x3000+76,owner+0x5000);m.write32(owner+0x5000+12,0xff0020);
        m.write32(owner+0x7000+4,owner+0x8000);
        RefCpu c(m);c.r[4]=preview;c.r[5]=supplied;c.r[15]=stack+0xf000;c.pr=stop;c.fscaHalfWave=fsca;
        for(unsigned i=0;i<16;++i)c.xf[i]=(i%5==0)?0x3f800000u:0u;
        unsigned draws=0,chunk=~0u;
        const auto compare=[&](const OriginalMatrix& matrix){for(unsigned i=0;i<16;++i){equal(c.xf[i],std::bit_cast<unsigned>(matrix.elements[i]),"Original tuning draw matrix");++matrixChecks;}};
        c.callHooks[0xc221fc0]=[&](auto& q){q.r[0]=owner+0x7000;};
        c.callHooks[0xc053480]=[](auto&){};
        c.callHooks[0xc05a8e0]=[&](auto& q){chunk=q.r[5];q.r[0]=owner+0x6000;};
        c.callHooks[0xc1d7120]=[&](auto&){equal(chunk,draws==0?0:3,"Preview ground chunk order");compare(draws==0?expected.background:expected.shadow);++draws;};
        c.callHooks[0xff0020]=[&](auto&){if(draws!=2&&draws!=3)throw std::runtime_error("Preview car draw order");compare(draws==2?expected.car:expected.reflection);++draws;};
        c.callHooks[0xc1fb440]=[](auto&){};
        instructions+=c.run(0xc078720,stop,20000);equal(draws,4,"Preview complete draw count");
    }
    std::cout<<"Original tuning preview: "<<cases<<" focus/rotation cases without hooks; "<<plateChecks<<" plate digits with explicit unsigned divide boundary; "<<matrixChecks<<" matrix comparisons across3actual parent scenes,35cars,5yaws with explicit TLS/light/draw boundaries; "<<instructions<<" bounded original instructions.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
