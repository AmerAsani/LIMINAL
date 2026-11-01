#include "netdll/lobby_client.hpp"

#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>

#include <mbedtls/sha256.h>

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "netdll/lobby_proto.hpp"

namespace lim::netc {

namespace {
std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }  // nur ASCII (geprueft)

std::string fpOf(const unsigned char* der, size_t n) {
    unsigned char h[32];
    mbedtls_sha256(der, n, h, 0);
    static const char* d = "0123456789ABCDEF";
    std::string s;
    for (int i = 0; i < 32; ++i) {
        if (i) s += ':';
        s += d[h[i] >> 4], s += d[h[i] & 15];
    }
    return s;
}

std::string upperHex(const std::string& s) {
    std::string o;
    for (char c : s) o += (char)std::toupper((unsigned char)c);
    return o;
}
}  // namespace

struct LobbyClient::Impl {
    std::mutex m;  // Griffe, Eingang
    std::deque<std::string> inbox;
    std::atomic<int> state{0};  // 0 verbinde, 1 offen, 2 zu
    std::atomic<Error> err{Error::None};
    std::atomic<bool> stopping{false};
    HINTERNET session = nullptr, connection = nullptr, request = nullptr, ws = nullptr;
    std::mutex sendMx;
    std::thread th;

    void fail(Error e) {
        Error none = Error::None;
        err.compare_exchange_strong(none, e);
        state = 2;
    }
    void closeHandles() {
        std::lock_guard<std::mutex> l(m);
        if (ws) WinHttpCloseHandle(ws), ws = nullptr;
        if (request) WinHttpCloseHandle(request), request = nullptr;
        if (connection) WinHttpCloseHandle(connection), connection = nullptr;
        if (session) WinHttpCloseHandle(session), session = nullptr;
    }

    void run(std::string url, std::string pin, bool proxy) {
        bool tls = false;
        std::string rest;
        if (url.rfind("wss://", 0) == 0) tls = true, rest = url.substr(6);
        else if (url.rfind("ws://", 0) == 0) rest = url.substr(5);
        else return fail(Error::Connect);
        const size_t slash = rest.find('/');
        const std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
        rest = rest.substr(0, slash);
        std::string host = rest;
        INTERNET_PORT port = tls ? 443 : 80;
        if (!rest.empty() && rest[0] == '[') {
            const size_t e = rest.find(']');
            host = rest.substr(1, e == std::string::npos ? std::string::npos : e - 1);
            if (e != std::string::npos && e + 1 < rest.size() && rest[e + 1] == ':') port = (INTERNET_PORT)std::atoi(rest.c_str() + e + 2);
        } else if (const size_t p = rest.rfind(':'); p != std::string::npos) {
            host = rest.substr(0, p);
            port = (INTERNET_PORT)std::atoi(rest.c_str() + p + 1);
        }
        if (host.empty() || !hostOk(host) || port == 0) return fail(Error::Connect);
        {
            std::lock_guard<std::mutex> l(m);
            if (stopping) return fail(Error::Closed);
            session = WinHttpOpen(L"LIMINAL", proxy ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session && proxy) session = WinHttpOpen(L"LIMINAL", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) return fail(Error::Connect);
            WinHttpSetTimeouts(session, 5000, 8000, 8000, 8000);
            DWORD keep = 15000;  // Lebenszeichen alle 15 s (kleinster Wert)
            WinHttpSetOption(session, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &keep, sizeof(keep));
            connection = WinHttpConnect(session, widen(host).c_str(), port, 0);
            if (connection)
                request = WinHttpOpenRequest(connection, L"GET", widen(path).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             tls ? WINHTTP_FLAG_SECURE : 0);
            if (!request) return fail(Error::Connect);
            if (tls && !pin.empty()) {  // festgelegtes Zertifikat: nur der Fingerabdruck zaehlt (unten geprueft)
                DWORD f = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                          SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &f, sizeof(f));
            }
            WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
        }
        // Verbindung, TLS-Handshake und Upgrade-Anfrage (enthaelt nur den Pfad, keine Lobby-Daten)
        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
            const DWORD e = GetLastError();
            if (e == ERROR_WINHTTP_SECURE_FAILURE || e == ERROR_WINHTTP_SECURE_INVALID_CA || e == ERROR_WINHTTP_SECURE_CERT_CN_INVALID ||
                e == ERROR_WINHTTP_SECURE_CERT_DATE_INVALID || e == ERROR_WINHTTP_SECURE_INVALID_CERT || e == ERROR_WINHTTP_SECURE_CERT_REVOKED)
                return fail(Error::Tls);
            return fail(e == ERROR_WINHTTP_NAME_NOT_RESOLVED ? Error::Resolve : Error::Connect);
        }
        if (tls && !pin.empty()) {
            PCCERT_CONTEXT cc = nullptr;
            DWORD len = sizeof(cc);
            if (!WinHttpQueryOption(request, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &cc, &len) || !cc) return fail(Error::Pin);
            const std::string fp = fpOf(cc->pbCertEncoded, cc->cbCertEncoded);
            CertFreeCertificateContext(cc);
            if (fp != upperHex(pin)) return fail(Error::Pin);
        }
        DWORD status = 0, sl = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sl, WINHTTP_NO_HEADER_INDEX);
        if (status != 101) return fail(Error::Upgrade);
        {
            std::lock_guard<std::mutex> l(m);
            if (stopping) return fail(Error::Closed);
            ws = WinHttpWebSocketCompleteUpgrade(request, 0);
            WinHttpCloseHandle(request), request = nullptr;
            if (!ws) return fail(Error::Upgrade);
            DWORD inf = 0;
            WinHttpSetOption(ws, WINHTTP_OPTION_RECEIVE_TIMEOUT, &inf, sizeof(inf));
        }
        state = 1;
        std::vector<char> buf(kMaxLobbyMsg + 1);
        size_t have = 0;
        for (;;) {
            DWORD got = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
            HINTERNET h;
            {
                std::lock_guard<std::mutex> l(m);
                h = ws;
            }
            if (!h || stopping) return fail(Error::Closed);
            const DWORD r = WinHttpWebSocketReceive(h, buf.data() + have, (DWORD)(buf.size() - have), &got, &type);
            if (r != NO_ERROR) return fail(Error::Closed);
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) return fail(Error::Closed);
            if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || type == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE)
                return fail(Error::Protocol);
            have += got;
            if (have > kMaxLobbyMsg) return fail(Error::Protocol);  // zu gross: die Lobby spricht falsch
            if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                std::lock_guard<std::mutex> l(m);
                if (inbox.size() < 256) inbox.emplace_back(buf.data(), have);
                have = 0;
            }
        }
    }
};

