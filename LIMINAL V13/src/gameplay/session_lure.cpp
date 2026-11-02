// V13.3: Leuchtstab und Ansprechen in der Sitzung (KREATUR_Smiler.md, KREATUR_SkinStealer.md).
//
// Leuchtstab: F auf einem Leuchtstab - die Hand knickt und schuettelt ihn (er leuchtet auf), holt aus und wirft (Loslassen
// nach 0,62 s). Einzelspieler/Host werfen selbst; der Gast spielt nur die Geste und schickt beim Loslassen die Eingabe
// "Platz benutzen" - der Host wirft aus der Hand der Figur des Gasts und meldet den Stab allen (Dropped "glowstick_lit");
// beim Gast fliegt er mit demselben Wurf aus der Hand des Werfers. Wen das Licht fortlockt, rechnet allein der Host.
// Ansprechen: E ohne Gegenstand im Blick, wenn man eine menschenaehnliche Gestalt in wenigen Metern ansieht. Der Spieler
// ruft einen kurzen Satz (synthetisch); antworten kann nur ein Skin-Stealer (Praesenz, Host). Der Gast schickt die
// Eingabe "Interact" mit der Nummer des Satzes (0..3) statt einer Kennzahl.
#include <algorithm>
#include <format>

#include "core/log.hpp"
#include "core/rng.hpp"
#include "gameplay/navfield.hpp"
#include "gameplay/session.hpp"

namespace lim::game {

namespace {
constexpr double kReleaseT = 0.62;  // Body::Gesture::Throw laesst hier los
const char* const kPhrases[4] = {"„Hallo?“", "„Ist da wer?“", "„Hey … hallo?“", "„Wer bist du?“"};

// Abwurf ueber die Schulter: aus der Hand rechts vor dem Kopf, in Blickrichtung, etwas nach oben, mit dem Schwung des Laufens
void launchParams(const Player& p, vec3 eye, vec3& start, vec3& vel) {
    const vec3 fwd = forwardFromAngles((float)p.angle, (float)std::clamp(p.pitch, -1.0, 1.0));
    const vec3 flat = normalize(vec3{fwd.x, fwd.y, 0.0f});
    const vec3 right{flat.y, -flat.x, 0.0f};
    start = eye + flat * 0.34f + right * 0.15f + vec3{0, 0, 0.02f};
    vel = fwd * 8.0f + vec3{0, 0, 1.7f} + vec3{(float)p.vx, (float)p.vy, 0.0f} * 0.6f;
}
}  // namespace

bool GameSession::startGlowThrow(int idx) {
    if (idx < 0 || idx >= Inventory::kSize || !inventory_.slots[(size_t)idx] || inventory_.slots[(size_t)idx]->kind != "glowstick") return false;
    if (glowPending_ > 0.0 || stats_.dead || ride_.active) return true;  // wirft gerade (oder kann nicht): nichts tun
    if (netRole != NetRole::Guest) inventory_.take(idx);  // Gast: das Inventar fuehrt der Host
    glowSlot_ = idx;
    glowPending_ = kReleaseT;
    body_.gesture(Body::Gesture::Throw, {}, "glowstick_lit", 0.0f, 1.0f);
    GameEvent ev{GameEvent::SoundAt};
    ev.sound = "glow_crack", ev.pos = body_.eye() - vec3{0, 0, 0.2f}, ev.intensity = 0.8f;
    events_.push_back(ev);
    if (netRole == NetRole::Host) netSoundOut.push_back({ev.sound, ev.pos, 0.8f, netSelfId});
    return true;
}

void GameSession::launchGlowstick(const Player& p, vec3 eye, int by) {
    vec3 start, vel;
    launchParams(p, eye, start, vel);
    const world::Tile* t = world_->tileIfLoaded(ifloor(start.x), ifloor(start.y));
    if (!t || !t->open() || start.z < t->floor + 0.1f || start.z > t->ceil - 0.05f || !clearSight(*world_, eye, start)) start = eye;
    const unsigned seed = hash32((u32)(stats_.playTime * 1000.0) ^ ((u32)by * 0x9E3779B9u));
    const int id = lures_.launch(start, vel, (float)p.angle, by, seed);
    GameEvent ev{GameEvent::SoundAt};
    ev.sound = "glow_throw", ev.pos = start, ev.intensity = 0.55f;
    events_.push_back(ev);
    if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Dropped, worldItemHash(std::format("lure:{}", id)), "glowstick_lit", start, by});
    log::info("Leuchtstab geworfen (Spieler {}) bei ({:.1f}, {:.1f}, {:.1f})", by, start.x, start.y, start.z);
}

// Gast: der Host meldet einen geworfenen Stab (Ort = Hand). Der Werfer ist, wessen Hand am naechsten ist - derselbe Wurf
void GameSession::netAddLure(vec3 pos) {
    const Player* thrower = &player_;
    vec3 eye = body_.eye();
    float best = length(pos - eye);
    for (Pawn* q : pawns)
        if (q && length(pos - q->eye()) < best) best = length(pos - q->eye()), thrower = &q->player, eye = q->eye();
    vec3 start, vel;
    launchParams(*thrower, eye, start, vel);
    if (best > 1.5f) vel = {};  // niemand in der Naehe: er faellt nur
    lures_.launch(pos, vel, (float)thrower->angle, -1, hash32((u32)(pos.x * 1000.0f) ^ (u32)(pos.y * 977.0f)));
    if (thrower != &player_) {
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "glow_throw", ev.pos = pos, ev.intensity = 0.55f;
        events_.push_back(ev);
    }
}

