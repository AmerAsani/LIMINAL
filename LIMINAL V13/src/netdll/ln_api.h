/* V13.3: Schnittstelle von liminal_net.dll (Transport des Koop-Modus; MULTIPLAYER_ARCHITEKTUR.md, Abschnitte 4-7).
   Das Spiel laedt die DLL erst beim Hosten oder Beitreten. Alle Nutzdaten sind hier bereits entschluesselt und auf
   die Hoechstgroesse geprueft; was sie bedeuten, prueft das Spiel (src/net).

   Transport: WebRTC-Datenkanaele (libdatachannel: ICE/STUN/TURN ueber libjuice, DTLS 1.2 ueber Mbed TLS, SCTP).
   Verbindungsaufbau ueber eine Lobby (Beitrittscode statt IP; MULTIPLAYER_SETUP.md) oder direkt (die Lobby laeuft
   dann im Host selbst, Adresse + langer Code). Je Gast zwei Kanaele: zuverlaessig/geordnet und unzuverlaessig.

   Ablauf beim Beitritt:
     Gast -> Lobby -> Host   Wunsch zum Beitritt (Raumnummer, Name)
     Host -> Lobby -> Gast   Beschreibung (SDP mit Zertifikats-Fingerabdruck), ICE-Kandidaten; Gast antwortet ebenso
     ICE findet einen Weg (direkt, ueber NAT oder ueber das TURN-Relay); DTLS prueft die Fingerabdruecke
     Gast                    prueft den Fingerabdruck des Hosts gegen den Pruefteil des Codes (wenn angegeben)
     Gast -> Host   HELLO    Version, Geheimteil des Codes, Name (im DTLS-Kanal)
     Host -> Gast   WELCOME  erst jetzt melden beide CONNECTED; sonst REFUSE mit Grund
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

#define LN_VERSION 3
#define LN_MAX_PAYLOAD 1200   /* groesste Nachricht des Spiels */
#define LN_NAME_MAX 16
#define LN_CODE_MAX 40        /* Beitrittscode als Text (lang: 24 Zeichen + Bindestriche) */

enum LnChannel { LN_RELIABLE = 0, LN_UNRELIABLE = 1 };
enum LnEventType {
    LN_EV_NONE = 0,
    LN_EV_CONNECTED = 1,     /* Host: Gast angenommen (peer, name); Gast: verbunden */
    LN_EV_DISCONNECTED = 2,  /* reason; Host: peer -1 = die Sitzung selbst (z. B. Lobby nicht erreichbar) */
    LN_EV_MESSAGE = 3,
    LN_EV_ROOM = 4,          /* Host: Beitrittscode bereit (data: kurzer Code, name: leer) oder verloren (len 0) */
    LN_EV_PROGRESS = 5       /* Gast: Fortschritt (reason: LnProgress) */
};
enum LnReason {
    LN_R_NONE = 0, LN_R_NORMAL = 1, LN_R_TIMEOUT = 2, LN_R_BADKEY = 3, LN_R_FULL = 4, LN_R_VERSION = 5, LN_R_PROTOCOL = 6,
    LN_R_BANNED = 7, LN_R_KICKED = 8, LN_R_RATE = 9, LN_R_UNREACHABLE = 10,
    LN_R_LOBBY = 11,         /* Lobby nicht erreichbar oder Verbindung zur Lobby verloren */
    LN_R_NOROOM = 12,        /* Code unbekannt oder Raum geschlossen */
    LN_R_ICE = 13,           /* kein Weg zur Gegenstelle (NAT/Firewall, Relay nicht erreichbar) */
    LN_R_CERT = 14,          /* Zertifikat passt nicht (Pruefteil des Codes oder TLS der Lobby) */
    LN_R_BADCODE = 15,       /* Code hat das falsche Format (oder direkt ohne Pruefteil) */
    LN_R_HOSTGONE = 16,      /* Host hat die Sitzung beendet */
    LN_R_BUSY = 17           /* Host nimmt gerade andere Gaeste auf (halbe Beitritte voll) */
};
enum LnProgress { LN_P_LOBBY = 1, LN_P_SIGNALING = 2, LN_P_CONNECTING = 3, LN_P_HELLO = 4 };
enum LnPath { LN_PATH_UNKNOWN = 0, LN_PATH_DIRECT = 1, LN_PATH_NAT = 2, LN_PATH_RELAY = 3 };

