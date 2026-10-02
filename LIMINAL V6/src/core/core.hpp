// Grundlegende Typen, Makros und Hilfsfunktionen, die jedes Modul nutzt.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lim {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Nicht kopierbare Basisklasse fuer Ressourcenbesitzer.
struct NonCopyable {
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

[[noreturn]] void fatalError(std::string_view message, const char* file, int line);

}  // namespace lim

#define LIM_CHECK(cond, msg)                                    \
    do {                                                        \
        if (!(cond)) ::lim::fatalError((msg), __FILE__, __LINE__); \
    } while (0)

#ifndef NDEBUG
#define LIM_ASSERT(cond) LIM_CHECK((cond), "Assertion fehlgeschlagen: " #cond)
#else
#define LIM_ASSERT(cond) ((void)0)
#endif