void GameSession::updateLures(double dt) {
    speakCool_ = std::max(0.0, speakCool_ - dt);
    if (glowPending_ > 0.0 && (glowPending_ -= dt) <= 0.0) {
        glowPending_ = -1.0;
        if (netRole == NetRole::Guest) {  // nur die Eingabe: der Host wirft
            if (netAction && glowSlot_ >= 0) netAction(2, (u32)glowSlot_);
        } else if (!stats_.dead) {
            launchGlowstick(player_, body_.eye(), netSelfId);
        }
        glowSlot_ = -1;
    }
    std::vector<LureSystem::Impact> hits;
    lures_.update(dt, *world_, hits);
    for (const LureSystem::Impact& h : hits) {
        GameEvent ev{GameEvent::SoundAt};
        const u32 v = hash32((u32)(h.pos.x * 4096.0f) ^ (u32)(h.pos.y * 2048.0f));
        ev.sound = h.kind == 3 ? "glow_splash" : std::format("glow_land_{}", v % 3);
        ev.pos = h.pos, ev.intensity = 0.25f + 0.65f * h.strength;
        events_.push_back(ev);
        // leiser Kunststoff: hoeren koennen ihn nur Wesen, die auf Geraeusche in der Naehe achten (nicht der Smiler)
        if (netRole != NetRole::Guest) noisePings_.push_back({h.pos, 0.35f * std::clamp(h.strength * 1.6f, 0.4f, 1.0f)});
    }
    presenceLures_.clear();
    for (const LureSystem::Lure& l : lures_.lures())
        if (const float g = LureSystem::glow(l.age); g > 0.03f) presenceLures_.push_back({l.pos, 1.6f * g, l.id});
    // Hinweis beim ersten Leuchtstab im Inventar
    if (!hinted_.count("glowstick") && inventory_.count("glowstick") > 0) {
        hinted_.insert("glowstick");
        events_.push_back({GameEvent::Toast, "Leuchtstab: auswählen und F – knicken und werfen. Smiler gehen zum stärkeren Licht.", 5.0f});
    }
}

void GameSession::glowLights(std::vector<GlowLight>& out, float radius) const {
    const vec3 eye = body_.eye();
    // in der Hand: schwaecher (ganz nah an Auge und Arm - sonst ueberstrahlt er alles)
    if (glowPending_ > 0.0 && body_.holdsItem()) out.push_back({body_.heldPos(), 0.4f * LureSystem::glow(kReleaseT - glowPending_ + 0.08)});
    for (const LureSystem::Lure& l : lures_.lures()) {
        const float g = LureSystem::glow(l.age);
        if (g < 0.01f || length(l.pos - eye) > radius) continue;
        out.push_back({l.pos + vec3{0, 0, 0.05f}, g});
    }
    std::sort(out.begin(), out.end(), [&](const GlowLight& a, const GlowLight& b) { return length(a.pos - eye) < length(b.pos - eye); });
}

// --- Ansprechen ------------------------------------------------------------------------------------------------------
bool GameSession::speakTarget(vec3* head) const {
    if (stats_.dead || ride_.active) return false;
    const vec3 look = forwardFromAngles((float)player_.angle, (float)player_.pitch);
    return presence_.speakable(*world_, body_.eye(), look, 6.0f, head);
}

void GameSession::speak() {
    if (speakCool_ > 0.0 || stats_.dead) return;
    speakCool_ = 2.4;
    if (presence_.houndMute(netSelfId) > 0.0f) {  // V13.3 Level 3: die Kehle zerbissen - nur ein nasses Roecheln, kein Ruf
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "hp_rasp", ev.pos = body_.eye(), ev.intensity = 0.7f;
        events_.push_back(ev);
        return;
    }
    const int phrase = (int)(hash01((u32)(stats_.playTime * 977.0) ^ 0x5A17u) * 4.0f) & 3;
    const vec3 eye = body_.eye(), look = forwardFromAngles((float)player_.angle, (float)player_.pitch);
    GameEvent ev{GameEvent::SoundAt};
    ev.sound = std::format("talk_call_{}", phrase), ev.pos = eye + look * 0.25f, ev.intensity = 0.75f;
    events_.push_back(ev);
    events_.push_back({GameEvent::Toast, kPhrases[phrase], 1.8f});  // Untertitel
    if (netRole == NetRole::Guest) {
        if (netAction) netAction(1, (u32)phrase);  // Interact mit 0..3: Ansprechen (der Host entscheidet)
        return;
    }
    if (netRole == NetRole::Host) netSoundOut.push_back({ev.sound, ev.pos, 0.75f, netSelfId});
    presence_.address(*world_, eye, look, netSelfId, phrase);
}

bool GameSession::pawnSpeak(Pawn& p, int phrase) {
    if (!p.alive) return false;
    const vec3 eye = p.eye(), look = forwardFromAngles((float)p.player.angle, (float)p.player.pitch);
    phrase &= 3;
    if (presence_.houndMute(p.id) > 0.0f) {  // V13.3 Level 3: Kehle zerbissen - kein Ruf, die anderen hoeren nur ein Roecheln
        netSoundOut.push_back({"hp_rasp", eye, 0.5f, p.id});
        return false;
    }
    GameEvent ev{GameEvent::SoundAt};
    ev.sound = std::format("talk_call_{}", phrase), ev.pos = eye + look * 0.25f, ev.intensity = 0.75f;
    events_.push_back(ev);
    netSoundOut.push_back({ev.sound, ev.pos, 0.75f, p.id});
    presence_.address(*world_, eye, look, p.id, phrase);
    return true;
}

}  // namespace lim::game
