// V13.3: Verbindung des Spiels zur Lobby (WebSocket, ws:// oder wss://) ueber WinHTTP von Windows.
//
// Warum nicht der WebSocket-Client von libdatachannel: Unter Windows schaltet libdatachannel die Pruefung des
// TLS-Zertifikats ab ("not supported on Windows", websocket.cpp) - mit wss:// koennte sich jeder als Lobby ausgeben.
// WinHTTP prueft mit Schannel gegen die Stammzertifikate von Windows (samt automatischer Aktualisierung und Sperrlisten)
// und unterstuetzt WebSocket (RFC 6455). Kryptografie und Protokoll macht Windows; hier steht nur die Ansteuerung.
//
// Optional: festgelegtes Zertifikat der Lobby (Pinning, fuer selbst betriebene Lobbys mit eigenem Zertifikat):
// dann zaehlt nur, dass der SHA-256-Fingerabdruck des Server-Zertifikats genau passt - gepruft, bevor die erste
// Lobby-Nachricht gesendet wird.
#pragma once

#include <memory>
#include <optional>
#include <string>

namespace lim::netc {

class LobbyClient {
public:
    enum class Error { None, Resolve, Connect, Tls, Pin, Upgrade, Closed, Protocol };
    LobbyClient();
    ~LobbyClient();
    LobbyClient(const LobbyClient&) = delete;
    LobbyClient& operator=(const LobbyClient&) = delete;

    // url: ws://host:port/pfad oder wss://...; pinFingerprint: leer oder "AB:CD:..." (SHA-256 des Server-Zertifikats)
    // useProxy: Proxy-Einstellungen von Windows verwenden (Lobby im Internet); nicht fuer LAN/direkt
    void open(const std::string& url, const std::string& pinFingerprint, bool useProxy);
    bool isOpen() const;
    bool isClosed() const;  // geschlossen oder gescheitert
    Error error() const;
    bool send(const std::string& text);
    std::optional<std::string> receive();  // naechste Textnachricht (nicht blockierend)
    void close();

private:
    struct Impl;
    std::shared_ptr<Impl> d_;
};

}  // namespace lim::netc
