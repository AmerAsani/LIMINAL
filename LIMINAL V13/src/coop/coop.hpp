// V13.2/V13.3 Koop: Host und Gast ueber dem verschluesselten Transport (MULTIPLAYER_ARCHITEKTUR.md).
//
//   CoopHost   oeffnet einen Raum (Lobby oder direkt), nimmt Gaeste an (Code, Version, Spielstand), wendet ihre Eingaben auf ihre
//              Figuren an, prueft jede Absicht an seinem Zustand, sendet Momentaufnahmen (20 je Sekunde),
//              Ereignisse, Inventare und die Gruppe; entscheidet Ebenenwechsel, Tod und Wiedereinstieg.
//   CoopClient tritt bei, sagt die eigene Bewegung voraus und korrigiert sie mit dem Stand des Hosts, zeigt
//              Mitspieler und Wesen 100 ms in der Vergangenheit, sendet nur Eingaben und Absichten.
//
// Jede Nachricht geht durch net::coop::decode (Typ, Laenge, Bereich, Rolle, Zustand) und eine Rate je Typ. Fehler
// trennen die Verbindung, nie das Spiel.
#pragma once

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "coop/netapi.hpp"
#include "gameplay/session.hpp"
#include "net/coop_proto.hpp"

namespace lim::coop {

struct PlayerInfo {
    int id = 0;
    std::string name;
    bool alive = true, ready = false, connected = true, self = false;
    int rttMs = 0;
    int path = 0;     // LnPath: direkt, ueber NAT, ueber Relay (0: unbekannt)
    int lossPct = 0;  // unbeantwortete Pings der letzten 20 s
};

// Text zu einem Grund der DLL (LnReason), fuer Meldungen
const char* reasonText(int reason);

// Marken-Eimer je Nachrichtentyp
struct Bucket {
    double rate, burst, tokens, last = -1.0;
    Bucket(double r, double b) : rate(r), burst(b), tokens(b) {}
    bool take(double now);
};

class CoopHost {
public:
    CoopHost(NetApi api, std::string hostName, u32 buildHash);
    ~CoopHost();
    // Raum oeffnen: ueber die Lobby oder direkt (eingebaute Lobby); der Code kommt kurz danach (code())
    bool start(const NetSettings& net, int maxPlayers, std::string& error);
    void stop();
    std::string code(bool withPin) const;  // Beitrittscode; leer, solange keiner da ist
    bool direct() const { return !net_.useLobby; }
    const NetSettings& net() const { return net_; }
    std::string lobbyStatus() const;       // Zustand der Lobby (leer: alles gut)
    bool takeFailure(std::string& why);    // Raum liess sich nicht oeffnen (Lobby nicht erreichbar, Zertifikat ...)
    u16 port() const;
    bool running() const { return ctx_ != nullptr; }

    // je Ebene: die Sitzung anhaengen; die Gaeste folgen (Ereignis Level), ihre Figuren stehen am Anker
    void attach(game::GameSession& s, const std::string& anchor, int difficultyIndex);
    // je Bild: vorher Netz lesen und Eingaben anwenden, nachher senden und die Gruppe fuehren
    void beforeUpdate(double dt);
    void afterUpdate(double dt);

    std::vector<PlayerInfo> players() const;
    bool kick(int id, bool ban);
    bool takeLevelChange(std::string& level, std::string& anchor);
    bool takeEscape();
    double supplyScaleNext() const;  // Vorraete der naechsten Ebene (vor dem Bauen der Sitzung setzen)
    std::string status() const;  // Ausgang, Bereitschaft, Wiedereinstieg
    bool selfDead() const;
    double selfRespawnIn() const;
    int spectate() const { return spectate_; }  // Spieler, dem der tote Host zusieht (-1: keiner)
    void cycleSpectate();
    game::Pawn* pawnOf(int id);
    // Koop-Stand (nur der Host speichert): Werte, Inventar und Lampe der Gaeste nach Namen; wer mit demselben Namen
    // wieder beitritt, bekommt sie zurueck
    Json saveGuests();
    void loadGuests(const Json& j);

    struct Counters {
        u64 rejected = 0, dropped = 0, inputs = 0, inputsRefused = 0, actions = 0, actionsRefused = 0, snapshots = 0, kicked = 0;
    } counters;

private:
    struct Guest;
    void onConnected(int peer, const char* name);
    void onDisconnected(int peer, int reason);
    void onMessage(int peer, int channel, const u8* data, int len);
    void handleJoin(Guest& g, const net::coop::Join& j);
    void handleInput(Guest& g, const net::coop::Input& in);
    void handleAction(Guest& g, const net::coop::Action& a);
    void send(Guest& g, const net::coop::Msg& m, bool reliable);
    void sendAll(const net::coop::Msg& m, bool reliable, int except = -1);
    void sendEvent(Guest& g, net::coop::EventKind k, i32 a, i32 b, vec3 pos, const std::string& text);
    void eventAll(net::coop::EventKind k, i32 a, i32 b, vec3 pos, const std::string& text, int except = -1);
    void sendWelcome(Guest& g, bool reconnected);
    void sendRoster();
    void sendSnapshot(Guest& g);
    void sendInventory(Guest& g, bool force);
    void flushWorld();
    void runGroup(double dt);
    void runDeath(double dt);
    void placeAtAnchor(game::Pawn& p, int k);
    void toastSelf(const std::string& text, float dur = 3.0f);
    Guest* guestByPeer(int peer);
    Guest* guestById(int id);
    void rebuildPawnList();
    double now() const { return clock_; }

