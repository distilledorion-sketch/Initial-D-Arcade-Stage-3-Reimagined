// Execute the full E4320 caller, including ACar wheel lookup, matrix stack,
// source vector wrappers and actor fallback. Leaf resource callbacks,
// profiler markers and exception TLS are explicit boundaries.
void checkOriginalReplayLeafCaller(RefMemory& memory,const OriginalFscaTable& trig,
        const std::vector<unsigned>& wave,const std::filesystem::path& nativeRoot){
    constexpr unsigned raceAt=0xc920000,carAt=raceAt+0x1000,partAt=raceAt+0x3000,
        actorAt=raceAt+0x5000,tlsAt=raceAt+0x6000,leafAt=raceAt+0x7000;
    const auto parts=OriginalCarParts::load(nativeRoot/"data/original_models/toyota_ae86t/assembly_parts.bin");
    const auto origins=originalCarEffectOrigins(parts);
    for(unsigned variant=0;variant<48;++variant)for(unsigned cooldown:{0u,1u,4u}){
        memory.clear();memory.zeroRegion(particle,0x40000);
        memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
        memory.write32(tlsAt+4,tlsAt+64);memory.write32(tlsAt+68,tlsAt+128);
        memory.write32(raceAt+0x2a0,carAt);memory.write32(raceAt+108,variant%5==0?0:leafAt);
        memory.write32(raceAt+112,cooldown);memory.write32(carAt+0x2f0,partAt);
        memory.write32(carAt+0x95c,actorAt);memory.write32(carAt+0x960,variant%2?actorAt:0);
        memory.write32(carAt+0x964+64,0xc380b8c);
        unsigned pointer=partAt;for(const auto& t:parts.transforms)for(const auto* values:{&t.scale,&t.rotationDegrees,&t.translation})
            for(float value:*values){memory.writeFloat(pointer,value);pointer+=4;}
        const auto world=originalActorMatrix({float(variant)*.37f,-3.5f,27.f},
            {float(variant%4)*-.17f,float(variant)*.21f,float(variant%5)*.13f},trig);
        const std::array<float,3> delta{float(variant%3)*.11f,-.05f,float(variant%7)*.2f};
        std::array<std::uint8_t,4> surfaces;
        for(unsigned i=0;i<16;++i)memory.writeFloat(carAt+0x964+4*i,world.elements[i]);
        for(unsigned i=0;i<3;++i)memory.writeFloat(actorAt+12+4*i,delta[i]);
        for(unsigned i=0;i<4;++i){surfaces[i]=std::uint8_t(variant+i*13);memory.write8(actorAt+116+i,surfaces[i]);}
        struct Admission{std::array<float,3> position,velocity;};
        std::vector<Admission> admissions;std::array<float,3> wakePosition{},wakeVelocity{};unsigned wakeCalls=0;
        RefCpu cpu(memory);cpu.fscaHalfWave=wave;cpu.r[4]=raceAt;cpu.r[15]=stack;cpu.pr=stop;
        const auto identity=originalIdentityMatrix();for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(identity.elements[i]);
        cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tlsAt;};
        cpu.callHooks[0xc156700]=[](RefCpu&){};
        cpu.callHooks[0xc0d4b60]=[&](RefCpu& c){
            equal(c.r[4],leafAt,"Replay leaf owner");Admission a;
            for(unsigned i=0;i<3;++i){a.position[i]=std::bit_cast<float>(c.r[5+i]);a.velocity[i]=memory.readFloat(c.r[15]+4*i);}
            admissions.push_back(a);
        };
        cpu.callHooks[0xc0d4b00]=[&](RefCpu& c){
            equal(c.r[4],leafAt,"Replay wake owner");++wakeCalls;
            for(unsigned i=0;i<3;++i){wakePosition[i]=std::bit_cast<float>(c.r[5+i]);wakeVelocity[i]=memory.readFloat(c.r[15]+4*i);}
        };
        try{instructions+=cpu.run(0xc0e4320,stop,30000);}
        catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" leaf caller PC="+hex(cpu.pc)+" PR="+hex(cpu.pr));}
        const auto frame=originalReplayLeafFrame(world,origins,surfaces,delta,trig);
        unsigned admitted=0;
        if(cooldown==0&&variant%5!=0)for(unsigned i=0;i<4;++i)if(originalReplayLeafSurface(surfaces[i])){
            if(admitted>=admissions.size())throw std::runtime_error("Missing source leaf admission");
            for(unsigned axis=0;axis<3;++axis){equal(admissions[admitted].position[axis],frame.contacts[i][axis],"Replay leaf position");equal(admissions[admitted].velocity[axis],0.f,"Replay leaf launch velocity");}
            ++admitted;
        }
        equal(unsigned(admissions.size()),admitted,"Leaf contact admission count");
        equal(wakeCalls,unsigned(variant%5!=0),"Leaf update without resource");
        if(wakeCalls)for(unsigned i=0;i<3;++i){equal(wakePosition[i],frame.wakeOrigin[i],"Replay wake position");equal(wakeVelocity[i],frame.wakeDisplacement[i],"Replay wake velocity");}
        equal(memory.read32(raceAt+112),cooldown?cooldown-1:0,"Leaf contact cooldown");++cases;
    }
}

