cmake_minimum_required(VERSION 3.20)
project(ReimaginedDiscTools LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(native "${CMAKE_CURRENT_LIST_DIR}/../../Native")
foreach(tool IN ITEMS capture_original_assembly capture_course_roster capture_car_presentation capture_original_car_materials
                      capture_original_ranking_records capture_original_demo_course capture_original_demo_camera
                      capture_original_rival_dialog_data capture_course_object_selections
                      reference_original_tuning_course_packages reference_original_tuning_course_pulse)
    add_executable(${tool} "${native}/tools/${tool}.cpp")
    target_include_directories(${tool} PRIVATE "${native}/tests" "${native}/src")
endforeach()
target_sources(capture_original_demo_camera PRIVATE "${native}/src/original_demo_data.cpp")
target_sources(capture_original_rival_dialog_data PRIVATE "${native}/src/original_rival_dialog_data.cpp")
target_sources(reference_original_tuning_course_packages PRIVATE "${native}/src/original_tuning.cpp" "${native}/src/original_battle_profile.cpp")

