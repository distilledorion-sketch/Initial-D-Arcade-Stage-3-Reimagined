#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();const fs::path output=argv[2];fs::create_directories(output);
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.audio.configure(app.root);app.audio.scene(true,false,false);app.load();
    if(!app.renderer.initialize(nullptr,1280,720,false))throw std::runtime_error(app.renderer.error);
    std::ofstream report(output/"application.csv");report<<"child,frame,stream,vertices\n";
    auto draw=[&]{if(!app.render(0))throw std::runtime_error(app.renderer.error);};
    auto capture=[&](unsigned child,unsigned frame){
        unsigned guard=0;
        while((app.frontend.attractChild()!=child||app.frontend.attractFrame()<frame)&&guard++<30000)app.frontend.advance(1./60.);
        if(app.frontend.attractChild()!=child||app.frontend.attractFrame()!=frame)throw std::runtime_error("Capture source frame was skipped");
        draw();const auto name="child"+std::to_string(child)+"-"+std::to_string(frame)+".bmp";
        if(!app.renderer.saveBitmap((output/name).wstring()))throw std::runtime_error(app.renderer.error);
        const auto stream=app.audio.attractStatistics().stream;
        if(child==4&&frame>=32&&frame<390&&stream!=17)throw std::runtime_error("Application did not start original logo track");
        if(child==7&&stream!=11)throw std::runtime_error("Application did not start original opening track");
        report<<child<<','<<frame<<','<<stream<<','<<(app.demoPresentation&&child==7?app.demoPresentation->mesh(app.frontend.demoData(),app.frontend.demoCursor(),frame?frame-1:0).vertices.size():0)<<'\n';report.flush();
    };
    draw();capture(3,60);capture(4,31);capture(4,90);capture(4,170);capture(4,240);capture(4,380);capture(5,60);capture(6,30);
    std::ofstream timing(output/"frame-times.csv");timing<<"source_frame,width,height,mean_cpu_ms,p95_cpu_ms,mean_gpu_ms\n";
    for(unsigned frame:{0u,250u,500u,535u,700u,1200u,1450u,1800u,2240u,2800u,3500u,4600u,5400u,5650u,5800u}){
        capture(7,frame);
        if(frame==700||frame==1800||frame==3500||frame==5400){
            std::vector<double> samples;double gpu=0;app.renderer.measureGpuFrame=true;
            for(unsigned i=0;i<24;++i){const auto begin=std::chrono::steady_clock::now();
                if(!app.render(1./60.))throw std::runtime_error(app.renderer.error);
                const double cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                double ms=0;if(!app.renderer.readGpuMilliseconds(ms))throw std::runtime_error(app.renderer.error);
                if(i>=4){samples.push_back(cpu);gpu+=ms;}
            }
            app.renderer.measureGpuFrame=false;double total=0;for(double value:samples)total+=value;std::sort(samples.begin(),samples.end());
            timing<<frame<<",1280,720,"<<total/samples.size()<<','<<samples[18]<<','<<gpu/samples.size()<<'\n';timing.flush();
        }
    }
    // Pausing must freeze the opening sample cursor; resume must retain it.
    const auto musicFrame=app.audio.attractStatistics().frame;app.audio.scene(true,false,true);
    for(unsigned i=0;i<1024;++i)app.audio.renderStereo(800,0,0,0,false);
    if(app.audio.attractStatistics().frame!=musicFrame)throw std::runtime_error("Paused intro advanced audio");app.audio.scene(true,false,false);
    capture(8,100);capture(11,1100);capture(12,150);capture(12,330);
    app.frontend.confirm();for(unsigned i=0;i<4;++i){if(!app.render(1./60.))throw std::runtime_error(app.renderer.error);}
    if(app.frontend.stage!=FrontendStage::Make||app.audio.attractStatistics().stream!=-1)throw std::runtime_error("Start left attract presentation/audio active");
    app.renderer.saveBitmap((output/"start-to-selection.bmp").wstring());
    app.frontend.stage=FrontendStage::Car;draw();app.renderer.saveBitmap((output/"car-selection-after-attract.bmp").wstring());
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;app.courseIndex=3;app.reverse=false;app.night=false;app.wet=false;app.start();
    for(unsigned tick=0;tick<360;++tick)app.simulate({});draw();
    if(app.menu||app.drivingView!=OriginalDrivingView::Bumper)throw std::runtime_error("Game failed after attract exit");
    app.renderer.saveBitmap((output/"play-after-attract.bmp").wstring());
    std::cout<<"PASS actual application all8 attract owners, original logo/opening audio dispatch, authored moving scene, pause, Start, car selection and bumper gameplay. Headless captures; no driver saves or audio device opened.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
