#include "frontend.h"
#include "menu_font.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace fs=std::filesystem;
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
void bitmap(const fs::path& path,const std::vector<std::uint32_t>& pixels){
    std::ofstream out(path,std::ios::binary);auto u16=[&](unsigned n){out.put(char(n));out.put(char(n>>8));};auto u32=[&](unsigned n){u16(n);u16(n>>16);};
    u16(0x4d42);u32(54+640*480*4);u32(0);u32(54);u32(40);u32(640);u32(unsigned(-480));u16(1);u16(32);u32(0);u32(640*480*4);u32(0);u32(0);u32(0);u32(0);
    out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
}
int main(int argc,char** argv)try{
    check(argc==3,"native root and output folder required");const fs::path root=argv[1],out=argv[2];fs::create_directories(out);
    setOriginalUiLanguage(0);setCustomMenuLanguage(0);Frontend menu;menu.initialize(root);
    menu.stage=FrontendStage::Mode;const auto original=menu.paint(640,480);
    std::vector<std::uint32_t> englishSave;
    for(int language=0;language<3;++language){
        setCustomMenuLanguage(language);check(originalUiLanguage()==0,"Custom language changed arcade artwork setting");
        check(menu.paint(640,480)==original,"Original game-mode artwork changed");
        menu.stage=FrontendStage::SaveSelect;menu.saveFiles[0]={true,"READY","AE86 TRUENO","B1","2026/10/08",3661,9};menu.saveSelected=0;
        menu.saveActionsOpen=true;menu.saveDeleteOpen=false;menu.saveActionSelected=0;
        const auto save=menu.paint(640,480);bitmap(out/("save-"+std::to_string(language)+".bmp"),save);
        if(!language)englishSave=save;else check(save!=englishSave,"Save chrome did not switch language");
        menu.saveActionSelected=2;menu.confirm();check(menu.saveDeleteOpen&&menu.saveDeleteSelected==0,"Delete must still default to No");
        bitmap(out/("delete-"+std::to_string(language)+".bmp"),menu.paint(640,480));
        menu.confirm();check(!menu.saveDeleteOpen&&menu.takeSaveDeleteRequested()<0,"Default confirmation deleted save");
        check(menu.saveFiles[0].name=="READY"&&menu.saveFiles[0].car=="AE86 TRUENO","Language mutated saved identity");
        menu.stage=FrontendStage::Mode;
        const auto font=MenuFont::load(root,language==1?"data/localization/custom-menus/ja.bin":language==2?"data/localization/custom-menus/zh-Hans.bin":"data/native_assets/menu_font/font.bin");
        check(font.ready()&&font.width(customMenuText("ARE YOU SURE?"),25)<330,"Dialog caption clips");
        check(font.width(std::string("\xff\xc0",2),16)>=0,"Malformed UTF-8 failed");
    }
    setOriginalUiLanguage(1);setCustomMenuLanguage(2);check(originalUiLanguage()==1,"Custom Chinese reset Japanese arcade option");
    bool rejected=false;try{setCustomMenuLanguage(3);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&customMenuLanguage()==2,"Invalid language changed native selection");
    setOriginalUiLanguage(0);setCustomMenuLanguage(0);std::cout<<"PASS: three languages, native renders, artwork isolation, unchanged save names, delete defaults and UTF-8\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
