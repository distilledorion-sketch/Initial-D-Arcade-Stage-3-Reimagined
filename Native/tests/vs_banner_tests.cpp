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
    for(const auto& p:banner.metadataPlacements())check(p.left>=0&&p.top>=0&&p.left+p.width<=640&&p.top+p.height<=60,"Compact header outside original title bounds");
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
            unsigned white=0;
            for(int y=std::max(0,int(top+p.top*fit));y<std::min(height,int(std::ceil(top+(p.top+p.height+2)*fit)));++y)
                for(int x=std::max(0,int(left+p.left*fit));x<std::min(width,int(std::ceil(left+(p.left+p.width)*fit)));++x){
                    const auto color=pixels[std::size_t(y)*width+x];
                    if((color>>24)>0&&(color&255)>180&&((color>>8)&255)>180&&((color>>16)&255)>180)++white;
                }
            check(white>50,"Original-font record row produced no readable white glyphs at tested aspect ratio");
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
}

void importedHeaders(OriginalVsBanner& banner,const std::filesystem::path& root){
    const auto titles=NativeTextureBank::load(root/"data/original_assets/hud/imported_start_names/titles.idastex");
    for(unsigned course=0;course<2;++course)for(unsigned direction=0;direction<2;++direction)
    for(bool night:{false,true})for(bool wet:{false,true}){
        OriginalVsBannerSetup setup;setup.course=course;setup.direction=direction;
        setup.night=night;setup.wet=wet;setup.compactHeader=true;setup.drawBackdrop=false;setup.showVersus=false;
        banner.begin(setup);const auto source=banner.metadataPlacements();
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

int main(int argc, char** argv) try {
    const std::filesystem::path root = argc > 1 ? argv[1] : ".";
    check(OriginalVsBanner::available(root), "Start banner assets are not imported");
    OriginalVsBanner banner;
    banner.load(root);

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
    importedHeaders(banner,root);
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
