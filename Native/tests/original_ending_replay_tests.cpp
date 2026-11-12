#include "ending_replay.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and canonical image required");
    RefMemory m(argv[2]);
    const auto trig=original::OriginalFscaTable::load(std::filesystem::path(argv[1])/"data/original_physics/fsca_table.bin");
    constexpr unsigned a=0xd000000,b=a+0x100,da=a+0x200,db=a+0x300,oa=a+0x400,ob=a+0x500,
        input=a+0x1000,output=a+0x2000,owner=a+0x3000,ca=a+0x4000,cb=a+0x4100,
        local=a+0x5000,stack=a+0xf000,stop=0xf000000,ring=0xc63d438,highlight=0xc764a0c;
    unsigned checks=0,invalidReads=0;std::size_t instructions=0;
    auto equal=[&](unsigned x,unsigned y,const char* label){++checks;if(x!=y){
        std::cerr<<label<<": "<<x<<" != "<<y<<'\n';throw std::runtime_error("Ending replay source mismatch");}};
    // Opaque168-byte actor payloads are represented by one marker word per
    // car. Only their memcpy is hooked; all record selection, descriptor,
    // cursor setup/advance and derived ending actor-copy instructions execute.
    for(unsigned count:{0u,1u,8u,3599u,3600u,5000u,7199u,8500u,10600u}){
        m.clear();m.zeroRegion(a,0x10000);m.zeroRegion(0xc630000,0x2d0000);
        RefCpu c(m);
        c.callHooks[0xc222372]=[&](RefCpu& cpu){
            equal(cpu.r[6],20,"Actor copy length");
            m.write32(cpu.r[4],m.read32(cpu.r[5]));
        };
        auto call=[&](unsigned entry,unsigned r4,unsigned r5=0,unsigned r6=0,unsigned r7=0){
            c.r[4]=r4;c.r[5]=r5;c.r[6]=r6;c.r[7]=r7;c.r[15]=stack;c.pr=stop;
            instructions+=c.run(entry,stop,3000);
        };
        call(0xc0dfde0,da);call(0xc0dffe0,db);
        equal(m.read32(da+24),EndingReplay::capacity,"Ring capacity");
        equal(m.read32(db+24),EndingReplay::highlightCapacity,"Highlight capacity");
        m.write32(a+4,oa);m.write32(a+8,da);m.write32(oa+8,7200);m.write32(oa+12,ring);
        m.write32(b+4,ob);m.write32(b+8,db);m.write32(ob+8,10000);m.write32(ob+12,highlight);
        call(0xc0dfb20,da);call(0xc0dfb40,da);call(0xc0e11c0,b);
        equal(m.read32(b+12),std::bit_cast<unsigned>(EndingReplay::highlightThreshold),"Highlight trigger");
        // Verify the DERIVED getters. The base replay vtable has different
        // capacities and buffers and was the source of the previous bug.
        for(auto pair:std::array<std::pair<unsigned,unsigned>,5>{{
            {96,10000},{104,5000},{112,1},{120,highlight},{128,0xc764938}}}){
            call(m.read32(0xc387ebc+pair.first+4),owner);
            equal(c.r[0],pair.second,"Derived ending getter");
        }
        EndingReplay native;
        unsigned accepted=0;
        for(unsigned tick=0;tick<count;++tick){
            float fraction=.7f;int lap=0;
            if(tick==0)fraction=.2f;
            if(tick==1)fraction=EndingReplay::highlightThreshold;
            if(tick==2)fraction=std::nextafter(EndingReplay::highlightThreshold,0.f);
            if(tick==3)lap=1;
            if(tick==4){fraction=std::nextafter(EndingReplay::highlightThreshold,1.f);lap=0;}
            if(tick>4){fraction=.1f;lap=2;} // once triggered, keep recording
            m.write32(input,tick+1);m.write32(input+168,100000+tick+1);
            call(0xc0e0c60,a,input,std::bit_cast<unsigned>(fraction),unsigned(lap));
            call(0xc0e1200,b,input,std::bit_cast<unsigned>(fraction),unsigned(lap));
            EndingReplayFrame f;f.pathIndex=int(tick+1);f.cars[0].yaw=float(tick+1);f.cars[1].yaw=float(100000+tick+1);
            native.record(f,trig,fraction,lap);
            equal(unsigned(native.size()),std::min(tick+1,3600u),"Recent recording size");
            equal(unsigned(native.highlightSize()),m.read32(db+4),"Original highlight write count");
            equal(unsigned(native.recentFrame(unsigned(native.size()-1)).pathIndex),tick+1,"Latest recorded marker");
            accepted=m.read32(db+4);
        }
        call(0xc0e0780,a);call(0xc0e0780,b);native.seal();
        equal(native.ready(),accepted!=0,"Empty highlight readiness");
        const auto sealedCount=native.size();native.record({},trig,1,0);
        equal(unsigned(native.size()),unsigned(sealedCount),"Capture sealed");
        if(!accepted)continue;
        m.write32(owner+0x420,ca);m.write32(owner+0x4a4,cb);
        m.write32(owner+0x2a0,a+0x6000);m.write32(owner+0x2a4,a+0x7000);
        m.write32(local+0x200,owner);
        // Actual secondary setup block, including its unnormalized end+143.
        c.r[8]=local+0x1fc;c.r[14]=local;c.r[15]=stack;c.pr=stop;
        instructions+=c.run(0xc0eac6e,0xc0eaca6,3000);
        m.write32(stack,1);call(0xc157520,ca,highlight,5000,0xc764938);
        equal(m.read32(ca+20),0,"Primary start");
        for(unsigned tick=0;tick<10000;++tick){
            const auto stage=m.read32(owner+0x4a0),cursor=stage?cb:ca;
            const auto index=m.read32(cursor+20);
            const bool valid=stage?(index<std::min(count,3600u)):(index<accepted);
            call(0xc0eb220,owner,2,output);
            if(valid){
                const auto& f=native.playbackFrame(tick);
                equal(unsigned(f.pathIndex),m.read32(output),"Original ending chosen frame");
                equal(unsigned(f.cars[0].yaw),m.read32(output),"Player frame");
                equal(unsigned(f.cars[1].yaw),m.read32(output+168),"Opponent frame");
            }else{
                // Native safety guard substitutes the last highlight pose,
                // while preserving the original next-frame cursor timing.
                ++invalidReads;
                equal(unsigned(native.playbackFrame(tick).pathIndex),accepted+4,"Invalid slot guard");
            }
            c.r[10]=owner;c.r[11]=owner+0x47c;c.r[14]=local;c.r[15]=stack;c.pr=stop;
            instructions+=c.run(0xc0eb35c,0xc0eb4b2,3000);
        }
        equal(m.read32(owner+0x4a0),2,"Both playback passes completed");
        native.clear();equal(native.ready(),false,"Cleared readiness");equal(unsigned(native.highlightSize()),0,"Cleared highlight");
    }
    std::cout<<"PASS "<<checks<<" original ending capture/playback checks; "<<instructions
        <<" source instructions; "<<invalidReads<<" unrecorded/out-of-ring reads guarded. "
        <<"Only opaque actor memcpy hooked; synthetic actor markers, not a complete game run.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
