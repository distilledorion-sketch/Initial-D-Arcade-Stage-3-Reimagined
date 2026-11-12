#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/roadside_dust_race_fixture.inl"
#include <iostream>

// Read-only investigation: can existing IDR2 pose data reproduce the actual
// published dust contacts? It never substitutes candidate data into gameplay.
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    const auto geometry=original::OriginalPhysicsData::load(root/"data/original_physics/tables.bin").carRecord(0);
    const auto scalar=[&](unsigned i){return std::bit_cast<float>(geometry[i]);};
    const std::array<std::array<float,3>,4> corners{{{scalar(0),-1,scalar(2)},{-scalar(0),-1,scalar(2)},
        {scalar(1),-1,-scalar(3)},{-scalar(1),-1,-scalar(3)}}};
    struct Pose{Vec3 position;float yaw,pitch,roll;};
    const auto pose=[&](){return Pose{app->vehicle.position,app->vehicle.yaw,app->bodyPitch,app->bodyRoll};};
    unsigned cases=0;std::array<unsigned,3> mismatch{};unsigned displacementMismatch=0;
    std::ofstream csv(out/"mismatches.csv");csv<<"direction,path,tick,sourceMask,currentPose,previousPose,previousPosePriorTilt,displacementEqual\n";
    for(unsigned direction=0;direction<2;++direction)for(unsigned path:{300u,500u,1000u,1600u,2000u,2380u,2460u,2600u}){
        prepareRoadsideDustRaceFixture(*app,direction,path,path==2460?0.f:.55f);
        Pose previous=pose(),prior=previous;
        std::array<std::array<original::OriginalCollisionQuery,4>,3> queries{};
        for(auto& candidate:queries)for(auto& q:candidate)original::clearOriginalCollisionQuery(q);
        for(unsigned tick=0;tick<360;++tick){
            app->simulate(roadsideDustDiagnosticInput(tick));const auto current=pose();
            const auto& actual=app->presentedSession().publishedActors().player0C8FF388;
            unsigned mask=0;for(unsigned wheel=0;wheel<4;++wheel)if((actual[29]>>(wheel*8))&32u)mask|=1u<<wheel;
            const std::array<Pose,3> candidates{current,previous,Pose{previous.position,previous.yaw,prior.pitch,prior.roll}};
            std::array<unsigned,3> masks{};
            for(unsigned candidate=0;candidate<3;++candidate){
                const auto& p=candidates[candidate];
                const auto matrix=original::originalActorMatrix({p.position.x,p.position.y,p.position.z},
                    {-p.pitch,wrapAngle(p.yaw-pi),-p.roll},*app->courseLightFsca);
                original::OriginalTriangleSearchTrace trace;original::OriginalSurfaceScratch scratch;
                for(unsigned wheel=0;wheel<4;++wheel){
                    const auto point=original::transformOriginalPoint(matrix,corners[wheel]);auto& q=queries[candidate][wheel];
                    for(unsigned axis=0;axis<3;++axis)q.setf(32+axis*4,point[axis]);
                    original::queryOriginalCollisionSurface(app->presentedSession().collision(),q,trace,scratch);
                    if(q.u(28)&32u)masks[candidate]|=1u<<wheel;
                }
                if(tick>1)mismatch[candidate]+=masks[candidate]!=mask;
            }
            const auto delta=current.position-previous.position;
            const bool same=std::bit_cast<unsigned>(delta.x)==actual[3]&&std::bit_cast<unsigned>(delta.y)==actual[4]&&std::bit_cast<unsigned>(delta.z)==actual[5];
            if(tick>1){++cases;displacementMismatch+=!same;
                if(masks[0]!=mask||masks[1]!=mask||masks[2]!=mask||!same)
                    csv<<direction<<','<<path<<','<<tick<<','<<mask<<','<<masks[0]<<','<<masks[1]<<','<<masks[2]<<','<<same<<'\n';}
            prior=previous;previous=current;
        }
    }
    std::ofstream report(out/"result.txt");
    report<<"frames="<<cases<<" contact mismatches(current,previous,prior-tilt)="<<mismatch[0]<<','<<mismatch[1]<<','<<mismatch[2]
        <<" source displacement mismatches="<<displacementMismatch<<"\nExisting replay data only; no source contact/physics modified.\n";
    std::cout<<"Frames "<<cases<<" contact mismatches "<<mismatch[0]<<','<<mismatch[1]<<','<<mismatch[2]<<" displacement mismatches "<<displacementMismatch<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
