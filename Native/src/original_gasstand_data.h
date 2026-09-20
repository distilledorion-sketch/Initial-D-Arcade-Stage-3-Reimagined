#pragma once
#include <array>
#include <bit>
#include <string_view>
namespace idas3::original::gasstand_data {
constexpr float word(unsigned u){return std::bit_cast<float>(u);}
struct Message {unsigned speaker;std::array<std::string_view,3> lines;};
inline constexpr std::array<Message,17> script0{{
{0, {"HEY, COLE.","",""}},
{0, {"I'VE HEARD THAT I CAN CUSTOMIZE MY CAR","IF I HAVE AN INITIAL D CARD.",""}},
{1, {"THAT'S RIGHT.","",""}},
{1, {"RACING AGAINST RIVALS","EARNS YOU POINTS, RIGHT?",""}},
{1, {"IF YOU COLLECT ENOUGH POINTS,","YOU CAN CUSTOMIZE YOUR CAR.",""}},
{2, {"COOL!","",""}},
{1, {"YOU CAN CHOOSE CUSTOM","PARTS FROM 41 DIFFERENT","MANUFACTURERS."}},
{0, {"WHEW! I CAN'T MAKE UP MY MIND","WITH SO MANY CHOICES!",""}},
{2, {"BUT WHICH ONES SHOULD I PICK?","",""}},
{2, {"I DON'T KNOW MUCH ABOUT","CUSTOMIZING, YOU KNOW.",""}},
{1, {"YOU CAN SELECT A,","CUSTOMIZATION OPTION","WHEN YOU BUY A CARD."}},
{1, {"THAT WAY YOUR CAR WILL GET","CUSTOMIZED AUTOMATICALLY","AS YOU SAVE UP POINTS."}},
{1, {"IT'LL BE NO PROBLEM EVEN IF YOU","DON'T KNOW MUCH ABOUT MECHA-","NICAL STUFF AS WELL AS TAK."}},
{1, {"YOU CAN ALSO SAVE YOUR","GAME STATUS AND TIMES.",""}},
{1, {"AND CONTINUE YOUR GAME","WHENEVER YOU WANT.",""}},
{1, {"TAK, YOU SHOULD","DEFINITELY GET ONE.",""}},
{2, {"OKAY!","",""}},
}};
inline constexpr std::array<Message,18> script1{{
{0, {"HEY, COLE.","",""}},
{0, {"I'VE HEARD 'BATTLE RACE'","IS AVAILABLE IN THIS GAME.",""}},
{2, {"WHAT IS THAT?","",""}},
{1, {"BATTLE RACE, HUH?","",""}},
{1, {"WELL, LET'S SAY","SOMEONE'S PLAYING","THE GAME ALONE"}},
{2, {"YEAH.","",""}},
{1, {"AND HE HAS THE AWESOME","TECHNIQUE, DON'T YOU WANNA","BATTLE AGAINST HIM?"}},
{0, {"OF COURSE. THAT'S WHAT BEING","A SPEED MERCHANT'S ALL ABOUT.",""}},
{1, {"WELL, WITH BATTLE RACE YOU CAN","SIT DOWN AT THE NEXT UNIT","AND CHALLENGE THAT RACER."}},
{0, {"YEAH? PRETTY COOL!","",""}},
{2, {"BUT WHAT IF YOU JUST WANT TO RACE","BY YOURSELF, WITH NO CHALLENGERS?",""}},
{1, {"IN THAT CASE ALL YOU NEED TO DO","IS SELECT REFUSE CHALLENGERS","AT THE BEGINNING OF THE GAME."}},
{2, {"OH, OKAY.","",""}},
{1, {"NOT ONLY THAT, YOU CAN SWITCH","BETWEEN REFUSE CHALLENGERS AND","ACCEPT CHALLENGERS AT ANY TIME"}},
{1, {"IN THE MIDDLE OF A GAME JUST","BY HITTING THE START BUTTON.",""}},
{0, {"NOT BAD.","",""}},
{1, {"SO, YOU GOT ALL THAT, TAK?","",""}},
{2, {"SURE THING, BOSS.","",""}},
}};
inline constexpr std::array<Message,14> script2{{
{0, {"HEY, COLE.","",""}},
{0, {"HOW DO YOU USE","NET-RANKINGS?",""}},
{2, {"WHAT'S THAT?","",""}},
{1, {"WELL. FOR EXAMPLE,","IF YOU GET A REALLY","GOOD TIME ON TIME ATTACK,"}},
{1, {"YOU WANT TO KNOW","HOW YOUR TIME STACKS","UP NATIONALLY, RIGHT?"}},
{0, {"YEAH.","",""}},
{1, {"YOU WRITE DOWN THE PASSWORD","THAT APPEARS ON THE SCREEN","AFTER A TIME ATTACK RACE."}},
{1, {"THEN VISIT THE INITIAL D","ARCADE STAGE VER.3","OFFICIAL WEB SITE AND"}},
{1, {"KEY IN YOUR PASSWORD.","",""}},
{1, {"YOU'LL BE ABLE TO SEE","WHERE YOU RANK NATIONALLY.",""}},
{2, {"EXCELLENT!","",""}},
{1, {"YOU CAN ALSO SEE","YOUR PASSWORD ON THE VIEW","RECORDED DATA WINDOW,"}},
{1, {"SO DON'T WORRY IF YOU","FORGET TO WRITE IT DOWN!",""}},
{0, {"OKAY!","I'M GOING TO PUT UP A REAL FAST TIME!",""}},
}};
inline constexpr std::array<char,62> fontCharacters{32,44,46,97,58,59,63,33,39,40,41,43,45,61,60,62,36,37,38,42,64,48,49,50,51,52,53,54,55,56,57,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,98,99,100,101,102};
inline constexpr std::array<unsigned,62> fontWidthWords{0x41200000u,0x40c00000u,0x40c00000u,0x40a00000u,0x40a00000u,0x40c00000u,0x41100000u,0x40800000u,0x40800000u,0x40c00000u,0x40c00000u,0x41300000u,0x40c00000u,0x41300000u,0x41100000u,0x41100000u,0x41300000u,0x41500000u,0x41500000u,0x41200000u,0x41800000u,0x41600000u,0x40c00000u,0x41400000u,0x41600000u,0x41600000u,0x41500000u,0x41600000u,0x41200000u,0x41600000u,0x41600000u,0x41500000u,0x41500000u,0x41400000u,0x41400000u,0x41400000u,0x41300000u,0x41800000u,0x41400000u,0x40c00000u,0x41300000u,0x41400000u,0x41100000u,0x41600000u,0x41400000u,0x41500000u,0x41500000u,0x41400000u,0x41400000u,0x41500000u,0x41400000u,0x41300000u,0x41400000u,0x41700000u,0x41300000u,0x41600000u,0x41400000u,0x41500000u,0x41880000u,0x41a00000u,0x41200000u,0x41700000u};
inline constexpr std::array<unsigned,41> brandOrder{18,3,10,20,25,40,23,19,0,4,11,24,41,13,15,39,38,17,21,31,26,27,1,35,2,36,29,14,30,9,5,22,7,12,16,6,37,32,34,28,8};
inline constexpr std::array<std::array<float,2>,3> speakerXY{{{word(0x43340000u),word(0x43480000u)},{word(0x43860000u),word(0x432a0000u)},{word(0x42c80000u),word(0x43480000u)}}};
inline constexpr float lit_0C0D0F30=word(0x3c23d70au);
inline constexpr float lit_0C0D0F34=word(0x3d23d70au);
inline constexpr float lit_0C0D0F38=word(0x3e0f5c29u);
inline constexpr float lit_0C0D095C=word(0x40cccccdu);
inline constexpr float lit_0C0D1130=word(0xbca3d70au);
inline constexpr float lit_0C0D1134=word(0xc1200000u);
inline constexpr float lit_0C0D1138=word(0xc2100000u);
inline constexpr float lit_0C0D113C=word(0x41800000u);
inline constexpr float lit_0C0D114C=word(0x3e851eb8u);
inline constexpr float lit_0C0D1208=word(0x3f400000u);
inline constexpr float lit_0C0D1210=word(0x3e8a3d71u);
inline constexpr float lit_0C0D1214=word(0x3bf5c28fu);
inline constexpr float lit_0C0D13B0=word(0x3e8a3d70u);
inline constexpr float lit_0C0D13B4=word(0x3ea3d70au);
inline constexpr float lit_0C0D13B8=word(0x3ef5c28fu);
inline constexpr float lit_0C0D13BC=word(0xbf400000u);
inline constexpr float lit_0C0D13C0=word(0x3f400000u);
inline constexpr float lit_0C0D13C4=word(0x3d75c28fu);
inline constexpr float lit_0C0D13CC=word(0x3d000000u);
inline constexpr float lit_0C0D13D0=word(0x3c23d70au);
inline constexpr float lit_0C0D13D4=word(0x3f828f5cu);
inline constexpr float lit_0C0D13D8=word(0xbf23d70au);
inline constexpr float lit_0C0D14F0=word(0x3cf3b645u);
inline constexpr float lit_0C0D14F8=word(0x3fa33333u);
inline constexpr float lit_0C0D14FC=word(0x3ea8f5c3u);
inline constexpr float lit_0C0D1500=word(0x3f4ccccdu);
inline constexpr float lit_0C112E28=word(0x3c020821u);
inline constexpr float lit_0C112F88=word(0x40228f5cu);
}
