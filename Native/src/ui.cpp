#include "unity_ui_capture.h"
#include "ui.h"
#include "frontend.h"
#include "original_battle_metrics.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <stdexcept>

namespace idas3 {
static unsigned uiArgb(COLORREF c){return 0xff000000u|(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c);}
constexpr COLORREF white=RGB(238,242,240),muted=RGB(142,158,166),yellow=RGB(255,207,63),cyan=RGB(76,211,212),panel=RGB(13,22,29);
#if defined(IDAS3_PORTABLE_SCENE)
Hud::~Hud()=default;
#else
Hud::~Hud(){for(auto& f:fonts)DeleteObject(f.second);if(dc){SelectObject(dc,originalBitmap);DeleteObject(bitmap);DeleteDC(dc);}}
#endif
void Hud::loadOriginal(const std::filesystem::path& root){
#if defined(IDAS3_PORTABLE_SCENE)
    sceneFont_=MenuFont::load(root);
    if(!sceneFont_.ready())throw std::runtime_error("Portable HUD requires data/native_assets/menu_font/font.bin");
#endif
    originalHud=OriginalRaceHud::load(root);originalResults=OriginalTimeAttackResults::load(root);originalBattleHud=OriginalBattleHudAssets::load(root);
    originalBattleResults=OriginalBattleResults::load(root);
    originalBattleNames=OriginalBattleNames::load(root);
    portraits.clear();
    for(std::uint32_t enemy=0;enemy<31;++enemy){const std::string name(original::originalBattlePortraitBank(enemy,0));if(portraits.contains(name))continue;
        const auto folder=root/"data/original_assets/hud/faces"/name;
        portraits.emplace(name,PortraitAssets{NativeModel::load(folder/(name+".idasmesh")),NativeTextureBank::load(folder/"textures/textures.idastex")});
    }
    battleAnimation={};battleFrameAnimation={};lastBattleFrame=-1;lastBattleEnemy=lastBattleProfileMode=0xffffffff;originalHudReady=true;
}
#if defined(IDAS3_PORTABLE_SCENE)
void Hud::resize(int w,int h){
    if(width==w&&height==h)return;
    width=w;height=h;sx=sy=std::min(w/1280.f,h/720.f);
    offsetX=(w-1280.f*sx)*.5f;offsetY=(h-720.f*sy)*.5f;
    scenePixels_.assign(std::size_t(w)*h,0u);pixels=scenePixels_.data();
}
void Hud::rect(float x,float y,float w,float h,COLORREF c){unityUiSolid(pixels,width,height,offsetX+x*sx,offsetY+y*sy,w*sx,h*sy,uiArgb(c));}
void Hud::line(float x,float y,float xx,float yy,COLORREF c,int thick){unityUiLine(pixels,width,height,offsetX+x*sx,offsetY+y*sy,offsetX+xx*sx,offsetY+yy*sy,float(std::max(1,int(thick*sx))),uiArgb(c));}
void Hud::text(float x,float y,const std::string& value,int size,COLORREF color,bool){
    sceneFont_.paint(scenePixels_,width,height,value,offsetX+x*sx,offsetY+y*sy,float(std::max(10,int(size*sy))),uiArgb(color));
}
#else
void Hud::resize(int w,int h){
    if(width==w&&height==h)return;
    for(auto& f:fonts)DeleteObject(f.second);fonts.clear();
    if(dc){SelectObject(dc,originalBitmap);DeleteObject(bitmap);DeleteDC(dc);}
    width=w;height=h;sx=sy=std::min(w/1280.f,h/720.f);
    offsetX=(w-1280.f*sx)*.5f;offsetY=(h-720.f*sy)*.5f;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    dc=CreateCompatibleDC(nullptr);bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);originalBitmap=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);
}
void Hud::rect(float x,float y,float w,float h,COLORREF c){if(unityUiEnabled()){unityUiSolid(pixels,width,height,offsetX+x*sx,offsetY+y*sy,w*sx,h*sy,uiArgb(c));return;}RECT r{LONG(offsetX+x*sx),LONG(offsetY+y*sy),LONG(offsetX+(x+w)*sx),LONG(offsetY+(y+h)*sy)};HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);}
void Hud::line(float x,float y,float xx,float yy,COLORREF c,int thick){if(unityUiEnabled()){unityUiLine(pixels,width,height,offsetX+x*sx,offsetY+y*sy,offsetX+xx*sx,offsetY+yy*sy,float(std::max(1,int(thick*sx))),uiArgb(c));return;}HPEN pen=CreatePen(PS_SOLID,std::max(1,int(thick*sx)),c);auto old=SelectObject(dc,pen);MoveToEx(dc,int(offsetX+x*sx),int(offsetY+y*sy),nullptr);LineTo(dc,int(offsetX+xx*sx),int(offsetY+yy*sy));SelectObject(dc,old);DeleteObject(pen);}
void Hud::text(float x,float y,const std::string& value,int size,COLORREF color,bool bold){
    int key=size*2+bold;
    if(!fonts.count(key))fonts[key]=CreateFontW(-std::max(10,int(size*sy)),0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Bahnschrift");
    auto old=SelectObject(dc,fonts[key]);SetTextColor(dc,color);
    int count=MultiByteToWideChar(CP_UTF8,0,value.c_str(),int(value.size()),nullptr,0);std::wstring wide(count,L' ');MultiByteToWideChar(CP_UTF8,0,value.c_str(),int(value.size()),wide.data(),count);
    if(unityUiEnabled()){unityUiText(pixels,width,height,dc,offsetX+x*sx,offsetY+y*sy,wide.data(),count,uiArgb(color));SelectObject(dc,old);return;}
    TextOutW(dc,int(offsetX+x*sx),int(offsetY+y*sy),wide.data(),count);SelectObject(dc,old);
}
#endif
void Hud::label(float x,float y,const std::string& value,int size,COLORREF color,bool bold){
    text(x+1,y+1,value,size,RGB(2,4,6),bold);text(x,y,value,size,color,bold);
}
namespace{
// The map writes its own pixels rather than going through the GDI helpers: the
// field is see-through while the road and the indicators must stay solid, and
// only a direct write controls the alpha of each of them independently.
inline void plot(std::uint32_t* pixels,int width,int height,int x,int y,std::uint32_t argb){
    if(x>=0&&y>=0&&x<width&&y<height)pixels[std::size_t(y)*width+x]=argb;
}
void solidDisc(std::uint32_t* pixels,int width,int height,float cx,float cy,float radius,std::uint32_t argb){
    const int r=int(std::ceil(radius));
    const float rr=radius*radius;
    for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx)
        if(float(dx*dx+dy*dy)<=rr)plot(pixels,width,height,int(cx)+dx,int(cy)+dy,argb);
}
void solidLine(std::uint32_t* pixels,int width,int height,float x0,float y0,float x1,float y1,
        float thickness,std::uint32_t argb){
    const float dx=x1-x0,dy=y1-y0;
    const int steps=int(std::max(std::abs(dx),std::abs(dy)))+1;
    const float radius=std::max(.5f,thickness*.5f);
    for(int i=0;i<=steps;++i){
        const float t=steps?float(i)/float(steps):0.f;
        solidDisc(pixels,width,height,x0+dx*t,y0+dy*t,radius,argb);
    }
}
// Liang-Barsky. The map shows a window around the car, so every road segment is
// trimmed to the frame instead of being drawn across the rest of the screen.
bool clipToFrame(float& x0,float& y0,float& x1,float& y1,
        float left,float top,float right,float bottom){
    const float dx=x1-x0,dy=y1-y0;
    const float p[4]={-dx,dx,-dy,dy},q[4]={x0-left,right-x0,y0-top,bottom-y0};
    float t0=0,t1=1;
    for(unsigned i=0;i<4;++i){
        if(p[i]==0){if(q[i]<0)return false;continue;}
        const float r=q[i]/p[i];
        if(p[i]<0){if(r>t1)return false;if(r>t0)t0=r;}
        else{if(r<t0)return false;if(r<t1)t1=r;}
    }
    const float ax=x0,ay=y0;
    x0=ax+t0*dx;y0=ay+t0*dy;x1=ax+t1*dx;y1=ay+t1*dy;
    return true;
}
}
// The whole authored route, for the course-selection panel. This one is static
// and complete on purpose: it is the authoring view, not the race instrument.
// Device-space fill. Unity does not read the pixel buffer -- it collects UI
// triangles -- so anything the map paints has to go through the same helpers
// the rest of the HUD uses, while still carrying its own alpha.
void Hud::mapFill(float x,float y,float w,float h,COLORREF color,unsigned alpha){
    const std::uint32_t argb=(std::min(alpha,255u)<<24)|(GetRValue(color)<<16)|(GetGValue(color)<<8)|GetBValue(color);
    if(unityUiEnabled()){unityUiSolid(pixels,width,height,x,y,w,h,argb);return;}
    if(!pixels)return;
    const int x0=std::max(0,int(x)),y0=std::max(0,int(y));
    const int x1=std::min(width,int(x+w)),y1=std::min(height,int(y+h));
    for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)pixels[std::size_t(yy)*width+xx]=argb;
}
void Hud::mapStroke(float x0,float y0,float x1,float y1,COLORREF color,float thickness){
    const std::uint32_t argb=0xff000000u|(GetRValue(color)<<16)|(GetGValue(color)<<8)|GetBValue(color);
    if(unityUiEnabled()){unityUiLine(pixels,width,height,x0,y0,x1,y1,std::max(1.f,thickness),argb);return;}
    if(pixels)solidLine(pixels,width,height,x0,y0,x1,y1,thickness,argb);
}
// The whole authored route, for the course-selection panel. This one is static
// and complete on purpose: it is the authoring view, not the race instrument.
void Hud::routePreview(const Course& c,float x,float y,float w,float h){
    if(c.points.empty())return;
    float minx=c.points[0].x,maxx=minx,minz=c.points[0].z,maxz=minz;
    for(auto p:c.points){minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);
        minz=std::min(minz,p.z);maxz=std::max(maxz,p.z);}
    const float scale=std::min((w-24)/std::max(1.f,maxx-minx),(h-24)/std::max(1.f,maxz-minz));
    const auto px=[&](Vec3 p){return x+w*.5f+(p.x-(minx+maxx)*.5f)*scale;};
    const auto py=[&](Vec3 p){return y+h*.5f-(p.z-(minz+maxz)*.5f)*scale;};
    const int stride=std::max(1,int(c.points.size()/650));
    Vec3 previous=c.points.front();
    for(std::size_t i=std::size_t(stride);i<c.points.size();i+=std::size_t(stride)){
        const auto p=c.points[i];
        line(px(previous),py(previous),px(p),py(p),RGB(2,4,6),4);
        line(px(previous),py(previous),px(p),py(p),white,2);
        previous=p;
    }
}
void Hud::map(const Course& c,float x,float y,float w,float h,
        const VehicleState& player,const VehicleState* rival){
    if(c.points.size()<2||!pixels)return;
    // Laid out on the source's own 640x480 canvas and anchored to the bottom
    // left exactly as the original HUD anchors its instruments, so it scales
    // and sits with them instead of floating on the development canvas.
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const float baseY=float(height)-480.f*fit;
    const float left=x*fit,top=baseY+y*fit,right=(x+w)*fit,bottom=baseY+(y+h)*fit;
    // Sampled from the cabinet: a muted green field you can see the road
    // through, a dark outline with a lighter inner edge, and a near-black road.
    constexpr COLORREF field=RGB(120,142,130),outline=RGB(16,20,18),
        innerEdge=RGB(150,158,150),road=RGB(16,22,17);
    const float border=std::max(1.f,2.f*fit),inner=std::max(1.f,fit);
    // Strokes, not filled rectangles: a filled one under the field would sit
    // behind it and make the whole panel opaque once the quads composite.
    const auto outlineRect=[&](float bx,float by,float bw,float bh,COLORREF colour,float t){
        mapFill(bx,by,bw,t,colour,255);mapFill(bx,by+bh-t,bw,t,colour,255);
        mapFill(bx,by+t,t,bh-2*t,colour,255);mapFill(bx+bw-t,by+t,t,bh-2*t,colour,255);
    };
    outlineRect(left,top,right-left,bottom-top,outline,border);
    outlineRect(left+border,top+border,right-left-2*border,bottom-top-2*border,innerEdge,inner);
    const float fieldLeft=left+border+inner,fieldTop=top+border+inner;
    const float fieldRight=right-border-inner,fieldBottom=bottom-border-inner;
    mapFill(fieldLeft,fieldTop,fieldRight-fieldLeft,fieldBottom-fieldTop,field,140);
    mapX0_=std::max(0,int(left));mapY0_=std::max(0,int(top));
    mapX1_=std::min(width,int(right));mapY1_=std::min(height,int(bottom));
    // The car sits at the centre of the frame, as it does on the cabinet.
    const float cx=(fieldLeft+fieldRight)*.5f,cy=(fieldTop+fieldBottom)*.5f;
    // A stretch of road, not the route: the window is sized from the road's own
    // width so every course reads at the same zoom.
    const float roadWidth=std::max(1.f,c.sample(c.length*.5f).width);
    const float scale=(fieldBottom-fieldTop)/(roadWidth*10.f);
    const float sine=std::sin(player.yaw),cosine=std::cos(player.yaw);
    const auto project=[&](const Vec3& p,float& px,float& py){
        const float ox=p.x-player.position.x,oz=p.z-player.position.z;
        // Heading up: what is ahead of the car is up in the frame. The view is
        // right-handed (renderer.cpp builds it with XMMatrixLookAtRH), so what
        // is to the right on screen is cross(forward,up) = -right(yaw); using
        // right(yaw) here mirrors the map.
        px=cx+(oz*sine-ox*cosine)*scale;
        py=cy-(ox*sine+oz*cosine)*scale;
    };
    const float reach=std::max(fieldRight-fieldLeft,fieldBottom-fieldTop);
    float previousX=0,previousY=0;project(c.points.front(),previousX,previousY);
    const int stride=std::max(1,int(c.points.size()/1200));
    for(std::size_t i=std::size_t(stride);i<c.points.size();i+=std::size_t(stride)){
        float px=0,py=0;project(c.points[i],px,py);
        const float ax=previousX,ay=previousY;previousX=px;previousY=py;
        // Cheap reject before clipping; the window is a small part of the route.
        if((ax<fieldLeft-reach&&px<fieldLeft-reach)||(ax>fieldRight+reach&&px>fieldRight+reach)||
           (ay<fieldTop-reach&&py<fieldTop-reach)||(ay>fieldBottom+reach&&py>fieldBottom+reach))continue;
        float x0=ax,y0=ay,x1=px,y1=py;
        if(!clipToFrame(x0,y0,x1,y1,fieldLeft,fieldTop,fieldRight,fieldBottom))continue;
        mapStroke(x0,y0,x1,y1,road,std::max(1.5f,3.f*fit));
    }
    // The source's own indicators: game2d chunk 94 is the opponent, 93 the car.
    if(!originalHudReady)return;
    const std::span<std::uint32_t> destination(pixels,std::size_t(width)*height);
    const float blip=std::max(4.f,(fieldBottom-fieldTop)*.115f);
    const auto indicator=[&](unsigned chunk,float px,float py){
        // A solid backing keeps the sphere from picking up the field it sits on.
        mapFill(px-blip*.36f,py-blip*.36f,blip*.72f,blip*.72f,RGB(12,17,13),255);
        originalHud.paintChunkAt(destination,width,height,chunk,px,py,blip);
        if(chunk==93)++battlePresentation_.localMapMarkers;
        if(chunk==94)++battlePresentation_.opponentMapMarkers;
    };
    if(rival){
        float rx=0,ry=0;project(rival->position,rx,ry);
        // An opponent outside the window is held at the edge, pointing the way
        // they are, rather than disappearing.
        float ox=rx-cx,oy=ry-cy;
        const float halfW=std::max(1.f,(fieldRight-fieldLeft)*.5f-blip*.55f);
        const float halfH=std::max(1.f,(fieldBottom-fieldTop)*.5f-blip*.55f);
        const float over=std::max(std::abs(ox)/halfW,std::abs(oy)/halfH);
        if(over>1.f){ox/=over;oy/=over;}
        indicator(94,cx+ox,cy+oy);
    }
    indicator(93,cx,cy);
}
const std::uint32_t* Hud::paintResult(const OriginalBattleResultsState& state,
    std::span<const std::uint32_t> tuningOverlay,bool paused,bool showControls,bool suppressPauseOverlay){
    battlePresentation_={};
    if(!pixels)return nullptr;
    const auto count=std::size_t(width)*height;
    if(tuningOverlay.empty()){
        std::fill_n(pixels,count,0u);unityUiClear(pixels,width,height);
        originalBattleResults.paint(std::span<std::uint32_t>(pixels,count),width,height,state,true);
    }else{
        if(tuningOverlay.size()!=count)throw std::invalid_argument("Result overlay dimensions do not match viewport");
        std::copy(tuningOverlay.begin(),tuningOverlay.end(),pixels);unityUiCopy(pixels,tuningOverlay.data(),width,height);
    }
    // Desktop controls are optional; repainting this view never advances the
    // source result owner, part selection or countdown.
    if(paused&&!suppressPauseOverlay){
        rect(411,220,458,244,panel);text(453,248,"PAUSED",36,white,true);
        text(453,317,"ESC / START    Resume",18,white);
        text(453,358,"BACKSPACE    Course select",17,muted);
        text(453,399,"R    Restart this course",17,muted);
    }
    const float controlsX=-offsetX/sx,controlsY=(height-offsetY)/sy-42.f;
    if(showControls){
        rect(controlsX,controlsY,width/sx,42,panel);
        text(controlsX+28,controlsY+13,"LEFT / RIGHT  Select     ENTER / A  Confirm     ESC  Pause     F1  Hide controls",14,white);
    }
#if !defined(IDAS3_PORTABLE_SCENE)
    GdiFlush();
#endif
    const auto opaqueRect=[&](float x,float y,float w,float h){
        for(int yy=std::max(0,int(offsetY+y*sy));yy<std::min(height,int(offsetY+(y+h)*sy));++yy)
            for(int xx=std::max(0,int(offsetX+x*sx));xx<std::min(width,int(offsetX+(x+w)*sx));++xx)
                pixels[std::size_t(yy)*width+xx]|=0xff000000u;
    };
    if(!unityUiEnabled()&&paused&&!suppressPauseOverlay)opaqueRect(411,220,458,244);
    if(!unityUiEnabled()&&showControls)opaqueRect(controlsX,controlsY,width/sx,42);
    return pixels;
}
const std::uint32_t* Hud::paint(const UiState& s){
    battlePresentation_={};
    if(!pixels||!s.course||!s.car||!s.race)return pixels;
    // Unity uses this allocation only as a surface key: its triangles, map
    // strokes and glyphs are captured before rasterization. Preserve the
    // desktop pixel clear and an exact same-build performance baseline.
    if(!unityUiFrameReuseEnabled())std::memset(pixels,0,std::size_t(width)*height*4);
    unityUiClear(pixels,width,height);
    mapX0_=mapY0_=mapX1_=mapY1_=0;
    const auto& c=*s.course;const auto& v=*s.car;const auto& race=*s.race;
    const bool useOriginalHud=originalHudReady;
    const bool onlineBattle=s.onlineBattleHud.active;
    const bool battleHud=s.battle||onlineBattle;
    const bool settledBattleResults=useOriginalHud&&s.battle&&s.battleResults&&(s.battleResults->profileMode==0||s.battleResults->profileMode==2)&&race.phase==RacePhase::Finished&&!s.menu&&!s.paused&&s.finishBanner==OriginalHudState::FinishBanner::none;
    const bool settledTimeAttackResults=useOriginalHud&&s.results&&!battleHud&&race.phase==RacePhase::Finished&&!race.timeUp&&!s.menu&&!s.paused&&s.finishBanner==OriginalHudState::FinishBanner::none;
    const bool settledResults=settledBattleResults||settledTimeAttackResults;
    if(settledTimeAttackResults&&(s.results->recordFlags&(OriginalResultsState::newRecord|OriginalResultsState::courseRecord|OriginalResultsState::modelRecord|OriginalResultsState::personalBest))){
        auto results=*s.results;results.announcementOnly=true;
        originalResults.paint(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,results);
        return pixels;
    }
    // While the race-end announcement holds the road view the cabinet shows
    // nothing else over it, so the development panel, the status line and
    // the battle plates stay down.
    const bool announcement=s.finishBanner!=OriginalHudState::FinishBanner::none;
    if(s.menu||!battleHud){lastBattleFrame=-1;lastBattleEnemy=lastBattleProfileMode=0xffffffff;battleAnimation={};battleFrameAnimation={};}
    if(s.menu&&s.frontend){
        const auto& art=s.frontend->paint(width,height);std::copy(art.begin(),art.end(),pixels);unityUiCopy(pixels,art.data(),width,height);
        if(s.showControls){
            const float controlsX=-offsetX/sx,controlsY=(height-offsetY)/sy-42.f;
            rect(controlsX,controlsY,width/sx,42,RGB(10,15,22));
            text(controlsX+28,controlsY+13,s.frontend->stage==FrontendStage::Car?
                "LEFT / RIGHT  Car     Q / E or X / Y  Paint     ENTER / A  Confirm     ESC / B  Back     F1  Hide controls":
                "LEFT / RIGHT  Select     ENTER / A  Confirm     ESC / B  Back     F5  Quick run     F1  Hide controls",14,white);
        }
    }else if(s.menu){
        rect(0,0,1280,94,panel);rect(40,28,5,42,yellow);text(60,25,"INITIAL D",35,white,true);text(258,40,"ARCADE STAGE 3",18,muted);text(1030,34,"NATIVE REMAKE / 0.1",14,yellow);
        rect(40,136,382,488,panel);text(64,158,"01 / SELECT COURSE",14,yellow,true);text(64,190,c.name,46,white,true);
        text(65,250,c.reversed?"REVERSE AUTHORING PATH":"FORWARD AUTHORING PATH",12,muted);routePreview(c,68,282,320,217);
        char info[96];std::snprintf(info,sizeof(info),"%.2f km*    /    %zu path points",c.length/1000,c.points.size());text(65,532,info,16,white);
        text(65,561,"Original centerline + road boundaries",13,cyan);text(65,584,"LEFT / RIGHT  Select course",14,muted);
        rect(844,136,396,488,panel);text(868,160,"02 / DRIVE SETUP",14,yellow,true);
        text(868,196,"TIME ATTACK",31,white,true);text(868,242,"Engineering handling presets",14,muted);
        const char* names[]={"FR / LIGHT COUPE","FF / COMPACT","4WD / TURBO SEDAN"};
        text(868,277,names[s.carProfile],21,white,true);text(868,307,"A / D  Change preset",13,muted);
        line(868,340,1214,340,RGB(53,67,75));
        text(868,357,"M   TRANSMISSION",14,muted);text(1090,354,s.automatic?"AUTO":"MANUAL",19,white,true);
        text(868,402,"W   ROAD",14,muted);text(1090,399,s.wet?"WET":"DRY",19,white,true);
        text(868,447,"N   LIGHT",14,muted);text(1090,444,s.night?"NIGHT":"DAY",19,white,true);
        text(868,492,"V   DIRECTION",14,muted);text(1090,489,c.reversed?"REVERSE":"FORWARD",17,white,true);
        rect(868,548,346,49,yellow);text(904,559,"ENTER / A   START RUN",21,RGB(15,22,28),true);
        text(448,576,"AUTHENTIC PATH DATA",17,white,true);text(448,603,"Handling and visual reconstruction in progress",12,muted);
        rect(0,652,1280,68,panel);text(40,668,"WASD / arrows: drive     Q / E: shift     ESC: pause     F1: telemetry     F2: sound",15,white);
        text(40,695,"* Native distance treated as meters provisionally. Car and scenery are development geometry; presets are uncalibrated.",11,muted);
    }else{
        if(!battleHud&&!s.results){label(race.originalTiming?510.f:28.f,20,c.name+" / TIME ATTACK",15,white,true);
            label(1110,20,(s.snow?"SNOW":s.wet?"WET":"DRY")+std::string(" / ")+(s.night?"NIGHT":"DAY"),13,white);}
        // Square and on the source canvas, anchored to the bottom left.
        if(!settledResults&&!announcement&&s.car)map(c,14,352,112,112,*s.car,s.rival);
        if(race.originalTiming&&!useOriginalHud){
            label(28,110,"TIME "+std::to_string(std::max(0,(race.remaining6000+5999)/6000)),25,yellow,true);
            label(28,148,"SECTION TIME",15,white,true);
            std::uint32_t previousSection=0;
            for(int i=0;i<4;++i){
                const bool recorded=i<race.sector,active=i==race.sector&&race.phase!=RacePhase::Finished;
                const auto stamp=recorded?race.sectionTimes6000[std::size_t(i)]:race.elapsed6000;
                const auto duration=stamp>=previousSection?stamp-previousSection:0;
                label(28,float(171+i*22),std::to_string(i+1)+"  "+((recorded||active)?formatTime(double(duration)/6000):"--:--.---"),17,active?yellow:white,true);
                if(recorded)previousSection=stamp;
            }
        }
        if(!useOriginalHud){
            label(28,51,formatTime(race.seconds()),28,yellow,true);
            char speed[20];std::snprintf(speed,sizeof(speed),"%03d",int(std::max(0.f,v.speedKmh())));
            label(1023,594,speed,64,white,true);label(1144,643,"km/h",14,white);label(1200,605,std::to_string(v.gear),51,yellow,true);
            label(1110,674,std::to_string(int(v.rpm))+" RPM",12,white);
        }
        // Cabinet HUD art owns the racing view; host button hints belong in menus.
        if(!s.paused&&!useOriginalHud){
            if(race.phase==RacePhase::Countdown)label(595,241,std::to_string(race.countdownDigit()),112,yellow,true);
            else if(race.phase==RacePhase::Running&&race.ticks<60)label(552,245,"GO!",82,yellow,true);
        }
        if(s.debug){rect(27,132,389,304,panel);text(45,148,"PHYSICS / RECONSTRUCTION",16,cyan,true);
            char lineText[160];int yy=181;auto row=[&](const char* fmt,float a){std::snprintf(lineText,sizeof(lineText),fmt,a);text(45,float(yy),lineText,14,white);yy+=25;};
            row(s.originalHandling?"Heading error  %.3f (normalized)":"Slip angle     %+.3f rad",v.slip);row("Yaw rate       %+.3f rad/s",v.yawRate);row("Steering       %+.3f",v.steering);row("Signed square  %+.3f",v.steeringBasis[1]);row("Load proxy     %+.3f",v.accelProxy);row("Render         %.1f FPS",s.fps);
            text(45,354,"Simulation: fixed 60 Hz",14,yellow);text(45,380,s.originalHandling?"Original player solver and car data":"Development force law / car parameters",12,muted);text(45,405,v.wallContact?"WALL CONTACT":"Free contact",12,v.wallContact?yellow:muted);
        }
        if(!(s.paused&&s.suppressPauseOverlay)&&(s.paused||(race.phase==RacePhase::Finished&&!settledResults&&!announcement))){rect(411,182,458,333,panel);rect(411,182,458,4,yellow);text(453,213,s.paused?"PAUSED":race.timeUp?"TIME UP":s.battle?(s.battleWon?"YOU WIN":"YOU LOSE"):"RUN COMPLETE",36,white,true);
            text(454,276,formatTime(race.seconds()),45,yellow,true);text(453,353,s.paused?"ESC / START    Resume":"ENTER / A    Back to course select",18,white);
            if(!s.multiplayer){text(453,395,"R    Restart this course",17,muted);if(s.paused)text(453,434,"BACKSPACE    Course select",17,muted);}
            if(s.paused)text(453,479,s.originalHandling?"Original player physics / "+c.name:"Development handling / selected course",12,muted);
            if(s.paused&&!s.originalWeatherScenery)text(453,498,"Rain handling active; wet scenery incomplete",11,muted);
        }
    }
    // Startup scope notices belong in pause/debug, not over the road.
    if(!s.message.empty()&&!announcement&&!(s.paused&&s.suppressPauseOverlay)&&(s.menu||s.paused||s.debug||(race.phase==RacePhase::Finished&&!settledResults))) {rect(391,110,484,45,panel);text(407,122,s.message,14,yellow);}
#if !defined(IDAS3_PORTABLE_SCENE)
    GdiFlush();
#endif
    if(!unityUiEnabled())for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
        if(xx>=mapX0_&&xx<mapX1_&&yy>=mapY0_&&yy<mapY1_)continue;
        const std::size_t i=std::size_t(yy)*width+xx;
        if(pixels[i]&0x00ffffff)pixels[i]|=0xff000000;
    }
    if(settledBattleResults){
        originalBattleResults.paint(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,*s.battleResults);
    }else if(settledTimeAttackResults){
        originalResults.paint(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,*s.results);
    }else if(!s.menu&&useOriginalHud){
        OriginalHudState state;state.speedKmh=v.speedKmh();state.gear=v.gear;state.automatic=s.automatic;
        state.elapsedTicks6000=race.originalTiming?race.elapsed6000:std::uint32_t(std::min<std::uint64_t>(race.ticks*100,UINT32_MAX));
        state.extendedCountdown=s.extendedCountdown;state.timePanel=race.originalTiming&&!s.debug;state.remainingTicks6000=race.remaining6000;
        if(s.useDisplayedRemaining)state.remainingTicks6000=s.displayedRemaining6000;
        state.timeExtended=s.timeExtended&&!s.paused&&race.phase==RacePhase::Running;
        state.sectionCount=unsigned(race.sector);state.sectionCapacity=race.sectionCapacity;state.sectionTimes6000=race.sectionTimes6000;
        if(race.originalTiming&&race.phase==RacePhase::Finished&&!race.timeUp)state.finishTicks6000=race.elapsed6000;
        state.finishBanner=s.finishBanner;
        // Source 065032 reads the saved upgrade/package bytes as well as car:
        // AE86 packages A-C switch to the 12k racing-engine dial after stage 4.
        state.tachType=originalTachTypeForCar(s.frontend?unsigned(s.frontend->car):0u,
            s.frontend?s.frontend->battleProfile.byte(164):0,
            s.frontend?s.frontend->battleProfile.byte(152):0);
        state.rpm=originalTachDisplayRpm(v.rpm,state.tachType);
        state.edgeAnchored=true;
        originalHud.paint(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,state);
        if(state.timePanel&&!announcement){
            battlePresentation_.sectionCount=state.sectionCount;battlePresentation_.sectionCapacity=state.sectionCapacity;
            battlePresentation_.elapsed6000=state.elapsedTicks6000;battlePresentation_.cumulativeSections=state.sectionTimes6000;
            battlePresentation_.renderedSectionDurations.fill(0xffffffff);
            std::uint32_t previous=0;
            for(unsigned row=0;row<state.sectionCapacity&&row<=state.sectionCount;++row){
                const auto cumulative=row<state.sectionCount?state.sectionTimes6000[row]:state.finishTicks6000!=0xffffffff?state.finishTicks6000:state.elapsedTicks6000;
                battlePresentation_.renderedSectionDurations[row]=cumulative-previous;previous=cumulative;
            }
        }
        if(s.results&&s.results->livePanel&&!battleHud&&!announcement){
            const auto target=std::span<std::uint32_t>(pixels,std::size_t(width)*height);
            originalResults.paint(target,width,height,*s.results);
            originalBattleNames.paintTimeAttack(target,width,height,s.frontend?unsigned(s.frontend->car):0u,s.frontend?&s.frontend->battleProfile:nullptr);
        }
        if(battleHud&&!announcement){
            const auto frame=onlineBattle?s.onlineBattleHud.frame:s.battleHudFrame;
            const auto enemy=onlineBattle?0xffffffffu:s.battleEnemy;
            const auto mode=onlineBattle?3u:s.battleProfileMode;
            const bool newBattle=frame<lastBattleFrame||enemy!=lastBattleEnemy||mode!=lastBattleProfileMode;
            if(newBattle){battleAnimation={};battleFrameAnimation={};}
            const bool newFrame=newBattle||frame!=lastBattleFrame;
            if(newFrame)battleFrameAnimation=battleAnimation;
            OriginalBattleHudState battle; //0C71E0 source default plus caller fields
            battle.flags104=0x001ffffe|(s.rearView?1u:0u);battle.profileMode0C31C99C=mode;
            battle.frame204=frame;battle.validity96=onlineBattle?s.onlineBattleHud.rivalPositionFraction:s.battleRivalPositionFraction;
            battle.signedAdvantage100=onlineBattle?s.onlineBattleHud.advantage:s.battleAdvantage;
            // Render repeats and pause reuse the animation input for this 60 Hz tick.
            auto nextAnimation=battleFrameAnimation;
            const auto draws=drawOriginalBattleHud(battle,nextAnimation);
            if(newFrame){battleAnimation=nextAnimation;lastBattleFrame=frame;lastBattleEnemy=enemy;lastBattleProfileMode=mode;}
            const auto target=std::span<std::uint32_t>(pixels,std::size_t(width)*height);
            originalBattleHud.paintGame2d(target,width,height,draws,true);
            battlePresentation_.online=onlineBattle;battlePresentation_.profileMode=mode;
            battlePresentation_.signedAdvantage=battle.signedAdvantage100;
            for(const auto& draw:draws){
                if(draw.kind==OriginalBattleHudDraw::Kind::game2d){++battlePresentation_.game2dCommands;if(draw.draw.index==186)battlePresentation_.mirrorEnabled=true;}
                if(draw.kind==OriginalBattleHudDraw::Kind::portrait228)++battlePresentation_.portraitCommands;
            }
            const std::string name=onlineBattle?std::string{}:std::string(original::originalBattlePortraitBank(s.battleEnemy,s.battleProfileMode));
            if(!name.empty()){
                const auto& asset=portraits.at(name);const float fit=std::min(float(width)/640.f,float(height)/480.f);
                for(const auto& draw:draws)if(draw.kind==OriginalBattleHudDraw::Kind::portrait228){
                    auto chunk=asset.model.chunks.at(draw.draw.index);
                    for(auto& batch:chunk.batches)for(auto& vertex:batch.vertices){const auto p=original::transformOriginalPoint(draw.draw.matrix,{vertex.position.x,vertex.position.y,vertex.position.z});vertex.position={p[0],p[1],p[2]};}
                    SpritePlacement placement;placement.scale=100.f*fit;placement.invertY=true;placement.authoredHeight=0;placement.offsetX=float(width)-640.f*fit;
                    compositeOriginalMenuChunk(target,width,height,asset.textures,chunk,placement);
                }
            }
            if(frame>40){
                if(onlineBattle){
                    const auto& online=s.onlineBattleHud;
                    const auto rows=originalBattleNames.paintOnline(target,width,height,online.playerName,online.rivalName,online.playerCar,online.rivalCar);
                    battlePresentation_.game2dCommands+=4;
                    battlePresentation_.playerName=rows[0].text;battlePresentation_.rivalName=rows[1].text;
                    battlePresentation_.playerCarCode=rows[0].carCode;battlePresentation_.rivalCarCode=rows[1].carCode;
                    battlePresentation_.playerGlyphs=std::uint32_t(rows[0].glyphs.size());battlePresentation_.rivalGlyphs=std::uint32_t(rows[1].glyphs.size());
                }else originalBattleNames.paint(target,width,height,s.battleEnemy,s.battleProfileMode,
                    s.frontend?unsigned(s.frontend->car):0u,s.battleRivalCar,s.frontend?&s.frontend->battleProfile:nullptr);
            }
        }
        if(!s.paused&&race.originalTiming&&race.originalStartDigit>=0)originalHud.paintStartSignal(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,race.originalStartDigit,race.originalStartElapsed);
        else if(!s.paused&&!race.originalTiming&&race.phase==RacePhase::Countdown)originalHud.paintStartSignal(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,std::clamp(race.countdownDigit(),1,3),unsigned(180-race.countdown));
        else if(!s.paused&&!race.originalTiming&&race.phase==RacePhase::Running&&race.ticks<60)originalHud.paintStartSignal(std::span<std::uint32_t>(pixels,std::size_t(width)*height),width,height,0,180+unsigned(race.ticks));
    }
    return pixels;
}
}
