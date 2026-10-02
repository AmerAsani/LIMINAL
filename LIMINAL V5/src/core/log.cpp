#include "core/log.hpp"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>

#include "core/core.hpp"
#include "core/paths.hpp"

#include <windows.h>

namespace lim::log {
namespace {

std::mutex gMutex;
std::FILE* gFile = nullptr;
std::string gPath;

const char* levelName(Level l) {
    switch (l) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO ";
        case Level::Warn: return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?";
}

}  // namespace

void init(const std::string& path) {
    std::lock_guard lock(gMutex);
    gPath = path;
    paths::createDirectories(paths::parent(path));
    gFile = _wfopen(paths::widen(path).c_str(), L"w");
}

void shutdown() {
    std::lock_guard lock(gMutex);
    if (gFile) {
        std::fclose(gFile);
        gFile = nullptr;
    }
}

const std::string& filePath() { return gPath; }

void write(Level level, std::string_view message) {
    using namespace std::chrono;
    const auto now = floor<milliseconds>(system_clock::now());
    std::string line = std::format("{:%H:%M:%S} {} {}\n", now, levelName(level), message);
    std::lock_guard lock(gMutex);
    if (gFile) {
        std::fwrite(line.data(), 1, line.size(), gFile);
        if (level >= Level::Warn) std::fflush(gFile);
    }
    OutputDebugStringW(paths::widen(line).c_str());
#ifndef NDEBUG
    std::fwrite(line.data(), 1, line.size(), stderr);
#endif
}

}  // namespace lim::log

namespace lim {

[[noreturn]] void fatalError(std::string_view message, const char* file, int line) {
    std::string text = std::format("{}\n\n({}:{})", message, file, line);
    log::write(log::Level::Error, text);
    log::shutdown();
    std::wstring msg = paths::widen(std::string(message)) + L"\n\nDetails stehen in der Protokolldatei:\n" +
                       paths::widen(log::filePath());
    MessageBoxW(nullptr, msg.c_str(), L"LIMINAL - Fehler", MB_OK | MB_ICONERROR);
    ExitProcess(3);
}

}  // namespace lim
