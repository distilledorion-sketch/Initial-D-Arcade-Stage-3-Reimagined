#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/imported_road_presentation_app_tests.inl"
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("Native root, Sadamine pack and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[3]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;
    app->settings();app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->sadamineCourseRoot=fs::absolute(argv[2]);app->frontend.enableHakoneCourse(app->sadamineCourseRoot,10);
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error("Camera renderer");
    const auto result=runImportedRoadPresentationAppTests(*app,true);
    std::cout<<"PASS wet Sadamine AE86 Levin driving/camera checks\n";return result;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
