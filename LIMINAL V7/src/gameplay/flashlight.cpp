#include "gameplay/flashlight.hpp"

#include <algorithm>
#include <cmath>

#include "core/core.hpp"
#include "core/rng.hpp"

namespace lim::game {

bool Flashlight::toggle() {
    if (!on && charge <= 0.0) return false;
    on = !on;
    return true;
}

bool Flashlight::update(double dt) {
    if (!on) return false;
    charge = std::max(0.0, charge - kDrainPerSecond * dt);
    if (charge <= 0.0) {
        on = false;
        return true;
    }
    return false;
}

float Flashlight::output(double time) const {
    if (!on) return 0.0f;
    float base = charge < 5.0 ? 0.35f + 0.65f * (float)(charge / 5.0) : 1.0f;  // fast leer: schwach
    if (charge >= 15.0) return base;
    // schwache Batterie: unruhige Einbrueche, je leerer desto haeufiger und tiefer
    float k = (float)(1.0 - charge / 15.0);
    float t = (float)time * 9.0f;
    float n0 = hash01((u32)std::floor(t) * 2654435761u), n1 = hash01((u32)(std::floor(t) + 1.0f) * 2654435761u);
    float f = t - std::floor(t);
    float n = n0 + (n1 - n0) * f * f * (3.0f - 2.0f * f);
    float dip = n > 0.75f - 0.35f * k ? (n - (0.75f - 0.35f * k)) / (0.25f + 0.35f * k) : 0.0f;
    return base * (1.0f - 0.75f * k * dip);
}

Json Flashlight::toJson() const {
    Json j = Json::object();
    j.set("on", on);
    j.set("charge", charge);
    return j;
}

void Flashlight::fromJson(const Json& j) {
    on = j["on"].asBool(false);
    charge = std::clamp(j["charge"].asNumber(kStartCharge), 0.0, 100.0);
    if (charge <= 0.0) on = false;
}

}  // namespace lim::game
