#include "original_vs_banner.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace idas3;

namespace {
unsigned checks = 0;
void check(bool value, const std::string& why) { ++checks; if (!value) throw std::runtime_error(why); }
std::string list(const std::vector<int>& chunks) {
    std::string out;
    for (const auto chunk : chunks) out += (out.empty() ? "" : " ") + std::to_string(chunk);
    return out;
}
void battleRecords(OriginalVsBanner& banner,const std::filesystem::path& root){
    OriginalVsBannerSetup setup;setup.drawBackdrop=false;
    setup.localNameUtf8="PLAYER";setup.opponentNameUtf8="RIVAL";
    setup.battles={400,99};setup.wins={217,85};
    banner.begin(setup);
    check(banner.displayBattleRecord(0).empty()&&banner.displayBattleRecord(1).empty(),"Offline default unexpectedly enables record rows");
    const auto offlineChunks=banner.chunks();
    for(unsigned i=0;i<152;++i)banner.tick();
    std::vector<std::uint32_t> offline(640*480),unchanged(640*480);
    banner.paint(offline,640,480);
    setup.battles={};setup.wins={};banner.begin(setup);
    for(unsigned i=0;i<152;++i)banner.tick();banner.paint(unchanged,640,480);
    check(offline==unchanged&&banner.chunks()==offlineChunks,"Unused online records changed offline banner pixels");

    setup.compactHeader=true;setup.showBattleRecords=true;banner.begin(setup);
    check(!banner.metadataPlacements().empty(),"Online compact header missing course and conditions");
    // Localized Myogi's source quad has 0.402px of transparent top padding.
    const float headerTop=originalUiLanguage()==1?-.5f:0.f;
    for(const auto& p:banner.metadataPlacements())check(p.left>=0&&p.top>=headerTop&&p.left+p.width<=640&&p.top+p.height<=60,"Compact header outside original title bounds");
    setup.compactHeader=false;
    setup.showBattleRecords=true;setup.battles={400,99};setup.wins={217,85};banner.begin(setup);
    check(banner.displayBattleRecord(0)=="400 BATTLE(S)  217 WIN(S)  54.2%","400/217 screenshot record must truncate54.25 to54.2");
    check(banner.displayBattleRecord(1)=="99 BATTLE(S)  85 WIN(S)  85.8%","99/85 screenshot record must truncate to85.8");
    check(list(banner.chunks())=="119"&&banner.metadataPlacements().empty(),"Online record layout retained unrelated course/race/weather header");
    std::vector<std::uint32_t> hidden(640*480);banner.paint(hidden,640,480);
    check(std::all_of(hidden.begin(),hidden.end(),[](auto p){return p==0;}),"Record rows appeared before source name animation");
    for(unsigned i=0;i<152;++i)banner.tick();
    const std::array<OriginalVsBattleRecordPlacement,2> settled{banner.battleRecordPlacement(0),banner.battleRecordPlacement(1)};
    // Independent read of the already exported source31EE90 motion table.
    // Both stats rows must follow the corresponding name's staggered start,
    // signed translations and quantized alpha on every owner tick.
    std::array<std::array<float,3>,80> motion{};
    std::ifstream source(root/"data/original_assets/hud/start_names/start_names.bin",std::ios::binary);
    source.seekg(-std::streamoff(sizeof(motion)),std::ios::end);
    source.read(reinterpret_cast<char*>(motion.data()),sizeof(motion));
    check(bool(source),"Cannot read independent source name-motion fixture");
    banner.begin(setup);
    for(unsigned tick=0;tick<=160;++tick){
        for(unsigned side=0;side<2;++side){
            const auto index=tick<23?0u:side?std::min(tick>54?tick-54:0u,79u):std::min(tick-23,79u);
            const auto& sample=motion[index];const float sign=side?-1.f:1.f;
            const float alpha=tick<23?0.f:float(std::clamp(int(sample[2]*255.f),0,255))/255.f;
            const auto p=banner.battleRecordPlacement(side);
            check(std::abs(p.left-(settled[side].left+sign*sample[0]*1.2f))<.0001f&&
                  std::abs(p.top-(settled[side].top+sign*sample[1]))<.0001f&&p.opacity==alpha,
                  "Record row detached from source name translation/alpha at tick"+std::to_string(tick));
        }
        banner.tick();
    }
    for(const auto size:std::array<std::array<int,2>,3>{{{640,480},{1280,720},{480,640}}}){
        const int width=size[0],height=size[1];std::vector<std::uint32_t> pixels(std::size_t(width)*height);
        banner.paint(pixels,width,height);
        const float fit=std::min(float(width)/640,float(height)/480);
        const float left=(width-640*fit)*.5f,top=(height-480*fit)*.5f;
        for(unsigned side=0;side<2;++side){
            const auto p=banner.battleRecordPlacement(side);
            check(p.left>=16&&p.left+p.width<=624&&p.top>100&&p.top+p.height<480&&p.opacity==1,"Settled record row left the640x480 safe area");
            unsigned white=0,green=0,lavender=0;
            for(int y=std::max(0,int(top+p.top*fit));y<std::min(height,int(std::ceil(top+(p.top+p.height+2)*fit)));++y)
                for(int x=std::max(0,int(left+p.left*fit));x<std::min(width,int(std::ceil(left+(p.left+p.width)*fit)));++x){
                    const auto color=pixels[std::size_t(y)*width+x];
                    if((color>>24)>0&&(color&255)>180&&((color>>8)&255)>180&&((color>>16)&255)>180)++white;
                    if((color>>24)>0&&((color>>8)&255)>180&&(color&255)<80&&((color>>16)&255)<80)++green;
                    if((color>>24)>0&&((color>>16)&255)>100&&int(color&255)-int((color>>16)&255)>20)++lavender;
                }
            check((originalUiLanguage()==1?green:white)>50,"Original stat row lost its language's numeric ink at tested aspect ratio");
            check(lavender>20,"Battle stats lost their original lavender atlas gradient");
        }
    }
    setup.battles={0,99};setup.wins={17,100};banner.begin(setup);
    check(banner.displayBattleRecord(0)=="0 BATTLE(S)  0 WIN(S)  0.0%","Zero battles must display zero wins and0.0percent");
    check(banner.displayBattleRecord(1)=="99 BATTLE(S)  99 WIN(S)  100.0%","Wins above battles were not clamped");
    setup.battles={std::numeric_limits<std::uint32_t>::max(),std::numeric_limits<std::uint32_t>::max()};
    setup.wins={std::numeric_limits<std::uint32_t>::max(),std::numeric_limits<std::uint32_t>::max()-1};
    setup.localNameUtf8=setup.opponentNameUtf8=std::string(32,'W');banner.begin(setup);
    check(banner.displayBattleRecord(0)=="4294967295 BATTLE(S)  4294967295 WIN(S)  100.0%"&&
          banner.displayBattleRecord(1)=="4294967295 BATTLE(S)  4294967294 WIN(S)  99.9%","Large profile counters overflowed/truncated incorrectly");
    for(unsigned i=0;i<152;++i)banner.tick();
    for(unsigned side=0;side<2;++side){const auto p=banner.battleRecordPlacement(side);
        check(p.width<=412.001f&&p.left>=16&&p.left+p.width<=624,"Large counters or long names exceed their side's safe area");}
    setup.showVersus=false;banner.begin(setup);for(unsigned i=0;i<160;++i)banner.tick();
    std::fill(hidden.begin(),hidden.end(),0);banner.paint(hidden,640,480);
    check(std::all_of(hidden.begin(),hidden.end(),[](auto p){return p==0;}),"Disabled VS still painted online record rows");
}

void recordColumnCapture(OriginalVsBanner& banner){
    const bool japanese=originalUiLanguage()==1;
    // Independent bounds from the original stat-label quads: English83 and
    // Japan Rev C82. The Japanese import adds a30.5px anchor translation,
    // which must not move the label columns relative to the separate digits.
    const std::vector<std::array<float,2>> labels=japanese?
        std::vector<std::array<float,2>>{{254,274},{191,219},{170,198},{278,306},{135,163},{54,82}}:
        std::vector<std::array<float,2>>{{315,335},{339,367},{207,273},{54,154}};
    OriginalVsBannerSetup setup;setup.drawBackdrop=false;setup.showBattleRecords=true;
    setup.localNameUtf8="PLAYER";setup.opponentNameUtf8="RIVAL";
    for(const auto values:std::array<std::array<std::uint32_t,2>,5>{{{400,217},{99,100},{0,17},{4294967295u,4294967294u},{10000,5000}}}){
        setup.battles={values[0],values[0]};setup.wins={values[1],values[1]};banner.begin(setup);
        for(unsigned i=0;i<152;++i)banner.tick();
        const auto battles=values[0],wins=std::min(values[0],values[1]);
        const float extraBattles=18.f*std::max(0,int(std::to_string(battles).size())-3);
        const float extraWins=18.f*std::max(0,int(std::to_string(wins).size())-3);
        const float natural=(japanese?306.f:367.f)+extraBattles+extraWins;
        const float scale=std::min(1.f,412.f/natural);
        std::vector<std::array<float,2>> expected;
        for(const auto& label:labels){const auto shift=extraBattles+(label[0]>(japanese?100.f:200.f)?extraWins:0.f);expected.push_back({label[0]+shift,label[1]+shift});}
        auto number=[&](std::uint32_t value,float right,float advance){
            const auto digits=std::to_string(value);float x=right-float(digits.size())*advance;
            for(char digit:digits){(void)digit;expected.push_back({x,x+20});x+=advance;}
        };
        number(battles,54+extraBattles,18);
        number(wins,(japanese?135.f:207.f)+extraBattles+extraWins,18);
        const auto tenths=battles?std::uint64_t(wins)*1000/battles:0;
        number(unsigned(tenths/10),(japanese?254.f:315.f)+extraBattles+extraWins,14);
        number(unsigned(tenths%10),(japanese?280.f:341.f)+extraBattles+extraWins,18);
        Idas3UiEnable(1);Idas3UiBeginFrame(640,480);
        std::vector<std::uint32_t> pixels(640*480);unityUiClear(pixels.data(),640,480);
        banner.paint(pixels,640,480);unityUiSubmit(pixels.data(),640,480,false,false,true);
        UnityUiFrame frame{sizeof(frame)};check(Idas3UiGetFrame(&frame)==1,"Missing battle record capture");
        std::vector<UnityUiVertex> vertices(frame.vertexCount);
        check(Idas3UiCopyVertices(vertices.data(),int(vertices.size()))==int(vertices.size()),"Missing record geometry");
        for(unsigned side=0;side<2;++side){
            const auto row=banner.battleRecordPlacement(side);
            check(std::abs(row.width-natural*scale)<.001f,"Stat row width did not follow localized labels");
            std::vector<std::array<float,2>> actual;
            for(std::size_t i=0;i+5<vertices.size();i+=6){
                float left=INFINITY,right=-INFINITY,top=INFINITY,bottom=-INFINITY;
                for(unsigned j=0;j<6;++j){const auto& v=vertices[i+j];left=std::min(left,v.x);right=std::max(right,v.x);top=std::min(top,v.y);bottom=std::max(bottom,v.y);}
                if(top>=row.top-1&&bottom<=row.top+row.height+2)actual.push_back({(left-row.left)/scale,(right-row.left)/scale});
            }
            check(actual.size()==expected.size(),"Stat labels or digits missing from Unity capture");
            for(std::size_t i=0;i<expected.size();++i)check(std::abs(actual[i][0]-expected[i][0])<.002f&&std::abs(actual[i][1]-expected[i][1])<.002f,
                "Localized stat column overlaps or separates from its number at quad "+std::to_string(i));
        }
        Idas3UiEnable(0);
    }
}
}

