#pragma once
#include <array>
namespace idas3 {
// Direct original semantic1/2/3/4/79 slot maps: day rear, day brake, night rear, night brake, illuminated plate. -1 means absent.
inline constexpr std::array<std::array<int,5>,35> originalCarLampChunks={{
    {{75,76,77,78,99}}, // 0 toyota_ae86t
    {{30,31,32,33,74}}, // 1 toyota_ae86l
    {{26,27,28,29,64}}, // 2 toyota_ae85l
    {{2,3,4,5,32}}, // 3 toyota_sw20
    {{1,2,3,4,20}}, // 4 toyota_zzw30
    {{27,28,29,30,71}}, // 5 toyota_sxe10
    {{23,24,25,26,62}}, // 6 toyota_st205
    {{2,3,4,5,34}}, // 7 nissan_bnr32
    {{26,27,28,29,70}}, // 8 nissan_bnr34
    {{2,3,4,5,42}}, // 9 nissan_s13k
    {{33,34,35,36,79}}, // 10 nissan_s14q
    {{2,3,4,5,31}}, // 11 nissan_s14
    {{2,3,4,5,31}}, // 12 nissan_s15
    {{2,3,4,5,31}}, // 13 nissan_rps13
    {{2,3,4,5,29}}, // 14 nissan_rps13k
    {{2,3,4,5,29}}, // 15 honda_ek9
    {{2,3,4,5,29}}, // 16 honda_eg6
    {{29,30,31,32,69}}, // 17 honda_dc2
    {{2,3,4,5,32}}, // 18 honda_ap1
    {{20,21,22,23,57}}, // 19 mitsu_ce9a
    {{2,3,4,5,30}}, // 20 mitsu_cn9a
    {{24,25,26,27,66}}, // 21 mitsu_ct9a
    {{39,40,41,42,84}}, // 22 mazda_fd3s
    {{29,30,31,32,71}}, // 23 mazda_fd3sa
    {{32,33,34,35,73}}, // 24 mazda_fc3s
    {{34,35,36,37,78}}, // 25 mazda_na6c
    {{2,3,4,5,31}}, // 26 mazda_nb8c
    {{30,31,32,33,75}}, // 27 subaru_gc8s6
    {{2,3,4,5,29}}, // 28 subaru_gdb
    {{27,28,29,30,49}}, // 29 subaru_gc8s5
    {{24,25,26,27,64}}, // 30 suzuki_ea11r
    {{23,24,25,26,63}}, // 31 nissan_er34
    {{34,35,36,37,75}}, // 32 mitsu_cp9a
    {{45,46,47,48,87}}, // 33 mitsu_cp9at
    {{58,59,60,61,102}}, // 34 mazda_se3p
}};
}
