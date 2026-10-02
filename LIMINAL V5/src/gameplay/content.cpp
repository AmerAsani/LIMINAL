#include "gameplay/content.hpp"

#include <algorithm>

#include "core/json.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim::game {

bool Content::load(const std::string& dir, std::string& error) {
    auto items = loadJsonFile(paths::join(dir, "items.json"));
    if (!items) {
        error = "data/items.json fehlt oder ist fehlerhaft.";
        return false;
    }
    for (const auto& j : (*items)["items"].items()) {
        ItemDef d;
        d.key = j["key"].asString();
        d.name = j["name"].asString(d.key);
        d.description = j["description"].asString("");
        d.sanity = j["sanity"].asNumber(0);
        d.health = j["health"].asNumber(0);
        d.speed = j["speed"].asNumber(0);
        d.speedTime = j["speed_time"].asNumber(0);
        d.model = j["model"].asString(d.key);
        d.icon = j["icon"].asString(d.key);
        d.useSound = j["use_sound"].asString("");
        d.scale = j["scale"].asNumber(1.0);
        d.maxStack = (int)j["max_stack"].asInt(16);
        if (!d.key.empty()) items_.push_back(std::move(d));
    }

    auto diff = loadJsonFile(paths::join(dir, "difficulty.json"));
    if (!diff) {
        error = "data/difficulty.json fehlt oder ist fehlerhaft.";
        return false;
    }
    defaultDifficulty_ = (*diff)["default"].asString("medium");
    for (const auto& j : (*diff)["levels"].items()) {
        DifficultyDef d;
        d.key = j["key"].asString();
        d.label = j["label"].asString(d.key);
        d.description = j["description"].asString("");
        d.drain = j["drain"].asNumber(15.0);
        d.death = j["death"].asNumber(15.0);
        d.regen = j["regen"].asNumber(4.0);
        d.itemsNone = j["items"][0].asNumber(0.28);
        d.itemsOne = j["items"][1].asNumber(0.80);
        difficulties_.push_back(std::move(d));
    }
    if (difficulties_.empty()) difficulties_.push_back({"medium", "Medium", ""});

    // Alle Level im Ordner data/levels
    auto files = paths::listFiles(paths::join(dir, "levels"), "", ".json");
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        std::string err;
        auto def = world::LevelDef::loadFile(f, &err);
        if (!def) {
            log::error("Level {} ungueltig: {}", f, err);
            continue;
        }
        log::info("Level geladen: {} ({}), {} Zonen, {} Raumtypen", def->id, def->name, def->zones.size(),
                  def->roomTypes.size());
        levels_.push_back({f, std::make_shared<const world::LevelDef>(std::move(*def))});
    }
    if (levels_.empty()) {
        error = "Kein gueltiges Level in data/levels gefunden.";
        return false;
    }
    return true;
}

const ItemDef* Content::item(const std::string& key) const {
    for (const auto& i : items_)
        if (i.key == key) return &i;
    return nullptr;
}

const DifficultyDef& Content::difficulty(const std::string& key) const {
    for (const auto& d : difficulties_)
        if (d.key == key) return d;
    for (const auto& d : difficulties_)
        if (d.key == defaultDifficulty_) return d;
    return difficulties_.front();
}

std::shared_ptr<const world::LevelDef> Content::level(const std::string& id) const {
    for (const auto& l : levels_)
        if (l.def->id == id) return l.def;
    return levels_.front().def;
}

}  // namespace lim::game
