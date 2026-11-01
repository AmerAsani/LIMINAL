// V13.3: Erscheinungen von Level 1 ohne eigene Figur - siehe phenomena.hpp, KREATUR_Crawler.md, KREATUR_GameMaster.md,
// KREATUR_L1_Erscheinungen.md. Alle Texte in eigenen Worten (Wikidot level-1, entity-17, entity-99; CC BY-SA 3.0).
#include "gameplay/phenomena.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>

#include "core/log.hpp"
#include "gameplay/navfield.hpp"
#include "world/level_def.hpp"
#include "world/map_def.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
// Groesse der liegenden Bilder (halbe Breite quer, halbe Hoehe laengs, mit Rahmen) - wie tools/blender/models/
// models_l1_phenomena.py (painting_floor@0..3, painting_jester@0..2)
constexpr float kFloorHalf[4][2] = {{0.505f, 0.355f}, {0.355f, 0.43f}, {0.455f, 0.355f}, {0.3f, 0.4f}};
constexpr float kJesterHalf[2] = {0.37f, 0.47f};

const char* const kStageText[5] = {
    "",
    "Es juckt. Am Handgelenk, im Nacken – überall, wo das Wasser hinkam.",
    "Weiße, flaumige Flecken auf der Haut. Der Mund läuft voll Speichel, der Hunger ist kaum auszuhalten.",
    "Der Flaum überzieht die ganze Hand. Fieber – das Blut fühlt sich kochend an.",
    "Etwas wächst hinter den Augen. Gedanken, die nicht deine sind.",
};
const char* const kGardenText[4] = {
    "",
    "Erde unter den Fingernägeln, die du nie berührt hast. Die Luft schmeckt grün.",
    "Feine Ranken winden sich um dein Handgelenk. Jeder Schritt wird schwerer.",
    "Deine Haut wird zu Rinde. Raus aus dem Grün – sofort.",
};
// Die Spielleiterin (eigene Worte; das Wiki: Netzjargon, laedt zu Spielen ein, schummelt selbst, verliert schlecht)
const char* const kJesterLines[3] = {
    "„EIN NEUER SPIELER! Endlich. Hier war es sooo langweilig.“",
    "„Wir spielen. Du darfst dir das Spiel aussuchen … ach nein, doch ich.“",
    "„Geschummelt! Ich hab’s genau gesehen. RAUS mit dir!“",
};
constexpr double kJesterAt[3] = {0.4, 2.2, 4.1};
constexpr int kJesterExpr[3] = {0, 2, 1};  // Grinsen, lachend offen, verzogen
const char* const kNotes[2] = {
    "Zettel neben dem verpilzten Schlafsack: „Hab aus der offenen Flasche getrunken. Erst nur Jucken. Jetzt weiße Flecken "
    "an den Armen und so ein Hunger. Komm nicht näher. Trink nichts Offenes.“",
    "Gekritzel an der Wand, die Schrift wird immer zittriger: „Er war gestern noch normal. Heute kratzt er an der Tür. "
    "Weißes Zeug im Gesicht. Wir lassen ihn nicht rein.“",
};

float radiusOf(const world::MapSpawn& s, float def) {
    for (const std::string& t : s.tags)
        if (t.size() > 1 && t[0] == 'r' && (std::isdigit((unsigned char)t[1]) || t[1] == '.')) return (float)std::atof(t.c_str() + 1);
    return def;
}
}  // namespace

int Phenomena::stageOf(const Config& c, double t) {
    if (t < 0.0) return 0;
    double acc = 0.0;
    for (int k = 0; k < 4; ++k) {
        acc += c.stage[k];
        if (t < acc) return k + 1;
    }
    return 5;
}

u32 Phenomena::paintingHash(int idx) {
    const std::string id = std::format("painting:{}", idx);
    u32 h = 2166136261u;  // wie worldItemHash (gameplay/pawn.hpp)
    for (char c : id) h = (h ^ (u8)c) * 16777619u;
    return h ? h : 1u;
}

