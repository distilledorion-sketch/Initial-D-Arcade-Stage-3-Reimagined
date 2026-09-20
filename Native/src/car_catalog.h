#pragma once
#include <array>
#include <string_view>
namespace idas3 {
// Original ID0..34 from0C2EF384/0C2EF410; no alphabetical remapping.
inline constexpr std::array<std::string_view,35> originalCarFolders = {
    "toyota_ae86t", // 0
    "toyota_ae86l", // 1
    "toyota_ae85l", // 2
    "toyota_sw20", // 3
    "toyota_zzw30", // 4
    "toyota_sxe10", // 5
    "toyota_st205", // 6
    "nissan_bnr32", // 7
    "nissan_bnr34", // 8
    "nissan_s13k", // 9
    "nissan_s14q", // 10
    "nissan_s14", // 11
    "nissan_s15", // 12
    "nissan_rps13", // 13
    "nissan_rps13k", // 14
    "honda_ek9", // 15
    "honda_eg6", // 16
    "honda_dc2", // 17
    "honda_ap1", // 18
    "mitsu_ce9a", // 19
    "mitsu_cn9a", // 20
    "mitsu_ct9a", // 21
    "mazda_fd3s", // 22
    "mazda_fd3sa", // 23
    "mazda_fc3s", // 24
    "mazda_na6c", // 25
    "mazda_nb8c", // 26
    "subaru_gc8s6", // 27
    "subaru_gdb", // 28
    "subaru_gc8s5", // 29
    "suzuki_ea11r", // 30
    "nissan_er34", // 31
    "mitsu_cp9a", // 32
    "mitsu_cp9at", // 33
    "mazda_se3p", // 34
};

// Display names for the file list and the car panel. The roster carries no
// name strings of its own -- the cabinet paints those as artwork -- so these
// are written out to match the chassis each folder holds.
inline constexpr std::array<std::string_view,35> originalCarNames = {
    "AE86 TRUENO", // 0 toyota_ae86t
    "AE86 LEVIN", // 1 toyota_ae86l
    "AE85 LEVIN", // 2 toyota_ae85l
    "SW20 MR2", // 3 toyota_sw20
    "ZZW30 MR-S", // 4 toyota_zzw30
    "SXE10 ALTEZZA", // 5 toyota_sxe10
    "ST205 CELICA", // 6 toyota_st205
    "BNR32 SKYLINE", // 7 nissan_bnr32
    "BNR34 SKYLINE", // 8 nissan_bnr34
    "S13 SILVIA", // 9 nissan_s13k
    "S14 SILVIA", // 10 nissan_s14q
    "S14 SILVIA", // 11 nissan_s14
    "S15 SILVIA", // 12 nissan_s15
    "RPS13 180SX", // 13 nissan_rps13
    "RPS13 180SX", // 14 nissan_rps13k
    "EK9 CIVIC", // 15 honda_ek9
    "EG6 CIVIC", // 16 honda_eg6
    "DC2 INTEGRA", // 17 honda_dc2
    "AP1 S2000", // 18 honda_ap1
    "CE9A LANCER", // 19 mitsu_ce9a
    "CN9A LANCER", // 20 mitsu_cn9a
    "CT9A LANCER", // 21 mitsu_ct9a
    "FD3S RX-7", // 22 mazda_fd3s
    "FD3S RX-7", // 23 mazda_fd3sa
    "FC3S RX-7", // 24 mazda_fc3s
    "NA6CE ROADSTER", // 25 mazda_na6c
    "NB8C ROADSTER", // 26 mazda_nb8c
    "GC8 IMPREZA", // 27 subaru_gc8s6
    "GDB IMPREZA", // 28 subaru_gdb
    "GC8 IMPREZA", // 29 subaru_gc8s5
    "EA11R CAPPUCCINO", // 30 suzuki_ea11r
    "ER34 SKYLINE", // 31 nissan_er34
    "CP9A LANCER", // 32 mitsu_cp9a
    "CP9A LANCER", // 33 mitsu_cp9at
    "SE3P RX-8", // 34 mazda_se3p
};

// The grade line each name is shown with, as the source screens show it.
inline constexpr std::array<std::string_view,35> originalCarGrades = {
    "GT-APEX", // 0 toyota_ae86t
    "GT-APEX", // 1 toyota_ae86l
    "SR", // 2 toyota_ae85l
    "G-LIMITED", // 3 toyota_sw20
    "S EDITION", // 4 toyota_zzw30
    "RS200", // 5 toyota_sxe10
    "GT-FOUR", // 6 toyota_st205
    "GT-R", // 7 nissan_bnr32
    "V-SPEC II", // 8 nissan_bnr34
    "K'S", // 9 nissan_s13k
    "Q'S", // 10 nissan_s14q
    "K'S", // 11 nissan_s14
    "SPEC-R", // 12 nissan_s15
    "TYPE II", // 13 nissan_rps13
    "TYPE X", // 14 nissan_rps13k
    "TYPE R", // 15 honda_ek9
    "SiR-II", // 16 honda_eg6
    "TYPE R", // 17 honda_dc2
    "ROADSTER", // 18 honda_ap1
    "EVOLUTION III", // 19 mitsu_ce9a
    "EVOLUTION IV", // 20 mitsu_cn9a
    "EVOLUTION VII", // 21 mitsu_ct9a
    "TYPE RS", // 22 mazda_fd3s
    "TYPE R", // 23 mazda_fd3sa
    "GT-X", // 24 mazda_fc3s
    "S-SPECIAL", // 25 mazda_na6c
    "RS", // 26 mazda_nb8c
    "TYPE R STi VI", // 27 subaru_gc8s6
    "WRX STi", // 28 subaru_gdb
    "TYPE R STi V", // 29 subaru_gc8s5
    "TURBO", // 30 suzuki_ea11r
    "25GT TURBO", // 31 nissan_er34
    "EVOLUTION V", // 32 mitsu_cp9a
    "EVOLUTION VI TME", // 33 mitsu_cp9at
    "TYPE S", // 34 mazda_se3p
};
}
