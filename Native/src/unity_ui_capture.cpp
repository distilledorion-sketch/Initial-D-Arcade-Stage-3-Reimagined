#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "unity_ui_capture.h"
#include "native_assets.h"
#if !defined(IDAS3_PORTABLE_SCENE)
#include <windows.h>
#endif
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
namespace idas3 {
static_assert(sizeof(UnityUiVertex)==24&&sizeof(UnityUiDraw)==48&&sizeof(UnityUiFrame)==40);
namespace {
struct Surface{int width=0,height=0;std::vector<UnityUiVertex> vertices;std::vector<UnityUiDraw> draws;};
struct Capture {
 unsigned hudGroup=0;bool enabled=false;UnityUiFrame frame{sizeof(UnityUiFrame)};Surface output;
 std::unordered_map<const std::uint32_t*,Surface> surfaces;
 std::vector<NativeImage> textures;
 std::unordered_map<const std::uint32_t*,std::pair<std::size_t,unsigned>> texturePointers;
 std::map<std::wstring,unsigned> glyphs;
};
Capture& state(){static Capture s;return s;}
#if defined(IDAS3_PORTABLE_SCENE)
bool performanceBaseline(){return false;}
#else
bool performanceBaseline(){
 static const bool enabled=[](){
  const std::wstring_view command=GetCommandLineW(),flag=L"-idas3-scene-perf-baseline";
  const auto separator=[](wchar_t c){return c==L' '||c==L'\t'||c==L'\r'||c==L'\n'||c==L'\"';};
  for(std::size_t at=0;(at=command.find(flag,at))!=std::wstring_view::npos;++at)
   if((at==0||separator(command[at-1]))&&(at+flag.size()==command.size()||separator(command[at+flag.size()])))return true;
  return false;
 }();
 return enabled;
}
#endif
unsigned texture(const NativeImage& image){
 auto& s=state();const auto pointer=image.argb.data();const auto bytes=image.argb.size();
 if(auto it=s.texturePointers.find(pointer);it!=s.texturePointers.end()&&it->second.first==bytes)return it->second.second;
 if(s.textures.size()>=16384)throw std::runtime_error("Unity UI source texture budget exceeded");
 const auto index=unsigned(s.textures.size());s.textures.push_back(image);s.texturePointers[pointer]={bytes,index};return index;
}
Surface& surface(const std::uint32_t* pointer,int width,int height){auto& s=state().surfaces[pointer];s.width=width;s.height=height;return s;}
void append(Surface& dst,const Surface& src,float sx,float sy,float ox,float oy,unsigned flags=0,float opacity=1){
 for(auto d:src.draws){
  if(d.clipRight<=d.clipLeft||d.clipBottom<=d.clipTop)continue;
  const auto first=unsigned(dst.vertices.size());
  for(unsigned i=d.first;i<d.first+d.count;++i){auto v=src.vertices[i];v.x=v.x*sx+ox;v.y=v.y*sy+oy;dst.vertices.push_back(v);}
  d.first=first;d.flags|=flags;d.opacity*=opacity;d.clipLeft=d.clipLeft*sx+ox;d.clipTop=d.clipTop*sy+oy;d.clipRight=d.clipRight*sx+ox;d.clipBottom=d.clipBottom*sy+oy;dst.draws.push_back(d);
 }
}
}
bool unityUiEnabled(){return state().enabled;}
bool unityUiFrameReuseEnabled(){return unityUiEnabled()&&!performanceBaseline();}
void unityUiForgetTexture(const NativeImage& image){state().texturePointers.erase(image.argb.data());}
bool unityUiTriangle(const std::uint32_t* target,int width,int height,const NativeImage& image,UnityUiVertex a,UnityUiVertex b,UnityUiVertex c,float opacity,unsigned tsp,bool original,unsigned pcw){
 if(!unityUiEnabled())return false;
 auto& out=surface(target,width,height);const auto first=unsigned(out.vertices.size());out.vertices.insert(out.vertices.end(),{a,b,c});
 out.draws.push_back({first,3,texture(image),tsp,pcw,(original?1u:0u)|(state().hudGroup<<8),opacity,0,0,0,float(width),float(height)});return true;
}
unsigned unityUiHudGroup(){return state().hudGroup;}
unsigned unityUiSetHudGroup(unsigned group){auto& value=state().hudGroup;const auto old=value;value=group;return old;}
void unityUiMarkHud(const std::uint32_t* target){
 if(!unityUiEnabled())return;
 auto found=state().surfaces.find(target);if(found==state().surfaces.end())return;
 for(auto& draw:found->second.draws)draw.flags|=8u;
}
void unityUiClear(const std::uint32_t* target,int width,int height,unsigned argb){
 if(!unityUiEnabled())return;auto& out=surface(target,width,height);out.vertices.clear();out.draws.clear();
 if(argb>>24)unityUiSolid(target,width,height,0,0,float(width),float(height),argb);
}
void unityUiCopy(const std::uint32_t* destination,const std::uint32_t* source,int width,int height,float sx,float sy,float ox,float oy,bool add,bool additive){
 if(!unityUiEnabled())return;auto& s=state();const auto found=s.surfaces.find(source);
 if(found==s.surfaces.end()){++s.frame.unresolvedSurfaces;return;}
 const Surface copied=found->second;auto& dst=surface(destination,width,height);if(!add){dst.vertices.clear();dst.draws.clear();}
 append(dst,copied,sx,sy,ox,oy,additive?4u:0u);
}
void unityUiCopyRegion(const std::uint32_t* target,int width,int height,float left,float top,float right,float bottom,float x,float y,float w,float h){
 if(!unityUiEnabled()||right<=left||bottom<=top)return;auto& st=state();auto found=st.surfaces.find(target);if(found==st.surfaces.end()){++st.frame.unresolvedSurfaces;return;}
 auto copy=found->second;for(auto& d:copy.draws){d.clipLeft=std::max(d.clipLeft,left);d.clipTop=std::max(d.clipTop,top);d.clipRight=std::min(d.clipRight,right);d.clipBottom=std::min(d.clipBottom,bottom);}
 append(surface(target,width,height),copy,w/(right-left),h/(bottom-top),x-left*w/(right-left),y-top*h/(bottom-top));
}
bool unityUiCopyImage(const std::uint32_t* destination,int width,int height,const NativeImage& image,float x,float y,float w,float h,float opacity){
 if(!unityUiEnabled())return false;const auto found=state().surfaces.find(image.argb.data());if(found==state().surfaces.end())return false;
 const auto copy=found->second;append(surface(destination,width,height),copy,w/image.width,h/image.height,x,y,0,opacity);return true;
}
void unityUiSolid(const std::uint32_t* target,int width,int height,float x,float y,float w,float h,unsigned argb){
 if(!unityUiEnabled())return;static const NativeImage white{1,1,{0xffffffff}};
 const UnityUiVertex a{x,y,0,0,argb,0},b{x+w,y,0,0,argb,0},c{x,y+h,0,0,argb,0},d{x+w,y+h,0,0,argb,0};
 unityUiTriangle(target,width,height,white,a,b,c,1,(4u<<29)|(5u<<26)|(1u<<20)|(3u<<6),true,0);
 unityUiTriangle(target,width,height,white,c,b,d,1,(4u<<29)|(5u<<26)|(1u<<20)|(3u<<6),true,0);
}
void unityUiLine(const std::uint32_t* target,int width,int height,float x,float y,float xx,float yy,float thickness,unsigned argb){
 if(!unityUiEnabled())return;static const NativeImage white{1,1,{0xffffffff}};
 const float length=std::hypot(xx-x,yy-y);if(length<=0)return;const float dx=(yy-y)*thickness/(2*length),dy=(x-xx)*thickness/(2*length);
 const UnityUiVertex a{x+dx,y+dy,0,0,argb,0},b{xx+dx,yy+dy,0,0,argb,0},c{x-dx,y-dy,0,0,argb,0},d{xx-dx,yy-dy,0,0,argb,0};
 unityUiTriangle(target,width,height,white,a,b,c,1,(4u<<29)|(5u<<26)|(1u<<20),true,0);unityUiTriangle(target,width,height,white,c,b,d,1,(4u<<29)|(5u<<26)|(1u<<20),true,0);
}
#if !defined(IDAS3_PORTABLE_SCENE)
void unityUiText(const std::uint32_t* target,int width,int height,void* context,float x,float y,const wchar_t* text,int length,unsigned argb){
 if(!unityUiEnabled()||length<=0)return;auto dc=static_cast<HDC>(context);auto& s=state();
 LOGFONTW font{};GetObjectW(GetCurrentObject(dc,OBJ_FONT),sizeof(font),&font);SIZE total{};std::vector<int> advances(std::size_t(length),0);
 GetTextExtentExPointW(dc,text,length,INT_MAX,nullptr,advances.data(),&total);
 for(int i=0;i<length;++i){
  const auto key=std::wstring(font.lfFaceName)+L":"+std::to_wstring(font.lfHeight)+L":"+std::to_wstring(font.lfWeight)+L":"+std::to_wstring(unsigned(text[i]));
  auto found=s.glyphs.find(key);unsigned id;
  if(found==s.glyphs.end()){
   SIZE size{};GetTextExtentPoint32W(dc,text+i,1,&size);const int gw=std::max(1,int(size.cx)),gh=std::max(1,int(size.cy));
   BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=gw;info.bmiHeader.biHeight=-gh;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
   std::uint32_t* bits=nullptr;auto glyphDC=CreateCompatibleDC(dc);auto bitmap=CreateDIBSection(glyphDC,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&bits),nullptr,0);
   if(!glyphDC||!bitmap){if(bitmap)DeleteObject(bitmap);if(glyphDC)DeleteDC(glyphDC);throw std::runtime_error("Unity UI host glyph allocation failed");}
   auto oldBitmap=SelectObject(glyphDC,bitmap);auto oldFont=SelectObject(glyphDC,GetCurrentObject(dc,OBJ_FONT));SetBkMode(glyphDC,TRANSPARENT);SetTextColor(glyphDC,RGB(255,255,255));std::fill_n(bits,std::size_t(gw)*gh,0u);TextOutW(glyphDC,0,0,text+i,1);GdiFlush();
   NativeImage image{unsigned(gw),unsigned(gh),{}};image.argb.resize(std::size_t(gw)*gh);for(std::size_t p=0;p<image.argb.size();++p)image.argb[p]=(std::max({bits[p]&255,(bits[p]>>8)&255,(bits[p]>>16)&255})<<24)|0xffffff;
   SelectObject(glyphDC,oldFont);SelectObject(glyphDC,oldBitmap);DeleteObject(bitmap);DeleteDC(glyphDC);
   id=unsigned(s.textures.size());s.textures.push_back(std::move(image));s.glyphs.emplace(key,id);
  }else id=found->second;
  const auto& image=s.textures[id];auto& out=surface(target,width,height);const float gx=x+(i?advances[std::size_t(i)-1]:0),gy=y;
  const unsigned first=unsigned(out.vertices.size());UnityUiVertex a{gx,gy,0,0,argb,0},b{gx+image.width,gy,1,0,argb,0},c{gx,gy+image.height,0,1,argb,0},d{gx+image.width,gy+image.height,1,1,argb,0};
  out.vertices.insert(out.vertices.end(),{a,b,c,c,b,d});out.draws.push_back({first,6,id,(1u<<15)|(1u<<16)|(1u<<20)|(3u<<6),8,0,1,0,0,0,float(width),float(height)});
 }
}
#endif
void unityUiSubmit(const std::uint32_t* pixels,int width,int height,bool behind,bool additive,bool originalCanvas){
 if(!unityUiEnabled()||!pixels)return;auto& s=state();const auto found=s.surfaces.find(pixels);
 if(found==s.surfaces.end()){++s.frame.unresolvedSurfaces;return;}
 const float sx=float(s.frame.width)/width,sy=float(s.frame.height)/height;
 const float fit=std::min(sx,sy);append(s.output,found->second,originalCanvas?fit:sx,originalCanvas?fit:sy,
  originalCanvas?(s.frame.width-width*fit)*.5f:0,originalCanvas?(s.frame.height-height*fit)*.5f:0,(behind?2u:0u)|(additive?4u:0u));
}
}
void Idas3UiEnable(int enabled){auto& s=idas3::state();s={};s.enabled=enabled!=0;}
void Idas3UiBeginFrame(int width,int height){auto& s=idas3::state();if(!s.enabled)return;
 if(idas3::unityUiFrameReuseEnabled()){s.output.width=s.output.height=0;s.output.vertices.clear();s.output.draws.clear();}
 else s.output={};
 s.frame.width=unsigned(width);s.frame.height=unsigned(height);s.frame.unresolvedSurfaces=0;++s.frame.revision;}