void Phenomena::setup(const world::LevelDef& level, u64 seed) {
    cfg_ = Config{};
    fungus_.clear(), paintSpots_.clear(), bottles_.clear(), notes_.clear(), expel_.clear(), claws_.clear(), paintings_.clear();
    moved_.clear();
    seed_ = seed;
    rng_ = Rng(hash64((i64)seed, "phenomena", level.id.c_str()));
    const Json& j = level.phenomena;
    if (!j.isObject() || !level.mapped()) return;
    cfg_.fungus = j["fungus"].asBool(false);
    cfg_.garden = j["garden"].asBool(false);
    cfg_.paintings = j["paintings"].asBool(false);
    cfg_.claws = j["claws"].asBool(false);
    cfg_.jester = cfg_.paintings && j["jester"].asBool(false);
    cfg_.touchInfect = j["touch_infect"].asNumber(cfg_.touchInfect);
    cfg_.gardenSigns = j["garden_signs"].asNumber(cfg_.gardenSigns);
    cfg_.gardenVines = j["garden_vines"].asNumber(cfg_.gardenVines);
    cfg_.gardenBegin = j["garden_begin"].asNumber(cfg_.gardenBegin);
    cfg_.gardenDeath = j["garden_death"].asNumber(cfg_.gardenDeath);
    cfg_.paintingCount = (int)j["painting_count"].asInt(cfg_.paintingCount);
    cfg_.clawEvery = j["claw_every"].asNumber(cfg_.clawEvery);
    const auto& sp = level.map->spawns;
    for (size_t i = 0; i < sp.size(); ++i) {
        const world::MapSpawn& s = sp[i];
        if (s.kind != "trace") continue;
        Spot p{{(float)s.x, (float)s.y, (float)s.z}, (float)s.yaw, 1.0f, std::format("t{}", i)};
        if (s.set == "fungus") p.radius = radiusOf(s, 1.2f), fungus_.push_back(p);
        else if (s.set == "painting") paintSpots_.push_back(p);
        else if (s.set == "bottle") bottles_.push_back(p);
        else if (s.set == "note") p.radius = radiusOf(s, 1.8f), notes_.push_back(p);
        else if (s.set == "expel") expel_.push_back(p);
        else if (s.set == "claws") p.radius = radiusOf(s, 9.0f), claws_.push_back({p});
    }
    noteSeen_.assign(notes_.size(), false);
    if (!cfg_.claws) claws_.clear();
    if (!cfg_.fungus) fungus_.clear(), bottles_.clear(), notes_.clear(), noteSeen_.clear();
    // Gemaelde: zufaellige Tueren (je Lauf); das erste ist die Spielleiterin
    if (cfg_.paintings && !paintSpots_.empty()) {
        std::vector<int> order((size_t)paintSpots_.size());
        for (size_t k = 0; k < order.size(); ++k) order[k] = (int)k;
        for (size_t k = order.size(); k > 1; --k) std::swap(order[k - 1], order[(size_t)rng_.randint(0, (i64)k - 1)]);
        const int n = std::min<int>(cfg_.paintingCount, (int)order.size());
        for (int k = 0; k < n; ++k) {
            Painting p;
            p.spot = order[(size_t)k];
            p.jester = cfg_.jester && k == 0;
            p.variant = p.jester ? 0 : (int)rng_.randint(0, 3);
            p.expr = 0;
            p.squeakT = rng_.uniform(4.0, 12.0);
            const Spot& s = paintSpots_[(size_t)p.spot];
            p.pos = s.pos, p.yaw = s.yaw + (float)rng_.uniform(-0.35, 0.35);
            p.halfW = p.jester ? kJesterHalf[0] : kFloorHalf[p.variant][0];
            p.halfH = p.jester ? kJesterHalf[1] : kFloorHalf[p.variant][1];
            modelOf(p);
            paintings_.push_back(std::move(p));
        }
    }
    log::info("Erscheinungen ({}): Pilz {} Stellen, {} Flaschen, {} Lager; Garden {}; Gemaelde {} ({} Plaetze, Spielleiterin {}); "
              "Tuerspuren {}", level.id, fungus_.size(), bottles_.size(), notes_.size(), cfg_.garden ? "ja" : "nein",
              paintings_.size(), paintSpots_.size(), cfg_.jester ? "ja" : "nein", claws_.size());
}

