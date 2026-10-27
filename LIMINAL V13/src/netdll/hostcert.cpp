#include "netdll/hostcert.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pem.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>
#include <psa/crypto.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <vector>

#include "netdll/joincode.hpp"

namespace lim::netc {

namespace {
const char kBeginCrt[] = "-----BEGIN CERTIFICATE-----\n";
const char kEndCrt[] = "-----END CERTIFICATE-----\n";

std::string hexFp(const unsigned char h[32]) {
    static const char* d = "0123456789ABCDEF";
    std::string s;
    for (int i = 0; i < 32; ++i) {
        if (i) s += ':';
        s += d[h[i] >> 4], s += d[h[i] & 15];
    }
    return s;
}

std::string pemOf(const unsigned char* der, size_t n) {
    std::vector<unsigned char> buf(n * 2 + 128);
    size_t olen = 0;
    if (mbedtls_pem_write_buffer(kBeginCrt, kEndCrt, der, n, buf.data(), buf.size(), &olen) != 0) return {};
    return std::string((const char*)buf.data(), olen > 0 ? olen - 1 : 0);  // olen zaehlt die Null mit
}

std::string utcStamp(std::time_t t) {
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char b[16];
    std::snprintf(b, sizeof(b), "%04d%02d%02d%02d%02d%02d", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
    return b;
}
}  // namespace

bool makeHostCert(HostCert& out, std::string& error, const char* subject) {
    psa_crypto_init();
    mbedtls_entropy_context ent;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_pk_context key;
    mbedtls_x509write_cert w;
    mbedtls_entropy_init(&ent);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_pk_init(&key);
    mbedtls_x509write_crt_init(&w);
    bool ok = false;
    do {
        static const char pers[] = "liminal host cert";
        if (mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &ent, (const unsigned char*)pers, sizeof(pers) - 1) != 0) {
            error = "kein Zufall";
            break;
        }
        if (mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0 ||
            mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), mbedtls_ctr_drbg_random, &drbg) != 0) {
            error = "Schluessel nicht erzeugt";
            break;
        }
        unsigned char serial[16];
        if (!randomBytes(serial, sizeof(serial))) {
            error = "kein Zufall";
            break;
        }
        serial[0] &= 0x7F;  // positiv
        const std::time_t now = std::time(nullptr);
        mbedtls_x509write_crt_set_version(&w, MBEDTLS_X509_CRT_VERSION_3);
        mbedtls_x509write_crt_set_md_alg(&w, MBEDTLS_MD_SHA256);
        mbedtls_x509write_crt_set_subject_key(&w, &key);
        mbedtls_x509write_crt_set_issuer_key(&w, &key);
        if (mbedtls_x509write_crt_set_subject_name(&w, subject) != 0 || mbedtls_x509write_crt_set_issuer_name(&w, subject) != 0 ||
            mbedtls_x509write_crt_set_serial_raw(&w, serial, sizeof(serial)) != 0 ||
            mbedtls_x509write_crt_set_validity(&w, utcStamp(now - 86400).c_str(), utcStamp(now + 30 * 86400).c_str()) != 0) {
            error = "Zertifikat nicht beschreibbar";
            break;
        }
        // DER einmal erzeugen (die Signatur ist zufaellig) und daraus PEM und Fingerabdruck bilden
        std::vector<unsigned char> buf(4096);
        const int n = mbedtls_x509write_crt_der(&w, buf.data(), buf.size(), mbedtls_ctr_drbg_random, &drbg);
        if (n <= 0) {
            error = "Zertifikat nicht signiert";
            break;
        }
        const unsigned char* der = buf.data() + buf.size() - (size_t)n;
        out.certPem = pemOf(der, (size_t)n);
        unsigned char h[32];
        mbedtls_sha256(der, (size_t)n, h, 0);
        out.fingerprint = hexFp(h);
        std::vector<unsigned char> kb(2048);
        if (out.certPem.empty() || mbedtls_pk_write_key_pem(&key, kb.data(), kb.size()) != 0) {
            error = "PEM nicht erzeugt";
            break;
        }
        out.keyPem = (const char*)kb.data();
        mbedtls_platform_zeroize(kb.data(), kb.size());
        ok = true;
    } while (false);
    mbedtls_x509write_crt_free(&w);
    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&ent);
    return ok;
}

std::string fingerprintOfPem(const std::string& certPem) {
    mbedtls_x509_crt c;
    mbedtls_x509_crt_init(&c);
    std::string out;
    if (mbedtls_x509_crt_parse(&c, (const unsigned char*)certPem.c_str(), certPem.size() + 1) == 0) {
        unsigned char h[32];
        mbedtls_sha256(c.raw.p, c.raw.len, h, 0);
        out = hexFp(h);
    }
    mbedtls_x509_crt_free(&c);
    return out;
}

}  // namespace lim::netc
