#include "gameplay/stats.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace lim::game {

Stats::Stats(const DifficultyDef& d)
    : drain(d.drain / 60.0), deathTime(d.death), regen(d.regen), sprintTime(d.sprintTime), recoverTime(d.staminaRecover) {}

void Stats::updateStamina(double dt, bool sprinting, bool moving) {
    if (dead) return;
    if (sprinting) {
        // Energy Bar wirkt: Sprint kostet nur die Haelfte
        double rate = 100.0 / std::max(1.0, sprintTime) * (speedLeft > 0.0 ? 0.5 : 1.0);
        stamina -= rate * dt;
        staminaDelay = 0.9;
        if (stamina <= 0.0) {
            stamina = 0.0;
            exhausted = true;
        }
    } else if (staminaDelay > 0.0) {
        staminaDelay -= dt;
    } else {
        stamina += 100.0 / std::max(1.0, recoverTime) * (moving ? 0.7 : 1.0) * dt;
    }
    stamina = std::clamp(stamina, 0.0, 100.0);
    if (exhausted && stamina >= kRecoverAt) exhausted = false;
    // Atemnot klingt weich nach
    double target = exhausted ? 1.0 : std::clamp((35.0 - stamina) / 35.0, 0.0, 1.0) * 0.8;
    puff += (target - puff) * std::min(1.0, dt * (target > puff ? 3.0 : 0.6));
}

double Stats::breathless() const { return std::clamp(puff, 0.0, 1.0); }

void Stats::update(double dt) {
    if (dead) return;
    playTime += dt;
    sanity = std::max(0.0, sanity - drain * dt);
    double target;
    if (sanity <= 0.0) {
        zeroTime += dt;
        health = std::max(0.0, 100.0 * (1.0 - zeroTime / deathTime));
        target = std::min(1.0, 0.25 + 0.75 * zeroTime / deathTime);
        if (zeroTime >= deathTime) {
            health = 0.0;
            dead = true;
        }
    } else {
        zeroTime = 0.0;
        health = std::min(100.0, health + regen * dt);
        target = 0.0;
    }
    // Rot weich nachfuehren (einblenden ~0.3 s, ausblenden ~1.5 s)
    double k = std::min(1.0, dt * (target > red ? 3.0 : 0.7));
    red += (target - red) * k;
    if (speedLeft > 0.0) {
        speedLeft = std::max(0.0, speedLeft - dt);
        if (speedLeft == 0.0) speedBonus = 0.0;
    }
}

std::string Stats::consume(const ItemDef& it) {
    std::string parts;
    auto add = [&](const std::string& s) { parts += parts.empty() ? s : ", " + s; };
    if (it.sanity != 0.0) {
        double before = sanity;
        sanity = std::min(100.0, sanity + it.sanity);
        add(std::format("Sanity +{} %", (int)std::lround(sanity - before)));
    }
    if (it.health != 0.0) {
        double before = health;
        health = std::min(100.0, health + it.health);
        add(std::format("Health +{} %", (int)std::lround(health - before)));
    }
    if (it.speed != 0.0) {
        speedBonus = it.speed;
        speedLeft = std::min(3.0 * it.speedTime, speedLeft + it.speedTime);
        add(std::format("Tempo +{} % ({} s)", (int)std::lround(it.speed * 100), (int)std::lround(speedLeft)));
    }
    if (it.stamina != 0.0) {
        stamina = std::min(100.0, stamina + it.stamina);
        if (stamina >= kRecoverAt) exhausted = false;
        add("Ausdauer aufgefüllt");
    }
    return it.name + ": " + parts;
}

Json Stats::toJson() const {
    Json j = Json::object();
    j.set("health", health);
    j.set("sanity", sanity);
    j.set("zero_time", zeroTime);
    j.set("speed_left", speedLeft);
    j.set("speed_bonus", speedBonus);
    j.set("play_time", playTime);
    j.set("stamina", stamina);
    return j;
}

void Stats::fromJson(const Json& j) {
    health = j["health"].asNumber(100.0);
    sanity = j["sanity"].asNumber(100.0);
    zeroTime = j["zero_time"].asNumber(0.0);
    speedLeft = j["speed_left"].asNumber(0.0);
    speedBonus = j["speed_bonus"].asNumber(0.0);
    playTime = j["play_time"].asNumber(0.0);
    stamina = std::clamp(j["stamina"].asNumber(100.0), 0.0, 100.0);  // V3-V5: voll
    exhausted = false;
    staminaDelay = puff = 0.0;
    red = 0.0;
    dead = false;
}

}  // namespace lim::game