void Phenomena::modelOf(Painting& p) const {
    p.model = p.jester ? std::format("prop:painting_jester@{}", p.expr) : std::format("prop:painting_floor@{}", p.variant);
    p.meshId = -2;
}

bool Phenomena::inFungus(vec3 feet) const {
    for (const Spot& s : fungus_) {
        const float dx = feet.x - s.pos.x, dy = feet.y - s.pos.y;
        if (dx * dx + dy * dy < s.radius * s.radius && std::fabs(feet.z - s.pos.z) < 1.2f) return true;
    }
    return false;
}

bool Phenomena::onPainting(const Painting& p, vec3 feet, float grow) const {
    if (!p.shown || std::fabs(feet.z - p.pos.z) > 0.5f) return false;
    const float c = std::cos(p.yaw), s = std::sin(p.yaw);
    const float dx = feet.x - p.pos.x, dy = feet.y - p.pos.y;
    const float lx = dx * c + dy * s, ly = -dx * s + dy * c;  // laengs (Bildhoehe), quer (Bildbreite)
    return std::fabs(lx) < p.halfH + grow && std::fabs(ly) < p.halfW + grow;
}

void Phenomena::infect(int id) {
    Ailment& a = ail_[(size_t)id];
    if (a.infectT >= 0.0) return;
    a.infectT = 0.0, a.stage = 1, a.hold = 0.0, a.itchT = 6.0, a.twitchT = 20.0;
}

