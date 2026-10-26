// V13.2: Kryptografie des Mehrspielers - ausschliesslich ueber die Windows-Kryptografie (CNG, bcrypt.dll), keine eigene
// Kryptografie (MULTIPLAYER_BEDROHUNGSMODELL.md, Regel 7; MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 5).
//   Zufall        BCryptGenRandom (System-RNG)
//   HMAC-SHA256   BCrypt-Hash mit HMAC-Flag
//   HKDF-SHA256   RFC 5869 aus HMAC (Extract + Expand)
//   AES-256-GCM   BCryptEncrypt/BCryptDecrypt, 96-Bit-Nonce, 128-Bit-Tag
// tests/net_transport_test.cpp prueft alles gegen die Testvektoren aus RFC 4231, RFC 5869 und der GCM-Spezifikation.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lim::netsec {

using u8 = std::uint8_t;
using u64 = std::uint64_t;

constexpr size_t kTag = 16;    // GCM-Tag
constexpr size_t kNonce = 12;  // GCM-Nonce

bool randomBytes(u8* out, size_t n);
bool hmacSha256(const u8* key, size_t keyLen, const u8* data, size_t n, u8 out[32]);
// HMAC ueber zwei Teile (spart Kopien beim Handshake)
bool hmacSha256(const u8* key, size_t keyLen, const u8* a, size_t na, const u8* b, size_t nb, u8 out[32]);
bool hkdfSha256(const u8* ikm, size_t ikmLen, const u8* salt, size_t saltLen, const u8* info, size_t infoLen, u8* out, size_t outLen);
// Vergleich ohne fruehen Abbruch (keine Zeitunterschiede je Byte)
bool equalConstTime(const u8* a, const u8* b, size_t n);

// AES-256-GCM mit festem Schluessel
class Aead {
public:
    Aead() = default;
    ~Aead();
    Aead(const Aead&) = delete;
    Aead& operator=(const Aead&) = delete;
    Aead(Aead&& o) noexcept : alg_(o.alg_), key_(o.key_) { o.alg_ = o.key_ = nullptr; }
    Aead& operator=(Aead&& o) noexcept;
    void reset();
    bool init(const u8 key[32]);
    bool ready() const { return key_ != nullptr; }
    // out: n Byte Geheimtext + 16 Byte Tag
    bool seal(const u8 nonce[kNonce], const u8* aad, size_t aadLen, const u8* pt, size_t n, u8* out) const;
    // in: n Byte Geheimtext + 16 Byte Tag; false bei falschem Tag (veraendert, falscher Schluessel)
    bool open(const u8 nonce[kNonce], const u8* aad, size_t aadLen, const u8* in, size_t n, u8* out) const;

private:
    void* alg_ = nullptr;  // BCRYPT_ALG_HANDLE
    void* key_ = nullptr;  // BCRYPT_KEY_HANDLE
};

}  // namespace lim::netsec
