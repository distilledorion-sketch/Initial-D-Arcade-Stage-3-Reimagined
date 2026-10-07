# Reuse the complete verified native application's translation units.
target_sources(idas3_native_assets PRIVATE src/unity_scene_capture.cpp src/unity_ui_capture.cpp)
set_source_files_properties(src/unity_scene_capture.cpp src/unity_ui_capture.cpp PROPERTIES COMPILE_DEFINITIONS "NOMINMAX;WIN32_LEAN_AND_MEAN")
if(TARGET InitialDRemake)
  get_target_property(IDAS3_APP_SOURCES InitialDRemake SOURCES)
  list(FILTER IDAS3_APP_SOURCES EXCLUDE REGEX "(^|/)main\\.cpp$")
else()
  # InitialDRemake is intentionally Windows-only. Keep the Android plugin's
  # source closure explicit so CMake can configure without creating that host.
  set(IDAS3_APP_SOURCES
    src/renderer.cpp src/ui.cpp src/audio.cpp src/original_audio.cpp
    src/original_menu_audio.cpp src/frontend.cpp src/original_mode_menu.cpp
    src/original_choice_menu.cpp src/original_hud.cpp src/original_results.cpp
    src/car_shadow.cpp src/car_presentation.cpp src/course_scene_catalog.cpp
    src/original_number_plate.cpp src/original_car_body_position.cpp
    src/original_legend_menu.cpp src/original_battle_hud.cpp
    src/original_battle_names.cpp src/original_demo_presentation.cpp
    src/original_ranking_presentation.cpp src/original_tuning_preview.cpp)
endif()
if(ANDROID)
  # Android uses the portable scene ABI: native code publishes CPU scene
  # records and Unity owns the Vulkan/GLES device and presentation.
  foreach(IDAS3_PORTABLE_TARGET IN ITEMS idas3_core idas3_native_assets idas3_original)
    if(TARGET ${IDAS3_PORTABLE_TARGET})
      target_compile_definitions(${IDAS3_PORTABLE_TARGET} PUBLIC IDAS3_PORTABLE_SCENE)
      target_compile_options(${IDAS3_PORTABLE_TARGET} PRIVATE -ffp-contract=off -fno-fast-math)
    endif()
  endforeach()
  add_library(Idas3Unity SHARED src/unity_bridge.cpp src/unity_audio_output.cpp ${IDAS3_APP_SOURCES})
  target_include_directories(Idas3Unity PRIVATE src)
  target_compile_definitions(Idas3Unity PRIVATE IDAS3_UNITY_PLUGIN IDAS3_PORTABLE_SCENE NOMINMAX)
  target_compile_options(Idas3Unity PRIVATE -Wall -Wextra -ffp-contract=off -fno-fast-math)
  target_link_options(Idas3Unity PRIVATE "-Wl,-z,max-page-size=16384")
  target_link_libraries(Idas3Unity PRIVATE idas3_core idas3_native_assets idas3_original log android)
  set(IDAS3_ANDROID_PLUGIN_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../Assets/Plugins/Android/arm64-v8a" CACHE PATH "Android Unity plugin staging directory")
  set_target_properties(Idas3Unity PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${IDAS3_ANDROID_PLUGIN_OUTPUT_DIRECTORY}"
    OUTPUT_NAME "Idas3Unity")
else()
  add_library(Idas3Unity SHARED src/unity_bridge.cpp src/unity_audio_output.cpp ${IDAS3_APP_SOURCES})
  target_include_directories(Idas3Unity PRIVATE src)
  target_compile_definitions(Idas3Unity PRIVATE IDAS3_UNITY_PLUGIN UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(Idas3Unity PRIVATE /W4 /fp:strict)
  target_link_libraries(Idas3Unity PRIVATE idas3_core idas3_native_assets idas3_original d3d11 dxgi d3dcompiler gdi32 user32 shell32 xinput9_1_0 winmm)
  add_executable(unity_scene_smoke tests/unity_scene_smoke.cpp)
  target_include_directories(unity_scene_smoke PRIVATE src)
  target_compile_definitions(unity_scene_smoke PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(unity_scene_smoke PRIVATE /W4 /fp:strict)
  target_link_libraries(unity_scene_smoke PRIVATE d3d11)
  set_target_properties(Idas3Unity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../Assets/Plugins/x86_64"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT ANDROID AND EXISTS "${CMAKE_SOURCE_DIR}/tests/unity_shared_renderer_tests.cpp")
  add_executable(unity_shared_renderer_tests tests/unity_shared_renderer_tests.cpp src/renderer.cpp)
  target_compile_definitions(unity_shared_renderer_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(unity_shared_renderer_tests PRIVATE /W4 /fp:strict)
  target_link_libraries(unity_shared_renderer_tests PRIVATE idas3_native_assets idas3_original d3d11 dxgi d3dcompiler user32)
  add_test(NAME unity_shared_renderer COMMAND unity_shared_renderer_tests "${CMAKE_SOURCE_DIR}")
endif()

if(NOT ANDROID AND EXISTS "${CMAKE_SOURCE_DIR}/tests/unity_bridge_smoke.cpp")
  add_executable(unity_bridge_smoke tests/unity_bridge_smoke.cpp)
  target_include_directories(unity_bridge_smoke PRIVATE src)
  target_compile_definitions(unity_bridge_smoke PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(unity_bridge_smoke PRIVATE /W4 /fp:strict)
  target_link_libraries(unity_bridge_smoke PRIVATE d3d11)
endif()