// --- ein Spieler ------------------------------------------------------------------------------------------------------
void Phenomena::updatePlayer(int id, const PlayerIn& in, double dt, bool authoritative, std::vector<Event>& out) {
    Ailment& a = ail_[(size_t)id];
    auto toast = [&](const std::string& t, float dur = 4.0f) { out.push_back({Event::Toast, id, t, {}, dur}); };
    if (!in.alive || a.dead) {
        a.inGarden = false, a.jesterT = -1.0;
        return;
    }
    // Pilz: Kontakt mit Befall (Waten in einer befallenen Pfuetze zaehlt voll, Streifen an der Wand halb)
    a.touchWarn = std::max(0.0, a.touchWarn - dt);
    if (cfg_.fungus && inFungus(in.feet)) {
        if (a.touchWarn <= 0.0 && a.infectT < 0.0) {
            toast("Weißer Flaum auf Wand und Wasser. Hier nicht stehen bleiben.", 3.5f);
            a.touchWarn = 40.0;
        }
        a.touch += dt * (in.onPuddle ? 1.0 : 0.5);
        if (a.infectT < 0.0 && a.touch >= cfg_.touchInfect) {
            infect(id);
            out.push_back({Event::Infected, id, "", in.feet});
            toast("Die Haut kribbelt, wo das Wasser sie berührt hat.", 3.5f);
            log::info("Erscheinungen: Spieler {} angesteckt (Kontakt)", id);
        }
    } else {
        a.touch = std::max(0.0, a.touch - dt * 0.5);
    }
    // Stadien (Mandelwasser haelt sie eine Weile an)
    if (a.infectT >= 0.0) {
        if (a.hold > 0.0) a.hold = std::max(0.0, a.hold - dt);
        else a.infectT += dt;
        const int st = stageOf(cfg_, a.infectT);
        if (st != a.stage) {
            a.stage = st;
            if (st <= 4) toast(kStageText[st], 5.0f);
            if (st == 3) out.push_back({Event::Sound2D, id, "fever_pulse", {}, 0.7f});
        }
        if (st >= 5 && authoritative) {
            a.dead = true, a.cause = "pilz";
            out.push_back({Event::Died, id, "pilz", in.feet});
            log::info("Erscheinungen: Spieler {} - der Pilz hat den Koerper", id);
            return;
        }
        // Jucken: kratzen (Klang), spaeter zuckt der Kopf (Fieber, fremde Gedanken)
        if ((a.itchT -= dt) <= 0.0 && st <= 3) {
            out.push_back({Event::Sound2D, id, "itch_scratch", {}, 0.45f + 0.1f * (float)st});
            a.itchT = rng_.uniform(9.0, 20.0) / (double)st;
        }
        if ((a.twitchT -= dt) <= 0.0 && st >= 2) {
            const float sgn = rng_.random() < 0.5 ? -1.0f : 1.0f;
            out.push_back({Event::Twitch, id, "", {}, sgn * (st >= 4 ? 0.09f : 0.04f), st >= 3 ? 0.05f : 0.02f});
            if (st >= 4) out.push_back({Event::Sound2D, id, "hal_whisper", {}, 0.35f});
            a.twitchT = rng_.uniform(st >= 4 ? 7.0 : 14.0, st >= 4 ? 16.0 : 30.0);
        }
    }
    // Garden-Uhr
    const bool wasIn = a.inGarden;
    a.inGarden = cfg_.garden && in.inGarden;
    if (a.inGarden) a.garden += dt;
    else a.garden = std::max(0.0, a.garden - dt * cfg_.gardenRecover);
    const int g = a.garden >= cfg_.gardenBegin ? 3 : a.garden >= cfg_.gardenVines ? 2 : a.garden >= cfg_.gardenSigns ? 1 : 0;
    if (g > a.gstage) toast(kGardenText[g], 5.0f);
    if (g == 0 && a.gstage > 0) toast("Die Ranken an deinem Arm sind welk abgefallen.", 3.5f);
    a.gstage = g;
    if (wasIn && !a.inGarden && a.garden >= cfg_.gardenSigns) toast("Weg vom Grün … langsam lässt das Ziehen nach.", 3.5f);
    if (a.inGarden && a.gstage >= 1 && (a.gardenSoundT -= dt) <= 0.0) {
        out.push_back({Event::Sound2D, id, a.gstage >= 3 ? "garden_creak" : "garden_rustle", {}, 0.35f + 0.15f * (float)a.gstage});
        a.gardenSoundT = rng_.uniform(10.0, 22.0) / (double)a.gstage;
    }
    if (cfg_.garden && a.garden >= cfg_.gardenDeath && authoritative) {
        a.dead = true, a.cause = "garten";
        out.push_back({Event::Died, id, "garten", in.feet});
        log::info("Erscheinungen: Spieler {} - im Garden zur Pflanze geworden", id);
        return;
    }
    if (cfg_.jester) jesterTick(id, a, in, dt, authoritative, out);
}

void Phenomena::jesterTick(int id, Ailment& a, const PlayerIn& in, double dt, bool authoritative, std::vector<Event>& out) {
    a.jesterCool = std::max(0.0, a.jesterCool - dt);
    if (a.jesterT >= 0.0) {
        a.jesterT += dt;
        while (a.jesterLine < 3 && a.jesterT >= kJesterAt[a.jesterLine]) {
            out.push_back({Event::Toast, id, kJesterLines[a.jesterLine], {}, 2.1f});
            out.push_back({Event::Sound2D, id, a.jesterLine == 2 ? "jester_buzz" : "jester_giggle", {}, 0.7f});
            ++a.jesterLine;
        }
        if (a.jesterT >= kJesterLen && (authoritative || a.jesterT >= kJesterLen + 1.5)) {
            a.jesterT = -1.0, a.jesterLine = 0, a.jesterCool = cfg_.jesterCooldown;
            if (!authoritative) return;  // Gast: den Ort setzt der Host
            // irgendwo anders in Level 1 - moeglichst weit weg
            vec3 to = in.feet;
            float yaw = 0.0f;
            if (!expel_.empty()) {
                std::vector<size_t> far;
                for (size_t k = 0; k < expel_.size(); ++k)
                    if (length(expel_[k].pos - in.feet) > 30.0f) far.push_back(k);
                const size_t k = far.empty() ? (size_t)rng_.randint(0, (i64)expel_.size() - 1) : far[(size_t)rng_.randint(0, (i64)far.size() - 1)];
                to = expel_[k].pos, yaw = expel_[k].yaw;
            }
            out.push_back({Event::JesterExpel, id, "", to, yaw});
            for (Painting& p : paintings_)  // sie ist danach woanders
                if (p.jester && relocate(p, {}, nullptr)) moved_.push_back((int)(&p - paintings_.data()));
            log::info("Erscheinungen: Spieler {} aus dem Spiel geworfen -> ({:.1f}, {:.1f})", id, to.x, to.y);
        }
        return;
    }
    if (a.jesterCool > 0.0) return;
    for (const Painting& p : paintings_)
        if (p.jester && onPainting(p, in.feet, -0.08f)) {
            a.jesterT = 0.0, a.jesterLine = 0;
            out.push_back({Event::JesterStart, id, "", p.pos});
            log::info("Erscheinungen: Spieler {} tritt durch das Bild der Spielleiterin", id);
            return;
        }
}

