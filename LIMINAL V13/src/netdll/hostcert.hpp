// V13.3: Zertifikat des Hosts fuer DTLS (eines je Sitzung). Erzeugung und Pruefung macht Mbed TLS; hier werden nur
// dessen Funktionen aufgerufen.
#pragma once

#include <string>

namespace lim::netc {

struct HostCert {
    std::string certPem, keyPem;
    std::string fingerprint;  // SHA-256 der DER-Form, "AB:CD:..." (wie in SDP)
};

// ECDSA P-256, selbst signiert (RFC 8827 verlangt nichts anderes); gueltig von gestern bis in 30 Tagen
// subject: Name im Zertifikat (Tests der Lobby mit TLS: "CN=localhost")
bool makeHostCert(HostCert& out, std::string& error, const char* subject = "CN=liminal");
// SHA-256 eines PEM-Zertifikats als "AB:CD:..." (Pruefung, Tests); leer bei Fehler
std::string fingerprintOfPem(const std::string& certPem);

}  // namespace lim::netc
