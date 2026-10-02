#include "platform/crash.hpp"

#include <ctime>
#include <string>

#include "core/log.hpp"
#include "core/paths.hpp"

#include <windows.h>
#include <dbghelp.h>

namespace lim::plat {

namespace {

using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

LONG WINAPI onCrash(EXCEPTION_POINTERS* ep) {
    static volatile LONG once = 0;
    if (InterlockedExchange(&once, 1)) return EXCEPTION_EXECUTE_HANDLER;
    std::string dir = paths::join(paths::localDataDir(), "crash");
    paths::createDirectories(dir);
    char name[64];
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::strftime(name, sizeof(name), "liminal_v6_%Y%m%d_%H%M%S.dmp", &tm);
    std::string path = paths::join(dir, name);
    DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    HMODULE base = GetModuleHandleW(nullptr);
    log::error("Absturz: Code 0x{:08X} bei Adresse {} (Modulbasis {}) - Minidump: {}", (unsigned)code, addr, (void*)base,
               path);
    log::shutdown();
    if (HMODULE dbg = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        auto write = (MiniDumpWriteDumpFn)(void*)GetProcAddress(dbg, "MiniDumpWriteDump");
        HANDLE f = CreateFileW(paths::widen(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (write && f != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), ep, FALSE};
            write(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, ep ? &mei : nullptr, nullptr, nullptr);
        }
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    }
    std::wstring msg = L"LIMINAL ist leider abgestürzt.\n\nEin Fehlerbericht wurde gespeichert unter:\n" + paths::widen(path) +
                       L"\n\nDer Spielstand der letzten automatischen Speicherung bleibt erhalten.";
    MessageBoxW(nullptr, msg.c_str(), L"LIMINAL", MB_OK | MB_ICONERROR);
    // Exitcode 3 = "Fehler wurde bereits gemeldet": der Starter zeigt dann keine zweite Meldung
    TerminateProcess(GetCurrentProcess(), 3);
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

void installCrashHandler() {
    SetUnhandledExceptionFilter(onCrash);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
}

}  // namespace lim::plat