// --- Welt -------------------------------------------------------------------------------------------------------------
bool Phenomena::seenBy(const Painting& p, const std::vector<Viewer>& viewers, world::World& w) const {
    const float c = std::cos(p.yaw), s = std::sin(p.yaw);
    const vec3 ax{c * p.halfH, s * p.halfH, 0.0f}, ay{-s * p.halfW, c * p.halfW, 0.0f}, up{0, 0, 0.04f};
    const vec3 pts[5] = {p.pos + up, p.pos + ax + ay + up, p.pos + ax - ay + up, p.pos - ax + ay + up, p.pos - ax - ay + up};
    for (const Viewer& v : viewers) {
        if (length(p.pos - v.eye) < 3.5f) return true;  // so nah merkt man es auch am Rand des Blickfelds
        for (const vec3& q : pts) {
            const vec3 d = q - v.eye;
            const float len = length(d);
            if (len < 0.01f || len > 60.0f) continue;
            if (dot(d / len, v.look) < std::cos(1.15f)) continue;  // Bildschirm samt Rand (unten ist der Boden weit im Bild)
            if (clearSight(w, v.eye, q)) return true;
        }
    }
    return false;
}

bool Phenomena::relocate(Painting& p, const std::vector<Viewer>& viewers, world::World* w) {
    std::vector<int> free;
    for (size_t k = 0; k < paintSpots_.size(); ++k) {
        if ((int)k == p.spot) continue;
        bool used = false;
        for (const Painting& o : paintings_) used = used || (&o != &p && o.spot == (int)k);
        if (used) continue;
        Painting t = p;
        t.pos = paintSpots_[k].pos;
        bool bad = false;
        for (const Viewer& v : viewers) bad = bad || length(t.pos - v.eye) < 8.0f;
        if (!bad && w && seenBy(t, viewers, *w)) bad = true;
        if (!bad) free.push_back((int)k);
    }
    if (free.empty()) return false;
    p.spot = free[(size_t)rng_.randint(0, (i64)free.size() - 1)];
    const Spot& s = paintSpots_[(size_t)p.spot];
    p.pos = s.pos, p.yaw = s.yaw + (float)rng_.uniform(-0.35, 0.35);
    p.unseen = 0.0;
    if (!p.jester) {
        p.variant = (int)rng_.randint(0, 3);
        p.halfW = kFloorHalf[p.variant][0], p.halfH = kFloorHalf[p.variant][1];
        modelOf(p);
    }
    return true;
}

