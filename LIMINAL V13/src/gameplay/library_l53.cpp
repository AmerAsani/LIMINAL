// V13.3 Teil 4 Level 5.3: Orte, Funde und Saetze der Bibliothek (gameplay/library_l53.hpp; KREATUR_Gentleman.md,
// KREATUR_Bibliothekswissen.md). Eigene Worte, keine Zitate; im Spiel faellt kein Wiki-Name (E-039): er ist "eine ruhige
// Maennerstimme" bzw. "der Herr im Frack", die Wesen der Buecher heissen "rote Lichter" und "das Volk in den Anzuegen".
#include "gameplay/library_l53.hpp"

#include <cmath>
#include <cstdlib>

#include "anim/creature.hpp"
#include "gameplay/kin_l3.hpp"
#include "world/level_def.hpp"
#include "world/map_def.hpp"

namespace lim::game {

namespace lib53 {
const KinLine* gentLine(int cue) {
    static const KinLine k[GM_COUNT] = {
        {"gm_door", nullptr, 0.8f, 40.0f},
        {"gm_sigh", "(Ein langer, gelangweilter Seufzer durch die Nase.)", 0.55f, 14.0f, 2.4f},
        {"gm_mutter", "Eine ruhige Männerstimme, zu sich selbst: „Dieser Ort. Ich hatte gehofft, ihn nie wiederzusehen.“", 0.6f, 16.0f, 4.0f},
        {"gm_bookdrop", nullptr, 0.8f, 22.0f},
        {"gm_pages", nullptr, 0.5f, 10.0f},
        {"gm_throw", nullptr, 0.7f, 16.0f},
        {"gm_land", nullptr, 0.85f, 24.0f},
        {"gm_speak_0", "Er spricht in den Raum, nicht zu dir: „Lange her. Ich wüsste gern, warum du wieder da bist – aber ich weiß es ja.“",
         0.9f, 30.0f, 5.5f},
        {"gm_speak_1", "„Mutig. Und so unhöflich. Es wird nicht funktionieren – so wenig wie beim letzten Mal.“", 0.9f, 30.0f, 5.0f},
        {"gm_hush", "(Stille. Kein Buch fällt, kein Holz knarrt. Die Bibliothek antwortet nicht.)", 0.7f, 60.0f, 4.0f},
        {"gm_speak_2", "„Hier bist du nicht willkommen. Versuch es ruhig – ich nehme dir die Hoffnung gern noch einmal.“", 0.9f, 30.0f, 5.5f,
         "Rat der Reisenden: Der Herr des Hotels sieht alles. Nicht ansehen, nicht stören, nicht im Weg stehen."},
        {"gm_notice", "Der Kopf dreht sich zu dir – viel zu weit, der Körper bleibt stehen: „Ein Gast. Hier unten.“", 0.9f, 24.0f, 3.5f},
        {"gm_hunt", "„Bleiben Sie doch. Mein Haus ist so viel bequemer als dieser Ort.“", 0.9f, 22.0f, 4.0f},
        {"gm_announce", "Der Arm entfaltet sich, Gelenk um Gelenk: „Erlauben Sie.“", 1.0f, 18.0f, 2.2f, "Abstand! Weit weg von seiner Hand."},
        {"gm_grip", "Ganz nah, kalt und freundlich: „Vergessen Sie, was Sie hier gelesen haben.“", 1.0f, 12.0f, 3.5f},
        {"gm_compose", "Er zieht die Revers glatt: „Wie Sie wünschen. Genießen Sie Ihren Aufenthalt.“", 0.85f, 20.0f, 4.0f},
        {"gm_light", "„Nehmen Sie das Licht herunter. Das gehört sich nicht.“", 0.85f, 18.0f, 3.0f},
        {"gm_crack", nullptr, 0.5f, 8.0f},
        {"gm_gone", nullptr, 0.7f, 40.0f},
        {"lib_wrath", "(Irgendwo in den Regalen kracht es – Holz ächzt, Bücher stürzen. Die Bibliothek ist zornig.)", 1.0f, 60.0f, 4.0f},
        {"lib_creak", nullptr, 0.6f, 30.0f},
        {"lib_dicta", "Das Diktiergerät am Pult klickt von selbst. Eine Stimme, die du kennst – aber zu glatt, wie aus einer Maschine: "
                      "„Komm mit. Ich zeig dir, wo ich jetzt wohne.“",
         0.75f, 10.0f, 6.0f, "Rat der Reisenden: Vertrauten Stimmen nicht glauben."},
    };
    return cue >= 0 && cue < GM_COUNT ? &k[cue] : nullptr;
}
const KinLine* gentLineBySound(const std::string& s) {
    for (int i = 0; i < GM_COUNT; ++i)
        if (const KinLine* L = gentLine(i); L && L->sound && s == L->sound) return L;
    return nullptr;
}
}  // namespace lib53

namespace {
vec3 dirOfY(float yaw) { return {std::cos(yaw), std::sin(yaw), 0.0f}; }
}  // namespace

void Library53::setup(const world::LevelDef& level) {
    *this = Library53{};
    if (!level.mapped()) return;
    const auto& sp = level.map->spawns;
    for (size_t i = 0; i < sp.size(); ++i) {
        const world::MapSpawn& s = sp[i];
        if (s.kind != "trace") continue;
        const vec3 p{(float)s.x, (float)s.y, (float)s.z};
        if (s.set == "gm") {
            if (s.hasTag("door")) door_ = p;
            if (s.hasTag("stand")) stand_ = p, yaw_ = (float)s.yaw, scene_ = true;
        } else if (s.set == "lore") {
            Lore L;
            L.pos = p, L.yaw = (float)s.yaw;
            for (const std::string& t : s.tags)  // der Spawnpunkt steht vor dem Moebel; Tag p<x>,<y>: dort liegt der Fund
                if (t.size() > 3 && t[0] == 'p' && t.find(',') != std::string::npos)
                    L.pos.x = (float)std::atof(t.c_str() + 1), L.pos.y = (float)std::atof(t.c_str() + t.find(',') + 1);
            if (s.hasTag("e141")) {  // rote Lichter (Entity 141): Folio auf dem Lesepult
                L.id = "trace:l53_lore141", L.title = "Folio mit roter Tafel", L.model = "prop:lore_folio", L.prompt = "Folio lesen";
            } else if (s.hasTag("e189")) {  // das Volk in den Anzuegen (Entity 189): Karteikarten auf dem Katalogschrank
                L.id = "trace:l53_lore189", L.title = "Karteikarten mit Zahnrad", L.model = "prop:lore_cards", L.prompt = "Karteikarten lesen";
            } else {
                continue;
            }
            lore_.push_back(std::move(L));
        }
    }
    if (scene_ && door_.x == 0.0f && door_.y == 0.0f) scene_ = false;  // ohne Gang keine Szene
}

vec3 Library53::landing() const {
    const vec3 f = dirOfY(yaw_), l{-f.y, f.x, 0.0f}, b = anim::gm::bookLand();
    return stand_ + f * b.x + l * b.y + vec3{0, 0, b.z};
}

vec3 Library53::pileAt() const {
    const vec3 f = dirOfY(yaw_), l{-f.y, f.x, 0.0f};
    return stand_ + f * 1.6f - l * 0.95f;  // rechts von seinem Platz, vor dem Regal, aus dem das Notizbuch fiel
}

}  // namespace lim::game
