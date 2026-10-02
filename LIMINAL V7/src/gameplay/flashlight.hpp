// V7: Taschenlampe. Die linke Hand des Strichmaennchens haelt sie (Gesten bleiben rechts),
// der Lichtkegel ist eine echte Leuchte mit Schatten und sichtbarem Strahl im Dunst.
//
// Die Batterie haelt eingeschaltet rund fuenf Minuten. Unter 15 % beginnt das Licht zu
// flackern, unter 5 % wird es schwach; leer geht sie aus. Batterien finden sich selten
// irgendwo in der Welt (siehe finds.hpp) und laden um einen festen Anteil nach.
#pragma once

#include <algorithm>

#include "core/json.hpp"

namespace lim::game {

class Flashlight {
public:
    static constexpr double kDrainPerSecond = 100.0 / 300.0;  // voll -> leer in 5 min
    static constexpr double kStartCharge = 70.0;

    bool on = false;
    double charge = kStartCharge;  // Prozent

    // Ein/Aus; liefert false, wenn die Batterie leer ist (bleibt aus)
    bool toggle();
    // Verbrauch; true, wenn die Batterie gerade leer geworden ist
    bool update(double dt);
    // Helligkeit 0..1 inkl. Flackern bei schwacher Batterie (time: Spielzeit)
    float output(double time) const;
    void addCharge(double pct) { charge = std::min(100.0, charge + pct); }
    bool low() const { return charge < 15.0; }

    Json toJson() const;
    void fromJson(const Json& j);
};

}  // namespace lim::game
