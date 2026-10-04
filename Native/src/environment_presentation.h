#pragma once
#include "renderer.h"
#include "driving_effects.h"
#include <fstream>

namespace idas3 {
// Original art/admission data, with a bounded host presentation clock. This
// owner never consumes the driving RNG or writes contact/handling state.
class EnvironmentPresentation {
public:
    NativeTextureBank leafTextures,flareTextures;
    static constexpr unsigned capacity=160; // original 80-leaf pool per car
    struct Leaf {Vec3 position{},velocity{},angles{},spin{};float floor=0;unsigned frames=0,chunk=4,owner=0;};
    struct FlarePiece {float distance=0,size=0;unsigned chunk=0;};
    void load(const std::filesystem::path& root){
        if(!leafModel_.chunks.empty())return;
        const auto base=root/"data/original_assets/environment";
        auto leaf=NativeModel::load(base/"leaf/leaf.idasmesh");
        auto flare=NativeModel::load(base/"flare/flare.idasmesh");
        auto lt=NativeTextureBank::load(base/"leaf/textures.idastex");
        auto ft=NativeTextureBank::load(base/"flare/textures.idastex");
        if(leaf.chunks.size()!=8||flare.chunks.size()!=7||lt.size()!=4||ft.size()!=7)
            throw std::runtime_error("Unexpected environment effect assets");
        std::ifstream file(base/"environment.bin",std::ios::binary);
        auto read=[&](void* p,std::size_t n){if(!file.read(static_cast<char*>(p),std::streamsize(n)))throw std::runtime_error("Incomplete environment effect data");};
        std::array<char,8> magic;read(magic.data(),magic.size());
        if(std::string(magic.data(),magic.size())!="IDASENV1")throw std::runtime_error("Invalid environment effect data");
        read(sun_.data(),sizeof(sun_));read(pieces_.data(),sizeof(pieces_));
        for(auto& table:visibility_){unsigned count=0;read(&count,4);if(count==0||count>10000)throw std::runtime_error("Invalid sun visibility table");table.resize(count);read(table.data(),count);}
        leafModel_=std::move(leaf);flareModel_=std::move(flare);leafTextures=std::move(lt);flareTextures=std::move(ft);
    }
    void reset(){leaves_={};cursor_={};last_={};seen_={};accumulator_=0;seed_=0x4c454146;}
    static bool vegetation(unsigned flags){const auto material=flags&15u;return material==1||material==2||material==11;}
    void advance(double dt,bool enabled,bool paused,const std::array<DrivingEffects::Car,2>& cars,
                 const std::array<std::array<unsigned,4>,2>& surfaces){
        if(!enabled){reset();return;}if(paused||!std::isfinite(dt)||dt<=0)return;
        accumulator_+=std::min(dt,.1);
        while(accumulator_+1e-9>=1./60){accumulator_-=1./60;
            for(unsigned c=0;c<2;++c){const auto& car=cars[c];
                if(!car.visible||(seen_[c]&&length(car.position-last_[c])>8))for(auto& p:leaves_)if(p.owner==c)p.frames=0;
                if(car.visible){last_[c]=car.position;seen_[c]=true;}
            }
            for(auto& p:leaves_)if(p.frames){
                --p.frames;p.velocity*=.95f;p.velocity.y-=.0007f;p.position+=p.velocity;p.angles+=p.spin;
                if(p.position.y<p.floor){p.position.y=p.floor;p.velocity.y=std::max(0.f,p.velocity.y);p.angles.x=p.angles.z=0;}
            }
            for(unsigned c=0;c<2;++c){const auto& car=cars[c];
                if(!car.visible||!car.grounded||car.speed<1.f)continue;
                for(unsigned wheel=0;wheel<4;++wheel)if(vegetation(surfaces[c][wheel])){
                    //06A66A: materials1,2,11;0D4B60: chunks4/5/6 at X0,+.1,-.1.
                    for(unsigned k=0;k<3;++k){auto& p=leaves_[c*80+(cursor_[c]++%80)];p={};
                        p.owner=c;p.chunk=4+k;p.frames=90;p.floor=car.points[wheel].y+.008f;
                        p.position=car.points[wheel]+Vec3{k==1?.1f:k==2?-.1f:0.f,.02f,0};
                        p.angles={random()*2*pi,random()*2*pi,random()*2*pi};
                        p.spin={random()*.31415927f,0,random()*.062831856f};
                        // Host wake: raise and scatter contact leaves without
                        // changing the source car velocity or shared random stream.
                        p.velocity=-forward(car.yaw)*std::min(car.speed*.0015f,.07f)+right(car.yaw)*((random()-.5f)*.035f);
                        p.velocity.y=.025f+random()*.035f;
                    }
                }
            }
        }
    }
    void appendLeaves(Mesh& mesh,unsigned textureBase)const{
        if(leafModel_.chunks.empty())return;
        for(const auto& p:leaves_)if(p.frames){NativeAssembly a;auto& i=a.instances.emplace_back();i.chunk=p.chunk;
            i.transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            mesh.originalCar(leafModel_,a,p.position,p.angles.y,p.angles.x,p.angles.z,textureBase);
        }
    }
    bool sunVisible(unsigned course,bool night,bool wet,bool reverse,float progress)const{
        if(course>=8||night||wet||!std::isfinite(progress))return false;
        const auto& table=visibility_[course];if(table.empty()||progress<0||progress>=float(table.size()))return false;
        auto index=std::size_t(progress);if(reverse)index=table.size()-1-index;
        return table[index]!=0;
    }
    Vec3 sunDirection(unsigned course)const{return course<8?normalized(sun_[course]):Vec3{};}
    unsigned appendSun(Mesh& mesh,unsigned course,bool night,bool wet,bool reverse,float progress,
                       Vec3 eye,Vec3 target,Vec3 cameraUp,float verticalFov,float nearClip,unsigned textureBase)const{
        if(!sunVisible(course,night,wet,reverse,progress)||flareModel_.chunks.empty())return 0;
        const auto view=normalized(target-eye),across=normalized(cross(view,cameraUp)),up=normalized(cross(across,view));
        //180C94: retain camera height while removing horizontal translation.
        const auto sun=normalized(sun_[course]-Vec3{0,eye.y,0});const float facing=dot(view,sun);
        if(facing<.2f)return 0;
        const float strength=std::min(1.f,(.2f+facing)/1.2f)*.8f;
        const float depth=std::max(nearClip*1.1f,2.f),halfHeight=depth*std::tan(verticalFov*.5f);
        const Vec3 projected=across*(dot(sun,across)/facing*depth)+up*(dot(sun,up)/facing*depth);
        unsigned count=0;
        //180D3A..180E18 uses entries1..11. Host camera-space positioning keeps
        // the authored lens pieces beneath HUD elements at any display aspect.
        for(unsigned n=1;n<pieces_.size();++n){const auto& p=pieces_[n];if(p.chunk>=flareModel_.chunks.size())continue;
            const auto center=eye+view*depth+projected*(-p.distance*.25f);
            const float size=halfHeight*.24f*p.size*strength;
            for(const auto& b:flareModel_.chunks[p.chunk].batches){
                mesh.beginRange(textureBase+b.material[9],b.ich[2],b.ich[0],
                    (b.ich[1]&~(7u<<29))|(7u<<29)|(1u<<26),b.material[2],true,true,0,false,std::nullopt,false,1);
                // Lens flare is an optical overlay: source additive blend,
                // no depth writes, no rear-camera reuse of the primary lens.
                for(auto index:b.indices){const auto& v=b.vertices[index];
                    mesh.vertices.push_back({center+across*(v.position.x*size)+up*(v.position.z*size),-view,{1,1,1,strength},v.u,v.v});
                    ++mesh.ranges.back().count;
                }
            }++count;
        }return count;
    }
    const auto& leaves()const{return leaves_;}
    unsigned leafCount()const{unsigned count=0;for(const auto& p:leaves_)count+=p.frames!=0;return count;}
private:
    NativeModel leafModel_,flareModel_;
    std::array<Vec3,8> sun_{};
    std::array<FlarePiece,12> pieces_{};
    std::array<std::vector<std::uint8_t>,8> visibility_;
    std::array<Leaf,capacity> leaves_{};
    std::array<unsigned,2> cursor_{};
    std::array<Vec3,2> last_{};
    std::array<bool,2> seen_{};
    double accumulator_=0;
    std::uint32_t seed_=0x4c454146;
    float random(){seed_=seed_*1664525u+1013904223u;return float(seed_>>8)*(1.f/16777216.f);}
};
}
