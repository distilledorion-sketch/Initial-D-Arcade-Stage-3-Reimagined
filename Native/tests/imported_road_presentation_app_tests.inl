// Exercise the presentation through real source driving in private saves.
int runImportedRoadPresentationAppTests(App& app,bool wetCornersOnly=false){
    std::ofstream log(app.saveRoot.parent_path()/"imported-road-presentation.txt");
    unsigned checks=0;
    const auto check=[&](bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);};
    app.validationMode=true;app.replayPlaybackActive=false;app.multiplayer.active=false;
    for(unsigned route=0;route<6;++route){
        if(wetCornersOnly&&route<4)continue;
        const int course=route==0?3:route==1?9:10;
        app.paused=false;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=app.courseIndex=course;app.frontend.car=route>=4?1:0;app.frontend.automatic=true;
        app.frontend.reverse=app.reverse=route==3||route==5;
        app.frontend.night=app.night=false;app.frontend.wet=app.wet=route>=4;
        app.start();app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;app.menu=false;
        double rawEnergy=0,filteredEnergy=0;float lastRaw=0,lastFiltered=0,steer=0,topSpeed=0;
        unsigned samples=0,missing=0,ticks=0;bool prior=false;
        const bool fullDrive=course==10;
        if(fullDrive){
            const auto& imported=*app.importedCourse;const auto gates=imported.routeCheckpoints(app.reverse);
            const int first=app.reverse?int(imported.center.size())-1-gates[0]:gates[0];
            const int last=app.reverse?int(imported.center.size())-1-gates[4]:gates[4];
            const float start=imported.source.cumulative[first],finish=imported.source.cumulative[last];
            const float distance=std::abs(finish-start),direction=app.reverse?-1.f:1.f;
            unsigned surveyed=0;float largestStep=0;
            for(float lane:{-.65f,0.f,.65f}){
                original::OriginalCollisionQuery q;original::clearOriginalCollisionQuery(q);
                original::OriginalTriangleSearchTrace search;original::OriginalSurfaceScratch scratch;
                ImportedRoadPresentation noisy,quiet;float lastHeight=0;bool hadHeight=false;
                const unsigned steps=unsigned(std::ceil(distance/.5f));
                for(unsigned step=0;step<=steps;++step){
                    const auto p=imported.source.sample(start+direction*std::min(step*.5f,distance));
                    auto point=lerp(p.center,lane<0?p.left:p.right,std::abs(lane));
                    q.setf(32,point.x);q.setf(36,point.y+.2f);q.setf(40,point.z);
                    check(original::queryOriginalCollisionSurface(app.presentedSession().collision(),q,search,scratch),"Missing road contact in full Sadamine survey");
                    const float height=q.f(16);const Vec3 normal{q.f(0),q.f(4),q.f(8)};
                    check(std::isfinite(height)&&normal.y>.6f,"Invalid ground normal in full Sadamine survey");
                    if(hadHeight)largestStep=std::max(largestStep,std::abs(height-lastHeight));lastHeight=height;hadHeight=true;
                    const float yaw=std::atan2(p.tangent.x*direction,p.tangent.z*direction),noise=.18f*std::sin(step*1.31f);
                    point.y=height+.02f+noise;noisy.anchor(point,noise*2,noise,height,normal,yaw,true,1./60);
                    point.y=height+.02f;quiet.anchor(point,0,0,height,normal,yaw,true,1./60);
                    check(noisy.ready()&&quiet.ready(),"Ground anchoring missing from a Sadamine section");
                    check(noisy.position().y==height+.02f,"Vertical chatter survived on the surveyed road");
                    check(noisy.pitch()==quiet.pitch()&&noisy.roll()==quiet.roll(),"Angular chatter survived on the surveyed road");
                    ++surveyed;
                }
            }
            log<<"full_road_survey reverse "<<app.reverse<<" length_m "<<distance<<" lane_samples "<<surveyed<<" max_half_metre_height_step "<<largestStep<<" retained_solver_jitter 0\n";log.flush();
            check(largestStep<.5f,"Large road-height seam in Sadamine survey");
        }
        std::ofstream trace(app.saveRoot.parent_path()/("road-presentation-"+std::to_string(route)+".csv"));
        trace<<"tick,roadY,rawY,presentedY,rawPitch,presentedPitch,eligible,speed,wallContact,normalY,surfaceFound,progress\n";
        for(unsigned frame=0;frame<(fullDrive?36000u:course==10?1800u:360u);++frame){
            if(app.race.phase==RacePhase::Finished)break;
            if(fullDrive&&app.originalRace.state().remaining.value<60000u){
                auto timer=app.originalRace.state();timer.remaining.value+=360000u;app.originalRace.restoreNumericalState(timer);
            }
            DriverInput input;input.automatic=true;input.throttle=1;
            if(app.race.phase==RacePhase::Running){
                const auto p=app.projectRacePosition(app.vehicle.position);
                const float look=std::max(6.f,app.vehicle.speed*.35f);
                const auto aim=app.sampleRaceDistance(p.sample.distance+look).center-app.vehicle.position;
                const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
                const float demand=std::clamp(3.5f*std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-1.f,1.f);
                steer+=std::clamp(demand-steer,-.07f,.07f);input.steer=-steer;
                if(fullDrive){
                    const float limit=wetCornersOnly?38.f:24.f;
                    float target=limit;
                    for(float offset:{8.f,18.f,32.f,50.f})target=std::min(target,std::sqrt((wetCornersOnly?7.f:4.5f)/std::max(.001f,std::abs(app.sampleRaceDistance(p.sample.distance+offset).curvature))));
                    target=std::clamp(target,10.f,limit);
                    input.throttle=std::clamp((target-app.vehicle.speed)*.6f,0.f,1.f);input.brake=std::clamp((app.vehicle.speed-target)*.3f,0.f,.8f);
                }
            }
            app.simulate(input);
            ++ticks;
            const auto& d=app.presentedSession().vehicle().drive;
            check(length(app.vehicle.position-Vec3{d.f(0),d.f(4),d.f(8)})<.00001f,"Presentation changed physics actor position");
            const auto& pose=app.importedRoadPresentation;
            if(course==3)check(!pose.ready(),"Imported correction leaked onto an original course");
            else{
                const float road=app.playerBody.query().f(16),raw=app.vehicle.position.y-road,filtered=pose.position().y-road;
                const bool sample=pose.ready()&&app.vehicle.speedKmh()>20&&!app.vehicle.wallContact;
                if(sample&&prior){rawEnergy+=(raw-lastRaw)*(raw-lastRaw);filteredEnergy+=(filtered-lastFiltered)*(filtered-lastFiltered);++samples;}
                prior=sample;lastRaw=raw;lastFiltered=filtered;
                trace<<frame<<','<<road<<','<<app.vehicle.position.y<<','<<pose.position().y<<','<<-d.f(0x0C)<<','<<app.bodyPitch<<','<<pose.ready()<<','<<app.vehicle.speedKmh()<<','<<app.vehicle.wallContact<<','<<app.playerBody.query().f(4)<<','<<app.playerBody.surfaceFound()<<','<<app.originalRace.state().progress.index<<'\n';
                if(pose.ready())check(pose.position().x==app.vehicle.position.x&&pose.position().z==app.vehicle.position.z,"Road presentation changed horizontal position");
                if(course==10&&pose.ready()){
                    check(pose.position().y==road+.02f,"Sadamine retained simulated vertical bounce");
                    const auto& q=app.playerBody.query();
                    const auto expected=Vec3{app.vehicle.position.x,road,app.vehicle.position.z}+Vec3{q.f(0),q.f(4),q.f(8)}*originalCarRideHeight(unsigned(app.frontend.car));
                    check(length(app.playerBodyWorld-expected)==0,"Displayed Sadamine body retained solver height");
                }
                if(course==10&&!pose.ready())++missing;
            }
            topSpeed=std::max(topSpeed,app.vehicle.speedKmh());
            if(frame%(fullDrive?600:120)==0){
                const auto words=d.words;
                auto chase=app.originalCamera,bumper=app.bumperCamera;
                const Vec3 anchor=pose.ready()?pose.position():app.vehicle.position;
                const Vec3 angles{pose.ready()?-app.bodyPitch:d.f(0x0C),d.f(0x10),pose.ready()?-app.bodyRoll:d.f(0x14)};
                const auto expectedChase=chase.update(anchor,angles),expectedBumper=bumper.update(anchor,angles);
                const auto savedChase=app.originalCamera,savedBumper=app.bumperCamera;
                app.advanceOriginalCamera();
                check(length(expectedChase.eye-app.originalCamera.frame().eye)<.00001f,"Chase camera bypassed corrected pose");
                check(length(expectedBumper.eye-app.bumperCamera.frame().eye)<.00001f,"Bumper camera bypassed corrected pose");
                app.originalCamera=savedChase;app.bumperCamera=savedBumper;
                for(auto view:{OriginalDrivingView::Chase,OriginalDrivingView::Bumper,OriginalDrivingView::Natural}){
                    app.drivingView=view;Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
                    check(app.render(1./120),"Road presentation frame failed");
                    check(words==d.words,"Presentation/render changed original driving state");
                }
            }
        }
        log<<"course "<<course<<" reverse "<<app.reverse<<" wet "<<app.wet<<" ticks "<<ticks<<" finished "<<(app.race.phase==RacePhase::Finished)<<" progress "<<app.originalRace.state().progress.index<<" goal "<<app.originalRace.rules().goalIndex<<" unanchored "<<missing<<" samples "<<samples<<" top_kmh "<<topSpeed
           <<" residual_step_RMS_ratio "<<(rawEnergy>0?std::sqrt(filteredEnergy/rawEnergy):0)<<'\n';log.flush();
        if(course==10){check(samples>300,"Sadamine drive did not exercise moving road contact");check(filteredEnergy<rawEnergy*.00001,"Real Sadamine contact chatter was retained");}
        if(fullDrive)check(app.race.phase==RacePhase::Finished&&!app.race.timeUp,"Sadamine driving did not reach the finish");
    }
    app.paused=true;app.drivingView=OriginalDrivingView::Chase;
    Idas3UiBeginFrame(app.renderer.width,app.renderer.height);check(app.render(0),"Final Sadamine capture failed");
    log<<"PASS "<<checks<<" actual App driving/presentation checks. Source driving words unchanged by all three camera render paths.\n";
    return 0;
}