int Idas3UiGetFrame(idas3::UnityUiFrame* frame){try{if(!frame||frame->size!=sizeof(*frame))return 0;auto& s=idas3::state();s.frame.drawCount=unsigned(s.output.draws.size());s.frame.vertexCount=unsigned(s.output.vertices.size());s.frame.textureCount=unsigned(s.textures.size());*frame=s.frame;return 1;}catch(...){return 0;}}
int Idas3UiCopyDraws(idas3::UnityUiDraw* dst,int capacity){auto& v=idas3::state().output.draws;if(capacity<0||std::size_t(capacity)<v.size()||(!dst&&!v.empty()))return 0;std::copy(v.begin(),v.end(),dst);return int(v.size());}
int Idas3UiCopyVertices(idas3::UnityUiVertex* dst,int capacity){auto& v=idas3::state().output.vertices;if(capacity<0||std::size_t(capacity)<v.size()||(!dst&&!v.empty()))return 0;std::copy(v.begin(),v.end(),dst);return int(v.size());}
int Idas3UiGetTextureInfo(unsigned id,idas3::UnityUiTextureInfo* info){auto& images=idas3::state().textures;if(!info||info->size!=sizeof(*info)||id>=images.size())return 0;const auto& im=images[id];*info={sizeof(*info),im.width,im.height,unsigned(im.argb.size()*4)};return 1;}
int Idas3UiCopyTextureRGBA(unsigned id,void* dst,int capacity){auto& images=idas3::state().textures;if(id>=images.size()||capacity<0||!dst)return 0;const auto& im=images[id];if(std::size_t(capacity)<im.argb.size()*4)return 0;auto* bytes=static_cast<std::uint8_t*>(dst);for(auto value:im.argb){*bytes++=std::uint8_t(value>>16);*bytes++=std::uint8_t(value>>8);*bytes++=std::uint8_t(value);*bytes++=std::uint8_t(value>>24);}return int(im.argb.size()*4);}