bool Phenomena::netMove(u32 hash, vec3 pos) {
    for (size_t i = 0; i < paintings_.size(); ++i) {
        if (paintingHash((int)i) != hash) continue;
        Painting& p = paintings_[i];
        int best = -1;
        float bd = 1e9f;
        for (size_t k = 0; k < paintSpots_.size(); ++k)
            if (const float d = length(paintSpots_[k].pos - pos); d < bd) bd = d, best = (int)k;
        if (best < 0) return false;
        // Lage und Bild aus dem Ereignis: z traegt beim Gast die Variante und den Winkel (Hundertstel)
        p.spot = best;
        p.pos = paintSpots_[(size_t)best].pos;
        const float code = pos.z - p.pos.z;  // Variante * 10 + (Winkel + 0,5) im Bereich 0..1
        if (!p.jester) {
            const int v = std::clamp((int)std::floor(code / 10.0f + 0.001f), 0, 3);
            p.variant = v, p.halfW = kFloorHalf[v][0], p.halfH = kFloorHalf[v][1];
            modelOf(p);
        }
        const float frac = code - std::floor(code / 10.0f + 0.001f) * 10.0f;
        p.yaw = paintSpots_[(size_t)best].yaw + std::clamp(frac - 0.5f, -0.5f, 0.5f);
        p.unseen = 0.0;
        return true;
    }
    return false;
}

void Phenomena::updateWorld(double dt, const std::vector<Viewer>& viewers, const PlayerIn& self, bool authoritative,
                            world::World& w, std::vector<Event>& out) {
    // Gemaelde: verschwinden, wenn niemand hinsieht, und liegen dann vor einer anderen Tuer
    for (size_t i = 0; i < paintings_.size(); ++i) {
        Painting& p = paintings_[i];
        const bool seen = seenBy(p, viewers, w);
        p.unseen = seen ? 0.0 : p.unseen + dt;
        if (authoritative && p.unseen > cfg_.vanishAfter && rng_.random() < cfg_.vanishRate * dt && relocate(p, viewers, &w))
            moved_.push_back((int)i);
        if (!p.jester) continue;
        // ihr Gesicht wechselt, wenn keiner hinsieht; leises Quietschen der roten Kugeln in der Naehe
        if (!seen && (p.exprT -= dt) <= 0.0) {
            p.expr = (p.expr + 1 + (int)rng_.randint(0, 1)) % 3;
            p.exprT = rng_.uniform(3.0, 8.0);
            modelOf(p);
        }
        const float d = length(p.pos - self.feet);
        if (d < 10.0f && (p.squeakT -= dt) <= 0.0) {
            out.push_back({Event::SoundAt, 0, "jester_squeak", p.pos + vec3{0, 0, 0.05f}, 0.32f});
            p.squeakT = rng_.uniform(6.0, 15.0);
        }
    }
    if (!self.alive) return;
    // Spuren an der Zimmertuer: selten kommt etwas mit klatschenden Schritten an die Tuer, kratzt und atmet
    for (ClawDoor& cd : claws_) {
        const Spot& s = cd.spot;
        const vec3 into{std::cos(s.yaw), std::sin(s.yaw), 0.0f};
        const world::Room* inside = w.roomAt(s.pos.x + into.x * 1.5, s.pos.y + into.y * 1.5);
        const bool inRoom = inside && w.roomAt(self.feet.x, self.feet.y) == inside;
        const float d = length(vec3{self.feet.x - s.pos.x, self.feet.y - s.pos.y, 0.0f});
        if (cd.seqT >= 0.0) {
            cd.seqT += dt;
            // fuenf Schritte von hinten an die Tuer, dann Krallen am Blech, Atem, still
            static const double at[8] = {0.0, 0.62, 1.2, 1.74, 2.24, 2.9, 3.6, 4.6};
            while (cd.seqStep < 8 && cd.seqT >= at[cd.seqStep]) {
                const int k = cd.seqStep++;
                if (k < 5) {
                    const vec3 q = s.pos + into * (3.6f - 0.62f * (float)k) + vec3{0, 0, 0.05f};
                    out.push_back({Event::SoundAt, 0, "slap_step", q, 0.55f + 0.09f * (float)k});
                } else if (k == 5 || k == 6) {
                    out.push_back({Event::SoundAt, 0, "claw_scrape", s.pos + into * 0.2f + vec3{0, 0, 1.1f + 0.3f * (float)(k - 5)}, 0.8f});
                } else {
                    out.push_back({Event::SoundAt, 0, "wet_breath", s.pos + into * 0.4f + vec3{0, 0, 1.4f}, 0.6f});
                }
            }
            if (cd.seqStep >= 8) cd.seqT = -1.0, cd.seqStep = 0, cd.cool = cfg_.clawEvery * rng_.uniform(0.6, 1.4);
            continue;
        }
        if (inRoom || d > s.radius) continue;
        if ((cd.cool -= dt) <= 0.0) cd.seqT = 0.0, cd.seqStep = 0;
    }
    // Lager eines Infizierten: was er zurueckliess (einmal je Lauf)
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (noteSeen_[i]) continue;
        const float d = length(vec3{self.feet.x - notes_[i].pos.x, self.feet.y - notes_[i].pos.y, 0.0f});
        if (d > notes_[i].radius) continue;
        noteSeen_[i] = true;
        out.push_back({Event::Toast, 0, kNotes[i % 2], {}, 8.0f});
        out.push_back({Event::Sound2D, 0, "paper", {}, 0.5f});
    }
}

