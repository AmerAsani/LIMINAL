# LIMINAL.exe: Starter ohne C-Laufzeitbibliothek (laeuft auch, wenn genau diese fehlt)
add_executable(LIMINAL WIN32 launcher.cpp launcher.rc)
target_compile_definitions(LIMINAL PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
target_compile_options(LIMINAL PRIVATE -Os -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector
    -fno-builtin -mno-stack-arg-probe -Wall)
target_link_options(LIMINAL PRIVATE -nostdlib -nostartfiles -Wl,-e,LauncherMain -s)
target_link_libraries(LIMINAL PRIVATE kernel32 user32 shell32 advapi32 wintrust)
