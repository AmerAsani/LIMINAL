#include "gameplay/prefabs.hpp"

#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim::game {

PrefabLibrary::PrefabLibrary() {
    // Leser fuer alle eingebauten Komponenten
    registerComponent("transform", [](ecs::Registry& r, ecs::Entity e, const Json& j) {
        r.add<Transform>(e, Transform{{}, j["yaw"].asFloat(0.0f), j["scale"].asFloat(1.0f)});
    });
    registerComponent("mesh", [](ecs::Registry& r, ecs::Entity e, const Json& j) {
        MeshRenderer m;
        m.model = j["model"].asString("");
        m.drawDistance = j["draw_distance"].asFloat(40.0f);
        r.add<MeshRenderer>(e, m);
    });
    registerComponent("pickup", [](ecs::Registry& r, ecs::Entity e, const Json& j) {
        r.add<Pickup>(e, Pickup{j["item"].asString(""), ""});
    });
    registerComponent("interactable", [](ecs::Registry& r, ecs::Entity e, const Json& j) {
        r.add<Interactable>(e, Interactable{j["prompt"].asString(""), j["range"].asFloat(2.1f), j["cone"].asFloat(42.0f)});
    });
    registerComponent("audio_emitter", [](ecs::Registry& r, ecs::Entity e, const Json& j) {
        r.add<AudioEmitter>(e, AudioEmitter{j["sound"].asString(""), j["volume"].asFloat(1.0f), j["radius"].asFloat(10.0f), -1});
    });
    registerComponent("wanderer", [](ecs::Registry& r, ecs::Entity e, const Json& j) {
        Wanderer w;
        w.speed = j["speed"].asFloat(1.2f);
        r.add<Wanderer>(e, w);
    });
}

void PrefabLibrary::loadDirectory(const std::string& dir) {
    for (const auto& f : paths::listFiles(dir, "", ".json")) {
        auto j = loadJsonFile(f);
        if (!j) continue;
        std::string name = (*j)["name"].asString("");
        if (name.empty()) {
            log::warn("Prefab ohne Namen: {}", f);
            continue;
        }
        for (const auto& [comp, val] : (*j)["components"].members())
            if (!readers_.count(comp)) log::warn("Prefab {}: unbekannte Komponente '{}'", name, comp);
        prefabs_[name] = std::move(*j);
    }
    log::info("Prefabs geladen: {}", prefabs_.size());
}

ecs::Entity PrefabLibrary::spawn(ecs::Registry& reg, const std::string& name, vec3 pos, float yaw) const {
    auto it = prefabs_.find(name);
    if (it == prefabs_.end()) return ecs::kNull;
    ecs::Entity e = reg.create();
    for (const auto& [comp, val] : it->second["components"].members()) {
        auto r = readers_.find(comp);
        if (r != readers_.end()) r->second(reg, e, val);
    }
    Transform* t = reg.get<Transform>(e);
    if (!t) t = &reg.add<Transform>(e);
    t->pos = pos;
    t->yaw = yaw;
    reg.add<Tag>(e, Tag{name});
    return e;
}

}  // namespace lim::game
