#include "original_body_contact.h"
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
using Point=std::array<float,3>;
constexpr std::array<std::array<unsigned,2>,12> edges{{{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}}}; //25C944
constexpr std::array<unsigned,25> masks{1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,1,1,1,0,0,1,1,1}; //25C9A4
Point point(const OriginalBodyShape& s,unsigned offset){return {s.f(offset),s.f(offset+4),s.f(offset+8)};}
void storePoint(OriginalBodyShape& s,unsigned offset,const Point& p){for(unsigned i=0;i<3;++i)s.setf(offset+i*4,p[i]);}
Point subtract(Point a,const Point& b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
float dot4(const std::array<float,4>& a,const std::array<float,4>& b){double sum=double(a[0])*double(b[0]);for(unsigned i=1;i<4;++i)sum+=double(a[i])*double(b[i]);return float(sum);}
float lengthSquared(const Point& p){return dot4({p[0],p[1],p[2],0.f},{p[0],p[1],p[2],0.f});}
float magnitude(const Point& p){return std::sqrt(lengthSquared(p));}
OriginalMatrix matrix(const OriginalBodyShape& s,unsigned offset){OriginalMatrix m;for(unsigned i=0;i<16;++i)m.elements[i]=s.f(offset+i*4);return m;}
void storeMatrix(OriginalBodyShape& s,unsigned offset,const OriginalMatrix& m){for(unsigned i=0;i<16;++i)s.setf(offset+i*4,m.elements[i]);}
//1F66A0 transposes the rigid3x3 and negates its translation projections.
// Retain its FIPR operands and zero/subtraction sequence, including signed zero.
OriginalMatrix inverseRigid(OriginalMatrix m){auto& a=m.elements;a[3]=0.f;a[11]-=a[11];a[7]=0.f;a[15]=1.f;
    const std::array<float,4> translation{a[12],a[13],a[14],a[15]};const float old1=a[1];
    a[3]=dot4({a[0],a[1],a[2],a[3]},translation);a[1]=a[4];
    a[7]=dot4({a[4],a[5],a[6],a[7]},translation);a[4]=old1;
    a[11]=dot4({a[8],a[9],a[10],a[11]},translation);a[12]=0.f;a[14]-=a[14];a[12]-=a[3];a[13]=0.f;a[13]-=a[7];
    const float old2=a[2];a[14]-=a[11];a[2]=a[8];a[3]-=a[3];a[8]=old2;a[7]-=a[7];const float old6=a[6];a[11]-=a[11];a[6]=a[9];a[9]=old6;return m;
}
OriginalMatrix multiply(const OriginalMatrix& a,const OriginalMatrix& b){OriginalMatrix out;for(unsigned column=0;column<4;++column){auto v=transformOriginalVector(a,{b.elements[column*4],b.elements[column*4+1],b.elements[column*4+2],b.elements[column*4+3]});for(unsigned row=0;row<4;++row)out.elements[column*4+row]=v[row];}return out;}
Point direction(const OriginalMatrix& m,const Point& p){auto v=transformOriginalVector(m,{p[0],p[1],p[2],0.f});return {v[0],v[1],v[2]};}
void prepare(OriginalBodyShape& s,const std::array<std::uint32_t,42>& actor,const OriginalRivalData& data,const OriginalFscaTable& fsca){
    const auto flags=actor[20],car=flags&63;if(car>=35)throw std::invalid_argument("Original body shape requires one of35 car geometry rows");
    s.setu(0,(flags>>8)&31);s.setf(4,data.scalar(0x0c2716b8+car*44));s.setf(8,std::bit_cast<float>(0x3f99999au));s.setf(12,data.scalar(0x0c2716c0+car*44));
    for(unsigned i=0;i<3;++i){s.setu(16+i*4,actor[i]);s.setu(28+i*4,actor[6+i]);}
    s.setu(40,0);const auto world=originalActorMatrix(point(s,16),point(s,28),fsca);storeMatrix(s,496,world);storeMatrix(s,560,inverseRigid(world));s.setf(492,magnitude(point(s,4)));
}
//0C00C0's eight-corner construction uses explicit add/subtract, not FTRV.
std::array<Point,8> corners(const OriginalMatrix& transform,const Point& half){std::array<Point,8> out;
    for(unsigned axis=0;axis<3;++axis){const float x=half[0]*transform.elements[axis],y=half[1]*transform.elements[axis+4],z=half[2]*transform.elements[axis+8],t=transform.elements[axis+12];
        float plusX=t+x,plusY=plusX+y,minusY=plusX-y;out[0][axis]=plusY+z;out[1][axis]=plusY-z;out[2][axis]=minusY+z;out[3][axis]=minusY-z;
        float minusX=t-x;plusY=minusX+y;minusY=minusX-y;out[4][axis]=plusY+z;out[5][axis]=plusY-z;out[6][axis]=minusY+z;out[7][axis]=minusY-z;
    }return out;
}
int classify(float value,float half){if(value>half)return 2;if(value==half)return 1;if(value> -half)return 0;if(value== -half)return -1;return -2;} //0BFC20
//0BFD40/X,0BFDE0/Y,0BFCC0/Z. There is no extra segment-t clamp.
bool clipPlane(Point& out,const Point& a,const Point& b,const Point& half,unsigned axis,float plane){
    const float delta=b[axis]-a[axis];float fraction=0.f;if(delta!=0.f){fraction=plane-a[axis];fraction/=delta;}out[axis]=plane;
    for(unsigned i=0;i<3;++i)if(i!=axis){const float difference=b[i]-a[i];out[i]=std::fma(difference,fraction,a[i]);}
    for(unsigned i=0;i<3;++i)if(i!=axis&&(-half[i]>out[i]||out[i]>half[i]))return false;return true;
}
unsigned planeMask(int a,int b,unsigned negative,unsigned positive){const auto index=unsigned((b+2)*5+a+2);return negative*masks[index]+positive*masks[24-index];} //0BFE60
//0BFEA0 visits six faces in-X,+X,-Y,+Y,-Z,+Z order. Keep duplicate
// edge/face intersections:0BFB40's centroid includes every appended point.
void appendEdge(OriginalBodyContactState& state,const Point& a,const Point& b,const std::array<int,3>& ca,const std::array<int,3>& cb,const Point& half,const OriginalMatrix& world){
    for(unsigned axis=0;axis<3;++axis)if(ca[axis]*cb[axis]==4)return;
    const unsigned mask=planeMask(ca[0],cb[0],32,4)|planeMask(ca[1],cb[1],16,2)|planeMask(ca[2],cb[2],8,1);
    constexpr std::array<unsigned,6> bits{32,4,16,2,8,1};
    for(unsigned plane=0;plane<6;++plane)if((mask&bits[plane])==0){Point local;const auto axis=plane/2;const float bound=(plane&1)?half[axis]:-half[axis];
        if(clipPlane(local,a,b,half,axis,bound)&&state.count0CA9B360<32)state.intersections0CA9B364[state.count0CA9B360++]=transformOriginalPoint(world,local);
    }
}
//0C03A0: transform all8 corners of B into A's local space, then clip12edges.
void collect(OriginalBodyContactState& state,const OriginalBodyShape& a,const OriginalBodyShape& b){
    const auto world=matrix(a,496),relative=multiply(matrix(a,560),matrix(b,496));const auto vertices=corners(relative,point(b,4));const auto half=point(a,4);std::array<std::array<int,3>,8> classes;
    for(unsigned i=0;i<8;++i)for(unsigned k=0;k<3;++k)classes[i][k]=classify(vertices[i][k],half[k]);
    for(auto edge:edges)appendEdge(state,vertices[edge[0]],vertices[edge[1]],classes[edge[0]],classes[edge[1]],half,world);
}
//0C0220 uses the averaged contact point and center direction to find the
// outward point on each body's face, tryingX thenZ thenY. On failure it
// writes only world point and preserves prior local-point record fields.
void contactPoint(OriginalBodyShape& shape,const OriginalBodyShape& other,const Point& center,const Point& towardOther){
    constexpr unsigned record=44;const auto inv=matrix(shape,560);const Point local=transformOriginalPoint(inv,center),normal=direction(inv,towardOther),end=subtract(local,normal),half=point(other,4);Point intersection;
    bool found=false;for(unsigned axis:{0u,2u,1u}){if(normal[axis]==0.f)continue;const float plane=normal[axis]<0.f?half[axis]:-half[axis];if(clipPlane(intersection,local,end,half,axis,plane)){found=true;break;}}
    if(found){storePoint(shape,record+8,transformOriginalPoint(matrix(shape,496),intersection));storePoint(shape,record+20,intersection);}else storePoint(shape,record+8,center);
}
void pair(OriginalBodyContactState& state){auto& a=state.shapes0C401B04[0];auto& b=state.shapes0C401B04[1];const float radius=a.f(492)+b.f(492);Point delta;
    delta[0]=b.f(16)-a.f(16);if(!(radius>delta[0]))return;delta[2]=b.f(24)-a.f(24);if(!(radius>delta[2]))return;delta[1]=b.f(20)-a.f(20);const float radiusSquared=radius*radius;if(!(radiusSquared>lengthSquared(delta)))return;
    state.count0CA9B360=0;collect(state,a,b);collect(state,b,a);if(state.count0CA9B360==0)return;
    Point center{0.f,0.f,0.f};for(unsigned i=0;i<state.count0CA9B360;++i)for(unsigned k=0;k<3;++k)center[k]+=state.intersections0CA9B364[i][k];const float inverseCount=1.f/float(state.count0CA9B360);for(auto& v:center)v*=inverseCount;
    Point unit=subtract(point(b,16),point(a,16));const float distance=magnitude(unit);const float inverseDistance=std::bit_cast<float>(0x00800000u)>distance?0.f:1.f/distance;for(auto& v:unit)v*=inverseDistance;
    a.setu(40,1);b.setu(40,1);a.setu(44,b.u(0));b.setu(44,a.u(0));Point negative=unit;for(auto& v:negative)v=-v;
    contactPoint(a,b,center,negative);contactPoint(b,a,center,unit);
    const Point difference=subtract(point(b,52),point(a,52));storePoint(a,76,difference);Point reverse=difference;for(auto& v:reverse)v*= -1.f;storePoint(b,76,reverse);
    storePoint(a,88,direction(matrix(a,560),difference));storePoint(b,88,direction(matrix(b,560),reverse));
}
}
OriginalBodyCollisionResult produceOriginalBodyContact(const OriginalPublishedActors& published,OriginalBodyContactState& state,const OriginalRivalData& data,const OriginalFscaTable& fsca){
    return produceOriginalBodyPairContact(published,state,data,fsca)[0];
}
std::array<OriginalBodyCollisionResult,2> produceOriginalBodyPairContact(const OriginalPublishedActors& published,OriginalBodyContactState& state,const OriginalRivalData& data,const OriginalFscaTable& fsca){
    prepare(state.shapes0C401B04[0],published.player0C8FF388,data,fsca);prepare(state.shapes0C401B04[1],published.secondary0C8FF430,data,fsca);pair(state);
    const auto& a=state.shapes0C401B04[0];const auto& b=state.shapes0C401B04[1];
    return {{{a.u(40),a.f(76),a.f(84)},{b.u(40),b.f(76),b.f(84)}}};
}
} // namespace idas3::original
