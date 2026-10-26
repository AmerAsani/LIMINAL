/* V13.3: Einstellungen fuer Mbed TLS 3.6 (third_party/mbedtls, unveraendert) - nur fuer liminal_net.dll, den
   Lobby-Server und die Netztests. Ergaenzt die Standardkonfiguration (include/mbedtls/mbedtls_config.h).
   Begruendung: MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 5. */
#ifndef LIMINAL_MBEDTLS_CONFIG_H
#define LIMINAL_MBEDTLS_CONFIG_H

/* libdatachannel setzt im DTLS-Handshake die SRTP-Erweiterung (RFC 5764) - ohne sie baut es nicht. Medien
   (Ton, Bild) werden nicht uebertragen. */
#define MBEDTLS_SSL_DTLS_SRTP

/* libdatachannel arbeitet mit mehreren Threads; der globale Zustand von PSA (Schluesselplaetze) braucht dann
   Sperren. winpthreads gehoert zu llvm-mingw. */
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_PTHREAD

/* Sockets oeffnet libdatachannel selbst; die Socket-Hilfen von Mbed TLS werden nicht gebraucht. */
#undef MBEDTLS_NET_C

#endif
