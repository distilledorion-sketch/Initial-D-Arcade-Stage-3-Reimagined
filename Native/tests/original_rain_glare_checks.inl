// Included in the rain source verifier, sharing its canonical image and counters.
{
    constexpr unsigned camera=owner+0x30000,collision=0xcaae2a0;
    const auto makeChunk=[&](unsigned chunk,unsigned variant){
        makeRainGlareChunk(m,chunk,variant);
    };
    const auto checkMaterial=[&](unsigned chunk,unsigned variant,unsigned alpha){
        std::array<unsigned,8> ich{};std::array<unsigned,16> gmp{};
        ich[2]=0x840020c0+variant;ich[4]=0x940004c0+variant;
        originalRainGlareMaterial(ich,gmp,alpha);
        equal(m.read32(chunk+328),ich[2],"Glare primary TSP");equal(m.read32(chunk+336),ich[4],"Glare secondary TSP");
        equal(m.read32(chunk+268),gmp[3],"Glare primary diffuse");equal(m.read32(chunk+276),gmp[5],"Glare secondary diffuse");
    };
    unsigned accepted=0,zeroAlpha=0,rejected=0,collisionCalls=0;
    for(unsigned trial=0;trial<768;++trial){
        Random random{next()};setup(m,random.state);m.zeroRegion(camera,512);m.zeroRegion(collision,256);
        m.write32(owner+272,camera);makeChunk(material,trial%64);
        auto view=originalIdentityMatrix();translateOriginalMatrix(view,{float(int(trial%13)-6),-2.f,3.f});
        rotateOriginalMatrixPhase(view,1,std::uint16_t((trial%7)*1700),fsca);
        rotateOriginalMatrixPhase(view,0,std::uint16_t((trial%3)*711),fsca);
        auto world=originalIdentityMatrix();translateOriginalMatrix(world,{float(int(next()%300)-150)/10.f,1.f,-float(next()%1000)/10.f});
        if(trial%19==0)world.elements[13]=300.f; // Explicit one-plane rejection.
        rotateOriginalMatrixPhase(world,1,std::uint16_t(32768+(trial%11)*1900),fsca);
        rotateOriginalMatrixPhase(world,2,std::uint16_t((trial%4)*167),fsca);put(m,matrix,world);
        const std::array<float,3> plane{0,-.70710677f,-.70710677f};put(m,camera+192,plane);
        std::array<float,3> anchor{trial%2?.73f:-.73f,.35f,-1.5f};put(m,motionAt,anchor);
        auto inverse=view;invertOriginalMatrix(inverse);
        std::array<float,3> eye{inverse.elements[12],inverse.elements[13],inverse.elements[14]};
        if(trial%23==0)eye[0]+=150;put(m,scratch,eye);
        const unsigned level=trial%4,hitMode=trial%7;
        const OriginalRainGlareHit hit{hitMode!=0,hitMode==1?0x8000u:0x12u};
        std::vector<std::array<float,3>> sourceStarts,sourceEyes;
        unsigned draws=0;OriginalMatrix sourceDraw;unsigned sourceAlpha=0;
        RefCpu cpu(m);cpu.r[4]=owner;cpu.r[5]=matrix;cpu.r[6]=motionAt;cpu.r[7]=scratch;cpu.r[15]=stack;cpu.pr=stop;
        m.write32(stack,material);m.write32(stack+4,level);
        for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
        cpu.callHooks[0xc022d20]=[&](auto& c){
            equal(c.r[4],collision,"Original glare collision owner");
            std::array<float,3> from{},to{};for(unsigned axis=0;axis<3;++axis){from[axis]=m.readFloat(collision+32+axis*4);to[axis]=m.readFloat(collision+44+axis*4);}
            sourceStarts.push_back(from);sourceEyes.push_back(to);c.r[0]=hit.hit?1:0;m.write32(collision+28,hit.flags);
        };
        cpu.callHooks[0xc1d7120]=[&](auto& c){
            equal(c.r[4],material,"Glare authored model");++draws;sourceAlpha=m.read32(material+268)>>24;
            for(unsigned i=0;i<16;++i)sourceDraw.elements[i]=std::bit_cast<float>(c.xf[i]);
        };
        instructions+=cpu.run(0xc17f260,stop,20000);
        unsigned queries=0;const auto native=originalRainGlare(view,plane,world,anchor,eye,level,[&](const auto& from,const auto& to){
            for(unsigned axis=0;axis<3;++axis){equal(sourceStarts.at(queries)[axis],from[axis],"Glare query start");equal(sourceEyes.at(queries)[axis],to[axis],"Glare query end");}
            ++queries;return hit;
        },random);
        equal(unsigned(sourceStarts.size()),queries,"Glare collision query count");collisionCalls+=queries;
        eqPoint(m,motionAt,native.position,"Glare mutable world position");equal(draws,unsigned(native.draw),"Glare draw admission");
        if(native.draw){++accepted;if(!native.alpha)++zeroAlpha;equal(sourceAlpha,native.alpha,"Glare opacity");
            checkMaterial(material,trial%64,native.alpha);
            for(unsigned i=0;i<16;++i)equal(sourceDraw.elements[i],native.viewMatrix.elements[i],"Glare draw matrix");
        }else ++rejected;
        equal(m.read32(0xc37c778),random.state,"Glare RNG");
        for(unsigned i=0;i<16;++i)equal(cpu.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Glare caller matrix restored");++cases;
    }
    if(accepted<50||zeroAlpha<10||rejected<50||collisionCalls<100)throw std::runtime_error("Insufficient glare branch coverage");
    // Execute complete17F500 across all35 cars. Use original anchor records,
    // real virtual matrix access, actor fallback, inverse camera, material
    // walker and17F260; only model lookup, swept collision and final draw hook.
    unsigned rearDraws=0,emptyPasses=0;
    for(unsigned trial=0;trial<35*24;++trial){
        Random random{next()};setup(m,random.state);m.zeroRegion(owner+0x20000,0x11000);m.zeroRegion(collision,256);
        m.write32(owner+272,camera);m.write32(owner+64,refs);m.write32(owner+68,refs+4);
        const unsigned night=(trial/35)%3;m.write32(owner+56,night);
        const std::array<bool,2> allowed{trial%5!=0,trial%7!=0};
        std::array<OriginalRainGlareCar,2> cars;
        for(unsigned car=0;car<2;++car){
            auto& input=cars[car];const unsigned id=(trial+car*17)%35,at=carsAt+car*0x2000,actor=actorsAt+car*0x800;
            input.available=car==0||trial%4!=0;input.blindAttack=(trial/7+car)%2;input.braking=(trial/5+car)%2;
            m.write32(refs+4*car,input.available?at:0);m.write32(at+0x34c,id);m.write8(at+80,input.braking?255:0);
            m.write32(at+0x95c,actor);m.write32(at+0x960,trial%2?actor+256:0);
            m.write32(actor+80,trial%2?0x10000:input.blindAttack?0x10000:0);m.write32(actor+256+80,input.blindAttack?0x10000:0);
            m.write32(at+0x964+64,0xc380b8c);
            const unsigned record=0xc32437c+id*16,anchorAt=m.read32(record);input.rearCount=m.read32(record+8);
            for(unsigned lamp=0;lamp<input.rearCount;++lamp)for(unsigned axis=0;axis<3;++axis)
                input.rearAnchors[lamp][axis]=m.readFloat(anchorAt+lamp*12+axis*4);
            input.world=originalIdentityMatrix();translateOriginalMatrix(input.world,{float(car)*3,float(trial%3)*.1f,-10.f-float(trial%41)});
            rotateOriginalMatrixPhase(input.world,1,std::uint16_t(32768+(trial%9)*2500+car*4000),fsca);
            put(m,at+0x964,input.world);makeChunk(material+car*1024,car);
        }
        auto view=originalIdentityMatrix();translateOriginalMatrix(view,{-2.f,-1.8f,-float(trial%5)});
        rotateOriginalMatrixPhase(view,1,std::uint16_t((trial%7)*1500),fsca);
        const std::array<float,3> plane{0,-.70710677f,-.70710677f};put(m,camera+192,plane);
        struct Draw {unsigned chunk,alpha;OriginalMatrix matrix;};std::vector<Draw> draws;
        std::vector<std::array<float,3>> starts,ends;
        RefCpu cpu(m);cpu.r[4]=owner;cpu.r[5]=allowed[0];cpu.r[6]=allowed[1];cpu.r[15]=stack;cpu.pr=stop;
        for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(view.elements[i]);
        unsigned currentChunk=0;
        cpu.callHooks[0xc05a8e0]=[&](auto& c){currentChunk=c.r[5];if(currentChunk<7||currentChunk>8)throw std::runtime_error("Unexpected glare chunk");c.r[0]=material+(currentChunk-7)*1024;};
        const auto queryResult=[&](unsigned index){return OriginalRainGlareHit{(trial+index)%13!=0,(trial+index)%11==0?0x8000u:0u};};
        cpu.callHooks[0xc022d20]=[&](auto& c){
            const auto hit=queryResult(unsigned(starts.size()));std::array<float,3> a{},b{};
            for(unsigned axis=0;axis<3;++axis){a[axis]=m.readFloat(collision+32+axis*4);b[axis]=m.readFloat(collision+44+axis*4);}
            starts.push_back(a);ends.push_back(b);c.r[0]=hit.hit?1:0;m.write32(collision+28,hit.flags);
        };
        cpu.callHooks[0xc1d7120]=[&](auto& c){
            equal(c.r[4],material+(currentChunk-7)*1024,"Rear pass selected chunk");Draw out{currentChunk,m.read32(c.r[4]+268)>>24,{}};
            for(unsigned i=0;i<16;++i)out.matrix.elements[i]=std::bit_cast<float>(c.xf[i]);draws.push_back(out);
        };
        instructions+=cpu.run(0xc17f500,stop,80000);
        unsigned queryIndex=0,drawIndex=0;
        originalRainRearGlare(view,plane,night,cars,allowed,[&](const auto& a,const auto& b){
            for(unsigned axis=0;axis<3;++axis){equal(starts.at(queryIndex)[axis],a[axis],"Rear pass ray start");equal(ends.at(queryIndex)[axis],b[axis],"Rear pass ray end");}
            return queryResult(queryIndex++);
        },random,[&](unsigned car,unsigned,unsigned chunk,const OriginalRainGlareDraw& out){
            const auto& expected=draws.at(drawIndex++);equal(expected.chunk,chunk,"Rear pass draw order");equal(expected.alpha,out.alpha,"Rear pass opacity");
            equal(chunk,7+car,"Rear pass car chunk");
            for(unsigned i=0;i<16;++i)equal(expected.matrix.elements[i],out.viewMatrix.elements[i],"Rear pass draw matrix");
        });
        equal(unsigned(starts.size()),queryIndex,"Rear pass query count");equal(unsigned(draws.size()),drawIndex,"Rear pass draw count");
        equal(m.read32(0xc37c778),random.state,"Rear pass shared RNG");
        for(unsigned i=0;i<16;++i)equal(cpu.xf[i],std::bit_cast<unsigned>(view.elements[i]),"Rear pass caller matrix restored");
        rearDraws+=drawIndex;if(!drawIndex)++emptyPasses;++cases;
    }
    if(rearDraws<200||emptyPasses<30)throw std::runtime_error("Insufficient rear-pass coverage");
    std::cout<<"Glare coverage: "<<accepted<<" issued / "<<zeroAlpha<<" zero-alpha / "<<rejected<<" rejected / "<<collisionCalls<<" queries; rear pass "<<rearDraws<<" draws / "<<emptyPasses<<" empty passes.\n";
}