std::string Phenomena::drink(int id, const std::string& itemKey, std::vector<Event>& out) {
    Ailment& a = ail_[(size_t)id];
    if (itemKey == "almond_open") {
        if (a.infectT < 0.0) {
            infect(id);
            out.push_back({Event::Infected, id});
            log::info("Erscheinungen: Spieler {} angesteckt (offenes Mandelwasser)", id);
        }
        return "Es schmeckt nach Keller. Und nach etwas Pelzigem.";
    }
    if (itemKey != "almond" || a.infectT < 0.0) return "";
    // Interpretation (KREATUR_Crawler.md): Mandelwasser heilt frueh und haelt spaeter nur auf
    if (a.stage <= cfg_.cureUntil) {
        a.infectT = -1.0, a.stage = 0, a.touch = 0.0, a.hold = 0.0;
        out.push_back({Event::Cured, id});
        log::info("Erscheinungen: Spieler {} - Mandelwasser hat den Pilz vertrieben", id);
        return "Das Jucken lässt nach. Die Flecken verblassen.";
    }
    a.hold = cfg_.holdSeconds;
    return "Das Fieber sinkt ein wenig. Aber der Pilz bleibt.";
}

// --- Wirkung ----------------------------------------------------------------------------------------------------------
double Phenomena::sanityDrain(int id) const {
    const Ailment& a = ail_[(size_t)id];
    static const double fs[6] = {0.0, 0.02, 0.06, 0.12, 0.2, 0.2};
    static const double gs[4] = {0.0, 0.02, 0.05, 0.12};
    return (a.hold > 0.0 ? 0.5 : 1.0) * fs[std::clamp(a.stage, 0, 5)] + gs[std::clamp(a.gstage, 0, 3)];
}

double Phenomena::speedMul(int id) const {
    const Ailment& a = ail_[(size_t)id];
    double m = a.stage >= 4 ? 0.88 : a.stage == 3 ? 0.93 : 1.0;
    if (a.gstage == 2) m *= 0.9;
    if (a.gstage >= 3) m *= 0.8 - 0.25 * std::clamp((a.garden - cfg_.gardenBegin) / std::max(1.0, cfg_.gardenDeath - cfg_.gardenBegin), 0.0, 1.0);
    return m;
}

namespace {
// Fortschritt innerhalb eines Stadiums 0..1
double stageProgress(const Phenomena::Config& c, double t, int st) {
    double acc = 0.0;
    for (int k = 0; k < st - 1 && k < 4; ++k) acc += c.stage[k];
    return st >= 1 && st <= 4 ? std::clamp((t - acc) / c.stage[st - 1], 0.0, 1.0) : 1.0;
}
}  // namespace

float Phenomena::fungusFx(int id) const {
    const Ailment& a = ail_[(size_t)id];
    if (a.infectT < 0.0) return 0.0f;
    const double p = stageProgress(cfg_, a.infectT, a.stage);
    switch (a.stage) {
        case 1: return (float)(0.04 * p);
        case 2: return (float)(0.12 + 0.18 * p);
        case 3: return (float)(0.4 + 0.25 * p);
        default: return (float)std::min(1.0, 0.68 + 0.32 * p);
    }
}

