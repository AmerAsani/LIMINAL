#include <windows.h>

#include "coop/netapi.hpp"
#include "core/log.hpp"

namespace lim::coop {

bool loadNetLibrary(NetApi& api, std::string& error) {
    static HMODULE mod = nullptr;
    if (!mod) {
        // nur neben dem Spiel suchen (kein Nachladen einer fremden DLL aus dem Arbeitsordner oder PATH)
        wchar_t path[MAX_PATH * 2];
        const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
        if (n == 0 || n >= MAX_PATH * 2) {
            error = "Programmpfad nicht lesbar";
            return false;
        }
        std::wstring p(path, n);
        p = p.substr(0, p.find_last_of(L"\\/") + 1) + L"liminal_net.dll";
        mod = LoadLibraryExW(p.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!mod) {
            error = "liminal_net.dll fehlt neben dem Spiel";
            return false;
        }
    }
    auto get = [&](const char* name) { return (void*)GetProcAddress(mod, name); };
    api.host = (decltype(api.host))get("ln_host");
    api.connect = (decltype(api.connect))get("ln_connect");
    api.poll = (decltype(api.poll))get("ln_poll");
    api.send = (decltype(api.send))get("ln_send");
    api.drop = (decltype(api.drop))get("ln_drop");
    api.stats = (decltype(api.stats))get("ln_stats");
    api.port = (decltype(api.port))get("ln_port");
    api.close = (decltype(api.close))get("ln_close");
    api.random = (decltype(api.random))get("ln_random");
    api.version = (decltype(api.version))get("ln_version");
    if (!api.ok()) {
        error = "liminal_net.dll ist unvollstaendig";
        return false;
    }
    if (api.version() != LN_VERSION) {
        error = "liminal_net.dll hat eine andere Version";
        return false;
    }
    log::info("Koop: liminal_net.dll geladen (Version {})", api.version());
    return true;
}

}  // namespace lim::coop
