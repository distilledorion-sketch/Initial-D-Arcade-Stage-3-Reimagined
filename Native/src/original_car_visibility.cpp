#include "original_car_visibility.h"
#include <numeric>
#include <stdexcept>

namespace idas3::original {
OriginalCarVisibility originalCarVisibility(const OriginalCarAppearanceConfig& appearance,std::int32_t enemy){
    //191440 table at298320; no inferred popup angles.
    constexpr std::array<std::uint32_t,35> popupPhases{0x3400,0,0,0x1d00,0,0,0,0,0,0,0,0,0,0x2100,0,0,0,0,0,0,0,0,0x2500,0x2500,0x2200,0x1f00,0,0,0,0,0,0,0,0,0};
    if(appearance.car>=35)throw std::out_of_range("Original visibility car");
    OriginalCarVisibility out;auto& s=out.slots;std::iota(s.begin(),s.end(),0u);
    out.maximumPopupPhase=popupPhases[appearance.car];out.popupMotorEnabled=out.maximumPopupPhase!=0;
    const auto word=appearance.word;const auto front=word&7,roof=(word>>7)&7,side=(word>>10)&7,rear=(word>>13)&7,paint=(word>>25)&7;
    const bool bit28=(word&0x10000000)!=0,bit29=(word&0x20000000)!=0,bit30=(word&0x40000000)!=0;
    const auto enemyMark=[&](){if(enemy>=0)s[209]=1000;};
    const auto firstPaintMarks=[&](){if(paint==0)s[203]=1000;else if(paint==1||paint==2)s[204]=1000;};
    switch(appearance.car){
    case 0: //029100
        if(enemy==13||enemy==29)s[209]=1000;
        if(front==1)s[5]=1000;
        if(roof==1)s[18]=s[19]=202;
        firstPaintMarks();break;
    case 1: //029160. The profile164 comparison is overwritten before branching.
        if(front==1)s[5]=1000;
        firstPaintMarks();enemyMark();
        if(bit30){s[17]=0xffffffff;s[205]=1000;}break;
    case 2: //0291E0
        if(paint==0||paint==2)s[203]=1000;else if(paint==1)s[204]=1000;
        enemyMark();if(bit28)s[209]=1000;break;
    case 4:if(bit30)s[203]=1000;break;
    case 5:case 6:if(front>=1&&front<=3)s[17]=0xffffffff;break;
    case 7:case 19:case 32:case 34:enemyMark();break;
    case 9:{ //0292C0: S13 factory palettes replace the matching body pieces.
        if(front==2||front==3)s[17]=0xffffffff;
        if(paint<=1){const unsigned base=paint==0?66:72;s[0]=base;
            if(front==0)s[5]=base+1;else if(front==1)s[6]=base+2;
            if(side==0)s[28]=base+3;else if(side==1)s[29]=base+4;
            if(rear==0)s[34]=base+5;
        }enemyMark();break;
    }
    case 10:
        if(front>=1&&front<=3)s[17]=0xffffffff;
        if(enemy>=0)s[203]=s[209]=1000;
        if(bit28)s[203]=1000;if(bit29)s[209]=1000;break;
    case 11:if(front==3)s[17]=0xffffffff;break;
    case 12:if(enemy>=0)s[203]=1000;break;
    case 13:
        if(((word>>16)&7)==1)s[40]=s[127]=1000;
        enemyMark();if(bit28)s[209]=1000;break;
    case 14:if(front==3)s[17]=0xffffffff;enemyMark();break;
    case 15:if(front==4)s[17]=0xffffffff;break;
    case 16:if(front==4)s[17]=0xffffffff;enemyMark();break;
    case 17:if(front!=0)s[17]=0xffffffff;break;
    case 18:
        if(rear==1){s[46+((word>>19)&7)]=47;s[46]=0xffffffff;}
        if(bit30)s[203]=1000;break;
    case 20:
        if(paint==0){s[11]=12;s[100]=101;s[148]=149;s[88]=89;}
        enemyMark();break;
    case 22:
        if(bit30)s[203]=1000;
        if(front==2){s[18]=s[19]=s[107]=s[108]=0xffffffff;out.popupMotorEnabled=false;}
        if(enemy==8)s[209]=1000;
        if(enemy==28)s[209]=s[203]=0xffffffff;
        if(enemy==7)s[203]=1000;break;
    case 24:
        if(enemy==12)s[26]=1000;
        if(rear==4)s[26]=210;break;
    case 25:
        s[109]=0xffffffff;if(roof==1)s[110]=0xffffffff;
        if(bit30)s[203]=1000;break;
    case 26:if(bit30)s[203]=1000;break;
    case 27:if(front==2)s[17]=0xffffffff;break;
    case 28:if(front==2||front==3)s[17]=0xffffffff;break;
    case 30:if(bit30||enemy>=0)s[203]=1000;break;
    case 31:enemyMark();if(bit28)s[209]=1000;break;
    case 33: //0297C0: Evolution VI T.M. Edition palette and aero dependencies.
        enemyMark();
        if(paint!=1)s[203]=1000;
        else{
            s[204]=1000;s[18]=191;s[19]=192;
            if(roof==0)s[207]=1000;if(front==0)s[208]=1000;
            if(rear==0)s[198]=1000;
        }
        if(rear!=2)s[paint<=2?205:206]=1000;
        break;
    default:break; //3,8,21,23,29 preserve every identity slot.
    }
    return out;
}
}
