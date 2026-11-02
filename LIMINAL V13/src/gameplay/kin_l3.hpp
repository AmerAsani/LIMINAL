// V13.3 Level 3 (KREATUR_Wretch.md, KREATUR_SkinStealer.md, Abschnitte "V13.3: Level 3"): Klaenge und Untertitel der
// Level-3-Auspraegungen von Wretch (Waffen, Sprachfetzen) und Skin-Stealer (getarnt im Mantel, einfache Saetze).
//
// Die Praesenz meldet einen Klang als Ereignis (Presence::Event Mimic, Swing oder Drag) mit der Staerke kKinCue + Nummer.
// Einzelspieler und Host spielen ihn nach dieser Nummer (app.cpp); fuer die Gaeste wird das Figurereignis durch den
// genauen Klang ersetzt (kinNetRoute: vorhandenes Klang-Ereignis mit Namen - das Protokoll bleibt, wie es ist), damit alle
// denselben Satz hoeren. Untertitel und der einmalige Hinweis haengen am Klangnamen (auch beim Gast).
#pragma once

#include <string>

namespace lim::game {

constexpr float kKinCue = 100.0f;  // Staerke ab hier: Klangnummer statt Lautstaerke

enum KinCue : int {
    // Wretch (Level 3): Sprachfetzen (0-3 verstaendlicher - er lockt, 4-7 verstuemmelt), Kampfschrei vor dem Schlag
    KC_FRAG0 = 0, KC_FRAG_LURE = 4, KC_FRAGS = 8,
    KC_CRY0 = 8, KC_CRIES = 3,
    // je Waffe (Rohr, Brett, Klinge): Schleifen/Klappern beim Gehen, Ausholen, Fehlschlag (Aufschlag), Treffer
    KC_DRAG0 = 11, KC_RAISE0 = 14, KC_MISS0 = 17, KC_HIT0 = 20,
    // Skin-Stealer (Level 3): Saetze in der Naehe, Antworten auf das Ansprechen, Ruf beim Weglaufen, Abschied, Verwandlung
    KC_SAY0 = 30, KC_SAYS = 8,
    KC_ANS0 = 38, KC_ANSWERS = 4,
    KC_WAIT = 42, KC_BYE = 43, KC_TEAR = 44,
    KC_COAT = 45,  // der schwere Mantel schwingt, Leder knarrt (Bewegungsgeraeusch beim Gehen)
    KC_COUNT = 46,
    // V13.3 Level 3 (KREATUR_DeathRat.md): Death Rats - Saetze des Sprachgeraets, Wetzen am Gitter, Zaehneklappern, Zischen
    // (Drohung), Biss, schwere Schritte, Fallenbau; die Falle: Knistern (Ankuendigung), Schlag, Versagen
    KC_RAT_SAY0 = 46, KC_RAT_SAYS = 12,
    KC_RAT_GRIND = 58, KC_RAT_CHATTER = 59, KC_RAT_HISS = 60, KC_RAT_BITE = 61, KC_RAT_STEP = 62, KC_RAT_TINKER = 63,
    KC_TRAP_CRACKLE = 64, KC_TRAP_ZAP = 65, KC_TRAP_FIZZLE = 66,
    // V13.3 Level 3 (KREATUR_Deathmoth.md): der Schwarm erhebt sich, Motten im Gesicht, eine wird abgeschuettelt; Praetorianer:
    // Beine klicken, Faecheln (Lockstoff), Aufbaeumen (Ankuendigung), Stich, Spucken
    KC_MOTH_SWARM = 67, KC_MOTH_FACE = 68, KC_MOTH_SHAKE = 69,
    KC_PR_CLICK = 70, KC_PR_FAN = 71, KC_PR_REAR = 72, KC_PR_STAB = 73, KC_PR_SPIT = 74,
    KC_L3_END = 75
};

struct KinLine {
    const char* sound = nullptr;  // Klangname (synth_kin_l3.cpp)
    const char* sub = nullptr;    // Untertitel (nullptr: keiner)
    float volume = 0.8f, radius = 24.0f, subSec = 2.6f;
    const char* hint = nullptr;   // einmaliger Hinweis beim ersten Hoeren (Rat des Wikis)
};

const KinLine* kinLine(int cue);                     // nullptr: keine Nummer
const KinLine* ratMothLine(int cue);                 // V13.3 Level 3: Death Rats, Deathmoths (presence_rat_l3.cpp)
const KinLine* ratMothLineBySound(const std::string& s);
const KinLine* kinLineBySound(const std::string& s);  // Gast: nach dem Klangnamen
bool kinRole(int role);  // Wretch, Skin-Stealer; V13.3 Level 3: Death Rat, Deathmoth (nur sie tragen Klangnummern)
inline int kinCueOf(float strength, int role) { return strength >= kKinCue && kinRole(role) ? (int)(strength - kKinCue + 0.5f) : -1; }

// Host: Figurereignisse mit Klangnummer aus der Liste fuer die Gaeste nehmen und als Klang mit Namen schicken
template <class Figs, class Sounds>
void kinNetRoute(Figs& figs, Sounds& sounds) {
    for (size_t i = 0; i < figs.size();) {
        const KinLine* L = kinLine(kinCueOf(figs[i].strength, (int)figs[i].role));
        if (!L) {
            ++i;
            continue;
        }
        sounds.push_back({L->sound, figs[i].pos, L->volume, -1});
        figs.erase(figs.begin() + (long long)i);
    }
}

}  // namespace lim::game
