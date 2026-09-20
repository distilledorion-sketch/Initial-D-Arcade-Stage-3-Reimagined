#include "original_demo_overlays.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
using namespace idas3;using namespace idas3::original;using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,bank=0x0cd01000,materials=0x0cd02000,objects=0x0cd03000,vt=0x0cd09000,stack=0x0cfee000,applyMaterial=0x00ee0000;
std::uint64_t comparisons=0,instructions=0,drawCount=0,fadeCount=0;
void eq(unsigned a,unsigned b,const char* label){++comparisons;if(a!=b)throw std::runtime_error(std::string(label)+": "+hex(a)+" != "+hex(b));}
void feq(float a,float b,const char* label){eq(std::bit_cast<unsigned>(a),std::bit_cast<unsigned>(b),label);}
void sourceFrames(RefMemory& m,const OriginalDemoOverlays& overlays){
    for(unsigned frame=0;frame<=5841;++frame){m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x1000,0x2000);
        m.write32(owner+1188,frame);m.write32(owner+1192,bank);m.write32(owner+1200,206);m.write32(owner+1204,0);m.write32(owner+1208,materials);m.write32(owner+1216,frame%3);
        m.write16(vt+24,0);m.write32(vt+28,applyMaterial);
        for(unsigned i=0;i<206;++i){const auto& row=overlays.cues()[i];if(row.start!=row.fadeInEnd||row.fadeOutStart!=row.end){m.write32(materials+i*4,objects+i*32);m.write32(objects+i*32,vt);}}
        RefCpu c(m);c.r[12]=owner;c.r[14]=stack;c.r[15]=stack-128;
        std::vector<OriginalDemoOverlayDraw> sourceDraws;std::vector<OriginalDemoFadeDraw> sourceFades;Vec3 translate;float sx=1,sy=1;
        for(unsigned fn:{0x0c156700u,0x0c1fcc60u,0x0c1fbd60u,0x0c1f65c0u,applyMaterial})c.callHooks[fn]=[](auto&){};
        c.callHooks[0x0c1f6ac0]=[&](auto& x){translate={x.getFloat(4),x.getFloat(5),x.getFloat(6)};};
        c.callHooks[0x0c1f6a00]=[&](auto& x){sx=m.readFloat(x.r[4]);sy=m.readFloat(x.r[4]+4);feq(m.readFloat(x.r[4]+8),1,"source scaleZ");};
        c.callHooks[0x0c0d7e60]=[&](auto& x){m.write32(x.r[4]+4,x.r[5]);};
        c.callHooks[0x0c145c00]=[&](auto& x){const unsigned row=x.r[13]/4;eq(x.r[4],bank,"source overlay bank");const auto material=m.read32(materials+row*4);
            OriginalDemoOverlayDraw d;d.row=row;d.chunk=x.r[5];d.alpha=material?x.r[9]>>24:x.r[9];d.translation=translate;d.scaleX=sx;d.scaleY=sy;d.materialOverride=material!=0;
            if(material)eq(m.read32(material+4),0xffffffu|(d.alpha<<24),"source material color");sourceDraws.push_back(d);
        };
        c.callHooks[0x0c0c5200]=[&](auto& x){sourceFades.push_back({x.r[5],x.getFloat(4)});};
        try{instructions+=c.run(0x0c0e7648,0x0c0e7cce,25000);}catch(const std::exception& e){throw std::runtime_error("frame "+std::to_string(frame)+" at "+hex(c.pc)+": "+e.what());}
        const auto nativeDraws=overlays.draws(frame);const auto nativeFades=overlays.fades(frame);eq(sourceDraws.size(),nativeDraws.size(),"source draw count");eq(sourceFades.size(),nativeFades.size(),"source fade count");
        for(unsigned i=0;i<sourceDraws.size();++i){const auto& a=sourceDraws[i];const auto& b=nativeDraws[i];
            eq(a.row,b.row,"row");eq(a.chunk,b.chunk,"chunk");eq(a.alpha,b.alpha,"alpha");eq(a.materialOverride,b.materialOverride,"material override");
            feq(a.translation.x,b.translation.x,"translationX");feq(a.translation.y,b.translation.y,"translationY");feq(a.translation.z,b.translation.z,"translationZ");feq(a.scaleX,b.scaleX,"scaleX");feq(a.scaleY,b.scaleY,"scaleY");++drawCount;
        }
        for(unsigned i=0;i<sourceFades.size();++i){eq(sourceFades[i].argb,nativeFades[i].argb,"fade argb");feq(sourceFades[i].depth,nativeFades[i].depth,"fade depth");++fadeCount;}
    }
}
void bmp(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels){std::ofstream out(path,std::ios::binary);
    const std::array<unsigned char,14> header{'B','M',54,0xc0,0x12,0,0,0,0,0,54,0,0,0};out.write(reinterpret_cast<const char*>(header.data()),14);
    const std::array<unsigned,10> info{40,640,unsigned(-480),0x00200001,0,640*480*4,0,0,0,0};out.write(reinterpret_cast<const char*>(info.data()),40);out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
}
void previews(const OriginalDemoOverlays& overlays,const std::filesystem::path& output){std::filesystem::create_directories(output);
    for(unsigned frame:{0u,1200u,1300u,1380u,1450u,1800u,2240u,2800u,3500u,4600u,5650u}){std::vector<std::uint32_t> pixels(640*480,0xff243042);overlays.paint(pixels,640,480,frame);bmp(output/("overlays-"+std::to_string(frame)+".bmp"),pixels);}
}
void backdrop(const OriginalDemoOverlays& overlays){
    const std::array<std::pair<unsigned,unsigned>,7> cases{{{0,0xff000000u},{500,0},{1150,0x7f000000u},{1250,0xff000000u},{2180,0x6dffffffu},{2240,0xffffffffu},{5315,0x7f000000u}}};
    for(auto [w,h]:std::array<std::pair<int,int>,4>{{{1280,720},{1919,1079},{640,480},{420,900}}})for(auto [frame,color]:cases){
        const float fit=std::min(float(w)/640.f,float(h)/480.f);const int dw=std::max(1,int(640*fit)),dh=std::max(1,int(480*fit)),left=(w-dw)/2,top=(h-dh)/2;
        std::vector<unsigned> pixels(std::size_t(w)*h,0);for(int y=top;y<top+dh;++y)for(int x=left;x<left+dw;++x)pixels[std::size_t(y)*w+x]=unsigned((y*w+x)*2654435761u);
        const auto before=pixels;overlays.extendBackdrop(pixels,w,h,frame);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){const bool center=x>=left&&x<left+dw&&y>=top&&y<top+dh;eq(pixels[std::size_t(y)*w+x],center?before[std::size_t(y)*w+x]:color,center?"backdrop preserves fitted canvas":"source fade extends wings");}
    }
    // These frames contain a fade without any original polygon covering(0,0).
    // Verify straight-alpha HUD output and preserve the prior opaque result.
    for(auto [frame,color]:cases){std::vector<unsigned> transparent(640*480,0),opaque(640*480,0xff243042u);overlays.paint(transparent,640,480,frame);overlays.paint(opaque,640,480,frame);
        eq(transparent[0],color,"central transparent fade alpha");unsigned expected=0xff000000;const unsigned a=color>>24;
        for(unsigned shift:{0u,8u,16u})expected|=((((0xff243042u>>shift)&255)*(255-a)+((color>>shift)&255)*a)/255)<<shift;
        eq(opaque[0],expected,"opaque source fade unchanged");
    }
}
unsigned hudOver(unsigned overlay,unsigned background){const unsigned a=overlay>>24;unsigned value=0xff000000u;
    for(unsigned shift:{0u,8u,16u})value|=((((overlay>>shift)&255)*a+((background>>shift)&255)*(255-a)+127)/255)<<shift;return value;
}
void overlayCarrier(const OriginalDemoOverlays& overlays){
    NativeTextureBank empty;NativeModelChunk fixture;NativeModelBatch batch;batch.material[2]=1;batch.material[9]=0xffffffffu;
    for(Vec3 p:{Vec3{0,0,0},Vec3{1,0,0},Vec3{0,1,0},Vec3{1,1,0}}){NativeModelVertex v;v.position=p;batch.vertices.push_back(v);}batch.indices={0,1,2,2,1,3};fixture.batches.push_back(batch);
    for(unsigned mode:{0u,1u})for(unsigned a:{0u,1u,32u,64u,127u,128u,254u,255u})for(unsigned background:{0xff000000u,0xff243042u,0xffd8aa90u,0xffffffffu}){
        auto& b=fixture.batches[0];b.ich[2]=mode?0x20100000u:0x94100000u;b.material[3]=(a<<24)|0x00285078u;
        std::array<unsigned,1> carrier{0},direct{background};SpritePlacement reference;compositeOriginalMenuChunk(direct,1,1,empty,fixture,reference);
        SpritePlacement overlay;overlay.straightAlphaOverlay=true;compositeOriginalMenuChunk(carrier,1,1,empty,fixture,overlay);
        eq(carrier[0]>>24,mode?255:a,"overlay carrier preserves edge alpha");const auto actual=hudOver(carrier[0],background);
        for(unsigned shift:{0u,8u,16u}){++comparisons;if(std::abs(int((actual>>shift)&255)-int((direct[0]>>shift)&255))>1)throw std::runtime_error("Overlay carrier differs from direct source blend");}
    }
    // Original artwork/fades over a real opaque scene color must match the
    // same drawing carried through a transparent HUD and blended once.
    for(unsigned frame:{500u,1150u,1200u,1450u,2180u,2240u,2800u,3500u,5315u}){
        constexpr unsigned background=0xff243042u;std::vector<unsigned> direct(640*480,background),carrier(640*480,0);
        overlays.paint(direct,640,480,frame);overlays.paintOverlay(carrier,640,480,frame);
        for(unsigned i=0;i<direct.size();++i){const auto actual=hudOver(carrier[i],background);
            for(unsigned shift:{0u,8u,16u}){++comparisons;if(std::abs(int((actual>>shift)&255)-int((direct[i]>>shift)&255))>2)throw std::runtime_error("Original overlay/direct RGB mismatch frame "+std::to_string(frame)+" pixel "+std::to_string(i));}
        }
    }
}
}
int main(int argc,char** argv){try{if(argc!=4)throw std::runtime_error("image root output required");RefMemory m(argv[1]);OriginalDemoOverlays overlays;overlays.load(argv[2]);sourceFrames(m,overlays);backdrop(overlays);overlayCarrier(overlays);previews(overlays,argv[3]);
    std::cout<<"Original demo overlays: 5842 source frames, "<<drawCount<<" polygon draws, "<<fadeCount<<" fades, "<<comparisons<<" comparisons, "<<instructions<<" original instructions\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
