cmake_minimum_required(VERSION 3.25)
project(LIMINAL VERSION 7.0.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(LIM_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${LIM_BIN_DIR}")

# ------------------------------------------------------------------------------------
# Compiler-Einstellungen
# ------------------------------------------------------------------------------------
add_library(lim_options INTERFACE)
target_compile_definitions(lim_options INTERFACE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00
    LIM_VERSION="${PROJECT_VERSION}")
target_compile_options(lim_options INTERFACE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-O0 -g>)
# Alles statisch binden (libc++, libunwind, winpthreads): ausser Windows-Systembibliotheken
# und der Universal CRT (Teil von Windows 10/11) wird keine DLL benoetigt.
target_link_options(lim_options INTERFACE -static $<$<CONFIG:Release>:-s>)
target_include_directories(lim_options INTERFACE "${CMAKE_SOURCE_DIR}/src")

function(lim_module name)
    file(GLOB _src CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/${name}/*.cpp")
    add_library(lim_${name} STATIC ${_src})
    target_link_libraries(lim_${name} PUBLIC lim_options ${ARGN})
endfunction()

# ------------------------------------------------------------------------------------
# Module (Abhaengigkeiten zeigen nur "nach unten")
# ------------------------------------------------------------------------------------
lim_module(core shell32 ole32 uuid)
lim_module(platform lim_core user32 gdi32 windowscodecs shlwapi)
lim_module(input lim_core user32)
lim_module(world lim_core)
lim_module(physics lim_world)
lim_module(ecs lim_core)
lim_module(render lim_world lim_platform d3d11 dxgi gdi32)
lim_module(audio lim_core ole32 ksuser avrt)
lim_module(gameplay lim_world lim_physics lim_ecs lim_platform)
lim_module(ui lim_gameplay lim_render lim_input)

# ------------------------------------------------------------------------------------
# Shader (HLSL -> Bytecode .cso beim Build; das Spiel braucht keinen Compiler)
# ------------------------------------------------------------------------------------
add_executable(shaderc tools/shaderc/shaderc.cpp)
target_link_libraries(shaderc PRIVATE lim_options)
target_link_options(shaderc PRIVATE -municode)
set_target_properties(shaderc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

set(LIM_SHADERS
    "world_gbuffer:VSMain:vs_5_0" "world_gbuffer:PSMain:ps_5_0"
    "mesh_gbuffer:VSMain:vs_5_0" "mesh_gbuffer:PSMain:ps_5_0"
    "fullscreen:VSMain:vs_5_0"
    "bloom:DownPS:ps_5_0" "bloom:UpPS:ps_5_0"
    "post:PSMain:ps_5_0"
    "ui:VSMain:vs_5_0" "ui:PSMain:ps_5_0"
    "lighting:CSMain:cs_5_0"
    "ssao:AoCS:cs_5_0" "ssao:BlurCS:cs_5_0"
    "taa:CSMain:cs_5_0"
    "exposure:CSMain:cs_5_0"
    "ssr:CSMain:cs_5_0"
    "particles:VSMain:vs_5_0" "particles:PSMain:ps_5_0"
    "volumetric:CSMain:cs_5_0")
file(GLOB LIM_SHADER_INCLUDES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/shaders/*.hlsli")
set(LIM_SHADER_OUTPUTS)
foreach(spec ${LIM_SHADERS})
    string(REPLACE ":" ";" parts ${spec})
    list(GET parts 0 file)
    list(GET parts 1 entry)
    list(GET parts 2 profile)
    set(out "${LIM_BIN_DIR}/shaders/${file}_${entry}.cso")
    set(dbg "")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(dbg "-g")
    endif()
    add_custom_command(OUTPUT "${out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${LIM_BIN_DIR}/shaders"
        COMMAND "$<TARGET_FILE:shaderc>" "${CMAKE_SOURCE_DIR}/shaders/${file}.hlsl" ${entry} ${profile} "${out}" ${dbg}
        DEPENDS shaderc "${CMAKE_SOURCE_DIR}/shaders/${file}.hlsl" ${LIM_SHADER_INCLUDES}
        COMMENT "Shader ${file}.hlsl:${entry}"
        VERBATIM)
    list(APPEND LIM_SHADER_OUTPUTS "${out}")
endforeach()
add_custom_target(lim_shaders ALL DEPENDS ${LIM_SHADER_OUTPUTS})

# Spieldaten und Symbole neben die EXE kopieren
file(GLOB_RECURSE LIM_DATA_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/data/*")
add_custom_target(lim_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/data" "${LIM_BIN_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${LIM_BIN_DIR}/assets"
    DEPENDS ${LIM_DATA_FILES}
    COMMENT "Kopiere Spieldaten")

# ------------------------------------------------------------------------------------
# Spiel
# ------------------------------------------------------------------------------------
file(GLOB LIM_APP_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/app/*.cpp")
add_executable(LiminalGame WIN32 ${LIM_APP_SRC} src/app/liminal.rc)
target_link_libraries(LiminalGame PRIVATE lim_ui lim_audio lim_gameplay lim_render lim_input lim_platform lim_core
    user32 gdi32 ole32 shell32 winmm)
target_link_options(LiminalGame PRIVATE -municode)
add_dependencies(LiminalGame lim_shaders lim_data)

# ------------------------------------------------------------------------------------
# Starter (Voraussetzungen pruefen, dann das Spiel starten)
# ------------------------------------------------------------------------------------
add_subdirectory(launcher)

# ------------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------------
add_executable(worldgen_test tests/worldgen_test.cpp)
target_link_libraries(worldgen_test PRIVATE lim_world)
set_target_properties(worldgen_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# V6: Item-Balance (Energie-Riegel seltener) ueber viele Sektoren und Seeds pruefen
add_executable(supply_balance_test tests/supply_balance_test.cpp)
target_link_libraries(supply_balance_test PRIVATE lim_gameplay)
set_target_properties(supply_balance_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# V6: Erkundungskarte, Sprung/Treppen und Koerperanimation ohne Grafik pruefen
add_executable(gameplay_test tests/gameplay_test.cpp)
target_link_libraries(gameplay_test PRIVATE lim_gameplay)
set_target_properties(gameplay_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# V6: Klangerzeugung und raeumliches Mischen (Richtung, Verdeckung, Nachhall, Pegel) ohne Audiogeraet
add_executable(audio_test tests/audio_test.cpp)
target_link_libraries(audio_test PRIVATE lim_audio)
set_target_properties(audio_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# V6: raeumliche Struktur (Raumdichte, Raumgroessen, Sichtachsen) alter gegen neuer Aufbau
add_executable(layout_stats tests/layout_stats.cpp)
target_link_libraries(layout_stats PRIVATE lim_gameplay)
set_target_properties(layout_stats PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
