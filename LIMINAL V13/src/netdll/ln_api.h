/* V13.2: Schnittstelle von liminal_net.dll (Transport des Mehrspielers: ENet, Handshake, Verschluesselung).
   Das Spiel laedt die DLL erst beim Hosten oder Beitreten (MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 6). Alle Nutzdaten
   sind hier bereits entschluesselt und auf die Hoechstgroesse geprueft; was sie bedeuten, prueft das Spiel (src/net).

   Ablauf (Abschnitt 5):
     Gast -> Host   HELLO      Magie, Version, Zufall des Gasts (16), Name (16)
     Host -> Gast   CHALLENGE  Magie, Zufall des Hosts (16), Keks (4)
     Gast -> Host   PROOF      Magie, Keks, HMAC-SHA256(Schluessel, "LIMN-proof" | Zufall Gast | Zufall Host | Name)[0..16]
     danach         DATA       Zaehler (8), AES-256-GCM(Nutzdaten) + Tag (16); Schluessel je Richtung aus HKDF
     Host -> Gast   REFUSE     Grund (unverschluesselt, kleiner als die Anfrage)
*/
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(LN_BUILD)
#define LN_API __declspec(dllexport)
#else
#define LN_API
#endif

#define LN_VERSION 2
#define LN_MAX_PAYLOAD 1200   /* groesste Nachricht des Spiels (entschluesselt) */
#define LN_NAME_MAX 16
#define LN_KEY_BYTES 16       /* Beitrittsschluessel: 128 Bit */

enum LnChannel { LN_RELIABLE = 0, LN_UNRELIABLE = 1 };
enum LnEventType { LN_EV_NONE = 0, LN_EV_CONNECTED = 1, LN_EV_DISCONNECTED = 2, LN_EV_MESSAGE = 3 };
enum LnReason {
    LN_R_NONE = 0, LN_R_NORMAL = 1, LN_R_TIMEOUT = 2, LN_R_BADKEY = 3, LN_R_FULL = 4, LN_R_VERSION = 5, LN_R_PROTOCOL = 6,
    LN_R_BANNED = 7, LN_R_KICKED = 8, LN_R_RATE = 9, LN_R_UNREACHABLE = 10
};

typedef struct LnEvent {
    int type;              /* LnEventType */
    int peer;              /* Nummer der Gegenstelle (Host: 0..max-1; Gast: immer 0) */
    int channel;           /* LnChannel */
    int reason;            /* LnReason bei LN_EV_DISCONNECTED */
    int len;               /* Laenge der Nutzdaten */
    uint8_t data[LN_MAX_PAYLOAD];
    char name[LN_NAME_MAX + 1]; /* Name des Gasts bei LN_EV_CONNECTED (Host) */
} LnEvent;

typedef struct LnStats {
    uint32_t rttMs, rttVarMs;
    float loss;            /* Anteil verlorener Pakete (ENet) */
    uint32_t sent, received, dropped; /* Nachrichten; verworfene: ungueltig, alt, ueber der Rate */
} LnStats;

typedef struct LnLimits {
    int maxPeers;          /* Gaeste (Host) */
    int maxHalfOpen;       /* gleichzeitige Handshakes */
    double rate, burst;    /* Nachrichten je Sekunde und Gast */
    int maxInvalid;        /* ungueltige Nachrichten bis zur Trennung */
    int handshakeMs, silenceMs;
    int banFails;          /* falsche Beweise je Adresse bis zur Sperre */
    int banMs;
} LnLimits;

typedef struct LnContext LnContext;

/* Host: oeffnet UDP-Port (0 = beliebig; tatsaechlicher Port ueber ln_port). key: Beitrittsschluessel. */
LN_API LnContext* ln_host(uint16_t port, const uint8_t key[LN_KEY_BYTES], const LnLimits* lim);
/* Gast: verbindet zu host:port (Name, Schluessel) */
LN_API LnContext* ln_connect(const char* host, uint16_t port, const uint8_t key[LN_KEY_BYTES], const char* name);
/* Naechstes Ereignis holen (nicht blockierend). 1 = ev gefuellt, 0 = keins */
LN_API int ln_poll(LnContext* c, LnEvent* ev);
/* Nachricht senden (Gast: peer 0). 1 = angenommen */
LN_API int ln_send(LnContext* c, int peer, int channel, const uint8_t* data, int len);
/* Gegenstelle trennen (Host: Kick; ban = 1 sperrt die Adresse fuer die Sitzung) */
LN_API void ln_drop(LnContext* c, int peer, int reason, int ban);
LN_API int ln_stats(LnContext* c, int peer, LnStats* out);
LN_API uint16_t ln_port(LnContext* c);
LN_API void ln_close(LnContext* c);
/* Hilfen: Zufall (BCryptGenRandom), Version der DLL */
LN_API int ln_random(uint8_t* out, int n);
LN_API int ln_version(void);

#ifdef __cplusplus
}
#endif
