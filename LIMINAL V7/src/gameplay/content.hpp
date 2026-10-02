// Spielinhalte aus data/: Items, Schwierigkeitsgrade, Level.
// Alles, was ohne Code-Aenderung erweiterbar sein soll, steht hier als Daten.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "world/level_def.hpp"
#include "world/room.hpp"

namespace lim::game {

struct ItemDef {
    std::string key, name, description;
    double sanity = 0, health = 0, speed = 0, speedTime = 0;
    double stamina = 0;  // V6: Ausdauer (Prozentpunkte)
    std::string model, icon, useSound;
    double scale = 1.0;
    int maxStack = 16;
    // V6: Seltenheit in Versorgungsraeumen ("spawn": keep, replace, replace_chance)
    double spawnKeep = 1.0;
    std::string spawnReplace;
    double spawnReplaceChance = 0.0;
    bool rare = false;  // seltener Fund: Schimmer auf der Theke, eigener Klang und Hinweis
    double holdOffset = 0.0;  // V6: Griffhoehe am Modell (Koerperanimation "benutzen")
    double battery = 0.0;     // V7: laedt die Taschenlampe um so viele Prozentpunkte
};

struct DifficultyDef {
    std::string key, label, description;
    double drain = 15.0, death = 15.0, regen = 4.0;
    double itemsNone = 0.28, itemsOne = 0.80;
    double sprintTime = 7.0, staminaRecover = 6.0;  // V6: Sekunden Sprint (voll -> leer) / Erholung (leer -> voll)
};

// V7: Notiz eines anderen Wanderers (data/notes.json)
struct NoteDef {
    std::string title, text;
    std::vector<std::string> levels;  // leer = ueberall
};

struct LevelInfo {
    std::string file;
    std::shared_ptr<const world::LevelDef> def;     // V6: Aufbau fuer neue Spiele (layout_v6, falls vorhanden)
    std::shared_ptr<const world::LevelDef> legacy;  // Aufbau aus V4/V5 (alte Spielstaende, bitgenau)
};

class Content {
public:
    bool load(const std::string& dataDir, std::string& error);

    const ItemDef* item(const std::string& key) const;
    const std::vector<ItemDef>& items() const { return items_; }
    // Seltenheitsregeln aller Items fuer die Weltgenerierung
    world::SupplyRules supplyRules() const;
    const DifficultyDef& difficulty(const std::string& key) const;
    const std::vector<DifficultyDef>& difficulties() const { return difficulties_; }
    const std::string& defaultDifficulty() const { return defaultDifficulty_; }
    // layout: world::kLayoutV6 (neue Spiele) oder world::kLayoutLegacy (Spielstaende aus V4/V5/fruehem V6)
    std::shared_ptr<const world::LevelDef> level(const std::string& id, int layout = world::kLayoutV6) const;
    const std::vector<LevelInfo>& levels() const { return levels_; }
    // V7: Notizen; noteFor waehlt stabil aus den Notizen dieses Levels (-1 = keine)
    const std::vector<NoteDef>& notes() const { return notes_; }
    int noteFor(const std::string& levelId, u64 hash) const;

private:
    std::vector<ItemDef> items_;
    std::vector<DifficultyDef> difficulties_;
    std::string defaultDifficulty_ = "medium";
    std::vector<LevelInfo> levels_;
    std::vector<NoteDef> notes_;
};

}  // namespace lim::game
