#include "netdll/joincode.hpp"

#include <psa/crypto.h>

#include <cctype>
#include <cstring>
#include <mutex>

namespace lim::netc {

namespace {
const char kAlpha[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

int symbol(char c) {
    c = (char)std::toupper((unsigned char)c);
    if (c == 'O') c = '0';
    if (c == 'I' || c == 'L') c = '1';
    const char* p = c ? std::strchr(kAlpha, c) : nullptr;
    return p ? (int)(p - kAlpha) : -1;
}
}  // namespace

std::string JoinCode::text(bool withPin) const {
    const std::string raw = room + secret + (withPin ? pin : std::string());
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (i && i % 4 == 0) out += '-';
        out += raw[i];
    }
    return out;
}

bool parseCode(const std::string& s, JoinCode& out) {
    if (s.size() > 64) return false;
    std::string raw;
    for (char c : s) {
        if (c == '-' || c == ' ') continue;
        const int v = symbol(c);
        if (v < 0) return false;
        raw += kAlpha[v];
    }
    if (raw.size() != (size_t)(kRoomChars + kSecretChars) && raw.size() != (size_t)(kRoomChars + kSecretChars + kPinChars)) return false;
    out.room = raw.substr(0, kRoomChars);
    out.secret = raw.substr(kRoomChars, kSecretChars);
    out.pin = raw.size() > (size_t)(kRoomChars + kSecretChars) ? raw.substr(kRoomChars + kSecretChars) : std::string();
    return true;
}

bool randomBytes(uint8_t* out, size_t n) {
    static std::once_flag once;
    static bool ok = false;
    std::call_once(once, [] { ok = psa_crypto_init() == PSA_SUCCESS; });
    return ok && psa_generate_random(out, n) == PSA_SUCCESS;
}

bool randomChars(int n, std::string& out) {
    out.clear();
    uint8_t b[64];
    if (n <= 0 || n > 64 || !randomBytes(b, (size_t)n)) return false;
    for (int i = 0; i < n; ++i) out += kAlpha[b[i] & 31];  // 256 ist ein Vielfaches von 32: gleichverteilt
    return true;
}

std::string pinFromFingerprint(const std::string& fp) {
    // "AB:CD:EF:..." (32 Byte SHA-256) -> die ersten 60 Bit als 12 Zeichen
    if (fp.size() != 32 * 3 - 1) return {};
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        c = (char)std::toupper((unsigned char)c);
        return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    };
    uint8_t bytes[32];
    for (int k = 0; k < 32; ++k) {
        const int hi = hex(fp[(size_t)k * 3]), lo = hex(fp[(size_t)k * 3 + 1]);
        if (hi < 0 || lo < 0 || (k < 31 && fp[(size_t)k * 3 + 2] != ':')) return {};
        bytes[k] = (uint8_t)(hi * 16 + lo);
    }
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) v = (v << 8) | bytes[k];
    std::string out;
    for (int k = 0; k < kPinChars; ++k) out += kAlpha[(v >> (59 - 5 * k)) & 31];  // Bits 63..4
    return out;
}

bool equalConst(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char d = 0;
    for (size_t i = 0; i < a.size(); ++i) d |= (unsigned char)(a[i] ^ b[i]);
    return d == 0;
}

bool roomOk(const std::string& s) {
    if (s.size() != (size_t)kRoomChars) return false;
    for (char c : s)
        if (!c || !std::strchr(kAlpha, c)) return false;
    return true;
}

}  // namespace lim::netc
