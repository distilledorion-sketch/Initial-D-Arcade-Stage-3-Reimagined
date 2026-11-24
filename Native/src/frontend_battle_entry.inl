// Linked cabinet presentation. Coordinates below are on the original 640x480 canvas.
namespace {
template<class T> struct BattleOverride {
    T& target; T saved;
    BattleOverride(T& target,T selected):target(target),saved(std::exchange(target,selected)){}
    ~BattleOverride(){target=saved;}
};
}
const std::vector<std::uint32_t>& Frontend::paintBattleEntry(int width,int height){
    const auto& b=battleEntry;
    const auto page=b.page;
    // The network menu borrows the artwork, never the offline menu's selection or save lifecycle.
    BattleOverride cm(make,b.make),cc(car,b.car),cr(course,b.course);
    BattleOverride ca(automatic,(b.flags&8)!=0),cd(reverse,(b.flags&1)!=0),cw(wet,(b.flags&2)!=0),cn(night,(b.flags&4)!=0);
    const auto choiceStage=page==BattlePage::Gearbox?FrontendStage::Transmission:page==BattlePage::Route?FrontendStage::Route:
        page==BattlePage::Surface?FrontendStage::Weather:FrontendStage::Time;
    BattleOverride cs(stage,choiceStage);
    BattleOverride cf(carFrame,decltype(carFrame)(b.frame));
    const float confirmation=b.confirmFrame<0?0.f:std::min(b.confirmFrame/36.f,1.f);
    BattleOverride confirm(carConfirmationFrame,b.confirmFrame);
    BattleOverride makerPhase(makerConfirmationPhase,confirmation),transmissionPhase(transmissionConfirmationPhase,confirmation);
    // Paint the moving logo once across the viewport. Its foreground still
    // uses cabinet coordinates, just like the standalone Saved Data screen.
    const bool logoPage=page==BattlePage::Card||page==BattlePage::SavedCar||
        page==BattlePage::AcceptChallengers||page==BattlePage::EnterBattle;
    const int w=logoPage?width:640,h=logoPage?height:480;
    const float artScale=std::min(w/640.f,h/480.f);
    const float artLeft=(w-640*artScale)*.5f,artTop=(h-480*artScale)*.5f;
    struct CanvasSwap{
        std::vector<std::uint32_t>& a;std::vector<std::uint32_t>& b;
        CanvasSwap(std::vector<std::uint32_t>& a,std::vector<std::uint32_t>& b):a(a),b(b){a.swap(b);}
        ~CanvasSwap(){a.swap(b);}
    };
    {
        CanvasSwap swap(pixels,battleCanvas);
        const unsigned bg=page==BattlePage::Waiting?0xaaffffff:0xff000000u;
        pixels.resize(std::size_t(w)*h);
        if(!unityUiEnabled()&&!logoPage)std::fill(pixels.begin(),pixels.end(),bg);
        unityUiClear(pixels.data(),w,h,logoPage?0:bg);
        const auto art=[&](const char* name,int index,float x=0,float y=0,float scale=1.f,float alpha=1.f){draw(name,index,w,h,x,y,scale,alpha);};
        const auto label=[&](const std::string& text,float x,float y,float height,unsigned tint,float maxWidth=600.f){
            const float tw=menuFont.width(text,height);if(tw>maxWidth)height*=maxWidth/tw;
            menuFont.paint(pixels,w,h,text,artLeft+x*artScale,artTop+y*artScale,height*artScale,tint);
        };
        const auto rect=[&](int x,int y,int rw,int rh,unsigned color){
            const int left=int(std::lround(artLeft+x*artScale)),top=int(std::lround(artTop+y*artScale));
            menuRect(pixels,w,h,left,top,int(std::lround(artLeft+(x+rw)*artScale))-left,
                int(std::lround(artTop+(y+rh)*artScale))-top,color);
        };
        const auto common=[&](int index,float x=0,float y=0){art("v3sS00common",index,x,y);};
        const auto road=[&](bool map){
            if(isImportedCourse(course)){drawHakoneBackdrop(w,h);return;}
            art("v3sK00common",courseBackgroundChunk.at(course));art("v3sK00common",30);
            art("v3sK00common",courseDifficultyChunk.at(course),0,168);
            if(map)art("v3sK01course",courseMapChunk.at(course));
        };
        const auto logoBackdrop=[&]{
            if(!battleLogoBackdrop.width){
                const auto folder=projectRoot/"data/original_assets/name_entry/select";
                const auto model=NativeModel::load(folder/"select.idasmesh");
                battleLogoBackdrop=original::buildMenuLogoBackdrop(NativeTextureBank::load(folder/"textures/textures.idastex"),model.chunks.at(45));
            }
            original::paintMenuLogoBackdrop(pixels,w,h,battleLogoBackdrop,b.frame);
        };
        const auto opponentTable=[&](int shift){
            // The linked wait scene loads cardcheck_car, cardcheck and the three
            // cardfont banks alongside the table at 13A0E0..13B492.
            art("v3sV00wait",std::clamp(b.focus,0,2),0,float(shift-91));
            if(b.focus==0){
                constexpr float middle=402;
                if(!b.driverGlyphs.empty()){
                    const float scale=std::min(1.f,284.f/(24.f*(b.driverGlyphs.size()-1)+38.4f));
                    const float step=24.f*scale;
                    float x=middle-(step*(b.driverGlyphs.size()-1)+38.4f*scale)*.5f;
                    for(auto glyph:b.driverGlyphs){
                        if(glyph<220)art(glyph<81?"cardfont_k":glyph<162?"cardfont_h":"cardfont_a",
                            int(glyph<81?glyph:glyph<162?glyph-81:glyph-162),x,105.f+shift-19.2f*scale,scale);
                        x+=step;
                    }
                }else if(!b.driver.empty()){
                    const float size=std::min(23.f,23.f*284.f/std::max(1.f,menuFont.width(b.driver,23.f)));
                    label(b.driver,middle-menuFont.width(b.driver,size)*.5f,93.f+shift,size,0xff111111);
                }
                // The panel already carries the units. Align the right edge of
                // each value with its printed suffix, including multi-digit stats.
                const auto number=[&](unsigned value,float right,float y){
                    auto text=std::to_string(value);
                    float x=right-16.f*text.size();
                    for(char c:text){art("cardcheck",36+c-'0',x,y+shift);x+=16;}
                };
                number(b.streak,396,125);
                art("cardcheck_car",car,0,float(shift-44));
                number(b.points,440,195);
                const unsigned tenths=b.battles?unsigned(std::llround(1000.0*std::min(b.wins,b.battles)/b.battles)):0;
                number(tenths/10,345,231);
                number(tenths%10,367,231);
                rect(406,237+shift,128,20,0xff66b2ff);
                if(tenths)rect(406,237+shift,int(128*tenths/1000),20,0xffffa000);
                number(b.level,422.f+8.f*std::to_string(b.level).size(),267);
            }else{
                constexpr int rows[]={0,1,2,3,5,8,4,6,7};
                constexpr int slots[]={0,17,27,43,53,63};
                for(int i=0;i<(b.focus==1?5:4);++i)for(int weather=0;weather<2;++weather)for(int route=0;route<2;++route){
                    const int id=rows[i+(b.focus==1?0:5)];
                    if(id==8&&!weather)continue;
                    const auto ticks=b.times.at(id*2+route+weather*18);
                    const unsigned ms=ticks/6;
                    const unsigned digits[]={ms/60000%10,ms/10000%6,ms/1000%10,ms/100%10,ms/10%10,ms%10};
                    // Apostrophe and double quote are baked into each cell.
                    const float x=float(258+weather*192),y=float(100+shift+i*45+route*18);
                    for(unsigned j=0;j<6;++j)art("cardcheck",ticks?36+digits[j]:46,x+slots[j],y,.60f);
                }
            }
        };
        if(page==BattlePage::Waiting){
            opponentTable(0);
            art("selvswait",2);art("selvswait",1);art("selvswait",0);
            const int lamp=(b.frame/5)%14;
            art("selvswait",3,float(lamp*45),0);art("selvswait",4,float((13-lamp)*45),0);
        }else if(page==BattlePage::AcceptChallengers||page==BattlePage::EnterBattle){
            const bool entering=page==BattlePage::EnterBattle;
            logoBackdrop();
            common(54);art("v3sV01challenge",entering?10:9);art("v3sV01challenge",8);
            const int right=entering?1:0,left=entering?2:3;
            const bool yes=(b.flags&16)!=0;
            const auto choice=[&](int index,bool active){
                auto& bankData=bank("v3sV01challenge");
                const auto colors=active?original::OriginalChoiceColors::Selected:original::OriginalChoiceColors::InactiveTransmission;
                const auto chunk=original::materializeOriginalChoiceColors(bankData.model.chunks.at(index),colors);
                SpritePlacement p;p.scale=(active?100.f:80.f)*artScale;
                p.offsetX=artLeft+320*artScale;p.offsetY=artTop+128*artScale;p.invertY=true;p.authoredHeight=0;
                compositeOriginalMenuChunk(pixels,w,h,bankData.textures,chunk,p);
            };
            art("v3sV01challenge",(yes?right:left)+4,320,128);
            choice(left,!yes);choice(right,yes);
            if(entering)opponentTable(120);else art("selreceive",0,0,128);
        }else if(page==BattlePage::Card){
            logoBackdrop();
            common(54);art("select01",12);
            for(int i=0;i<2;++i){
                art("select01",11,320,128+180.f*i);
                const bool pulse=i==b.focus&&(b.frame/16)%2;
                rect(122,104+i*180,396,38,i?(pulse?0xffc11b12:0xff9e120c):(pulse?0xff233dba:0xff112a8b));
            }
            for(int i:{39,40,42,43,23,24})art("select01",i,0,16);
        }else if(page==BattlePage::SavedCar){
            if(!savedData_){savedData_=std::make_unique<original::OriginalSavedData>();savedData_->load(projectRoot);}
            const original::OriginalSavedData::CarPicker picker{unsigned(b.frame),unsigned(b.seconds),unsigned(std::clamp(b.focus,0,6))};
            savedData_->paint(pixels,w,h,b.cardProfile,&picker);
            if(!b.caption.empty())label("< "+b.caption+" >",540,63,13,0xffffffff,96);

        }else if(page==BattlePage::Make){
            common(46);common(54);
            for(int i:{16,17,7,make,9+make})art("v3sS04maker",i);
            art("v3sS00minicar",1+make);drawMakeStrip(w,h);
        }else if(page==BattlePage::Model){
            common(46);common(54);art("v3sS05cars",42);art("v3sS05cars",43);
            art("v3sS00emblem",make+7,59,128,.9f);art("v3sS00emblem",make,59,128,.9f);
            art("v3sS05cars",0);art("v3sS05cars",carNameChunk(car));
            const auto roster=carsForMake(make);const int selected=int(std::find(roster.begin(),roster.end(),car)-roster.begin());
            BattleOverride slot(carouselSlot,std::min(selected,3)),scroll(displayedScroll,0);
            BattleOverride phase(carConfirmationPhase,confirmation);
            drawCarCarousel(w,h);
            const unsigned paints=original::originalCarColorCounts.at(car);
            drawCarColorIndicator(w,h,b.paint,std::max(paints,b.paint+1));
            // Retail 1B5E20: 134580 maps car -> character through 26B790;
            // 2A8044 supplies the face/name pair. Some cars have no associated driver.
            constexpr std::array<int,35> faces={30,34,10,11,-1,20,18,19,-1,9,15,-1,31,14,17,6,25,26,-1,28,24,-1,13,-1,22,33,-1,-1,-1,5,23,4,7,8,-1};
            constexpr std::array<int,35> names={57,61,41,42,-1,50,48,49,-1,40,45,-1,58,44,47,37,54,55,-1,56,53,-1,43,-1,51,60,-1,-1,-1,36,52,35,38,39,-1};
            if(faces[car]>=0){
                art("v3sK00rivalface",1);art("v3sK00rivalface",2,-121,0);
                art("v3sK00rivalface",names[car],69,0);
                art("v3sK00rivalface",faces[car],127,223,.8f);
            }
            if(roster.size()>4){common(48);common(50,-3,0);}
        }else if(page==BattlePage::Course&&b.modFocus){
            BattleOverride focus(modsFocus,ModsFocus(b.modFocus));
            const auto selected=std::lower_bound(b.sharedModCourses.begin(),b.sharedModCourses.end(),course);
            BattleOverride first(firstModRow,std::max(0,int(selected-b.sharedModCourses.begin())-visibleModRows+1));
            drawModsPicker(w,h,b.sharedModCourses);
        }else if(page==BattlePage::Course){
            art("v3sK00common",83);art("v3sK01course",26);art("v3sK01course",14);road(true);
            for(int row=b.scroll>0?-1:0;row<(b.scroll<0?6:5);++row){
                const float x=120.f+(float(row)+float(b.scroll)/5.f)*100.f;
                const int id=row<0?b.leadingTile:row==5?b.trailingTile:b.courseTiles[row];
                const bool selected=row==b.tileSlot;
                if(selected&&(b.confirmFrame<0||(b.frame&6)))art("v3sK01course",25,x,128);
                if(!selected&&b.confirmFrame>=0)continue;
                if(id<9)art("v3sK01course",courseThumbnailChunk.at(id),x,128,selected?1.f:.9f,selected?1.f:.42f);
                else drawModsTile(w,h,x,128,selected?1.f:.9f);
            }
            for(int i:{80,81,78,79})art("v3sK00common",i);
        }else if(page==BattlePage::Gearbox||page==BattlePage::Route||page==BattlePage::Surface||page==BattlePage::Time){
            drawChoiceMenu(w,h,false);drawChoiceMenu(w,h,true);
        }else if(page==BattlePage::Selected){
            road(false);common(20);art("v3sV10boost",11);art("v3sV10boost",10);
            art("v3sV10boost",3);art("v3sV10boost",0);art("v3sV10boost",1);
            float x=219;
            const auto word=[&](std::string_view text,float size,float y){
                menuFont.paint(pixels,w,h,text,x,y,size,0xffffffff,0xff101010,2);
                return menuFont.width(text,size);
            };
            x+=word("[",26,311)+5;
            if(b.caption=="COUNTERCLOCKWISE"||b.caption=="COUNTER-CLOCKWISE"){
                const float first=word("COUNTER-",15,310);
                x+=std::max(first,word("CLOCKWISE",15,325));
            }else x+=word(b.caption,23,312);
            const auto dot=[&]{
                x+=6;
                menuRect(pixels,w,h,int(x),322,8,8,0xff151515);
                menuRect(pixels,w,h,int(x)+2,324,4,4,0xffffffff);
                x+=14;
            };
            dot();x+=word(night?"NIGHT":"DAY",26,310);dot();
            x+=word(course==8?"SNOW":wet?"WET":"DRY",26,310)+5;
            word("]",26,310);
        }
        if(!b.modFocus&&page!=BattlePage::Waiting&&page!=BattlePage::Selected&&page!=BattlePage::Loading&&page!=BattlePage::SavedCar){
            const int offset=b.seconds<=5?10:0;
            if(b.seconds>=10)common(offset+b.seconds/10,535,16);
            common(offset+b.seconds%10,574,16);
        }
    }
    if(width==w&&height==h)return battleCanvas;
    const float fit=std::min(float(width)/w,float(height)/h);
    // Keep the card, its field artwork and the showroom car on the same
    // 640x480 transform, including when a resized window fits fractional pixels.
    const float xOrigin=(width-w*fit)*.5f,yOrigin=(height-h*fit)*.5f;
    const auto edge=[](float position,int limit){
        return std::clamp(int(std::ceil(position-.5f)),0,limit);
    };
    const int left=edge(xOrigin,width),right=edge(xOrigin+w*fit,width);
    const int top=edge(yOrigin,height),bottom=edge(yOrigin+h*fit,height);
    // The seated cabinet draws a real showroom car underneath the translucent white panel.
    // Clear only the margins to black. A full black quad hides that car at every scaled resolution.
    battleOutput.assign(std::size_t(width)*height,0xff000000);
    unityUiClear(battleOutput.data(),width,height,0);
    if(left>0)menuRect(battleOutput,width,height,0,0,left,height,0xff000000);
    if(right<width)menuRect(battleOutput,width,height,right,0,width-right,height,0xff000000);
    if(top>0)menuRect(battleOutput,width,height,left,0,right-left,top,0xff000000);
    if(bottom<height)menuRect(battleOutput,width,height,left,bottom,right-left,height-bottom,0xff000000);
    unityUiCopy(battleOutput.data(),battleCanvas.data(),width,height,fit,fit,xOrigin,yOrigin,true);
    if(!unityUiEnabled())for(int y=top;y<bottom;++y){
        const int sourceY=std::clamp(int((y+.5f-yOrigin)/fit),0,h-1);
        for(int x=left;x<right;++x){
            const int sourceX=std::clamp(int((x+.5f-xOrigin)/fit),0,w-1);
            battleOutput[std::size_t(y)*width+x]=battleCanvas[std::size_t(sourceY)*w+sourceX];
        }
    }
    return battleOutput;
}