LobbyClient::LobbyClient() : d_(std::make_shared<Impl>()) {}
LobbyClient::~LobbyClient() { close(); }

void LobbyClient::open(const std::string& url, const std::string& pin, bool useProxy) {
    if (d_->th.joinable()) return;
    std::shared_ptr<Impl> d = d_;
    d_->th = std::thread([d, url, pin, useProxy] { d->run(url, pin, useProxy); });
}

bool LobbyClient::isOpen() const { return d_->state == 1; }
bool LobbyClient::isClosed() const { return d_->state == 2; }
LobbyClient::Error LobbyClient::error() const { return d_->err; }

bool LobbyClient::send(const std::string& text) {
    if (d_->state != 1 || text.size() > kMaxLobbyMsg) return false;
    std::lock_guard<std::mutex> s(d_->sendMx);
    HINTERNET h;
    {
        std::lock_guard<std::mutex> l(d_->m);
        h = d_->ws;
    }
    if (!h) return false;
    return WinHttpWebSocketSend(h, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)text.data(), (DWORD)text.size()) == NO_ERROR;
}

std::optional<std::string> LobbyClient::receive() {
    std::lock_guard<std::mutex> l(d_->m);
    if (d_->inbox.empty()) return std::nullopt;
    std::string s = std::move(d_->inbox.front());
    d_->inbox.pop_front();
    return s;
}

void LobbyClient::close() {
    d_->stopping = true;
    {
        std::lock_guard<std::mutex> s(d_->sendMx);
        std::lock_guard<std::mutex> l(d_->m);
        if (d_->ws) WinHttpWebSocketShutdown(d_->ws, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        // Griffe schliessen bricht laufende (blockierende) Aufrufe des Lesefadens ab
        if (d_->ws) WinHttpCloseHandle(d_->ws), d_->ws = nullptr;
        if (d_->request) WinHttpCloseHandle(d_->request), d_->request = nullptr;
    }
    if (d_->th.joinable()) d_->th.join();
    d_->closeHandles();
    if (d_->state != 2) d_->state = 2;
}

}  // namespace lim::netc
