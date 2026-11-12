#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/roadside_dust_race_fixture.inl"
#include <iostream>

std::vector<unsigned> dustState(const RoadsideDustPresentation& dust){
    const auto saved=dust.checkpoint();const auto& s=saved.system;
    std::vector<unsigned> out{saved.random,s.freeParticles(),unsigned(s.order().size())};
    const auto floats=[&](const auto& values){for(float v:values)out.push_back(std::bit_cast<unsigned>(v));};
    for(const auto& car:s.suppressedTireSmoke())for(bool v:car)out.push_back(v);
    for(auto index:s.order()){
        const auto& c=s.cloud(index);const auto& e=c.emitter;
        out.insert(out.end(),{index,c.car,c.effect,c.color,std::bit_cast<unsigned>(c.gain)});floats(c.footprint.elements);
        floats(e.spread);floats(e.position);floats(e.velocity);floats(e.previousPosition);
        floats(std::array{e.scale,e.force,e.lifetimeScale});out.insert(out.end(),{e.stopped,e.frame,e.flags,unsigned(e.particles.size())});
        for(const auto& p:e.particles){floats(p.position);floats(p.velocity);out.insert(out.end(),{std::bit_cast<unsigned>(p.size),p.remaining,p.flags});}
    }
    return out;
}

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    check(app->renderer.initialize(nullptr,960,720,true),"Offscreen renderer failed");
    app->demoPreparationStarted=true;app->replayRecordingFlags=1;
    for(unsigned scenario=0;scenario<4;++scenario){
        prepareRoadsideDustRaceFixture(*app,scenario);
        // Explicitly exercise the retained revision2 writer/playback. New
        // ordinary captures use the combined revision3 controller instead.
        app->recording.beginCapture(true);if(scenario<2)app->recording.beginDustCapture();
        // Exercise a source-owner prefix before the first running clock tick,
        // like countdown updates. The race's clock and input stay untouched.
        for(unsigned tick=0;tick<60;++tick)app->advanceRoadsideDust();
        std::vector<std::vector<unsigned>> expected;bool sawDust=false;
        for(unsigned tick=0;tick<720;++tick){
            app->simulate(roadsideDustDiagnosticInput(tick));
            sawDust|=app->roadsideDust.system().particleCount()>0;expected.push_back(dustState(app->roadsideDust));
        }
        check(scenario<2?sawDust:!sawDust,"Live source dust condition gate");
        const auto bytes=app->recording.sharedBytes(unsigned(app->recording.frames.back().tick*100));
        const auto word=[&](unsigned at){unsigned value;std::memcpy(&value,bytes.data()+at,4);return value;};
        check(bytes.size()>=96,"Recording incomplete");
        check(word(24)==(scenario<2?2u:1u),"Replay discarded source dust or extended an excluded condition");
        if(scenario>=2){check(app->recording.dustFrames.empty(),"Excluded condition allocated dust capture");continue;}
        const auto count=word(8),stride=word(20),dustCount=word(92);
        check(count==720&&stride==164&&dustCount==780,"Capture prefix or source tick extent lost");
        const auto tail=std::span(bytes).subspan(96+count*stride);
        check(tail.size()==dustCount*ReplayRoadsideDustFrame::bytes,"Serialized source input extent");
        check(std::memcmp(tail.data(),app->recording.dustFrames.data(),tail.size())==0,"Captured float bits changed in serialization");
        std::ofstream raw(out/("direction-"+std::to_string(scenario)+".idr2"),std::ios::binary);raw.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());raw.close();
        RoadsideDustPresentation playback;ReplayRoadsideDustPlayback timeline;timeline.load(tail,playback);
        const auto compare=[&](unsigned index){
            timeline.seek(word(96+index*stride+160),playback);
            const auto actual=dustState(playback);
            if(actual!=expected[index]){
                std::cerr<<"state index="<<index<<" step="<<timeline.step()<<" expectedWords="<<expected[index].size()<<" actualWords="<<actual.size()<<'\n';
                for(unsigned i=0;i<std::min(actual.size(),expected[index].size());++i)if(actual[i]!=expected[index][i]){
                    std::cerr<<"first difference word="<<i<<" expected="<<expected[index][i]<<" actual="<<actual[i]<<'\n';break;
                }
            }
            check(actual==expected[index],"Serialized/seeked particles differ from live source state");
        };
        for(unsigned i=0;i<count;++i)compare(i);
        for(unsigned i:{0u,719u,299u,300u,599u,65u,0u,64u,300u,719u,719u})compare(i);
        // Reordered/random seeks cover checkpoint restoration and RNG reuse.
        unsigned random=0x159720;for(unsigned i=0;i<240;++i){random=random*1664525u+1013904223u;compare(random%count);}
        auto bad=std::vector<std::uint8_t>(tail.begin(),tail.end());
        const auto before=dustState(playback);bool rejected=false;bad[0]=4;
        try{timeline.load(bad,playback);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&dustState(playback)==before,"Invalid timeline changed active playback");
        rejected=false;try{timeline.seek(dustCount+1,playback);}catch(const std::out_of_range&){rejected=true;}
        check(rejected&&dustState(playback)==before,"Out-of-range seek changed active playback");
        std::cout<<"scenario="<<scenario<<" frames="<<count<<" sourceTicks="<<dustCount<<" serializedBytes="<<bytes.size()<<'\n';
    }
    // Real replay render: source state, restored recording and no-dust control.
    app->replayRecordingFlags=1;prepareRoadsideDustRaceFixture(*app,0);
    app->recording.beginCapture(true);app->recording.beginDustCapture();
    for(unsigned tick=0;tick<=60;++tick)app->simulate(roadsideDustDiagnosticInput(tick));
    const auto source=app->roadsideDust.checkpoint();const auto digest=app->presentedSession().rollbackDigest();
    const auto bytes=app->recording.sharedBytes(unsigned(app->recording.frames.back().tick*100));
    const auto tail=std::span(bytes).subspan(96+164*app->recording.frames.size());
    app->replayPlaybackActive=true;app->replayDetailed=true;
    const auto render=[&](const char* name){
        check(app->render(0,false),"Replay scene render failed");const auto path=out/(std::string(name)+".bmp");
        check(app->renderer.saveBitmap(path.wstring()),"Replay pixel readback failed");
        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof header);in.seekg(header.bfOffBits);
        std::vector<unsigned> pixels(960*720);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Replay pixels truncated");return pixels;
    };
    const auto expectedPixels=render("source");
    app->replayDust.load(tail,app->roadsideDust);app->replayDust.seek(unsigned(app->recording.dustFrames.size()),app->roadsideDust);
    check(dustState(app->roadsideDust)==dustState([&](){RoadsideDustPresentation d;d.restore(source);return d;}()),"Replay render state mismatch");
    const auto restoredPixels=render("restored");app->roadsideDust.reset();const auto absentPixels=render("absent");
    unsigned changed=0;for(unsigned i=0;i<expectedPixels.size();++i){check(expectedPixels[i]==restoredPixels[i],"Replay restored pixels differ");changed+=expectedPixels[i]!=absentPixels[i];}
    check(changed>100,"Replay still excludes visible roadside dust");
    check(app->presentedSession().rollbackDigest()==digest,"Playback changed live race physics");
    app->replayRecordingFlags=0;prepareRoadsideDustRaceFixture(*app,0);
    for(unsigned tick=0;tick<90;++tick)app->simulate(roadsideDustDiagnosticInput(tick));
    check(app->recording.dustFrames.empty()&&app->roadsideDust.system().particleCount()>0,"Disabling recording changed live dust or kept recording it");
    std::ofstream(out/"PASS.txt")<<checks<<" checks; exact live particles/RNG, serialized timeline, prefix, forward/reverse, seeks, legacy night/rain, optional capture, offscreen replay pixels="<<changed<<". No full Unity viewer or live two-PC claim.\n";
    std::cout<<"PASS "<<checks<<" checks; replay changed pixels="<<changed<<'\n';
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
