// Base64 (fuer eingebettete Vorschaubilder in Spielstaenden)
#pragma once

#include <string>
#include <vector>

namespace lim {

inline std::string base64Encode(const unsigned char* data, size_t n) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)data[i] << 16;
        if (i + 1 < n) v |= (unsigned)data[i + 1] << 8;
        if (i + 2 < n) v |= data[i + 2];
        out += t[(v >> 18) & 63];
        out += t[(v >> 12) & 63];
        out += i + 1 < n ? t[(v >> 6) & 63] : '=';
        out += i + 2 < n ? t[v & 63] : '=';
    }
    return out;
}

inline std::vector<unsigned char> base64Decode(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<unsigned char> out;
    unsigned buf = 0;
    int bits = 0;
    for (char c : s) {
        int v = val(c);
        if (v < 0) continue;
        buf = (buf << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((unsigned char)((buf >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace lim
