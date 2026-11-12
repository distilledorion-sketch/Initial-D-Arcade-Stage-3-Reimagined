// Full 150720 / 150B80 stroke and 14FE20 strip constructors, lists, vertex
// pools, cadence, color and bounds. Only TLS, heap and renderer cache binding
// are substituted. Geometry, material construction and list operations run.
void checkStroke(RefMemory& memory){
    constexpr unsigned stroke=base+0x20000,tls=stroke+0x1000,actor=stroke+0x2000;
    memory.clear();memory.zeroRegion(base,0x200000);memory.write32(tls+4,tls+64);memory.write32(tls+68,tls+128);
    memory.write32(0xc900954,actor);unsigned heap=base+0x40000;
    const auto execute=[&](unsigned entry,const std::function<void(RefCpu&)>& inputs){
        RefCpu cpu(memory);cpu.r[4]=stroke;cpu.r[15]=stack;cpu.pr=stop;
        cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tls;};
        cpu.callHooks[0xc021960]=[&](RefCpu& c){c.r[0]=heap;heap+=(c.r[5]+63)&~63u;if(heap>=base+0x1e0000)throw std::runtime_error("Heap test bound");};
        cpu.callHooks[0xc021980]=[](RefCpu&){};
        cpu.callHooks[0xc1d8a60]=[](RefCpu&){}; // renderer cache invalidation / texture binding
        if(inputs)inputs(cpu);
        try{instructions+=cpu.run(entry,stop,100000);}
        catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" stroke PC="+hex(cpu.pc)+" PR="+hex(cpu.pr));}
        return cpu.r[0];
    };
    execute(0xc150720,{});OriginalTireStroke host;
    for(unsigned tick=0;tick<1100;++tick){
        const OriginalTireContact contact{{float(tick)*.37f,float(tick%13)*.01f,2.f+float(tick)*.07f},{.01f,.98f,.02f}};
        const unsigned strength=tick%2?255:43;const bool brown=tick%3==0;
        memory.write8(actor+116,brown?8:0);
        execute(0xc150b80,[&](RefCpu& c){for(unsigned i=0;i<3;++i){c.r[5+i]=std::bit_cast<unsigned>(contact.position[i]);memory.writeFloat(stack+4*i,contact.normal[i]);}
            memory.write32(stack+12,strength);});
        host.advance(contact,strength,brown);
        equal(unsigned(memory.read8(stroke+96)),unsigned(host.started()),"Stroke initial point");
        equal(unsigned(memory.read8(stroke+97)),unsigned(host.hasEdges()),"Stroke edge state");
        equal(unsigned(memory.read32(stroke+56)!=0),unsigned(host.hasStrip()),"Stroke open strip");
        for(unsigned i=0;i<3;++i){equal(memory.readFloat(stroke+84+4*i),host.previous()[i],"Last point");
            if(host.hasEdges())for(unsigned edge=0;edge<2;++edge)equal(memory.readFloat(stroke+60+12*edge+4*i),host.edges()[edge][i],"Retained edges");}
        unsigned link=memory.read32(stroke+24),count=0;
        while(link!=memory.read32(stroke+20)){
            if(count>=host.strips().size())throw std::runtime_error("Extra source strip");
            const auto at=link-4;const auto& strip=host.strips()[count++];
            const auto model=memory.read32(at+24),ich=memory.read32(model+16),material=memory.read32(model+12);
            equal(memory.read32(ich),tireMarkPcw,"Mark PCW");equal(memory.read32(ich+4),tireMarkIsp,"Mark ISP");
            equal(memory.read32(ich+8),tireMarkTsp,"Mark TSP");equal(memory.read32(material+8),tireMarkGmp,"Mark GMP");
            equal(memory.read32(at+44),strip.cadence,"Sample cadence");
            const auto pool=memory.read32(at+28);equal(memory.read32(pool),20u-unsigned(strip.samples.size())*2u,"Vertex capacity");
            equal(memory.readFloat(at+60),strip.maxX,"Bounds max X");equal(memory.readFloat(at+68),strip.maxZ,"Bounds max Z");
            equal(memory.readFloat(at+72),strip.minX,"Bounds min X");equal(memory.readFloat(at+80),strip.minZ,"Bounds min Z");
            for(unsigned i=0;i<3;++i)equal(memory.readFloat(at+84+i*4),strip.center[i],"Bounds center");
            equal(memory.readFloat(at+96),strip.radiusSquared,"Bounds radius");
            auto head=memory.read32(at+32);
            for(unsigned edge=0;edge<2;++edge){
                unsigned vertex=memory.read32(head+4);
                for(auto sample=strip.samples.rbegin();sample!=strip.samples.rend();++sample){
                    if(vertex==head)throw std::runtime_error("Missing source vertex");
                    for(unsigned i=0;i<3;++i){equal(memory.readFloat(vertex+20+4*i),sample->edges[edge][i],"Vertex position");
                        equal(memory.readFloat(vertex+32+4*i),sample->normal[i],"Vertex normal");}
                    equal(memory.read32(vertex+52),sample->color,"Vertex color");vertex=memory.read32(vertex+4);
                }
                equal(vertex,head,"Vertex list end");head=memory.read32(head+8);
            }
            equal(head,0,"Two ribbon edges");link=memory.read32(link+4);
        }
        equal(count,unsigned(host.strips().size()),"Retained strip count");++cases;
    }
    constexpr unsigned camera=base+0x30000;
    memory.write32(0xc8ff2d8,20);
    for(float x:{-200.f,0.f,100.f,300.f,400.f})for(float z:{-100.f,0.f,40.f,80.f,100.f}){
        auto view=originalSmokeView(originalIdentityMatrix(),10922);
        const OriginalTirePoint eye{x,1,z};for(unsigned i=0;i<3;++i)view.worldToView.elements[12+i]=-eye[i];
        for(unsigned i=0;i<16;++i)memory.writeFloat(camera+64+i*4,view.worldToView.elements[i]);
        for(unsigned i=0;i<3;++i)memory.writeFloat(camera+48+i*4,eye[i]);
        for(unsigned p=0;p<4;++p)for(unsigned i=0;i<3;++i)memory.writeFloat(camera+192+p*12+i*4,view.planes[p][i]);
        std::vector<unsigned> draws;RefCpu cpu(memory);cpu.r[4]=stroke;cpu.r[5]=camera;cpu.r[15]=stack;cpu.pr=stop;
        cpu.callHooks[0xc1506a0]=[&](RefCpu& c){draws.push_back(c.r[4]);};
        instructions+=cpu.run(0xc151120,stop,100000);
        unsigned link=memory.read32(stroke+24),index=0,count=0;
        for(const auto& strip:host.strips()){
            if(originalTireStripVisible(strip,view,eye)){if(index>=draws.size())throw std::runtime_error("Missing source draw");
                equal(draws[index++],link-4,"Visible strip order");}
            link=memory.read32(link+4);++count;
        }
        equal(index,unsigned(draws.size()),"Visible strip count");++cases;
    }
    constexpr unsigned manager=base+0x31000;
    execute(0xc151200,[&](RefCpu& c){c.r[4]=manager;c.r[5]=50;});
    OriginalTireMarkSystem system;std::vector<unsigned> pointers;
    for(unsigned i=0;i<110;++i){
        const auto pointer=execute(0xc1514e0,[&](RefCpu& c){c.r[4]=manager;});
        const auto handle=system.acquire();if(i<50)pointers.push_back(pointer);
        equal(pointer,pointers[handle-1],"Recycled stroke identity");
        equal(memory.read32(manager+40),50u-unsigned(system.order().size()),"Remaining stroke capacity");
        unsigned link=memory.read32(manager+8);
        for(auto slot:system.order()){equal(link-4,pointers[slot],"Manager draw order");link=memory.read32(link+4);}
        equal(link,memory.read32(manager+4),"Manager list end");++cases;
    }
}
