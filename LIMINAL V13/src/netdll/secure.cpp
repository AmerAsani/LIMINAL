#include "netdll/secure.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <cstring>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

namespace lim::netsec {

namespace {
// Algorithmen einmal oeffnen (CNG-Handles sind threadsicher nutzbar)
BCRYPT_ALG_HANDLE hmacAlg() {
    static BCRYPT_ALG_HANDLE h = [] {
        BCRYPT_ALG_HANDLE a = nullptr;
        if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) a = nullptr;
        return a;
    }();
    return h;
}
}  // namespace

bool randomBytes(u8* out, size_t n) {
    return NT_SUCCESS(BCryptGenRandom(nullptr, out, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool hmacSha256(const u8* key, size_t keyLen, const u8* a, size_t na, const u8* b, size_t nb, u8 out[32]) {
    BCRYPT_ALG_HANDLE alg = hmacAlg();
    if (!alg) return false;
    BCRYPT_HASH_HANDLE h = nullptr;
    if (!NT_SUCCESS(BCryptCreateHash(alg, &h, nullptr, 0, (PUCHAR)key, (ULONG)keyLen, 0))) return false;
    bool ok = NT_SUCCESS(BCryptHashData(h, (PUCHAR)a, (ULONG)na, 0));
    if (ok && nb) ok = NT_SUCCESS(BCryptHashData(h, (PUCHAR)b, (ULONG)nb, 0));
    if (ok) ok = NT_SUCCESS(BCryptFinishHash(h, out, 32, 0));
    BCryptDestroyHash(h);
    return ok;
}

bool hmacSha256(const u8* key, size_t keyLen, const u8* data, size_t n, u8 out[32]) { return hmacSha256(key, keyLen, data, n, nullptr, 0, out); }

bool hkdfSha256(const u8* ikm, size_t ikmLen, const u8* salt, size_t saltLen, const u8* info, size_t infoLen, u8* out, size_t outLen) {
    if (outLen > 255 * 32) return false;
    u8 zero[32] = {};
    u8 prk[32];
    // Extract: PRK = HMAC(salt, IKM) (ohne Salz: 32 Nullbytes)
    if (!hmacSha256(saltLen ? salt : zero, saltLen ? saltLen : 32, ikm, ikmLen, prk)) return false;
    // Expand: T(i) = HMAC(PRK, T(i-1) | info | i)
    u8 t[32];
    size_t tLen = 0, done = 0;
    u8 buf[32 + 256 + 1];
    if (infoLen > 256) return false;
    for (u8 i = 1; done < outLen; ++i) {
        size_t m = 0;
        std::memcpy(buf, t, tLen), m += tLen;
        std::memcpy(buf + m, info, infoLen), m += infoLen;
        buf[m++] = i;
        if (!hmacSha256(prk, 32, buf, m, t)) return false;
        tLen = 32;
        const size_t take = outLen - done < 32 ? outLen - done : 32;
        std::memcpy(out + done, t, take);
        done += take;
    }
    SecureZeroMemory(prk, sizeof(prk));
    SecureZeroMemory(t, sizeof(t));
    return true;
}

bool equalConstTime(const u8* a, const u8* b, size_t n) {
    u8 d = 0;
    for (size_t i = 0; i < n; ++i) d |= (u8)(a[i] ^ b[i]);
    return d == 0;
}

Aead::~Aead() { reset(); }

void Aead::reset() {
    if (key_) BCryptDestroyKey((BCRYPT_KEY_HANDLE)key_);
    if (alg_) BCryptCloseAlgorithmProvider((BCRYPT_ALG_HANDLE)alg_, 0);
    alg_ = key_ = nullptr;
}

Aead& Aead::operator=(Aead&& o) noexcept {
    if (this != &o) {
        reset();
        alg_ = o.alg_, key_ = o.key_;
        o.alg_ = o.key_ = nullptr;
    }
    return *this;
}

bool Aead::init(const u8 key[32]) {
    reset();
    BCRYPT_ALG_HANDLE a = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&a, BCRYPT_AES_ALGORITHM, nullptr, 0))) return false;
    if (!NT_SUCCESS(BCryptSetProperty(a, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
        BCryptCloseAlgorithmProvider(a, 0);
        return false;
    }
    BCRYPT_KEY_HANDLE k = nullptr;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(a, &k, nullptr, 0, (PUCHAR)key, 32, 0))) {
        BCryptCloseAlgorithmProvider(a, 0);
        return false;
    }
    alg_ = a, key_ = k;
    return true;
}

bool Aead::seal(const u8 nonce[kNonce], const u8* aad, size_t aadLen, const u8* pt, size_t n, u8* out) const {
    if (!key_) return false;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)nonce;
    info.cbNonce = (ULONG)kNonce;
    info.pbAuthData = (PUCHAR)aad;
    info.cbAuthData = (ULONG)aadLen;
    info.pbTag = out + n;
    info.cbTag = (ULONG)kTag;
    ULONG written = 0;
    const NTSTATUS st = BCryptEncrypt((BCRYPT_KEY_HANDLE)key_, (PUCHAR)pt, (ULONG)n, &info, nullptr, 0, out, (ULONG)n, &written, 0);
    return NT_SUCCESS(st) && written == n;
}

bool Aead::open(const u8 nonce[kNonce], const u8* aad, size_t aadLen, const u8* in, size_t n, u8* out) const {
    if (!key_) return false;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = (PUCHAR)nonce;
    info.cbNonce = (ULONG)kNonce;
    info.pbAuthData = (PUCHAR)aad;
    info.cbAuthData = (ULONG)aadLen;
    info.pbTag = (PUCHAR)(in + n);
    info.cbTag = (ULONG)kTag;
    ULONG written = 0;
    const NTSTATUS st = BCryptDecrypt((BCRYPT_KEY_HANDLE)key_, (PUCHAR)in, (ULONG)n, &info, nullptr, 0, out, (ULONG)n, &written, 0);
    return NT_SUCCESS(st) && written == n;
}

}  // namespace lim::netsec
