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
        d.stamina = j["stamina"].asNumber(0);
        d.model = j["model"].asString(d.key);
        d.icon = j["icon"].asString(d.key);
        d.useSound = j["use_sound"].asString("");
        d.scale = j["scale"].asNumber(1.0);
        d.maxStack = (int)j["max_stack"].asInt(16);
        const Json& sp = j["spawn"];
        d.spawnKeep = std::clamp(sp["keep"].asNumber(1.0), 0.0, 1.0);
        d.spawnReplace = sp["replace"].asString("");
        d.spawnReplaceChance = std::clamp(sp["replace_chance"].asNumber(0.0), 0.0, 1.0);
        d.rare = j["rare"].asBool(false);
        d.holdOffset = j["hold_offset"].asNumber(0.0);
        d.battery = j["battery"].asNumber(0.0);
        if (!d.key.empty()) items_.push_back(std::move(d));
    }
    for (const auto& d : items_)
        if (!d.spawnReplace.empty() && !item(d.spawnReplace)) {
            log::warn("items.json: {} ersetzt durch unbekanntes Item '{}' - Ersatz entfaellt", d.key, d.spawnReplace);
            for (auto& m : items_)
                if (m.key == d.key) m.spawnReplace.clear();
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
        d.sprintTime = std::clamp(j["sprint_time"].asNumber(7.0), 1.0, 600.0);
        d.staminaRecover = std::clamp(j["stamina_recover"].asNumber(6.0), 1.0, 600.0);
        difficulties_.push_back(std::move(d));
    }
    if (difficulties_.empty()) difficulties_.push_back({"medium", "Medium", ""});

    // Alle Level im Ordner data/levels
    auto files = paths::listFiles(paths::join(dir, "levels"), "", ".json");
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        std::string err;
        auto def = world::LevelDef::loadFile(f, &err, world::kLayoutV6);
        auto legacy = def ? world::LevelDef::loadFile(f, &err, world::kLayoutLegacy) : std::nullopt;
        if (!def || !legacy) {
            log::error("Level {} ungueltig: {}", f, err);
            continue;
        }
        log::info("Level geladen: {} ({}), {} Zonen, {} Raumtypen, Aufbau V{}", def->id, def->name, def->zones.size(),
                  def->roomTypes.size(), def->layout);
        levels_.push_back({f, std::make_shared<const world::LevelDef>(std::move(*def)),
                           std::make_shared<const world::LevelDef>(std::move(*legacy))});
    }
    if (levels_.empty()) {
        error = "Kein gueltiges Level in data/levels gefunden.";
        return false;
    }
    // V7: Notizen (optional)
    if (auto nj = loadJsonFile(paths::join(dir, "notes.json"))) {
        for (const auto& n : (*nj)["notes"].items()) {
            NoteDef d;
            d.title = n["title"].asString("Notiz");
            d.text = n["text"].asString("");
            for (const auto& l : n["levels"].items()) d.levels.push_back(l.asString(""));
            if (!d.text.empty()) notes_.push_back(std::move(d));
        }
        log::info("Notizen geladen: {}", notes_.size());
    }
    return true;
}

int Content::noteFor(const std::string& levelId, u64 hash) const {
    std::vector<int> fit;
    for (size_t i = 0; i < notes_.size(); ++i) {
        const auto& l = notes_[i].levels;
        if (l.empty() || std::find(l.begin(), l.end(), levelId) != l.end()) fit.push_back((int)i);
    }
    return fit.empty() ? -1 : fit[(size_t)(hash % fit.size())];
}

const ItemDef* Content::item(const std::string& key) const {
    for (const auto& i : items_)
        if (i.key == key) return &i;
    return nullptr;
}

world::SupplyRules Content::supplyRules() const {
    world::SupplyRules out;
    for (const auto& d : items_)
        if (d.spawnKeep < 1.0) out.push_back({d.key, d.spawnKeep, d.spawnReplace, d.spawnReplaceChance});
    return out;
}

const DifficultyDef& Content::difficulty(const std::string& key) const {
    for (const auto& d : difficulties_)
        if (d.key == key) return d;
    for (const auto& d : difficulties_)
        if (d.key == defaultDifficulty_) return d;
    return difficulties_.front();
}

std::shared_ptr<const world::LevelDef> Content::level(const std::string& id, int layout) const {
    const LevelInfo* info = &levels_.front();
    for (const auto& l : levels_)
        if (l.def->id == id) info = &l;
    return layout >= world::kLayoutV6 ? info->def : info->legacy;
}

}  // namespace lim::game
