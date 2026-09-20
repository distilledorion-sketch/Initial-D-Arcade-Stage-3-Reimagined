#include "original_time_attack_stats.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace idas3::original {
namespace {
using Draws=std::vector<OriginalTimeAttackStatDraw>;
int ftrc(float value){
    if(std::isnan(value)||value<=-2147483648.f)return INT32_MIN;
    if(value>=2147483648.f)return INT32_MAX;
    return int(value);
}
void add(Draws& out,unsigned chunk,float x,float y){out.push_back({chunk,x,y+24.f});}
//1909C0 maps0..9 through338770 to3..12; -1 is the original dash28.
// Impossible/invalid speed digits cannot index outside the imported bank.
void digit(Draws& out,int value,float x,float y){add(out,value>=0&&value<=9?unsigned(value+3):28,x,y);}
void speed(Draws& out,float value){
    const int n=ftrc(std::trunc(value));digit(out,n%10,582,197);
    if(n>9)digit(out,(n%100)/10,574,197);
    if(n>99)digit(out,n/100,566,197);
}
void percentage(Draws& out,float fraction,float y){
    //18B1FC/18B210 first multiply by100;18BE00 multiplies by100 again.
    //1F9F40 rounds away from zero before SH4's saturating FTRC.
    const float percent=fraction*100.f;
    const float hundredths=percent*100.f;
    const float rounded=std::copysign(std::ceil(std::fabs(hundredths)),hundredths);
    const int n=std::min(ftrc(rounded),10000);
    constexpr float x[]{600,592,581,573,565};int power=1;
    for(unsigned i=0;i<5;++i,power*=10){
        if(n>=power)digit(out,(n/power)%10,x[i],y);
        else if(i<=2)digit(out,0,x[i],y);
    }
    add(out,23,589,y);
}
void count(Draws& out,std::uint32_t value,float y){
    //18C060's overflow display is100 (not99), with leading zeros hidden.
    const int n=std::bit_cast<std::int32_t>(value);
    if(n>99){digit(out,0,600,y);digit(out,0,592,y);digit(out,1,584,y);return;}
    digit(out,n%10,600,y);if(n>9)digit(out,n/10,592,y);
}
void clock(Draws& out,std::uint32_t ticks,bool available,float x,float y){
    //1568A0's BCD slots used by18C560/18C720: minute units, two seconds,
    // three milliseconds. Integer/6 discards the unrendered sixths of a ms.
    const unsigned ms=ticks/6u;
    const int values[]{int(ms%10),int((ms/10)%10),int((ms/100)%10),-2,
        int((ms/1000)%10),int((ms/10000)%6),-3,int((ms/60000)%10)};
    for(unsigned i=0;i<8;++i){
        if(i==3)add(out,38,x,y);else if(i==6)add(out,29,x,y);
        else digit(out,available?values[i]:-1,x,y);
        x-=i==2?5.5f:i==5?4.f:8.f;
    }
}
}
std::vector<OriginalTimeAttackStatDraw> originalTimeAttackStatDraws(
    const OriginalTimeAttackTelemetrySnapshot& telemetry,const OriginalTimeAttackAnalysisInput& in){
    Draws out;out.reserve(150);
    if(telemetry.valid){
        speed(out,telemetry.maxSpeedKph);
        percentage(out,telemetry.brakeFraction,212);
        percentage(out,telemetry.acceleratorFraction,227);
        //18B22C passes18D140/18D2C0's converted event count, not raw grazes.
        count(out,telemetry.convertedEventCount,242);count(out,telemetry.ditchCount,257);
    }else for(float y:{197.f,212.f,227.f,242.f,257.f})digit(out,-1,600,y);
    const unsigned sections=in.course==0?3:in.course==1?2:4;
    const bool laps=in.course<2; //18BB18 initializes1AF98 forMyogi/Usui.
    for(unsigned i=0;i<sections;++i){
        add(out,laps?25:39,455,284+26.f*i);
        add(out,16+i,laps?478.f:505.f,284+26.f*i);
    }
    add(out,42,455,284+26.f*sections);
    for(unsigned i=0;i<=sections;++i)add(out,13,522,297+26.f*i);
    for(unsigned i=0;i<sections;++i){
        const auto old=in.previousSections6000[i],now=in.currentSections6000[i];
        if(old&&now&&old/6u>now/6u)add(out,43,595,298+26.f*i);
    }
    if(in.previousBestTicks6000&&in.finishTicks6000&&in.previousBestTicks6000/6u>in.finishTicks6000/6u)
        add(out,43,595,298+26.f*sections);
    clock(out,telemetry.startIntervalTicks6000,telemetry.valid&&telemetry.startIntervalTicks6000!=0,613,272);
    //18C940 draws the previous personal record left, current run right.
    const bool previous=in.previousBestTicks6000!=0;
    for(unsigned i=0;i<sections;++i)clock(out,previous?in.previousSections6000[i]:0,
        in.previousSections6000[i]!=0,512,298+26.f*i);
    clock(out,in.previousBestTicks6000,previous,512,298+26.f*sections);
    for(unsigned i=0;i<sections;++i)clock(out,in.currentSections6000[i],in.currentSections6000[i]!=0,580,298+26.f*i);
    clock(out,in.finishTicks6000,in.finishTicks6000!=0,580,298+26.f*sections);
    return out;
}
}