// Actual replay admission, three-model emitter, pool allocate/link/traverse/
// unlink/recycle, particle callbacks, force, matrices and RNG execute together.
void checkOriginalReplayLeafLifecycle(RefMemory& memory,const OriginalFscaTable& trig,
        const std::vector<unsigned>& wave,const std::filesystem::path& nativeRoot){
    constexpr unsigned raceAt=0xc920000,carAt=raceAt+0x1000,partAt=raceAt+0x3000,
        actorAt=raceAt+0x5000,tlsAt=raceAt+0x6000,leafAt=raceAt+0x7000,
        poolAt=raceAt+0x8000,slotsAt=raceAt+0x10000,modelBase=raceAt+0x7800,tableAt=raceAt+0x7c00;
    const auto parts=OriginalCarParts::load(nativeRoot/"data/original_models/toyota_ae86t/assembly_parts.bin");
    const auto origins=originalCarEffectOrigins(parts);
    unsigned full=0,drained=0,refilled=0,drawCount=0;
    const unsigned capacity=memory.read16(0xc0e2d4a);
    equal(capacity,OriginalReplayLeafSystem::capacity,"Replay constructor pool capacity");
    for(unsigned scene=0;scene<4;++scene){
        memory.clear();memory.zeroRegion(particle,0x40000);
        memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
        memory.write32(tlsAt+4,tlsAt+64);memory.write32(tlsAt+68,tlsAt+128);
        memory.write32(raceAt+0x2a0,carAt);memory.write32(raceAt+108,leafAt);
        memory.write32(carAt+0x2f0,partAt);memory.write32(carAt+0x95c,actorAt);memory.write32(carAt+0x960,actorAt);
        memory.write32(carAt+0x964+64,0xc380b8c);memory.write32(leafAt+4,owner);
        memory.write32(owner,tableAt);memory.write16(tableAt+16,0);memory.write32(tableAt+20,0xc020c00);
        memory.write32(owner+4,poolAt);memory.write32(poolAt,capacity);
        const unsigned head=poolAt+4;memory.write32(head,head);memory.write32(head+4,head);memory.write32(head+12,1);
        for(unsigned i=0;i<capacity;++i)memory.write32(poolAt+24+4*i,slotsAt+88*i);
        unsigned pointer=partAt;for(const auto& t:parts.transforms)for(const auto* values:{&t.scale,&t.rotationDegrees,&t.translation})
            for(float value:*values){memory.writeFloat(pointer,value);pointer+=4;}
        OriginalReplayLeafSystem system;Random random{scene?0x37c778u*scene:0};memory.write32(0xc37c778,random.state);
        for(unsigned tick=0;tick<370;++tick){
            const auto world=originalActorMatrix({float(tick)*(.07f+.02f*scene),float(scene)*.1f,float(tick)*-.15f},
                {float(scene)*.05f,float(tick)*.013f,-float(scene)*.06f},trig);
            const std::array<float,3> delta=tick%19<3?std::array<float,3>{0,0,0}:std::array<float,3>{.07f,.013f,-.15f};
            std::array<std::uint8_t,4> surfaces{};
            for(unsigned i=0;i<16;++i)memory.writeFloat(carAt+0x964+4*i,world.elements[i]);
            for(unsigned i=0;i<3;++i)memory.writeFloat(actorAt+12+4*i,delta[i]);
            for(unsigned i=0;i<4;++i){
                surfaces[i]=(tick<120||(tick>=220&&tick<260))?std::uint8_t((tick+i+scene)%4==0?7:(i%3==0?1:i%3==1?2:11)|0x20):7;
                memory.write8(actorAt+116+i,surfaces[i]);
            }
            memory.write32(carAt+0x960,tick%2?actorAt:0);
            RefCpu cpu(memory);cpu.fscaHalfWave=wave;cpu.r[4]=raceAt;cpu.r[15]=stack;cpu.pr=stop;
            const auto identity=originalIdentityMatrix();for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(identity.elements[i]);
            cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tlsAt;};cpu.callHooks[0xc156700]=[](RefCpu&){};
            cpu.callHooks[0xc05a8e0]=[&](RefCpu& c){if(c.r[5]<4||c.r[5]>6)throw std::runtime_error("Unexpected source leaf model");c.r[0]=modelBase+c.r[5]*64;};
            std::vector<OriginalMatrix> draws;
            cpu.callHooks[0xc1d7120]=[&](RefCpu& c){OriginalMatrix matrix;for(unsigned i=0;i<16;++i)matrix.elements[i]=std::bit_cast<float>(c.xf[i]);draws.push_back(matrix);};
            try{instructions+=cpu.run(0xc0e4320,stop,300000);}
            catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" leaf lifecycle tick="+std::to_string(tick)+" PC="+hex(cpu.pc));}
            const auto frame=originalReplayLeafFrame(world,origins,surfaces,delta,trig);system.advance(frame,trig,random);
            equal(memory.read32(0xc37c778),random.state,"Complete leaf RNG");
            equal(memory.read32(owner+8),system.admitted(),"Successful leaf admissions");
            equal(memory.read32(poolAt),capacity-unsigned(system.order().size()),"Free leaf capacity");
            equal(unsigned(draws.size()),unsigned(system.order().size()),"Leaf draw count");
            equal(memory.readFloat(owner+36),system.wake().strength,"Decayed wake strength");
            unsigned at=memory.read32(head+4),index=0;
            while(at!=head){
                if(index>=system.order().size())throw std::runtime_error("Source leaf list too long");
                const auto& p=system.particle(system.order()[index]);
                const std::array<const std::array<float,3>*,5> fields{&p.position,&p.velocity,&p.angles,&p.spin,&p.origin};
                for(unsigned field=0;field<5;++field)for(unsigned i=0;i<3;++i)
                    equal(memory.readFloat(at+20+12*field+4*i),(*fields[field])[i],"Full leaf record");
                equal(memory.read32(at+80),modelBase+p.model*64,"Leaf model ordering");equal(memory.read32(at+84),p.remaining,"Leaf remaining");
                const auto matrix=originalActorMatrix(p.position,p.angles,trig);
                for(unsigned i=0;i<16;++i)equal(draws[index].elements[i],matrix.elements[i],"Leaf draw transform");
                at=memory.read32(at+4);++index;
            }
            equal(index,unsigned(system.order().size()),"Leaf list length");
            full+=index==capacity;drained+=tick>210&&index==0;refilled+=tick==230&&index>0;drawCount+=index;++cases;
        }
    }
    if(!full||!drained||refilled!=4||!drawCount)throw std::runtime_error("Leaf lifecycle coverage incomplete");
    std::cout<<"Leaf lifecycle coverage full="<<full<<" drained="<<drained<<" refilled="<<refilled<<" draws="<<drawCount<<'\n';
}
