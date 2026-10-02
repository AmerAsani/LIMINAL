// Einstiegspunkt von LiminalGame.exe
#include <windows.h>
#include <shellapi.h>

#include "app/app.hpp"
#include "platform/crash.hpp"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    lim::plat::installCrashHandler();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(lim::paths::narrow(argv[i]));
    LocalFree(argv);
    lim::App app;
    return app.run(args);
}
