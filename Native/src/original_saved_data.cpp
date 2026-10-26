#include "original_saved_data.h"
#include "original_time_attack_background.h"
#include "original_time_attack_visit.h"
#include "original_record_rules.h"
#include "original_net_password.h"
#include "original_car_color_catalog.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <limits>
namespace idas3::original {
namespace {
constexpr std::array<unsigned,5> ranges{3,3,9,2,2};
}
std::array<bool,16> originalSavedPartFlags(const OriginalBattleProfile& p){
    std::array<bool,16> flags{};const unsigned car=p.u(16),package=p.byte(152);
    for(unsigned slot=0;slot<10;++slot){const auto value=p.byte(156+slot);if(!value)continue;
        switch(slot){
        case 0:flags[(car==0||car==13)&&package==0?3:2]=true;break;
        case 1:flags[car==25?4:0]=true;break;
        case 2:flags[5]=true;break;
        case 3:flags[11]=true;break;
        case 4:flags[(car==32&&value==3)||(car==33&&value==2)||(car==21&&value==3)?1:7]=true;break;
        case 5:flags[car==33&&value==1?10:9]=true;break;
        case 6:flags[6]=true;break;
        case 7:flags[15]=true;break;
        case 9:flags[13]=true;flags[14]=(value&2)!=0;break;
        default:break;
        }
    }return flags;
}
void OriginalSavedData::load(const std::filesystem::path& root){
    const auto base=root/"data/original_assets";
    const auto add=[&](std::string name,const std::filesystem::path& folder){
        Bank bank{NativeModel::load(folder/(name+".idasmesh")),NativeTextureBank::load(folder/"textures/textures.idastex")};
        // Original planar menu geometry is authored in depth order, not file
        // order. Paint the backing before its white rows and lettering.
        for(auto& chunk:bank.model.chunks)std::stable_sort(chunk.batches.begin(),chunk.batches.end(),[](const auto& a,const auto& b){
            const auto z=[](const auto& batch){float sum=0;for(const auto& v:batch.vertices)sum+=v.position.z;return batch.vertices.empty()?0.f:sum/batch.vertices.size();};return z(a)<z(b);});
        banks_.emplace(std::move(name),std::move(bank));
    };
    for(const auto name:{"v3sS10conf","cardcheck","cardcheck2","cardcheck_car","cardfont_a","cardfont_h","cardfont_k"})add(name,base/"saved_data"/name);
    add("select",base/"name_entry/select");add("select0302",base/"name_entry/select0302");
    add("v3sS00common",base/"menus/v3/v3sS00common");
    add("v3sS07tune",base/"tuning_course/v3sS07tune");
    add("shift",base/"hud/shift");
    japanese_=originalUiLanguage()==1;
    const auto fontPath=base/"saved_data/help_font";
    optionFont_=NativeTextureBank::load(fontPath/"textures/textures.idastex");
    std::ifstream map(fontPath/"font.bin",std::ios::binary),spacing(fontPath/"spacing.bin",std::ios::binary);
    map.read(reinterpret_cast<char*>(glyphMap_.data()),sizeof(glyphMap_));
    for(auto& [code,trim]:spacing_){std::uint16_t padding;spacing.read(reinterpret_cast<char*>(&code),2);spacing.read(reinterpret_cast<char*>(&padding),2);spacing.read(reinterpret_cast<char*>(&trim),4);}
    if(!map||!spacing||optionFont_.size()!=36)throw std::runtime_error("Invalid Saved Data help font");
    tuning_=OriginalTuningData::load(root);
}
void OriginalSavedData::begin(OriginalBattleProfile& p){
    state_={};state_.fade464=15;state_.overlay460=1;input_={};background_=0;animationFrame_=0;
    loadOriginalCardCheckOptions(state_,p);
    for(unsigned i=0;i<5;++i)state_.committed1524[i]=std::min(state_.committed1524[i],ranges[i]-1);
    state_.draft1544=state_.committed1524;countdown_=2479;
}
void OriginalSavedData::change(int delta){
    if(!delta||!ready()||state_.page492!=7)return;
    // Desktop steering/Left/Right edits the highlighted row immediately.
    // The cabinet's explicit Confirm-to-edit path remains available; both
    // paths share its draft, cancel and commit handling below.
    if(state_.editPhase1456!=2){
        input_.selectorValue=state_.draft1544[state_.row1460];
        state_.editPhase1456=state_.editing496=2;
    }
    const auto count=int(ranges[state_.row1460]);
    input_.selectorValue=unsigned((int(input_.selectorValue)+delta%count+count)%count);
}
OriginalCardCheckEvents OriginalSavedData::advance(OriginalBattleProfile& p){
    const auto previousRow=state_.row1460,previousEdit=state_.editPhase1456;
    // The cabinet keeps its transient timer in the profile workspace. Keep
    // that workspace local: merely browsing a save must not persist a timer.
    auto workspace=p;workspace.setu(1176,countdown_);
    auto events=tickOriginalCardCheck(state_,workspace,input_);
    countdown_=workspace.u(1176);workspace.setu(1176,p.u(1176));
    events.profileWritten=events.profileWritten&&workspace.words!=p.words;
    p=workspace;
    // This desktop profile does not own the cabinet's physical network-battle
    // page. The reference cycles straight from Time Attack2 to Password1.
    if(state_.page492==4)state_.page492=5;
    if(state_.row1460!=previousRow||state_.editPhase1456!=previousEdit)
        state_.selectedValue1452=state_.draft1544[state_.row1460];
    input_={};input_.selectorValue=state_.selectedValue1452;
    background_=advanceOriginalTimeAttackBackground(background_);++animationFrame_;return events;
}
void OriginalSavedData::paintBackground(std::span<std::uint32_t> pixels,int width,int height)const{
    const float fit=std::min(width/640.f,height/480.f),left=(width-640*fit)*.5f,top=(height-480*fit)*.5f;
    const auto& bank=banks_.at("select");const auto& tile=bank.model.chunks.at(45);
    // Extend the same authored tile grid and displayed phase beyond the4:3
    // canvas. A separately timed attract backdrop creates a moving seam.
    const int extraX=int(std::ceil(left/(254*fit))),extraY=int(std::ceil(top/(254*fit)));
    for(int row=-1-extraY;row<2+extraY;++row)for(int col=-1-extraX;col<3+extraX;++col){
        const auto position=originalTimeAttackBackgroundPosition(background_,col,row);
        SpritePlacement placement;placement.scale=100*fit;placement.invertY=true;placement.authoredHeight=0;
        placement.offsetX=left+position.x*100*fit;placement.offsetY=top-position.y*100*fit;
        placement.defaultOriginalUiColors=true;
        compositeOriginalMenuChunk(pixels,width,height,bank.textures,tile,placement);
    }
}
void OriginalSavedData::paint(std::span<std::uint32_t> pixels,int width,int height,const OriginalBattleProfile& p)const{
    const float fit=std::min(width/640.f,height/480.f),left=(width-640*fit)*.5f,top=(height-480*fit)*.5f;
    const auto render=[&](const Bank& bank,const NativeModelChunk& chunk,float x=0,float y=0){
        SpritePlacement placement;placement.scale=100*fit;placement.invertY=true;placement.authoredHeight=0;
        placement.offsetX=left+x*fit;placement.offsetY=top+y*fit;placement.defaultOriginalUiColors=true;
        compositeOriginalMenuChunk(pixels,width,height,bank.textures,chunk,placement);
    };
    const auto draw=[&](const char* name,unsigned index,float x=0,float y=0){const auto& b=banks_.at(name);render(b,b.model.chunks.at(index),x,y);};
    // Keep authored padding when applying the cabinet's object scale.
    const auto scaled=[&](const char* name,unsigned index,float x,float y,float sx,float sy){const auto& b=banks_.at(name);auto c=b.model.chunks.at(index);
        for(auto& batch:c.batches)for(auto& v:batch.vertices){v.position.x*=sx;v.position.y*=sy;}render(b,c,x,y);};
    const auto placed=[&](const char* name,unsigned index,float x,float y,float w,float h,bool black=false){
        const auto& b=banks_.at(name);auto c=b.model.chunks.at(index);
        float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
        for(const auto& batch:c.batches)for(const auto& v:batch.vertices){minx=std::min(minx,v.position.x);maxx=std::max(maxx,v.position.x);miny=std::min(miny,v.position.y);maxy=std::max(maxy,v.position.y);}
        for(auto& batch:c.batches){if(black){batch.material[2]=0x600;batch.material[3]=batch.material[5]=0xff000000u;}
            for(auto& v:batch.vertices){v.position.x=(v.position.x-minx)/(maxx-minx)*w/100;v.position.y=(v.position.y-maxy)/(maxy-miny)*h/100;if(black){v.color0=0xff000000u;v.color1=0;}}}
        render(b,c,x,y);
    };
    const auto digits=[&](std::string value,float x,float y,float size=16.f){for(char ch:value){if(ch>='0'&&ch<='9')placed("cardcheck",36+ch-'0',x,y,size,size*2);else if(ch!=' ')placed("cardcheck",46,x,y,size,size*2);x+=size;}};
    paintBackground(pixels,width,height);
    draw("v3sS00common",20);
    const unsigned page=state_.page492;
    draw("v3sS10conf",page==7?(state_.editPhase1456==2?24u:state_.editPhase1456==1?25u:26u):22u);
    for(const auto& d:originalTimeAttackCountdownDraws(countdown_))draw("select",d.chunk,d.x,d.y);
    draw("v3sS10conf",14+page,0,32);
    if(page==0){
        const auto length=std::min(p.u(76),5u);float x=386.f-(length?length-1:0)*12.f;
        for(unsigned i=0;i<length;++i,x+=24){const auto glyph=p.u(44+4*i);if(glyph>=220)continue;
            const char* font=glyph<81?"cardfont_k":glyph<162?"cardfont_h":"cardfont_a";
            draw(font,glyph<81?glyph:glyph<162?glyph-81:glyph-162,x,140);
        }
        constexpr std::array<unsigned,7> makers{28,29,31,32,33,34,35};draw("cardcheck",makers.at(std::min(p.u(40),6u)),0,16);
        draw("cardcheck_car",std::min(p.u(16),34u),0,16);draw("cardcheck",48+std::min(p.u(68),1u),0,16);
        const auto rgb=carPaintPalettes.at(std::min(p.u(16),34u)).at(std::min(p.u(64),carPaintCount-1));
        // Paint is now shared across cars: retain the source swatch shape but
        // use the actual selected RGB instead of a misleading stock label.
        // The cabinet swatches include a dark rim around the paint sample.
        const auto fill=[&](int x,int y,int w,int h,unsigned color){if(unityUiEnabled()){unityUiSolid(pixels.data(),width,height,left+x*fit,top+y*fit,w*fit,h*fit,color);return;}
            for(int yy=int(top+y*fit);yy<std::min(height,int(top+(y+h)*fit));++yy)for(int xx=int(left+x*fit);xx<std::min(width,int(left+(x+w)*fit));++xx)if(xx>=0&&yy>=0)pixels[yy*width+xx]=color;};
        fill(382,264,48,22,0xff343434);fill(384,266,44,18,0xff000000u|rgb);
        // AE86 factory paints retain their original contrasting lower body.
        if(p.u(16)<=2&&p.u(64)<originalCarColorCounts[p.u(16)])fill(384,278,44,6,0xff151515);
        const auto points=std::to_string(p.u(72));digits(points,444-float(points.size())*16,339);
        const auto inspection=std::to_string(p.u(1140));digits(inspection,414-float(inspection.size())*16,381);
    }else if(page==1){
        const auto& car=tuning_.car(p.u(16));const unsigned package=std::min<unsigned>(p.byte(152),unsigned(car.packages.size()-1));
        draw("cardcheck",package==car.packages.size()-1?54:50+package,0,32);draw("cardcheck",55+std::min<unsigned>(p.byte(164),5),0,16);
        // Source tuning diagram and installed body-part markers use the same
        // per-car appearance bytes as the showroom and race car.
        draw("v3sS07tune",67,368,223);
        constexpr std::array<unsigned,14> flags{15,6,3,2,11,9,10,7,5,1,4,0,13,14};
        constexpr std::array<unsigned,14> markers{66,58,54,54,63,61,61,59,57,53,56,52,65,65};
        constexpr std::array<unsigned,14> labels{13,12,11,7,8,6,5,4,1,2,3,10,9,9};
        const auto installed=originalSavedPartFlags(p);unsigned row=0;
        for(unsigned i=0;i<flags.size();++i)if(installed[flags[i]]){
            draw("v3sS07tune",markers[i],368,223);
            draw("v3sS10conf",labels[i],108+148*(row/4),326+27*(row%4));++row;
        }
    }else if(page==2||page==3||page==5||page==6){
        constexpr std::array<unsigned,9> courses{0,1,2,3,5,8,4,6,7};const bool password=page>=5,second=page==3||page==6;
        const unsigned first=second?5:0,last=second?9:5;
        for(unsigned row=first;row<last;++row)for(unsigned weather=0;weather<2;++weather)for(unsigned route=0;route<2;++route){
            const auto course=courses[row];if(course==8&&weather==0)continue;
            const auto record=originalPersonalTimeAttackRecord(p,originalRecordPartition(course*2+route,weather!=0));
            const float x=259+weather*192,y=157+(row-first)*45+route*18;
            if(password){if(!record.ticks6000)continue;OriginalNetPasswordInput in;in.course=course;in.direction=route;in.weather=weather;in.night=record.night;in.ticks6000=record.ticks6000;in.car=p.u(16);in.manual=p.u(68);in.salt=p.u(1160);
                for(unsigned i=0;i<5;++i)in.name[i]=i<p.u(76)?std::uint8_t(p.u(44+i*4)):221;
                const auto code=originalNetPassword(in);float xx=224+weather*192;
                // 10DA00 uses a separate 43/17-pixel table and 1.3x glyphs.
                const float yy=160+(row-first)*43+route*17;
                for(unsigned i=0;i<14;++i){if(i==3||i==7||i==11)xx+=6;scaled("cardcheck2",originalNetPasswordGlyph(code[i]),xx,yy,1.3f,1);xx+=8;}
            }else{
                const auto ms=record.ticks6000/6;char text[16];std::snprintf(text,sizeof(text),"%u%02u%03u",ms/60000,(ms/1000)%60,ms%1000);
                constexpr std::array<float,6> offsets{0,17,27,43,53,63};
                for(unsigned i=0;i<6;++i)placed("cardcheck",record.ticks6000?36+text[i]-'0':46,x+offsets[i],y,10,19.2f);
                if(record.ticks6000)scaled("cardcheck",64+unsigned(record.night!=0),330+weather*192,y+3,.62f,.6f);
            }
        }
    }else if(page==7){
        for(unsigned row=0;row<5;++row){
            const unsigned count=ranges[row],value=state_.draft1544[row];
            for(unsigned choice=0;choice<count;++choice){unsigned chunk=0;const bool selected=choice==value;
                if(row==0)chunk=(selected?96:92)+choice;
                if(row==1)chunk=(selected?109:105)+choice;
                if(row==2)chunk=(selected?121:112)+choice;
                if(row==3)chunk=(selected?135:131)+choice;
                if(row==4)chunk=(selected?102:99)+choice;
                draw("cardcheck",chunk,0,32);
            }
        }
        // Match the authored label strips (centers160,194,228,262,296).
        // A32-pixel step drifts8 pixels above Forced Quit on the last row.
        // The Japanese panel's label column is13.168pixels farther right;
        // its shared cursor mesh retains the English authoring coordinates.
        if(state_.editPhase1456)draw("cardcheck",88,japanese_?13.168f:0.f,32+state_.row1460*34);
        if(japanese_){draw("cardcheck",state_.editPhase1456?std::array<unsigned,5>{95,108,130,134,101}[state_.row1460]:104,0,32);}
        else{
            constexpr std::array<const char*,5> help{
                "SET RACE BACKGROUND MUSIC.",
                "SET OPTIONS FOR NAVIGATION WINDOW\nTHAT IS DISPLAYED AT THE LOWER LEFT\nOF THE RACE SCENE.",
                "SET STEERING WHEEL STIFFNESS.",
                "SET TIME DISPLAY OF THE RACE SCENE.",
                "FORCED TERMINATIONS DURING GAME PLAY\nSUCH AS THE LEGEND OF THE STREET AND\nTIME ATTACK ARE AVAILABLE."};
            constexpr std::array<const char*,5> editing{
                "ON   :PLAY MUSIC AS NORMAL(DEFAULT SETTING).\nOFF  :DISABLE MUSIC.\nFIXED:BGM THAT IS SELECTED AT MUSIC\n      SELECTION WILL BE PLAYED.",
                "ON       :DISPLAY NAVIGATION WINDOW.\nOFF      :DISABLE NAVIGATION WINDOW.\nPAPER CUP:DISPLAY PAPER CUP ON\n          NAVIGATION WINDOW.",
                "1to9:1 IS THE LOOSEST. STIFFNESS\n     INCREASES UP TO 9.",
                "NORMAL :STANDARD TIME DISPLAY.\nSIMPLE :SIMPLIFIED TIME DISPLAY.",
                "OFF :DISABLE FORCED TERMINATION.\nON  :ENABLE FORCE TERMINATION. PRESS\n     BOTH START AND CHANGE VIEW BUTTONS\n     SIMULTANEOUSLY TO TERMINATE GAME."};
            const std::string text=state_.editPhase1456==2?editing[state_.row1460]:state_.editPhase1456?help[state_.row1460]:"CHANGE SETTINGS WITH GEAR SHIFT.";
            constexpr float fontSize=16;
            const float start=state_.editPhase1456==2&&(state_.row1460==0||state_.row1460==2)?100.f:state_.editPhase1456?120.f:200.f;float x=start,y=340;
            for(const char ch:text){if(ch=='\n'){x=start;y+=fontSize;continue;}
                const bool punctuation=ch==' '||ch=='.'||ch==':'||ch=='('||ch==')';
                const unsigned a=punctuation?0xa1:0xa3,b=ch==' '?0xa1:ch=='.'?0xa5:ch==':'?0xa7:ch=='('?0xca:ch==')'?0xcb:unsigned(ch)+0x80;
                const unsigned glyph=glyphMap_[(a-160)*96+b-160];
                if(glyph!=0xffff){const float px=left+x*fit,py=top+y*fit,size=fontSize*fit;OriginalSprite s;
                    s.vertices={OriginalSpriteVertex{px,py,0,0,1,0xffffffff,0},{px,py+size,0,0,0,0xffffffff,0},{px+size,py,0,1,1,0xffffffff,0},{px+size,py+size,0,1,0,0xffffffff,0}};
                    compositeOriginalSprite(pixels,width,height,optionFont_.at(glyph),s,{});}
                float trim=0;for(const auto& entry:spacing_)if(entry.first==(a|(b<<8))){trim=entry.second;break;}x=std::fma(fontSize,1-trim,x);
            }
        }
        if(!state_.editPhase1456){
            // Original 113700's 100-frame fore/aft demonstration.
            const unsigned t=animationFrame_%100;const float degrees=t<20?-1.7f*t:t<40?1.7f*(t-20)-35:t<50?0:t<70?1.7f*(t-50):t<90?35-1.7f*(t-70):0;
            const float angle=degrees*3.14159265359f/180,cs=std::cos(angle),sn=std::sin(angle);
            const auto& bank=banks_.at("shift");
            NativeModelChunk icon;
            for(unsigned index:{9u,8u}){auto chunk=bank.model.chunks.at(index);for(auto& batch:chunk.batches){
                const auto diffuse=batch.material[3],specular=batch.material[4];
                const bool lit=(batch.material[2]&0x600)!=0x600;
                for(auto& v:batch.vertices){
                    if(index==8){const float yy=v.position.y,ny=v.normal.y;v.position.y=yy*cs-v.position.z*sn;v.position.z=yy*sn+v.position.z*cs;v.normal.y=ny*cs-v.normal.z*sn;v.normal.z=ny*sn+v.normal.z*cs;}
                    v.position.x*=.9f;v.position.y*=.9f;
                    // The lever is a lit model, not a flat UI sprite. Bake its
                    // diffuse/specular materials into the menu's vertex colors.
                    const auto n=normalize(v.normal),light=normalize(Vec3{-.4f,.65f,1});
                    const float ndl=std::max(0.f,dot(n,light));
                    const float highlight=lit?std::pow(std::max(0.f,2*n.z*dot(n,light)-light.z),float(batch.material[1]&255)):0;
                    unsigned color=0xff000000;
                    for(unsigned shift:{0u,8u,16u}){const float d=float((diffuse>>shift)&255),s=float((specular>>shift)&255);
                        color|=unsigned(std::clamp(lit?d*(.3f+1.4f*ndl)+s*1.4f*highlight:d,0.f,255.f))<<shift;}
                    v.color0=color;v.color1=0;
                }
                batch.material[2]=0x600;batch.material[4]=0;
                // Flat menu composition has no depth buffer: preserve model
                // visibility by sorting individual triangles, not whole strips.
                for(std::size_t i=0;i<batch.indices.size();i+=3){NativeModelBatch triangle;triangle.ich=batch.ich;triangle.material=batch.material;triangle.vertices={batch.vertices[batch.indices[i]],batch.vertices[batch.indices[i+1]],batch.vertices[batch.indices[i+2]]};triangle.indices={0,1,2};icon.batches.push_back(std::move(triangle));}
            }}
            std::stable_sort(icon.batches.begin(),icon.batches.end(),[](const auto& a,const auto& b){return a.vertices[0].position.z+a.vertices[1].position.z+a.vertices[2].position.z<b.vertices[0].position.z+b.vertices[1].position.z+b.vertices[2].position.z;});
            render(bank,icon,140,374);
        }
    }
}
}
