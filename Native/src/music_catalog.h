#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace idas3 {
struct RaceMusicTrack {
    const char* id;
    const char* title;
    int stage;
    const char* relativePath; // Relative to data/original_audio/streams.
    const char* artist; // Existing verified music_title_metadata.json by stable ID.
};

// Append-only order: settings.txt stores this index. Original Stage3 indices
// 0..12 must stay fixed. IDs are the stable keys for the future music menu.
// Display titles may be corrected without changing IDs, source paths or indices.
inline constexpr std::array<RaceMusicTrack,117> raceMusicCatalog{{
    {"stage3.01_gamble_rumble", "Gamble Rumble", 3, "01_gamble_rumble.bin", "m.o.v.e"},
    {"stage3.02_speedy_speed_boy", "Speedy Speed Boy", 3, "02_speedy_speed_boy.bin", "Marko Polo"},
    {"stage3.03_remember_me", "Remember Me", 3, "03_remember_me.bin", "Leslie Parrish"},
    {"stage3.04_save_me", "Save Me", 3, "04_save_me.bin", "Leslie Parrish"},
    {"stage3.05_over_the_rainbow", "Over the Rainbow", 3, "05_over_the_rainbow.bin", "Powerful T."},
    {"stage3.06_stop_your_self_control", "Stop Your Self Control", 3, "06_stop_your_self_control.bin", "Marko Polo"},
    {"stage3.07_crazy_for_love", "Crazy for Love", 3, "07_crazy_for_love.bin", "Dusty"},
    {"stage3.08_express_love", "Express Love", 3, "08_express_love.bin", "Mega NRG Man"},
    {"stage3.09_blackout", "Black Out", 3, "09_blackout.bin", "Overload"},
    {"stage3.10_fall_in_the_web", "Fall in the Web of Desire", 3, "10_fall_in_the_web.bin", "Powerful T."},
    {"stage3.11_pamela", "Pamela", 3, "11_pamela.bin", "Matt Land"},
    {"stage3.12_fight_for_love_tonight", "Fight for Love Tonight", 3, "12_fight_for_love_tonight.bin", "Ace Warrior"},
    {"stage3.13_dancin_in_my_dreams", "Dancin' in My Dreams", 3, "13_dancin_in_my_dreams.bin", "J. Storm"},
    {"stage1.EZ001", "Space Boy", 1, "stage1/EZ001.bin", "Dave Rodgers"},
    {"stage1.MD001", "Love Is in Danger", 1, "stage1/MD001.bin", "Priscilla"},
    {"stage1.HR001", "Grand Prix", 1, "stage1/HR001.bin", "Mega NRG Man"},
    {"stage1.VH002", "Heartbeat", 1, "stage1/VH002.bin", "Nathalie"},
    {"stage1.VH003", "Beat of the Rising Sun", 1, "stage1/VH003.bin", "Dave Rodgers"},
    {"stage1.EZ002", "Don't Stop the Music", 1, "stage1/EZ002.bin", "Lou Grant"},
    {"stage2.EZ001", "Space Boy", 2, "stage2/EZ001.bin", "Dave Rodgers"},
    {"stage2.EZ002", "Night of Fire", 2, "stage2/EZ002.bin", "Niko"},
    {"stage2.NM001", "Don't Stop the Music", 2, "stage2/NM001.bin", "Lou Grant"},
    {"stage2.NM002", "Love Is in Danger", 2, "stage2/NM002.bin", "Priscilla"},
    {"stage2.HD001", "Killing My Love", 2, "stage2/HD001.bin", "Leslie Parrish"},
    {"stage2.HD002", "Running in the 90's", 2, "stage2/HD002.bin", "Max Coveri"},
    {"stage2.DF001", "Grand Prix", 2, "stage2/DF001.bin", "Mega NRG Man"},
    {"stage2.DF002", "Beat of the Rising Sun", 2, "stage2/DF002.bin", "Dave Rodgers"},
    {"stage2.VH001", "Heartbeat", 2, "stage2/VH001.bin", "Nathalie"},
    {"stage2.UH001", "Rock Me to the Top", 2, "stage2/UH001.bin", "Dusty"},
    {"stage2.UH002", "Station to Station", 2, "stage2/UH002.bin", "Derreck Simons"},
    {"stage4.avex_02_letsgocomeon", "Let's Go, Come On", 4, "stage4/avex_02_letsgocomeon.adx", "Manuel"},
    {"stage4.avex_03_gobeatcrazy", "Go Beat Crazy", 4, "stage4/avex_03_gobeatcrazy.adx", "Fastway"},
    {"stage4.avex_04_speedcar", "Speed Car", 4, "stage4/avex_04_speedcar.adx", "D-Team"},
    {"stage4.avex_05_flytometothemoon", "Fly to Me to the Moon & Back", 4, "stage4/avex_05_flytometothemoon.adx", "The Spiders from Mars"},
    {"stage4.avex_06_revolution", "Revolution", 4, "stage4/avex_06_revolution.adx", "Fastway"},
    {"stage4.avex_07_wellseeheaven", "We'll See Heaven", 4, "stage4/avex_07_wellseeheaven.adx", "Digital Planet"},
    {"stage4.avex_08_allaround", "All Around", 4, "stage4/avex_08_allaround.adx", "Lia"},
    {"stage4.avex_09_eldorado", "Eldorado", 4, "stage4/avex_09_eldorado.adx", "Dave Rodgers"},
    {"stage4.avex_10_raisinhell", "Raising Hell", 4, "stage4/avex_10_raisinhell.adx", "Fastway"},
    {"stage4.avex_11_spacelove", "Space Love", 4, "stage4/avex_11_spacelove.adx", "Fastway"},
    {"stage4.avex_12_nocontrol", "No Control", 4, "stage4/avex_12_nocontrol.adx", "Manuel"},
    {"stage4.avex_13_foreveryoung", "Forever Young", 4, "stage4/avex_13_foreveryoung.adx", "Symbol"},
    {"stage4.avex_14_riderofthesky", "Rider of the Sky", 4, "stage4/avex_14_riderofthesky.adx", "Ace"},
    {"stage4.avex_15_thefiresonme", "The Fire's on Me", 4, "stage4/avex_15_thefiresonme.adx", "Spock"},
    {"stage5.avex_02_sunintherain", "Sun in the Rain", 5, "stage5/avex_02_sunintherain.adx", "Manuel"},
    {"stage5.avex_03_lookabomba", "Looka Bomba", 5, "stage5/avex_03_lookabomba.adx", "Go 2"},
    {"stage5.avex_04_sweetsixteengirl", "Sweet Sixteen Girl", 5, "stage5/avex_04_sweetsixteengirl.adx", "Candy Taylor"},
    {"stage5.avex_05_loveisanameoflove", "Love Is the Name of Love", 5, "stage5/avex_05_loveisanameoflove.adx", "Irene"},
    {"stage5.avex_06_adrenaline", "Adrenaline", 5, "stage5/avex_06_adrenaline.adx", "Manuel"},
    {"stage5.avex_07_blackufo", "Black U.F.O.", 5, "stage5/avex_07_blackufo.adx", "Lupin"},
    {"stage5.avex_08_discofire", "Disco Fire", 5, "stage5/avex_08_discofire.adx", "Dave Rodgers"},
    {"stage5.avex_09_midnightlove", "Midnight Love", 5, "stage5/avex_09_midnightlove.adx", "Neo"},
    {"stage5.avex_10_gasgasgas", "Gas Gas Gas", 5, "stage5/avex_10_gasgasgas.adx", "Manuel"},
    {"stage5.avex_11_chemicallove", "Chemical Love", 5, "stage5/avex_11_chemicallove.adx", "Kevin & Cherry"},
    {"stage5.avex_12_rockinhardcore", "Rockin' Hardcore", 5, "stage5/avex_12_rockinhardcore.adx", "Fastway"},
    {"stage5.avex_13_speedman", "Speed Man", 5, "stage5/avex_13_speedman.adx", "Dave Simon"},
    {"stage5.avex_14_fighting", "Fighting", 5, "stage5/avex_14_fighting.adx", "Cody"},
    {"stage5.avex_15_rightnow", "Right Now", 5, "stage5/avex_15_rightnow.adx", "Dark Angels"},
    {"stage6.avex_01_super_rider", "Super Rider", 6, "stage6/avex_01_super_rider.wav", "Mark Astley"},
    {"stage6.avex_02_rock_beaten_wild", "Rock Beatin' Wild", 6, "stage6/avex_02_rock_beaten_wild.wav", "Fastway"},
    {"stage6.avex_03_once_upon_a_time", "Once Upon a Time", 6, "stage6/avex_03_once_upon_a_time.wav", "Pamsy"},
    {"stage6.avex_04_set_me_free", "Set Me Free", 6, "stage6/avex_04_set_me_free.wav", "Cherry"},
    {"stage6.avex_05_king_of_eurobeat", "King of Eurobeat", 6, "stage6/avex_05_king_of_eurobeat.wav", "Jordan"},
    {"stage6.avex_06_the_love_bite", "The Lovebite", 6, "stage6/avex_06_the_love_bite.wav", "Dusty"},
    {"stage6.avex_07_euro_night", "Euronight", 6, "stage6/avex_07_euro_night.wav", "Eurogroove"},
    {"stage6.avex_08_queen_of_meam", "Queen of Mean", 6, "stage6/avex_08_queen_of_meam.wav", "The Snake"},
    {"stage6.avex_09_mad_desire", "Mad Desire", 6, "stage6/avex_09_mad_desire.wav", "Stephy Martini"},
    {"stage6.avex_10_burn_into_the_beat", "Burn into the Beat", 6, "stage6/avex_10_burn_into_the_beat.wav", "Nick Mansell"},
    {"stage6.avex_11_forever_sad", "Forever Sad", 6, "stage6/avex_11_forever_sad.wav", "Hely"},
    {"stage6.avex_12_hurricane_man", "Hurricane Man", 6, "stage6/avex_12_hurricane_man.wav", "Gold-Rake"},
    {"stage6.avex_13_dont_turn_it_off", "Don't Turn It Off", 6, "stage6/avex_13_dont_turn_it_off.wav", "Go 2"},
    {"stage6.avex_14_you_are_my_wonder", "You Are My Wonder", 6, "stage6/avex_14_you_are_my_wonder.wav", "Queen 26"},
    {"stage7.avex_01_disconnected", "Disconnected", 7, "stage7/avex_01_disconnected.wav", "Hotblade"},
    {"stage7.avex_02_remember_me", "Remember Me", 7, "stage7/avex_02_remember_me.wav", "Leslie Parrish"},
    {"stage7.avex_03_night_of_fire", "Night of Fire", 7, "stage7/avex_03_night_of_fire.wav", "Niko"},
    {"stage7.avex_04_i_need_a_revolution", "I Need a Revolution", 7, "stage7/avex_04_i_need_a_revolution.wav", "Marko"},
    {"stage7.avex_05_power_two", "Power Two", 7, "stage7/avex_05_power_two.wav", "Hotblade"},
    {"stage7.avex_06_crazy_for_love", "Crazy for Love", 7, "stage7/avex_06_crazy_for_love.wav", "Dusty"},
    {"stage7.avex_07_burning_up_the_night(total_fire)", "Burning Up the Night (Total Fire)", 7, "stage7/avex_07_burning_up_the_night(total_fire).wav", "2 Fast"},
    {"stage7.avex_08_freedom_ride", "Freedom Ride", 7, "stage7/avex_08_freedom_ride.wav", "The Snake"},
    {"stage7.avex_09_ministry_of_power", "Ministry of Power", 7, "stage7/avex_09_ministry_of_power.wav", "Fastway"},
    {"stage7.avex_10_speed_of_light", "Speed of Light", 7, "stage7/avex_10_speed_of_light.wav", "The Snake"},
    {"stage7.avex_11_the_top", "The Top", 7, "stage7/avex_11_the_top.wav", "Ken Blast"},
    {"stage7.avex_12_up_and_dance_up_and_go", "Up & Dance, Up & Go", 7, "stage7/avex_12_up_and_dance_up_and_go.wav", "Lou Master"},
    {"stage7.avex_13_pamela", "Pamela", 7, "stage7/avex_13_pamela.wav", "Matt Land"},
    {"stage7.avex_14_limousine", "Limousine", 7, "stage7/avex_14_limousine.wav", "Manuel"},
    {"stage8.avex_01_breakin_out", "Breakin' Out", 8, "stage8/avex_01_breakin_out.wav", "Ace"},
    {"stage8.avex_02_notings_gonna_stop_us_tonight", "Nothing's Gonna Stop Us Tonight", 8, "stage8/avex_02_notings_gonna_stop_us_tonight.wav", "Annalise"},
    {"stage8.avex_03_come_on_baby", "Come On Baby", 8, "stage8/avex_03_come_on_baby.wav", "Fastway"},
    {"stage8.avex_04_sunlight", "Sunlight", 8, "stage8/avex_04_sunlight.wav", "Kaioh"},
    {"stage8.avex_05_prayer", "Prayer", 8, "stage8/avex_05_prayer.wav", "Ducky Chix"},
    {"stage8.avex_06_your_love_is_like_a_medicine", "Your Love Is Like a Medicine", 8, "stage8/avex_06_your_love_is_like_a_medicine.wav", "Mega NRG Man"},
    {"stage8.avex_07_when_the_sun_goes_down", "When the Sun Goes Down", 8, "stage8/avex_07_when_the_sun_goes_down.wav", "Ken Blast"},
    {"stage8.avex_08_super_driver", "Super Driver", 8, "stage8/avex_08_super_driver.wav", "Daniel"},
    {"stage8.avex_09_kiss", "Kiss", 8, "stage8/avex_09_kiss.wav", "Bamboo Bimbo"},
    {"stage8.avex_10_far_from_the_light", "Far from the Light", 8, "stage8/avex_10_far_from_the_light.wav", "Leo River"},
    {"stage8.avex_11_the_race_of_the_night", "The Race of the Night", 8, "stage8/avex_11_the_race_of_the_night.wav", "Dave Rodgers"},
    {"stage8.avex_12_nonsense_sensation", "Nonsense Sensation", 8, "stage8/avex_12_nonsense_sensation.wav", "Paul Harris"},
    {"stage8.avex_13_hearts_on_fire", "Heart's on Fire", 8, "stage8/avex_13_hearts_on_fire.wav", "David Dima"},
    {"stage8.avex_14_adrenaline", "Adrenaline", 8, "stage8/avex_14_adrenaline.wav", "Ace"},
    {"stage8.avex_15_never_say_never", "Never Say Never", 8, "stage8/avex_15_never_say_never.wav", "Manuel"},
    {"stage8.avex_16_i_just_wanna_stay_with_you", "I Just Wanna Stay with You", 8, "stage8/avex_16_i_just_wanna_stay_with_you.wav", "Dream Fighters"},
    {"specialstage.100", "100", 10, "specialstage/100.ADX", "Dave Rodgers"},
    {"specialstage.BACK_ON_THE_ROCKS", "Back on the Rocks", 10, "specialstage/BACK_ON_THE_ROCKS.ADX", "Mega NRG Man"},
    {"specialstage.BIG_IN_JAPAN", "Big in Japan", 10, "specialstage/BIG_IN_JAPAN.ADX", "Robert Patton"},
    {"specialstage.BURNING_DESIRE", "Burning Desire", 10, "specialstage/BURNING_DESIRE.ADX", "Mega NRG Man"},
    {"specialstage.CRAZY_FOR_YOUR_LOVE", "Crazy for Your Love", 10, "specialstage/CRAZY_FOR_YOUR_LOVE.ADX", "Morris"},
    {"specialstage.CRAZY_NIGHT", "Crazy Night", 10, "specialstage/CRAZY_NIGHT.ADX", "Boys Band"},
    {"specialstage.DONT_STAND_SO_CLOSE", "Don't Stand So Close", 10, "specialstage/DONT_STAND_SO_CLOSE.ADX", "Dr. Love"},
    {"specialstage.DONT_YOU", "Don't You (Forget About My Love)", 10, "specialstage/DONT_YOU.ADX", "Sophie"},
    {"specialstage.GET_ME_POWER", "Get Me Power", 10, "specialstage/GET_ME_POWER.ADX", "Mega NRG Man"},
    {"specialstage.I_NEED_YOUR_LOVE", "I Need Your Love", 10, "specialstage/I_NEED_YOUR_LOVE.ADX", "Dave Simon"},
    {"specialstage.MIKADO", "Mikado", 10, "specialstage/MIKADO.ADX", "Dave McLoud"},
    {"specialstage.NO_ONE_SLEEP_IN_TOKYO", "No One Sleep in Tokyo", 10, "specialstage/NO_ONE_SLEEP_IN_TOKYO.ADX", "Edo Boys"},
    {"specialstage.STAY", "Stay", 10, "specialstage/STAY.ADX", "Victoria"},
    {"specialstage.WEST_END_GUY", "West End Guy", 10, "specialstage/WEST_END_GUY.ADX", "Digital Planet"},
    {"specialstage.WHITE_LIGHT", "White Light", 10, "specialstage/WHITE_LIGHT.ADX", "Mr. Groove"},
}};

inline int clampMusicTrack(int index) {
    return std::clamp(index,0,int(raceMusicCatalog.size())-1);
}
inline int findMusicTrack(std::string_view id) {
    for(std::size_t i=0;i<raceMusicCatalog.size();++i)
        if(id==raceMusicCatalog[i].id)return int(i);
    return -1;
}
}
