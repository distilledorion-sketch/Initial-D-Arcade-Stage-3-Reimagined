#include "course.h"
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace idas3 {
namespace {
std::uint32_t u32(const unsigned char* b) {
    return std::uint32_t(b[0]) | (std::uint32_t(b[1])<<8) |
           (std::uint32_t(b[2])<<16) | (std::uint32_t(b[3])<<24);
}
// Standard SHA-256 over the original center, left, right files, in that order.
// Portable implementation keeps course import independent of graphics/Win32.
std::string sha256(std::vector<unsigned char> b) {
    static constexpr std::uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const std::uint64_t bits=std::uint64_t(b.size())*8;
    b.push_back(0x80); while(b.size()%64!=56) b.push_back(0);
    for(int i=7;i>=0;--i) b.push_back(static_cast<unsigned char>(bits>>(i*8)));
    for(std::size_t off=0;off<b.size();off+=64) {
        std::uint32_t w[64]{};
        for(int i=0;i<16;++i) { const auto* p=b.data()+off+i*4; w[i]=(std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3]; }
        for(int i=16;i<64;++i) { auto a=w[i-15],z=w[i-2]; w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(z,17)^std::rotr(z,19)^(z>>10)); }
        auto [a,c,d,e,f,g,j,m]=h;
        for(int i=0;i<64;++i) {
            auto t1=m+(std::rotr(f,6)^std::rotr(f,11)^std::rotr(f,25))+((f&g)^(~f&j))+k[i]+w[i];
            auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&c)^(a&d)^(c&d));
            m=j;j=g;g=f;f=e+t1;e=d;d=c;c=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=c;h[2]+=d;h[3]+=e;h[4]+=f;h[5]+=g;h[6]+=j;h[7]+=m;
    }
    std::ostringstream out; out<<std::hex<<std::setfill('0');
    for(auto v:h) out<<std::setw(8)<<v;
    return out.str();
}
std::vector<Vec3> readPath(const std::filesystem::path& path,std::vector<unsigned char>& provenance) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("Cannot open course path: "+path.string());
    auto size=input.tellg();
    if(size<32 || size>12000008) throw std::runtime_error("Invalid course path size: "+path.string());
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    input.seekg(0); input.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size()));
    if(!input) throw std::runtime_error("Truncated course path: "+path.string());
    auto count=u32(data.data()),components=u32(data.data()+4);
    if(count<2 || count>1000000 || components!=3 || data.size()!=8+std::size_t(count)*12)
        throw std::runtime_error("Invalid course path header: "+path.string());
    std::vector<Vec3> result; result.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
        const auto* p=data.data()+8+std::size_t(i)*12;
        Vec3 v{std::bit_cast<float>(u32(p)),std::bit_cast<float>(u32(p+4)),std::bit_cast<float>(u32(p+8))};
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z) ||
           std::abs(v.x)>1e7f||std::abs(v.y)>1e7f||std::abs(v.z)>1e7f)
            throw std::runtime_error("Invalid course coordinate: "+path.string());
        result.push_back(v);
    }
    provenance.insert(provenance.end(),data.begin(),data.end());
    return result;
}
Vec3 planarRight(Vec3 tangent) { return normalized(Vec3{tangent.z,0,-tangent.x}); }
}
Course Course::load(const std::filesystem::path& pathRoot,const std::string& courseId,const std::string& courseName,bool reverse) {
    if(courseId.empty() || courseId.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)
        throw std::runtime_error("Invalid course identifier");
    auto folder=pathRoot;
    if(std::filesystem::is_directory(folder/"HOSTFS")) folder/="HOSTFS";
    if(std::filesystem::is_directory(folder/"path")) folder/="path";
    Course c; c.id=courseId;c.name=courseName.empty()?courseId:courseName;c.sourceRoot=std::filesystem::absolute(folder).string();c.reversed=reverse;
    std::vector<unsigned char> bytes;
    c.points=readPath(folder/(courseId+"_path.bin"),bytes);
    c.left=readPath(folder/(courseId+"_path_l.bin"),bytes);
    c.right=readPath(folder/(courseId+"_path_r.bin"),bytes);
    c.provenanceSha256=sha256(std::move(bytes));
    if(c.left.size()!=c.points.size()||c.right.size()!=c.points.size()) throw std::runtime_error("Course edge counts differ");
    if(reverse) { std::reverse(c.points.begin(),c.points.end());std::reverse(c.left.begin(),c.left.end());std::reverse(c.right.begin(),c.right.end()); }
    double orientation=0;
    for(std::size_t i=0;i+1<c.points.size();++i) orientation+=dot(c.right[i]-c.left[i],planarRight(c.points[i+1]-c.points[i]));
    if(orientation<0) { std::swap(c.left,c.right);c.sourceEdgesSwapped=true; }
    c.cumulative.reserve(c.points.size());c.cumulative.push_back(0);
    for(std::size_t i=0;i<c.points.size();++i) {
        const float width=idas3::length(c.right[i]-c.left[i]);
        if(width<0.05f || width>200) throw std::runtime_error("Invalid course boundary width");
        if(i) { const float step=idas3::length(c.points[i]-c.points[i-1]);
            if(step<1e-6f||step>1000) throw std::runtime_error("Degenerate or discontinuous course path");
            c.length+=step;c.cumulative.push_back(c.length); }
    }
    c.closed=idas3::length(c.points.front()-c.points.back())<0.05f;
    return c;
}
CourseSample Course::sample(float distance) const {
    if(points.size()<2||left.size()!=points.size()||right.size()!=points.size()||cumulative.size()!=points.size())
        throw std::runtime_error("Course has not been loaded");
    if(!std::isfinite(distance)) throw std::runtime_error("Non-finite course distance");
    distance=std::clamp(distance,0.0f,length);
    auto it=std::upper_bound(cumulative.begin(),cumulative.end(),distance);
    std::size_t i=it==cumulative.begin()?0:static_cast<std::size_t>(it-cumulative.begin()-1);
    i=std::min(i,points.size()-2);
    float t=(distance-cumulative[i])/(cumulative[i+1]-cumulative[i]);
    CourseSample s;
    s.center=lerp(points[i],points[i+1],t);s.left=lerp(left[i],left[i+1],t);s.right=lerp(right[i],right[i+1],t);
    // Interpolate vertex tangents to avoid steering impulses at path records.
    auto vertexTangent=[&](std::size_t n) {
        if(closed && (n==0||n==points.size()-1)) return normalized(points[1]-points[points.size()-2]);
        const auto a=n?n-1:n;const auto b=std::min(n+1,points.size()-1);return normalized(points[b]-points[a]);
    };
    s.tangent=normalized(lerp(vertexTangent(i),vertexTangent(i+1),t));
    s.distance=distance;s.width=idas3::length(s.right-s.left);s.segmentIndex=i;
    s.grade=s.tangent.y/std::max(0.001f,std::hypot(s.tangent.x,s.tangent.z));
    // Curvature derives from authored positions over a 12-unit neighborhood.
    const auto lo=i>3?i-3:0;const auto hi=std::min(i+4,points.size()-1);
    Vec3 a=vertexTangent(lo),b=vertexTangent(hi);
    const float ds=cumulative[hi]-cumulative[lo];
    s.curvature=ds>0.001f?wrapAngle(std::atan2(b.x,b.z)-std::atan2(a.x,a.z))/ds:0;
    return s;
}
CourseProjection Course::project(Vec3 position,std::size_t hint,std::size_t radius) const {
    if(points.size()<2) throw std::runtime_error("Course has not been loaded");
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)) throw std::runtime_error("Non-finite projection position");
    float best=std::numeric_limits<float>::infinity(),bestT=0;
    std::size_t bestIndex=0,count=points.size()-1;
    auto hintDifference=[&](std::size_t i) { return i>hint?i-hint:hint-i; };
    auto inspect=[&](std::size_t i) {
        Vec3 d=points[i+1]-points[i];float t=std::clamp(dot(position-points[i],d)/dot(d,d),0.0f,1.0f);
        Vec3 delta=position-lerp(points[i],points[i+1],t);float d2=dot(delta,delta);
        // Closed paths duplicate their first position at the final record.
        // Prefer the hinted segment on a tie, so the start cannot become the
        // finish merely because a wrapped search visited the last segment first.
        bool tie=std::abs(d2-best)<1e-5f;
        if(d2<best-1e-5f || (tie && hint<count && hintDifference(i)<hintDifference(bestIndex)))
            {best=d2;bestT=t;bestIndex=i;}
    };
    bool preserveLapSeam=false;
    if(hint<count) {
        radius=std::min(radius,count);
        // This is a projection onto one authored lap, even if the geometry
        // closes. Wrapping a neighborhood lets an ordinary step beyond the
        // finish choose segment zero before the race has observed the finish.
        // A new lap/teleport must explicitly reset or omit the hint.
        preserveLapSeam=closed && (hint<=radius || count-1-hint<=radius);
        for(std::size_t i=hint>radius?hint-radius:0;i<=std::min(count-1,hint+radius);++i) inspect(i);
    }
    if(best>1600 && !preserveLapSeam) for(std::size_t i=0;i<count;++i) inspect(i);
    auto s=sample(cumulative[bestIndex]+bestT*(cumulative[bestIndex+1]-cumulative[bestIndex]));
    return {s,dot(position-s.center,planarRight(s.tangent)),best,bestIndex};
}
}
