# CMake-Toolchain fuer die portable llvm-mingw-Installation in <Projekt>/.toolchain.
# Wird von tools/build.ps1 automatisch verwendet.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

get_filename_component(_LIM_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_LIM_TC "${_LIM_ROOT}/.toolchain/llvm-mingw")

if(NOT EXISTS "${_LIM_TC}/bin/clang++.exe")
    message(FATAL_ERROR "llvm-mingw nicht gefunden unter ${_LIM_TC}. Bitte zuerst tools/setup_toolchain.ps1 ausfuehren.")
endif()

set(CMAKE_C_COMPILER   "${_LIM_TC}/bin/x86_64-w64-mingw32-clang.exe")
set(CMAKE_CXX_COMPILER "${_LIM_TC}/bin/x86_64-w64-mingw32-clang++.exe")
set(CMAKE_RC_COMPILER  "${_LIM_TC}/bin/x86_64-w64-mingw32-windres.exe")
set(CMAKE_AR           "${_LIM_TC}/bin/llvm-ar.exe")
set(CMAKE_RANLIB       "${_LIM_TC}/bin/llvm-ranlib.exe")

set(CMAKE_FIND_ROOT_PATH "${_LIM_TC}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
