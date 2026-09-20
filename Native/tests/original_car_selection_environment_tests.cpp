#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::reference;

int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical original image required");
    RefMemory m(argv[1]);
    constexpr unsigned parent=0x0d000000,cars=0x0d001000,child=0x0d002000;
    constexpr unsigned preview=0x0d003000,light=0x0d004000,stack=0x0d100000,stop=0x0f000000;
    m.zeroRegion(parent,0x10000);m.zeroRegion(stack,0x10000);m.zeroRegion(0x0d200000,0x50000);
    std::uint64_t instructions=0;unsigned checks=0;
    auto check=[&](bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);};
    auto run=[&](RefCpu& c,unsigned begin,unsigned end,unsigned limit=300){
        try{instructions+=c.run(begin,end,limit);}catch(const std::exception& e){throw std::runtime_error("Slice "+hex(begin)+": "+e.what());}
    };
    auto stringAt=[&](unsigned address){std::string s;while(m.read8(address))s+=char(m.read8(address++));return s;};

    check(m.read32(0x0c389db0)==0x0c12dc60,"Car owner vtable Init binding");
    check(m.read32(0x0c389db8)==0x0c12e520,"Car owner vtable Main binding");
    for(unsigned selector=0;selector<4;++selector){
        check(m.read32(0x0c2ef204+selector*4)==0x0c23b334,"Environment selector table alias");
        check(stringAt(m.read32(0x0c2ef204+selector*4))=="/driveA/binary/j_env_select128_b.bin.nz","Environment source filename");
    }
    // Parent11D720 sets up every ACar before passing its pointer array onward.
    // Exercise35 distinct array slots rather than substituting a single car.
    for(unsigned index=0;index<35;++index){
        const unsigned car=0x0d200000+index*0x800;
        m.write32(cars+index*4,car);m.write32(car+224,3);
        m.write32(parent+124,cars);m.write32(stack+196,parent+64);m.write32(stack+216,index*4);
        RefCpu c(m);c.r[14]=stack;c.r[15]=stack+0xf000;std::vector<unsigned> calls;
        c.callHooks[0x0c029da0]=[&](auto& q){check(q.r[4]==car&&q.r[5]==1,"Parent selector1/car identity");calls.push_back(1);};
        c.callHooks[0x0c029040]=[&](auto& q){check(q.r[4]==car,"Parent material-rebuild car identity");calls.push_back(2);};
        run(c,0x0c11d820,0x0c11d858);
        check(calls==std::vector<unsigned>{1,2}&&m.read32(car+224)==2,"Parent environment/rebuild/secondary-layer ordering");
    }
    // Both parent branches invoke the same700-byte12D1E0 child. Stack arg0
    // is the prepared car array; arg1 is the common light owner.
    for(unsigned branch=0;branch<2;++branch){
        m.write32(stack+196,parent+64);m.write32(stack+204,parent+124);
        m.write32(stack+(branch?184:176),child);m.write32(parent+120,light);
        m.write32(parent+104,7);m.write32(parent+128,9);
        RefCpu c(m);c.r[14]=stack;c.r[15]=stack+0xf000;unsigned calls=0;
        c.callHooks[0x0c12d1e0]=[&](auto& q){
            check(q.r[4]==child&&q.r[5]==0&&q.r[6]==7&&q.r[7]==9,"Parent car child register arguments");
            check(m.read32(q.r[15])==cars&&m.read32(q.r[15]+4)==light,"Parent prepared car-array and light arguments");++calls;
        };
        run(c,branch?0x0c11daaa:0x0c11d9ce,branch?0x0c11dace:0x0c11d9f2);
        check(calls==1,"Parent car child invocation count");
    }
    // Actual constructor prologue establishes the stack-argument locations.
    RefCpu ctor(m);const unsigned entryStack=stack+0x8000;
    ctor.r[4]=child;ctor.r[5]=0;ctor.r[15]=entryStack;ctor.pr=stop;
    m.write32(entryStack,cars);m.write32(entryStack+4,light);
    ctor.callHooks[0x0c221fc0]=[](auto& q){q.r[0]=0;};
    run(ctor,0x0c12d1e0,0x0c12d20a);
    check(ctor.r[14]==entryStack-508,"Constructor508-byte frame including register saves");
    // The preceding metadata block leaves r0=444 and loads r1=380 at12D5BA.
    // Execute the authored copies
    // of incoming stack arguments into child+440 and child+436.
    ctor.r[0]=444;ctor.r[1]=380;run(ctor,0x0c12d5c2,0x0c12d5d6);
    check(m.read32(child+440)==cars&&m.read32(child+436)==light,"Constructor preserves parent resources");

    // Init12DC60's preview construction passes this same array to10F040.
    m.write32(child+488,35);m.write32(stack+144,child);m.write32(stack+192,preview);
    RefCpu init(m);init.r[14]=stack;init.r[15]=stack+0xf000;
    init.r[1]=stack+188;init.r[3]=stack+124;init.r[7]=child+444;
    unsigned previewCalls=0;
    init.callHooks[0x0c05a8e0]=[&](auto& q){check(q.r[4]==light&&q.r[5]==4,"Original preview light lookup");q.r[0]=light+64;};
    init.callHooks[0x0c10f040]=[&](auto& q){
        check(q.r[4]==preview&&q.r[5]==35&&q.r[6]==cars&&q.r[7]==light+64,"Init reuses prepared array in preview");
        check(q.fr[4]==m.read32(0x0c12e218),"Original preview float argument");++previewCalls;
    };
    run(init,0x0c12e01c,0x0c12e04a);check(previewCalls==1,"Original preview construction count");
    // Selection/color changes rebuild the chosen original ACar, without
    // reloading its environment or replacing its already-enabled layer mask.
    for(unsigned index=0;index<35;++index){
        const unsigned car=m.read32(cars+index*4),profile=0x0d080000+index*16;
        m.write32(child+492,0x12345678);std::vector<unsigned> calls;
        RefCpu c(m);c.r[4]=child;c.r[5]=index;c.r[6]=index%12;c.r[15]=stack+0xf000;c.pr=stop;
        c.callHooks[0x0c133aa0]=[&](auto& q){check(q.r[4]==0x12345678&&q.r[5]==index,"Selected original car lookup");q.r[0]=profile;calls.push_back(0);};
        c.callHooks[0x0c029000]=[&](auto& q){check(q.r[4]==car&&q.r[5]==profile,"Selected appearance car identity");calls.push_back(1);};
        c.callHooks[0x0c028660]=[&](auto& q){check(q.r[4]==car&&q.r[5]==index%12,"Selected factory color");calls.push_back(2);};
        c.callHooks[0x0c0286a0]=[&](auto& q){check(q.r[4]==car&&q.r[5]==1,"Car selection material variant1");calls.push_back(3);};
        c.callHooks[0x0c029040]=[&](auto& q){check(q.r[4]==car,"Selected material rebuild");calls.push_back(4);};
        run(c,0x0c12e4a0,stop);
        check(calls==std::vector<unsigned>{0,1,2,3,4}&&m.read32(car+224)==2,"Selected car retains source secondary layer");
    }
    // Driver-entry parent11E260 deliberately uses variant0 for the shared
    // Transmission/Name pool; only car selection above uses variant1.
    m.write32(parent+8,cars);
    for(unsigned index=0;index<4;++index){
        const unsigned car=m.read32(cars+index*4);
        m.write32(stack+380,parent);m.write32(stack+384,index*4);m.write32(car+224,3);
        RefCpu c(m);c.r[14]=stack;c.r[15]=stack+0xf000;std::vector<unsigned> calls;
        c.callHooks[0x0c0286a0]=[&](auto& q){check(q.r[4]==car&&q.r[5]==0,"Driver entry material variant0");calls.push_back(0);};
        c.callHooks[0x0c029da0]=[&](auto& q){check(q.r[4]==car&&q.r[5]==3,"Driver entry environment selector3");calls.push_back(1);};
        c.callHooks[0x0c029040]=[&](auto& q){check(q.r[4]==car,"Driver entry material rebuild owner");calls.push_back(2);};
        run(c,0x0c11e4ea,0x0c11e54c);
        check(calls==std::vector<unsigned>{0,1,2}&&m.read32(car+224)==2,"Driver entry pool retains distinct variant/layer contract");
    }
    // Source descriptor upload for all aliases; resource allocation/upload are
    // explicit boundaries. No file loader, GPU or interrupt controller runs.
    for(unsigned selector=0;selector<4;++selector){
        const unsigned car=0x0d200000,texture=0x0d0a0000,pixels=0x0d0b0000;
        m.write32(car+1712,texture);m.write32(0x0c2f4b94,0x55);
        RefCpu c(m);c.r[4]=car;c.r[5]=selector;c.r[15]=stack+0xf000;c.pr=stop;
        unsigned uploads=0,loads=0;
        c.callHooks[0x0c04e480]=[&](auto& q){check(stringAt(q.r[4])=="/driveA/binary/j_env_select128_b.bin.nz"&&q.r[5]==0,"Loader exact path/flags");q.r[0]=pixels;++loads;};
        c.callHooks[0x0c1f6f80]=[](auto&){};
        c.callHooks[0x0c1f7020]=[&](auto& q){
            check(q.r[4]==texture&&q.r[6]==0,"Upload replaces existing shared texture handle");
            check(m.read32(q.r[5])==0x00800080&&m.read32(q.r[5]+4)==0x101,"Source128x128 RGB565 descriptor");
            check(m.read32(q.r[5]+8)==pixels&&m.read32(q.r[5]+12)==0,"Source pixel pointer and reserved word");++uploads;
        };
        c.callHooks[0x0c021980]=[&](auto& q){check(q.r[5]==pixels,"Release temporary source pixels");};
        // Exclude only interrupt-priority SR operations, retaining their
        // interleaved descriptor store. They have no image/pointer data effect.
        run(c,0x0c029da0,0x0c029dea);run(c,0x0c029df6,0x0c029df8);
        run(c,0x0c029dfc,0x0c029e08);run(c,0x0c029e16,stop);
        check(loads==1&&uploads==1&&m.read32(0x0c2f4b94)==0x55,"Source load/upload count and context restoration");
    }
    std::cout<<"PASS "<<checks<<" checks / "<<instructions<<" original instructions: parent selector1/layer2, both constructor routes, Init preview array,35 selection refreshes,4 driver-entry variant0 setups,4 exact environment aliases. No devices or user data.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