void importedHeaders(OriginalVsBanner& banner,const std::filesystem::path& root){
    const auto titles=NativeTextureBank::load(root/"data/original_assets/hud/imported_start_names/titles.idastex");
    for(unsigned course=0;course<2;++course)for(unsigned direction=0;direction<2;++direction)
    for(bool night:{false,true})for(bool wet:{false,true}){
        OriginalVsBannerSetup setup;setup.course=course;setup.direction=direction;
        setup.night=night;setup.wet=wet;setup.compactHeader=true;setup.drawBackdrop=false;setup.showVersus=false;
        // Imported titles deliberately use the Irohazaka header baseline,
        // regardless of the original course used for their handling.
        auto reference=setup;reference.course=5;banner.begin(reference);
        const auto source=banner.metadataPlacements();
        float firstHeight=0;
        for(unsigned name=0;name<2;++name){
            setup.customCourseName=name?"SADAMINE":"HAKONE";banner.begin(setup);
            const auto title=banner.importedTitlePlacement();const auto& texture=titles.at(name);
            check(title.height>=35&&title.height<=55,"Imported title no longer matches D3 ink height");
            check(std::abs(title.width/title.height-float(texture.width)/texture.height)<.0001f,"Imported title aspect ratio distorted");
            if(!name)firstHeight=title.height;else check(title.height==firstHeight,"Longer imported name was shrunk");
            float shift=0;bool first=true;
            for(const auto& box:banner.metadataPlacements()){
                const auto original=std::find_if(source.begin(),source.end(),[&](const auto& p){return p.chunk==box.chunk;});
                check(original!=source.end(),"Imported header changed condition texture");
                check(box.top==original->top&&box.height==original->height&&box.width==original->width,"Imported condition scale/baseline differs from original");
                if(first){shift=box.left-original->left;first=false;check(box.left>title.left+title.width,"Condition overlaps imported title");}
                else check(std::abs(box.left-original->left-shift)<.0001f,"Imported condition spacing changed");
                check(box.left+box.width<=640&&box.top+box.height<=55,"Imported condition clipped");
            }
        }
    }
}
void boostWarning(OriginalVsBanner& banner,const std::filesystem::path& output){
    OriginalVsBannerSetup setup;setup.drawBackdrop=false;setup.compactHeader=true;
    setup.showBattleRecords=true;setup.localNameUtf8="PLAYER";setup.opponentNameUtf8="RIVAL";
    const auto count=[&](int chunk){const auto draws=banner.chunks();return std::count(draws.begin(),draws.end(),chunk);};
    banner.begin(setup);check(!count(81)&&!count(82),"Boost warning appeared with boost enabled");
    setup.boostOff=true;banner.begin(setup);
    for(unsigned tick=0;tick<=241;++tick){
        check(count(81)==1,"Boost-off base must stay visible throughout introduction");
        // Recovered owner+8 starts at0, reaches21, then resets at phase1.
        const auto counter=tick<22?tick:tick-22;
        check(count(82)==(counter%30<=9?1:0),"Boost warning blink differs from original counter");
        banner.tick();
    }
    setup.showVersus=false;banner.begin(setup);
    check(!count(81)&&!count(82),"Solo introduction displayed boost warning");
    setup.showVersus=true;
    for(const auto size:std::array<std::array<int,2>,4>{{{640,480},{1280,720},{2560,1080},{800,600}}}){
        const int width=size[0],height=size[1];
        const float fit=std::min(width/640.f,height/480.f);
        std::vector<std::uint32_t> enabled(std::size_t(width)*height,0xff34485bu),disabled=enabled;
        setup.boostOff=true;banner.begin(setup);for(unsigned i=0;i<152;++i)banner.tick();
        const auto boxes=banner.metadataPlacements();
        const auto found=std::find_if(boxes.begin(),boxes.end(),[](const auto& p){return p.chunk==81;});
        check(found!=boxes.end(),"Boost warning has no placement");const auto box=*found;
        check(box.left>440&&box.left+box.width<625&&box.top>65&&box.top+box.height<110,
            "Boost warning is outside its original upper-right placement");
        banner.paint(enabled,width,height);
        setup.boostOff=false;banner.begin(setup);for(unsigned i=0;i<152;++i)banner.tick();
        banner.paint(disabled,width,height);
        unsigned changed=0;
        const float left=(width-640*fit)*.5f+box.left*fit,top=(height-480*fit)*.5f+box.top*fit;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(enabled[std::size_t(y)*width+x]!=disabled[std::size_t(y)*width+x]){
            ++changed;check(x>=left-1&&x<=left+box.width*fit+1&&y>=top-1&&y<=top+box.height*fit+1,
                "Boost option moved or repainted another intro element");
        }
        check(changed>300,"Original Boost Off artwork did not render");
        if(!output.empty()&&width==1280){
            std::filesystem::create_directories(output);
            std::ofstream image(output/"boost-off.ppm",std::ios::binary);image<<"P6\n"<<width<<' '<<height<<"\n255\n";
            for(auto pixel:enabled){const char rgb[]{char(pixel>>16),char(pixel>>8),char(pixel)};image.write(rgb,3);}
            check(bool(image),"Could not write Boost Off rendering evidence");
        }
        for(const std::string name:{"HAKONE","ODAWARA","GUNSAI"}){
            setup.customCourseName=name;setup.boostOff=true;banner.begin(setup);
            const auto imported=banner.metadataPlacements();
            const auto warning=std::find_if(imported.begin(),imported.end(),[](const auto& p){return p.chunk==81;});
            check(warning!=imported.end()&&warning->left==box.left&&warning->top==box.top&&
                warning->width==box.width&&warning->height==box.height,"Imported title shifted Boost Off warning");
        }
        setup.customCourseName.clear();
    }
    setup.boostOff=false;banner.begin(setup);check(!count(81)&&!count(82),"Boost warning survived an enabled rematch");
}

