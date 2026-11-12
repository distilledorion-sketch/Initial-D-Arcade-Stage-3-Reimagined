{
    app->endingReplay=footage;app->beginEnding();
    check(app->endingFlare.active()&&!app->endingFlare.visible(),"Day/dry ending flare admission/reset");
    unsigned selected=0,changed=0;
    const auto capture=[&](const std::string& name){
        check(app->renderEnding(0),"Ending flare frame");
        const auto path=out/(name+".bmp");check(app->renderer.saveBitmap(path.wstring()),"Ending flare readback");
        std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};
        file.read(reinterpret_cast<char*>(&header),sizeof header);file.seekg(header.bfOffBits);
        std::vector<unsigned> pixels(std::size_t(app->renderer.width)*app->renderer.height);
        file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(file),"Ending flare bitmap extent");return pixels;
    };
    for(unsigned tick=1;tick<=5000&&app->endingActive;++tick){
        app->advanceEnding(1./60);
        if(tick<180||tick%30||!app->endingFlare.visible()||app->ending.timeline.finalCard)continue;
        const auto saved=app->endingFlare;const auto seed=app->endingRandom;
        const auto on=capture("ending-flare-search-on");
        app->endingFlare.reset();const auto off=capture("ending-flare-search-off");app->endingFlare=saved;
        changed=0;for(unsigned i=0;i<on.size();++i)changed+=on[i]!=off[i];
        check(app->endingRandom==seed,"Flare draw consumed presentation RNG");
        if(changed>100){selected=tick;break;}
    }
    check(selected>0,"Natural daylight ending produced no visible flare pixels");
    const auto seed=app->endingRandom;const auto saved=app->endingFlare;
    const auto state=saved.matrices();
    for(const auto size:std::array<std::array<unsigned,2>,3>{{{960,720},{1280,720},{1920,1080}}}){
        check(app->renderer.resize(size[0],size[1]),"Flare output resize");
        const std::string prefix="ending-flare-"+std::to_string(size[0])+"x"+std::to_string(size[1]);
        const auto on=capture(prefix+"-on");app->endingFlare.reset();const auto off=capture(prefix+"-off");
        app->endingFlare=saved;const auto restored=capture(prefix+"-restored");
        check(on==restored,"Flare repaint did not reproduce pixels");
        unsigned count=0;for(unsigned i=0;i<on.size();++i)count+=on[i]!=off[i];
        check(count>100,"Flare disappeared after output resize");
        Mesh flare;check(app->endingFlare.append(flare,app->endingFlareTextureBase)==11,"Ending omitted authored flare pieces");
        for(const auto& range:flare.ranges){
            check(range.viewMask==1&&range.original&&range.emissive&&!range.courseLighting&&!range.carLighting,"Flare camera/material scope");
            check(range.isp==0x93000000u&&(range.tsp==0x9400245bu||range.tsp==0x94002452u),"Flare replaced authored depth/blend words");
            check(range.texture>=app->endingFlareTextureBase&&range.texture<app->endingFlareTextureBase+7,"Flare texture bank overlap");
        }
        for(const auto& vertex:flare.vertices)check(std::isfinite(vertex.position.x)&&std::isfinite(vertex.position.y)&&std::isfinite(vertex.position.z),"Nonfinite flare geometry");
        check(std::memcmp(state.data(),app->endingFlare.matrices().data(),sizeof(state))==0&&app->endingRandom==seed,"Flare repaint changed state/RNG");
        std::cout<<"Flare frame="<<selected<<" output="<<size[0]<<'x'<<size[1]<<" changed="<<count<<'\n'<<std::flush;
    }
    check(app->renderer.resize(960,720),"Restore ending test output");
    app->paused=true;app->advanceEnding(1);app->paused=false;
    check(std::memcmp(state.data(),app->endingFlare.matrices().data(),sizeof(state))==0&&app->endingRandom==seed,"Paused flare advanced");
    for(auto condition:std::array<std::array<unsigned,3>,4>{{{0,1,0},{0,0,1},{8,0,0},{9,0,0}}}){
        auto gated=saved;gated.begin(root,condition[0],condition[1]!=0,condition[2]!=0,false);Mesh none;
        check(!gated.active()&&!gated.visible()&&gated.append(none,0)==0,"Flare leaked into night/wet/unsupported course");
    }
    auto gated=saved;gated.advance(-1,app->endingCamera,*app->courseLightFsca);Mesh none;
    check(!gated.visible()&&gated.append(none,0)==0,"Hidden road retained previous flare");
    check(app->originalSession.rollbackDigest()==physics&&app->battleProfile.words==before,"Flare altered completed race/save");
}
