// V7: Laufbuch - was dieser Lauf hinter sich hat.
//
// Zaehlt betretene Raeume, besuchte Ebenen, durchschrittene Notausgaenge und erlebte
// Halluzinationen. Spielzeit, Strecke und Notizen fuehren Werte, Spieler und Notizbuch selbst.
// Wird gespeichert und beim Ebenenwechsel mitgenommen (die Raum-Kennungen gelten je Ebene).
#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "core/core.hpp"
#include "core/json.hpp"

namespace lim::game {

struct RunLog {
    long long rooms = 0;              // betretene Raeume (alle Ebenen zusammen)
    int exits = 0;                    // durchschrittene Notausgaenge
    int visions = 0;                  // Stromausfaelle und Gestalten
    std::vector<std::string> levels;  // besuchte Ebenen in Reihenfolge
    std::unordered_set<u64> seen;     // Raeume der aktuellen Ebene

    // Raum (Sektor, Index) betreten; true, wenn er neu ist
    bool enter(i64 sx, i64 sy, int index) {
        // 19 Bit je Sektorachse, 14 Bit Index: passt verlustfrei in eine JSON-Zahl (double)
        u64 key = ((u64)((sx + (1 << 18)) & 0x7FFFF) << 33) | ((u64)((sy + (1 << 18)) & 0x7FFFF) << 14) | (u64)(index & 0x3FFF);
        if (!seen.insert(key).second) return false;
        ++rooms;
        return true;
    }
    // Ebenenwechsel: Raum-Kennungen gelten nur je Ebene
    void nextLevel(const std::string& id) {
        seen.clear();
        ++exits;
        levels.push_back(id);
    }
    int distinctLevels() const {
        std::unordered_set<std::string> s(levels.begin(), levels.end());
        return (int)s.size();
    }

    Json toJson() const {
        Json j = Json::object();
        j.set("rooms", rooms);
        j.set("exits", exits);
        j.set("visions", visions);
        Json l = Json::array();
        for (const auto& s : levels) l.push(s);
        j.set("levels", std::move(l));
        Json r = Json::array();
        for (u64 k : seen) r.push((double)k);
        j.set("seen", std::move(r));
        return j;
    }
    void fromJson(const Json& j) {
        rooms = j["rooms"].asInt(0);
        exits = (int)j["exits"].asInt(0);
        visions = (int)j["visions"].asInt(0);
        levels.clear();
        for (const auto& s : j["levels"].items()) levels.push_back(s.asString(""));
        seen.clear();
        for (const auto& k : j["seen"].items()) seen.insert((u64)k.asNumber(0.0));
    }
};

}  // namespace lim::game
