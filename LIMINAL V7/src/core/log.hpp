// Protokollierung in Datei (%LOCALAPPDATA%\LIMINAL\logs) und Debug-Ausgabe.
#pragma once

#include <format>
#include <string_view>

namespace lim::log {

enum class Level { Debug, Info, Warn, Error };

void init(const std::string& filePath);
void shutdown();
void write(Level level, std::string_view message);
const std::string& filePath();

template <class... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace lim::log
