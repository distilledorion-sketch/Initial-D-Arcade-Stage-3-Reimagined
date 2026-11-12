{
    check(app->endingHeadlightGlare.active(),"Night ending omitted optical headlight owner");
    check(app->endingHeadlightGlareTextureBase==app->endingLeafTextureBase+
        (app->endingLeaves.active()?app->endingLeaves.textures().size():0),"Ending glare texture bank overlaps leaves");
    const auto owner=app->endingHeadlightGlare;
    const std::array<unsigned,2> cars{unsigned(app->frontend.car),unsigned(app->loadedRivalCar)};
    const auto append=[&](const auto& frame,const auto& camera,bool rival=true){
        Mesh mesh;const auto count=app->endingHeadlightGlare.append(mesh,frame,cars,rival,camera,app->endingHeadlightGlareTextureBase);
        for(const auto& range:mesh.ranges){
            check(range.emissive&&range.original&&range.viewMask==1&&!range.courseLighting&&!range.carLighting,
                "Ending glare inherited body lighting or another camera");
            check(range.texture>=app->endingHeadlightGlareTextureBase&&range.texture<app->endingHeadlightGlareTextureBase+4,
                "Ending glare references the wrong texture bank");
        }
        return count;
    };
    // Both recorded cars, independent light bits, pop-up state and visibility.
    // Arrange only the viewing camera for these admission cases.
    for(unsigned slot=0;slot<2;++slot){
        auto frame=shown;frame.cars[1-slot].visible=false;
        auto& car=frame.cars[slot];car.visible=true;car.lights=true;car.headlights.fraction=1;
        OriginalEndingCamera camera;camera.reset(car.body+forward(car.yaw)*12+Vec3{0,.7f,0},car.body,1);
        check(append(frame,camera)==2,"Ending lost one car's optical headlights");
        if(slot)check(append(frame,camera,false)==0,"Hidden ending rival still glows");
        car.lights=false;check(append(frame,camera)==0,"Recorded blind attack still glows");car.lights=true;
        car.headlights.fraction=0;check(append(frame,camera)==0,"Recorded closed pop-ups still glow");car.headlights.fraction=1;
        car.visible=false;check(append(frame,camera)==0,"Invisible recorded car still glows");car.visible=true;
        camera.reset(car.body-forward(car.yaw)*12+Vec3{0,.7f,0},car.body,1);
        check(append(frame,camera)==0,"Ending headlights glow through the back of the car");
    }
    auto disabled=owner;disabled.begin(root,false);Mesh day;
    check(!disabled.active()&&!disabled.append(day,shown,cars,true,app->endingCamera,0),"Daytime ending admitted glare");
    disabled=owner;disabled.reset();
    check(!disabled.append(day,shown,cars,true,app->endingCamera,0),"Ending reset retained glare");
    const auto captureGlare=[&](const char* suffix){
        check(app->renderEnding(0),"Ending glare render");
        const auto path=out/(std::string("ending-glare-")+(rain?"wet-":"dry-")+suffix+".bmp");
        check(app->renderer.saveBitmap(path.wstring()),"Ending glare readback");
        std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};file.read(reinterpret_cast<char*>(&header),sizeof header);file.seekg(header.bfOffBits);
        std::vector<unsigned> pixels(std::size_t(app->renderer.width)*app->renderer.height);
        file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(file),"Ending glare image extent");return pixels;
    };
    // Preserve the authored camera, then inspect the same recorded car poses
    // from in front. The normal sample600 camera sees their backs, where the
    // lens adapter correctly submits no glow; this is a visibility fixture,
    // not a claim that the source ending uses this inspection viewpoint.
    const auto authoredCamera=app->endingCamera;
    app->endingCamera.reset(shown.cars[0].body+forward(shown.cars[0].yaw)*12+Vec3{0,.7f,0},shown.cars[0].body,1);
    const auto seed=app->endingRandom,frame=app->ending.timeline.totalFrame;
    const auto on=captureGlare("on");app->endingHeadlightGlare.reset();const auto off=captureGlare("off");
    app->endingHeadlightGlare=owner;check(captureGlare("restored")==on,"Restored optical headlights changed pixels");
    unsigned changed=0;for(unsigned i=0;i<on.size();++i)changed+=(on[i]&0xffffffu)!=(off[i]&0xffffffu);
    std::cout<<"Ending optical headlights rain="<<rain<<" frame="<<frame<<" lamps="<<append(shown,app->endingCamera)
        <<" changed RGB pixels="<<changed<<'\n'<<std::flush;
    check(changed>8,"Ending optical headlights do not reach rendered pixels");
    check(seed==app->endingRandom&&frame==app->ending.timeline.totalFrame&&app->originalSession.rollbackDigest()==nightPhysics,
        "Optical headlight repaint advanced ending or live physics");
    app->endingCamera=authoredCamera;
}