    NetApi api_;
    LnContext* ctx_ = nullptr;
    std::string hostName_;
    u32 build_;
    NetSettings net_;
    std::string code_, codeLong_, failure_;
    bool lobbyLost_ = false, everRoom_ = false;
    int maxPlayers_ = 4;
    game::GameSession* s_ = nullptr;
    std::string anchor_;
    int difficulty_ = 0;
    std::vector<std::unique_ptr<Guest>> guests_;
    double clock_ = 0.0;
    u32 tick_ = 0;
    // Gruppe: Ausgang
    struct Group {
        bool active = false;
        std::string level, anchor, needs;
        double t = 0.0, emptyT = 0.0, announceT = 0.0, needsT = 0.0;
    } group_;
    std::string pendingLevel_, pendingAnchor_;
    bool pendingChange_ = false, pendingEscape_ = false;
    bool selfReady_ = false;
    // Tod und Wiedereinstieg
    double selfDeadT_ = -1.0, allDeadT_ = -1.0;
    int spectate_ = -1;
    bool rosterDirty_ = true;
    double rosterT_ = 0.0;
    std::map<std::string, Json> memory_;
    void remember(const Guest& g);
};

class CoopClient {
public:
    CoopClient(NetApi api, u32 buildHash);
    ~CoopClient();
    // beitreten: ueber die Lobby (Code) oder direkt (Adresse + langer Code)
    bool connect(const NetSettings& net, const std::string& directAddress, const std::string& code, const std::string& name, std::string& error);
    void stop();
    enum class State { Connecting, Joining, Welcomed, Playing, Closed };
    State state() const { return state_; }
    const std::string& closeReason() const { return reason_; }
    // vor der Sitzung: Willkommen (Ebene, Seeds) - die App baut daraus die Sitzung und haengt sie an
    bool takeWelcome(net::coop::Welcome& w);
    void pump();  // Netz lesen ohne Sitzung (Verbindungsaufbau, Laden)
    void attach(game::GameSession& s);
    bool takeLevelChange(std::string& level, std::string& anchor);
    bool takeEscape();
    double pendingSupply() const { return pendingSupply_; }
    // je Bild: vorher Netz lesen (Momentaufnahmen, Ereignisse), Mitspieler bewegen; nachher Eingabe senden
    void beforeUpdate(double dt);
    void afterUpdate(double dt, const game::PlayerControl& c);
    int selfId() const { return selfId_; }
    std::vector<PlayerInfo> players() const;
    std::string status() const { return status_; }
    bool dead() const { return dead_; }
    int respawnIn() const { return respawnIn_; }
    int spectate() const { return spectate_; }
    game::Pawn* pawnOf(int id);
    void cycleSpectate();
    int rttMs() const;
    LnStats netStats() const;
    std::string progress() const { return progress_; }  // Fortschritt beim Beitritt
    struct Counters {
        u64 snapshots = 0, rejected = 0, corrections = 0, snaps = 0, desync = 0, checks = 0, inputsSent = 0;
        double maxError = 0.0, sumError = 0.0;
        u64 errSamples = 0, over10cm = 0;
    } counters;

private:
    void onMessage(int channel, const u8* data, int len);
    void applySnapshot(const net::coop::Snapshot& s);
    void applyEvent(const net::coop::Event& e);
    void applyInventory(const net::coop::Inventory& v);
    void sendMsg(const net::coop::Msg& m, bool reliable);
    void action(int kind, u32 arg);

    NetApi api_;
    LnContext* ctx_ = nullptr;
    u32 build_;
    std::string name_;
    State state_ = State::Closed;
    std::string reason_, progress_;
    net::coop::Welcome welcome_;
    bool welcomeNew_ = false;
    game::GameSession* s_ = nullptr;
    int selfId_ = 0;
    std::array<u8, 16> token_{};
    double clock_ = 0.0;
    // Vorhersage: gesendete, noch nicht bestaetigte Eingaben
    struct Sent {
        u32 seq;
        double dt;
        game::PlayerControl c;
        float yaw, pitch;
    };
    std::deque<Sent> history_;
    u32 seq_ = 0;
    double accDt_ = 0.0;
    game::PlayerControl accC_;
    bool accJump_ = false;
    u32 actionSeq_ = 0;
    // Mitspieler (gezeichnet wie beim Host; Ort aus den Momentaufnahmen)
    std::vector<std::unique_ptr<game::Pawn>> others_;
    std::vector<PlayerInfo> roster_;
    std::string status_;
    bool dead_ = false;
    int respawnIn_ = 0, spectate_ = -1;
    double lampToggleT_ = -1.0;
    bool pendingChange_ = false, pendingEscape_ = false;
    std::string pendingLevel_, pendingAnchor_;
    double pendingSupply_ = 1.0;
    double pingT_ = 0.0;
    u32 lastSnapTick_ = 0;
    bool anySnap_ = false;
};

}  // namespace lim::coop