int main(int argc, char** argv) try {
    const std::filesystem::path root = argc > 1 ? argv[1] : ".";
    if(argc>3)setOriginalUiLanguage(std::stoi(argv[3]));
    check(OriginalVsBanner::available(root), "Start banner assets are not imported");
    OriginalVsBanner banner;
    banner.load(root);
    // Keep the warning's rule, timing, pixel and aspect checks independently
    // runnable for focused validation, alongside the full localized suite.
    if(argc>4&&std::string(argv[4])=="--boost-only"){
        boostWarning(banner,argv[2]);
        std::cout<<"PASS "<<checks<<" focused Boost Off checks for language "<<originalUiLanguage()<<".\n";
        return 0;
    }

    if(argc>2){
        const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
        OriginalVsBannerSetup preview;preview.drawBackdrop=false;preview.showBattleRecords=true;
        preview.localNameUtf8="JDR99";preview.opponentNameUtf8="PLAYER1";
        preview.battles={11,2};preview.wins={11,0};banner.begin(preview);
        for(unsigned i=0;i<152;++i)banner.tick();
        std::vector<std::uint32_t> pixels(640*480,0xff303030);banner.paint(pixels,640,480);
        std::ofstream image(out/"battle-stats.ppm",std::ios::binary);image<<"P6\n640 480\n255\n";
        for(auto c:pixels){const char rgb[]{char(c>>16),char(c>>8),char(c)};image.write(rgb,3);}
        check(bool(image),"Write original battle-stat preview");
    }

    // Save selection reuses the source name atlas independently of the
    // animated VS owner, including Japanese and the nonsequential digit zero.
    original::OriginalBattleProfile nameProfile{};
    nameProfile.words[76/4]=1;nameProfile.words[44/4]=197;
    check(banner.profileDisplayName(nameProfile)=="0","Saved zero glyph decoded as another digit");
    std::vector<std::uint32_t> japanese(640*480),substitute(640*480),repeat(640*480);
    const auto nameTick=banner.sourceTick();
    banner.paintDisplayName(japanese,640,480,"\xe3\x82\xb1\xe3\x82\xa4",42,51,20,30);
    banner.paintDisplayName(substitute,640,480,"??",42,51,20,30);
    banner.paintDisplayName(repeat,640,480,"\xe3\x82\xb1\xe3\x82\xa4",42,51,20,30);
    check(std::any_of(japanese.begin(),japanese.end(),[](auto p){return p!=0;}),"Saved Japanese name rendered blank");
    check(japanese!=substitute,"Saved Japanese name was replaced with question marks");
    check(japanese==repeat&&banner.sourceTick()==nameTick,"Static saved name rendering mutated banner state");
    bool outsideNameBounds=false;
    for(int y=0;y<480;++y)for(int x=0;x<640;++x)
        if(japanese[std::size_t(y)*640+x]&&(x<42||x>=72||y<51||y>=66))outsideNameBounds=true;
    check(!outsideNameBounds,"Saved name exceeded its fitted menu bounds");

    // The selection is the whole component: every word is its own authored
    // chunk, so getting the banner right is getting these numbers right. Each
    // was read by rendering the chunk on its own.
    struct Case { OriginalVsBannerSetup setup; const char* expected; };
    const Case cases[]{
        // Akina, uphill, day, dry, race 1.
        {{3, 0, true, false, false, false, 1, false}, "120 75 104 94 113 105 106 119"},
        // Happogahara is the game's course four but the bank's sixth name.
        {{4, 1, true, true, true, false, 2, false}, "120 77 104 95 109 112 118 119"},
        // Irohazaka is the game's five and the bank's fifth name; the pair
        // crosses over, and this is the case that catches it.
        {{5, 2, true, false, false, false, 3, false}, "120 76 104 96 114 105 106 119"},
        // Race zero is FINAL and carries no digit; extra wins over both.
        {{0, 3, true, false, false, false, 0, false}, "120 72 110 107 105 106 119"},
        {{8, 6, true, true, false, true, 4, true}, "120 80 116 108 112 0 119"},
        // A caller without the course's direction word leaves it out entirely.
        {{3, 0, false, false, false, false, 1, false, true}, "120 75 104 94 105 106 119"},
        // Laid over the cars the backdrop goes, and only the words remain.
        {{3, 0, true, false, false, false, 1, false, false}, "75 104 94 113 105 106 119"},
    };
    for (const auto& one : cases) {
        banner.begin(one.setup);
        const auto got = list(banner.chunks());
        check(got == one.expected, "Start banner selected " + got + " but should select " +
                                   one.expected);
    }

    // Every course and direction the game can ask for has an authored word,
    // and no two courses share one.
    std::vector<int> courses;
    for (std::uint32_t course = 0; course < 9; ++course) {
        banner.begin({course, 0, true, false, false, false, 1, false});
        courses.push_back(banner.chunks().at(1));
    }
    for (std::size_t a = 0; a < courses.size(); ++a)
        for (std::size_t b = a + 1; b < courses.size(); ++b)
            check(courses[a] != courses[b], "Two courses share a start banner name");
    for (std::uint32_t direction = 0; direction < 7; ++direction) {
        banner.begin({0, direction, true, false, false, false, 1, false});
        const auto words=banner.chunks();
        if(direction==4)check(std::find(words.begin(),words.end(),115)!=words.end(),"Clockwise uses CLOCKWISE asset115");
        if(direction==5)check(std::find(words.begin(),words.end(),111)!=words.end(),"Counterclockwise uses COUNTER-CLOCKWISE asset111");
        // Backdrop, course, RACE, digit, direction, day/night, weather, VS.
        check(banner.chunks().size() == 8, "A direction changed how many words the banner draws");
    }

    // Out of range is refused rather than clamped onto a neighbour's word.
    for (const auto& bad : {OriginalVsBannerSetup{9, 0, true, false, false, false, 1, false},
                            OriginalVsBannerSetup{0, 7, true, false, false, false, 1, false},
                            OriginalVsBannerSetup{0, 0, true, false, false, false, 6, false}}) {
        bool refused = false;
        try { banner.begin(bad); } catch (const std::invalid_argument&) { refused = true; }
        check(refused, "An out-of-range start banner setup was accepted");
    }

    // It paints without reaching outside the canvas it was handed.
    std::vector<std::uint32_t> canvas(640 * 480, 0xff000000u);
    banner.begin({3, 0, true, false, false, false, 1, false});
    banner.paint(canvas, 640, 480);
    std::size_t lit = 0;
    for (const auto pixel : canvas) if ((pixel & 0xffffffu) != 0) ++lit;
    check(lit > 2000, "The start banner painted almost nothing");
    ++checks;
    battleRecords(banner,root);
    recordColumnCapture(banner);
    importedHeaders(banner,root);
    boostWarning(banner,argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});
    // Unity enables command capture before loading assets. Measurements must
    // still produce pixels, without exporting their temporary surfaces/textures.
    Idas3UiEnable(1);Idas3UiBeginFrame(640,480);
    OriginalVsBanner captured;captured.load(root);importedHeaders(captured,root);
    UnityUiFrame frame{sizeof(frame)};
    check(Idas3UiGetFrame(&frame)==1&&frame.textureCount==0&&frame.vertexCount==0&&frame.drawCount==0,
        "Title measurement leaked temporary geometry into Unity capture");
    Idas3UiEnable(0);

    std::cout << "PASS " << checks << " start banner checks: chunk selection for course, "
              << "direction, day/night, weather and race number; the Happogahara/Irohazaka "
              << "cross-over; range refusal; source-timed multiplayer battle/win records, integer percentages, safe-area fitting and unchanged offline rendering; a paint that lit " << lit << " pixels."
              << std::endl;
} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
}