float Phenomena::gardenFx(int id) const {
    const Ailment& a = ail_[(size_t)id];
    if (a.garden < cfg_.gardenSigns * 0.8) return 0.0f;
    return (float)std::clamp((a.garden - cfg_.gardenSigns * 0.8) / (cfg_.gardenDeath - cfg_.gardenSigns * 0.8), 0.0, 1.0);
}

vec4 Phenomena::skinFx(int id) const {
    const Ailment& a = ail_[(size_t)id];
    float f = 0.0f;
    if (a.infectT >= 0.0) {
        const double p = stageProgress(cfg_, a.infectT, a.stage);
        f = a.stage == 1 ? 0.0f : a.stage == 2 ? (float)(0.38 + 0.2 * p) : a.stage == 3 ? (float)(0.62 + 0.3 * p) : 1.0f;
    }
    // Ranken ab den ersten Zeichen, bis zum Beginn der Verwandlung halb (der Shader legt ab 0,5 Rinde an), dann bis zum Tod
    float g = 0.0f;
    if (a.garden > cfg_.gardenSigns * 0.8) {
        const double s0 = cfg_.gardenSigns * 0.8;
        g = a.garden < cfg_.gardenBegin ? (float)(0.5 * (a.garden - s0) / std::max(1.0, cfg_.gardenBegin - s0))
                                        : (float)std::min(1.0, 0.5 + 0.5 * (a.garden - cfg_.gardenBegin) / std::max(1.0, cfg_.gardenDeath - cfg_.gardenBegin));
    }
    return {f, g, 0.0f, 0.0f};
}

float Phenomena::muffle(int id) const {
    const Ailment& a = ail_[(size_t)id];
    float m = a.stage >= 4 ? 0.35f : 0.0f;
    if (a.gstage >= 3) m = std::max(m, 0.3f + 0.3f * gardenFx(id));
    return m;
}

float Phenomena::jesterFx(int id) const {
    const Ailment& a = ail_[(size_t)id];
    if (a.jesterT < 0.0) return 0.0f;
    const double t = a.jesterT;
    const float k = (float)std::clamp(std::min(t / 0.5, (kJesterLen + 0.2 - t) / 0.5), 0.0, 1.0);
    int line = 0;
    for (int i = 0; i < 3; ++i)
        if (t >= kJesterAt[i]) line = i;
    return (float)(kJesterExpr[line] * 2) + std::min(k, 0.999f);
}

void Phenomena::collect(std::vector<VisibleMesh>& out, vec3 eye) {
    for (Painting& p : paintings_) {
        if (!p.shown || length(p.pos - eye) > 45.0f) continue;
        VisibleMesh v;
        v.model = &p.model;
        v.meshId = &p.meshId;
        v.world = mat4::translation(p.pos) * mat4::rotationZ(p.yaw);
        out.push_back(v);
    }
}

Json Phenomena::toJson(int id) const {
    const Ailment& a = ail_[(size_t)id];
    Json j = Json::object();
    j.set("touch", Json(a.touch));
    j.set("infect", Json(a.infectT));
    j.set("hold", Json(a.hold));
    j.set("garden", Json(a.garden));
    j.set("jester_cool", Json(a.jesterCool));
    return j;
}

void Phenomena::fromJson(int id, const Json& j) {
    Ailment& a = ail_[(size_t)id];
    a = Ailment{};
    if (!j.isObject()) return;
    a.touch = j["touch"].asNumber(0.0);
    a.infectT = j["infect"].asNumber(-1.0);
    a.hold = j["hold"].asNumber(0.0);
    a.garden = j["garden"].asNumber(0.0);
    a.jesterCool = j["jester_cool"].asNumber(0.0);
    a.stage = stageOf(cfg_, a.infectT);
    a.gstage = a.garden >= cfg_.gardenBegin ? 3 : a.garden >= cfg_.gardenVines ? 2 : a.garden >= cfg_.gardenSigns ? 1 : 0;
}

}  // namespace lim::game
