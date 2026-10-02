#include "core/paths.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

namespace lim::paths {
namespace fs = std::filesystem;

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

static fs::path P(const std::string& s) { return fs::path(widen(s)); }
static std::string S(const fs::path& p) { return narrow(p.wstring()); }

std::string join(const std::string& a, const std::string& b) { return S(P(a) / P(b)); }
std::string parent(const std::string& path) { return S(P(path).parent_path()); }
std::string fileName(const std::string& path) { return S(P(path).filename()); }

bool exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(P(path), ec);
}

bool createDirectories(const std::string& path) {
    std::error_code ec;
    fs::create_directories(P(path), ec);
    return !ec;
}

bool removeFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(P(path), ec);
}

const std::string& exeDir() {
    static const std::string dir = [] {
        std::wstring buf(32768, L'\0');
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        buf.resize(n);
        return S(fs::path(buf).parent_path());
    }();
    return dir;
}

std::string dataDir() { return join(exeDir(), "data"); }
std::string shaderDir() { return join(exeDir(), "shaders"); }

static std::string knownFolder(REFKNOWNFOLDERID id, const char* envFallback) {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p)) && p) out = narrow(p);
    if (p) CoTaskMemFree(p);
    if (out.empty()) {
        wchar_t buf[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(widen(envFallback).c_str(), buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) out = narrow(std::wstring(buf, n));
    }
    if (out.empty()) out = exeDir();  // letzter Ausweg: portabel neben der EXE
    return out;
}

std::string userDataDir() {
    // LIMINAL_USER_DIR: eigener Ordner fuer Spielstaende/Einstellungen (Tests, portable Nutzung)
    static const std::string d = [] {
        wchar_t buf[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(L"LIMINAL_USER_DIR", buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) return narrow(std::wstring(buf, n));
        return join(knownFolder(FOLDERID_RoamingAppData, "APPDATA"), "LIMINAL");
    }();
    return d;
}

std::string localDataDir() {
    static const std::string d = join(knownFolder(FOLDERID_LocalAppData, "LOCALAPPDATA"), "LIMINAL");
    return d;
}

std::string screenshotDir() {
    static const std::string d = join(knownFolder(FOLDERID_Pictures, "USERPROFILE"), "LIMINAL");
    return d;
}

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream f(P(path), std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::optional<std::vector<unsigned char>> readBinary(const std::string& path) {
    std::ifstream f(P(path), std::ios::binary | std::ios::ate);
    if (!f) return std::nullopt;
    auto size = f.tellg();
    std::vector<unsigned char> data((size_t)size);
    f.seekg(0);
    if (size > 0 && !f.read(reinterpret_cast<char*>(data.data()), size)) return std::nullopt;
    return data;
}

bool writeFileAtomic(const std::string& path, const std::string& content, bool keepBackup) {
    createDirectories(parent(path));
    std::wstring wpath = widen(path);
    std::wstring tmp = wpath + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, content.data(), (DWORD)content.size(), &written, nullptr) && written == content.size();
    ok = ok && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (keepBackup && GetFileAttributesW(wpath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        MoveFileExW(wpath.c_str(), (wpath + L".bak").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
    if (!MoveFileExW(tmp.c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

std::vector<std::string> listFiles(const std::string& dir, const std::string& prefix, const std::string& suffix) {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto it = fs::directory_iterator(P(dir), ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        std::string name = S(it->path().filename());
        if (name.size() >= prefix.size() + suffix.size() && name.compare(0, prefix.size(), prefix) == 0 &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
            out.push_back(S(it->path()));
        }
    }
    return out;
}

}  // namespace lim::paths