typedef struct LnEvent {
    int type;              /* LnEventType */
    int peer;              /* Nummer der Gegenstelle (Host: 0..max-1; Gast: immer 0) */
    int channel;           /* LnChannel */
    int reason;            /* LnReason bei LN_EV_DISCONNECTED, LnProgress bei LN_EV_PROGRESS */
    int len;               /* Laenge der Nutzdaten */
    uint8_t data[LN_MAX_PAYLOAD];
    char name[LN_NAME_MAX + 1]; /* Name des Gasts bei LN_EV_CONNECTED (Host) */
} LnEvent;

typedef struct LnStats {
    uint32_t rttMs, rttVarMs;  /* Laufzeit hin und zurueck (Ping), Schwankung */
    float loss;                /* Anteil unbeantworteter Pings der letzten 20 s */
    uint32_t sent, received, dropped; /* Nachrichten; verworfene: ungueltig oder ueber der Rate */
    int path;                  /* LnPath: direkt, ueber NAT (STUN), ueber Relay (TURN) */
    uint64_t bytesSent, bytesReceived;
} LnStats;

typedef struct LnLimits {
    int maxPeers;          /* Gaeste (Host) */
    int maxHalfOpen;       /* gleichzeitige Beitritte im Aufbau */
    double rate, burst;    /* Nachrichten je Sekunde und Gast */
    int maxInvalid;        /* ungueltige Nachrichten bis zur Trennung */
    int handshakeMs, silenceMs;
    int banFails;          /* falsche Codes je Adresse bis zur Sperre */
    int banMs;
} LnLimits;

typedef struct LnConfig {
    const char* lobby;       /* "wss://name:port" oder "ws://..." ; leer/NULL: ohne Lobby (direkt) */
    const char* lobbyCert;   /* festgelegtes Zertifikat der Lobby: PEM oder SHA-256 "AB:CD:..."; leer: Stammzertifikate
                                von Windows (wss:// ueber WinHTTP/Schannel) */
    const char* ice;         /* weitere Server, durch Leerzeichen getrennt: "stun:host:port turn:user:pass@host:port" */
    const char* bindAddress; /* nur Tests: z. B. "127.0.0.1" */
    uint16_t port;           /* direkt: TCP-Port der eingebauten Lobby und UDP-Port der Verbindungen (0: 27500) */
    int relayOnly;           /* 1: nur ueber das Relay (die eigene IP bleibt der Gegenseite verborgen) */
} LnConfig;

typedef struct LnContext LnContext;

/* Host: Raum oeffnen (Lobby) bzw. eingebaute Lobby starten (direkt). Der Code kommt mit LN_EV_ROOM. */
LN_API LnContext* ln_host(const LnConfig* cfg, const LnLimits* lim);
/* Gast: beitreten. direct: "adresse[:port]" ohne Lobby (verlangt den langen Code), sonst NULL */
LN_API LnContext* ln_join(const LnConfig* cfg, const char* direct, const char* code, const char* name);
/* Naechstes Ereignis holen (nicht blockierend). 1 = ev gefuellt, 0 = keins */
LN_API int ln_poll(LnContext* c, LnEvent* ev);
/* Nachricht senden (Gast: peer 0). 1 = angenommen */
LN_API int ln_send(LnContext* c, int peer, int channel, const uint8_t* data, int len);
/* Gegenstelle trennen (Host: Kick; ban = 1 sperrt die Adresse fuer die Sitzung, auch in der Lobby). Meldet kein
   LN_EV_DISCONNECTED: der Aufrufer weiss es schon */
LN_API void ln_drop(LnContext* c, int peer, int reason, int ban);
LN_API int ln_stats(LnContext* c, int peer, LnStats* out);
/* Host: Beitrittscode (lang = mit Pruefteil); 0, solange es keinen gibt */
LN_API int ln_code(LnContext* c, int withPin, char* out, int cap);
/* Host direkt: Port der eingebauten Lobby (TCP) und der Verbindungen (UDP) */
LN_API uint16_t ln_port(LnContext* c);
LN_API void ln_close(LnContext* c);
/* Hilfen: Zufall (Mbed TLS/PSA), Version der DLL */
LN_API int ln_random(uint8_t* out, int n);
LN_API int ln_version(void);

#ifdef __cplusplus
}
#endif
