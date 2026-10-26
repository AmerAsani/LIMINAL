// V13.2 Koop: ein weiterer Spieler in dieser Sitzung (MULTIPLAYER_ARCHITEKTUR.md, Abschnitte 3 und 8).
//   Host: die Figur eines Gasts. Der Host bewegt sie mit den Eingaben des Gasts (dieselbe Physik wie der eigene
//         Spieler) und rechnet ihre Werte, ihr Inventar und ihre Lampe.
//   Gast: ein Mitspieler. Ort, Blick und Haltung kommen aus den Momentaufnahmen des Hosts, 100 ms zurueck
//         zwischen zwei Proben interpoliert.
// Gezeichnet wird er mit dem Spielermodell und der Lampenhaltung von aussen (B-001); sein Lichtkegel ist ein
// eigenes Licht im Renderer.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "gameplay/body.hpp"
#include "gameplay/figure.hpp"
#include "gameplay/flashlight.hpp"
#include "gameplay/inventory.hpp"
#include "gameplay/player.hpp"
#include "gameplay/stats.hpp"
#include "gameplay/visible.hpp"

namespace lim::game {

struct DifficultyDef;

// Kennzahl eines Gegenstands der Welt (FNV-1a ueber seine stabile Kennung) - gleich bei Host und Gast
inline u32 worldItemHash(const std::string& id) {
    u32 h = 2166136261u;
    for (char c : id) h = (h ^ (u8)c) * 16777619u;
    return h ? h : 1u;
}

struct Pawn {
    Pawn(int id, std::string name, const DifficultyDef& diff, const Content* content);

    int id = 1;  // 1..3 (0 = Host)
    std::string name;
    Player player;
    Stats stats;
    Inventory inventory;
    Flashlight flash;
    Body body;
    FigureRig rig;
    bool alive = true, ready = false, connected = true;
    double deadT = 0.0;        // Sekunden seit dem Tod
    double noise = 0.0, landNoise = 0.0;
    int killRole = -1;
    bool lampLit = false;      // Gast: die Lampe leuchtet (laut Host)
    PlayerControl control;     // Host: letzte Eingabe
    double simBudget = 0.25;   // Host: noch erlaubte Simulationszeit aus Eingaben (gegen zu schnelle Uhren)
    u32 lastSeq = 0;           // Host: letzte verarbeitete Eingabe
    bool anySeq = false;

    // Gast: Proben fuer die Interpolation
    struct Sample {
        double t = 0.0;
        vec3 p;
        float yaw = 0.0f, pitch = 0.0f, speed = 0.0f;
        u8 flags = 0;
    };
    std::array<Sample, 8> samples{};
    int nSamples = 0;
    void addSample(const Sample& s);
    void interpolate(double t, double dt);  // setzt player (Ort, Blick, Gang) und bewegt den Koerper

    // gemeinsam
    void setupBody();                       // Koerperbau wie der eigene Spieler (realistisch)
    void updateBody(double dt);             // Haltung aus player (Lampe von aussen)
    void collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail);
    void shadow(std::vector<ShadowCaster>& out) const;
    bool beam(vec3& pos, vec3& dir, float& power, double time) const;  // Lichtkegel, false: aus
    vec3 eye() const { return body.eye(); }

private:
    bool bodyReady_ = false;
};

}  // namespace lim::game
