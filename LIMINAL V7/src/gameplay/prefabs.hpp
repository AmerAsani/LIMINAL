// Prefabs: Entity-Vorlagen aus data/prefabs/*.json.
//
// Eine Vorlage listet Komponenten mit ihren Werten. Neue Entity-Arten (NPCs,
// Gegner, interaktive Objekte) entstehen als neue JSON-Datei; nur voellig neue
// Verhaltensweisen brauchen eine neue Komponente und ein System.
#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "core/json.hpp"
#include "ecs/registry.hpp"
#include "gameplay/components.hpp"

namespace lim::game {

class PrefabLibrary {
public:
    using Reader = std::function<void(ecs::Registry&, ecs::Entity, const Json&)>;

    PrefabLibrary();
    void loadDirectory(const std::string& dir);
    bool has(const std::string& name) const { return prefabs_.count(name) != 0; }
    // Erzeugt eine Entity; pos/yaw ueberschreiben die Transform-Komponente.
    ecs::Entity spawn(ecs::Registry& reg, const std::string& name, vec3 pos, float yaw = 0.0f) const;
    // Neue Komponententypen anmelden
    void registerComponent(const std::string& name, Reader reader) { readers_[name] = std::move(reader); }

private:
    std::unordered_map<std::string, Json> prefabs_;
    std::unordered_map<std::string, Reader> readers_;
};

}  // namespace lim::game
