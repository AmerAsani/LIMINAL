// Spielinhalte aus data/: Items, Schwierigkeitsgrade, Level.
// Alles, was ohne Code-Aenderung erweiterbar sein soll, steht hier als Daten.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "world/level_def.hpp"

namespace lim::game {

struct ItemDef {
    std::string key, name, description;
    double sanity = 0, health = 0, speed = 0, speedTime = 0;
    std::string model, icon, useSound;
    double scale = 1.0;
    int maxStack = 16;
};

struct DifficultyDef {
    std::string key, label, description;
    double drain = 15.0, death = 15.0, regen = 4.0;
    double itemsNone = 0.28, itemsOne = 0.80;
};

struct LevelInfo {
    std::string file;
    std::shared_ptr<const world::LevelDef> def;
};

class Content {
public:
    bool load(const std::string& dataDir, std::string& error);

    const ItemDef* item(const std::string& key) const;
    const std::vector<ItemDef>& items() const { return items_; }
    const DifficultyDef& difficulty(const std::string& key) const;
    const std::vector<DifficultyDef>& difficulties() const { return difficulties_; }
    const std::string& defaultDifficulty() const { return defaultDifficulty_; }
    std::shared_ptr<const world::LevelDef> level(const std::string& id) const;
    const std::vector<LevelInfo>& levels() const { return levels_; }

private:
    std::vector<ItemDef> items_;
    std::vector<DifficultyDef> difficulties_;
    std::string defaultDifficulty_ = "medium";
    std::vector<LevelInfo> levels_;
};

}  // namespace lim::game
