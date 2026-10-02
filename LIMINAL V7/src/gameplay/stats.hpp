// Spielerwerte: Health und Sanity (Port von V4, stats.py).
//
// Sanity sinkt stetig. Bei 0 % faerbt sich der Bildschirmrand zunehmend rot,
// Health faellt bis zum Tod. Steigt Sanity wieder (Item), erholt sich Health
// langsam und das Rot blendet weich aus.
#pragma once

#include <string>

#include "core/json.hpp"
#include "gameplay/content.hpp"

namespace lim::game {

class Stats {
public:
    explicit Stats(const DifficultyDef& d);

    void update(double dt);
    // Wendet die Wirkung eines Items an und liefert eine kurze Meldung.
    std::string consume(const ItemDef& item);

    double speedMul() const { return speedLeft > 0.0 ? 1.0 + speedBonus : 1.0; }
    // Sekunden bis zum Tod, wenn Sanity bei 0 ist (sonst < 0)
    double deathCountdown() const { return sanity <= 0.0 ? std::max(0.0, deathTime - zeroTime) : -1.0; }

    // --- V6: Ausdauer ----------------------------------------------------------------
    // Sprinten verbraucht Ausdauer. Bei 0 ist man erschoepft: Sprinten bleibt gesperrt, bis
    // sich die Ausdauer auf kRecoverAt erholt hat (kein Stottern am Limit). Die Erholung
    // beginnt kurz nach dem Sprint, im Stehen schneller als beim Gehen.
    static constexpr double kRecoverAt = 30.0;
    void updateStamina(double dt, bool sprinting, bool moving);
    // Nur der Erschoepft-Zustand sperrt (eine zweite Schwelle liesse den Sprint am Limit stottern)
    bool canSprint() const { return !exhausted; }
    // Atemnot 0..1 (Keuchen, Koerperhaltung): nach Erschoepfung und bei niedriger Ausdauer
    double breathless() const;
    void spendStamina(double amount) { stamina = std::max(0.0, stamina - amount); }

    Json toJson() const;
    void fromJson(const Json& j);

    double drain, deathTime, regen;
    double health = 100.0, sanity = 100.0;
    double zeroTime = 0.0;   // Sekunden bei 0 % Sanity
    double red = 0.0;        // Staerke des roten Rands (0..1), weich nachgefuehrt
    double speedLeft = 0.0, speedBonus = 0.0;
    double playTime = 0.0;
    bool dead = false;
    // V6
    double stamina = 100.0;
    bool exhausted = false;
    double sprintTime, recoverTime;  // Sekunden: voller Sprint bis leer / Erholung von leer bis voll
    double staminaDelay = 0.0;       // Wartezeit bis zur Erholung
    double puff = 0.0;               // nachklingende Atemnot (weich)
};

}  // namespace lim::game
