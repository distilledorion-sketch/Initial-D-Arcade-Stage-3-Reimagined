#pragma once
#include <algorithm>
#include <cmath>

namespace idas3 {
constexpr float pi = 3.14159265358979323846f;
struct Vec3 {
    float x=0, y=0, z=0;
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator-() const { return {-x,-y,-z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
    Vec3 operator/(float s) const { return {x/s,y/s,z/s}; }
    Vec3& operator+=(Vec3 b) { x+=b.x;y+=b.y;z+=b.z;return *this; }
    Vec3& operator-=(Vec3 b) { x-=b.x;y-=b.y;z-=b.z;return *this; }
    Vec3& operator*=(float s) { x*=s;y*=s;z*=s;return *this; }
};
inline Vec3 operator*(float s,Vec3 v) { return v*s; }
inline float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 normalized(Vec3 a) { float n=length(a);return n>1e-7f?a/n:Vec3{0,0,1}; }
inline Vec3 normalize(Vec3 a) { return normalized(a); }
inline Vec3 lerp(Vec3 a,Vec3 b,float t) { return a+(b-a)*t; }
inline float wrapAngle(float a) { return std::remainder(a,2*pi); }
inline float lerpAngle(float a,float b,float t) { return a+wrapAngle(b-a)*t; }
inline Vec3 forward(float yaw) { return {std::sin(yaw),0,std::cos(yaw)}; }
inline Vec3 right(float yaw) { return {std::cos(yaw),0,-std::sin(yaw)}; }
}
