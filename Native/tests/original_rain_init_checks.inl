{
    //0202C0's successful allocation tail clears its entire payload through
    //020160. Prove this before using zero-filled allocation fixtures below.
    for(unsigned bytes:{280u,2300u,1200u,216u,2592u}){
        setup(m,1);for(unsigned i=0;i<bytes;++i)m.write8(records+32+i,0xa5);
        RefCpu zero(m);zero.r[8]=records;zero.r[9]=bytes+32;zero.r[15]=stack;
        instructions+=zero.run(0xc020374,0xc02038c,bytes*6+100);
        for(unsigned i=0;i<bytes;++i)equal(unsigned(m.read8(records+32+i)),0u,"Allocator clears rain state");++cases;
    }
    constexpr unsigned refsAt=owner+0x21000,carAt=owner+0x22000,bank=owner+0x30000,context=owner+0x31000;
    for(unsigned trial=0;trial<12;++trial){
        setup(m,trial+1);m.zeroRegion(owner+0x20000,0x40000);m.zeroRegion(0xcaae2a0,256);m.write32(0xc2f4b94,0);
        std::array<OriginalRainCar,2> cars;
        for(unsigned car=0;car<2;++car){auto& input=cars[car];input.available=car==0||trial%2;
            input.world=originalIdentityMatrix();translateOriginalMatrix(input.world,{float(trial+car),2.f,-float(trial)*3.f});
            rotateOriginalMatrixPhase(input.world,1,std::uint16_t(trial*2003+car*16384),fsca);
            const unsigned at=carAt+car*0x2000;m.write32(refsAt+car*4,input.available?at:0);
            put(m,at+0x964,input.world);m.write32(at+0x964+64,0xc380b8c);
        }
        for(unsigned chunk=0;chunk<3;++chunk)makeRainGlareChunk(m,bank+0x1000+chunk*1024,chunk);
        unsigned heap=owner+0x40000;std::vector<unsigned> allocations;
        RefCpu cpu(m);cpu.r[4]=owner;cpu.r[5]=refsAt;cpu.r[6]=refsAt+4;cpu.r[7]=trial%3;cpu.r[15]=stack;cpu.pr=stop;
        const unsigned camera=owner+0x2f000,preserve=trial%4;m.write32(stack,camera);m.write32(stack+4,preserve);
        cpu.callHooks[0xc221fc0]=[&](auto& c){c.r[0]=context;}; // Exception-context service only.
        const auto allocate=[&](auto& c){const unsigned bytes=c.r[4];allocations.push_back(bytes);c.r[0]=heap;heap+=(bytes+63)&~63u;if(heap>=owner+0x60000)throw std::runtime_error("Rain fixture heap limit");};
        cpu.callHooks[0xc021f00]=allocate;cpu.callHooks[0xc021e80]=allocate;cpu.callHooks[0xc021ee0]=allocate;
        cpu.callHooks[0xc021960]=[&](auto& c){c.r[0]=bank;}; // Model-bank allocation/decoding boundary.
        cpu.callHooks[0xc057920]=[&](auto& c){c.r[0]=bank;};
        cpu.callHooks[0xc05a8e0]=[&](auto& c){if(c.r[5]<6||c.r[5]>8)throw std::runtime_error("Unexpected rain constructor chunk");c.r[0]=bank+0x1000+(c.r[5]-6)*1024;};
        cpu.callHooks[0xc1d8a60]=[](auto&){};
        instructions+=cpu.run(0xc17e0e0,stop,100000);
        OriginalRainOwner native;native.reset(cars,preserve);
        equal(m.read32(owner+64),refsAt,"Rain primary binding");equal(m.read32(owner+68),refsAt+4,"Rain rival binding");
        equal(m.read32(owner+56),trial%3,"Rain night binding");equal(m.read32(owner+272),camera,"Rain view binding");
        equal(m.read32(owner+276),native.preserveStreakMotion,"Rain motion mode");
        eqPoint(m,owner+248,native.previousVisualPosition,"Initial visual history");eqPoint(m,owner+260,native.streakMotion,"Initial streak motion");
        for(unsigned car=0;car<2;++car){eqPoint(m,owner+72+car*12,native.previousActorPosition[car],"Initial raw actor history");
            eqPoint(m,owner+96+car*12,native.actorMotion[car],"Initial actor motion");
            if(cars[car].available)for(unsigned i=0;i<16;++i)equal(m.readFloat(owner+120+car*64+i*4),native.previousWorld[car].elements[i],"Initial car matrix");}
        for(unsigned i=0;i<25;++i)eqStreak(m,m.read32(owner+24)+i*92,native.fields.streaks[i]);
        for(unsigned i=0;i<9;++i)eqSplash(m,m.read32(owner+40)+i*24,native.fields.splashes[i]);
        for(unsigned i=0;i<72;++i)eqMist(m,m.read32(owner+48)+i*36,native.fields.wheelMist[i]);
        for(unsigned offset:{36u,44u,52u})equal(m.read32(owner+offset),0u,"Initial rain pool cursor");
        for(unsigned chunk=0;chunk<3;++chunk){const auto at=bank+0x1000+chunk*1024;const unsigned alpha=chunk?80:56;
            equal(m.read32(at+268),(alpha<<24)|0xffffffu,"Initial weather alpha");equal(m.read32(at+276),(alpha<<24)|0xffffffu,"Initial weather second alpha");
            equal(m.read32(at+328),(0x840020c0+chunk)|0x001000c0,"Initial weather material flags");}
        equal(m.read32(0xc37c778),trial+1,"Rain construction consumes no random draws");
        if(allocations.size()!=5)throw std::runtime_error("Unexpected rain allocations");++cases;
    }
    //E2CA8 supplies the ending's effect-manager arguments;17D560 stores
    //them. The rain admission block forwards parent+84/+92, NOT fields
    //inside the view object. Replay rain preserves zero streak motion.
    for(unsigned trial=0;trial<4;++trial){
        setup(m,17+trial);m.zeroRegion(owner+0x20000,0x40000);
        constexpr unsigned replay=owner+0x22000,frame=owner+0x25000,manager=owner+0x28000;
        const unsigned primary=carAt,rival=trial&1?carAt+0x2000:0,night=trial>>1;
        m.write32(frame+0x184,replay);m.write32(frame+0x1bc,manager);
        m.write32(replay+0x2a0,primary);m.write32(replay+0x2a4,rival);
        m.write32(replay+0xf0,night);m.write32(replay+0xf4,1);m.write32(replay+0xe8,3);
        RefCpu bind(m);bind.r[14]=frame;bind.r[15]=stack;bind.r[2]=trial&1;
        unsigned admissions=0;
        bind.callHooks[0xc17d6c0]=[&](auto& c){equal(c.r[4],manager,"Ending effect manager binding");++admissions;};
        instructions+=bind.run(0xc0e2ca8,0xc0e2cda,1000);
        equal(admissions,1u,"Ending constructs effect manager once");
        equal(m.read32(manager+76),primary,"Ending primary car binding");
        equal(m.read32(manager+80),rival,"Ending optional rival binding");
        equal(m.read32(manager+84),replay+0x180,"Ending embedded view binding");
        equal(m.read32(manager+92),1u,"Ending preserves rain streak motion");
        equal(m.read32(manager+4),night,"Ending rain night setting");
        equal(m.read32(manager+88),1u,"Ending rain weather setting");
        m.write32(frame+0x120,manager);m.write32(frame+0x170,manager+64);m.write32(frame+0x13c,owner);
        RefCpu admission(m);admission.r[14]=frame;admission.r[15]=stack;
        unsigned constructors=0;
        admission.callHooks[0xc17e0e0]=[&](auto& c){
            equal(c.r[4],owner,"Admitted rain owner");equal(c.r[5],manager+76,"Admitted primary reference");
            equal(c.r[6],manager+80,"Admitted rival reference");equal(c.r[7],night,"Admitted night setting");
            equal(m.read32(c.r[15]),replay+0x180,"Admitted source view");
            equal(m.read32(c.r[15]+4),1u,"Admitted replay streak mode");c.r[0]=owner;++constructors;
        };
        instructions+=admission.run(0xc17d8ea,0xc17d90e,1000);
        equal(constructors,1u,"One rain constructor at the admission boundary");++cases;
    }
}
