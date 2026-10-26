#include "world/map_def.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/paths.hpp"

namespace lim::world {

bool MapRoom::hasTag(const std::string& t) const { return std::find(tags.begin(), tags.end(), t) != tags.end(); }
bool MapSpawn::hasTag(const std::string& t) const { return std::find(tags.begin(), tags.end(), t) != tags.end(); }

const MapTransition* MapDef::transition(const std::string& tid) const {
    for (const auto& t : transitions)
        if (t.id == tid) return &t;
    return nullptr;
}

int MapDef::roomAt(double x, double y) const {
    const i64 tx = (i64)std::floor(x), ty = (i64)std::floor(y);
    for (size_t i = 0; i < rooms.size(); ++i) {
        const MapRoom& r = rooms[i];
        if (r.x0 <= tx && tx < r.x1 && r.y0 <= ty && ty < r.y1) return (int)i;
    }
    return -1;
}

namespace {

std::vector<std::string> strings(const Json& a) {
    std::vector<std::string> out;
    for (const auto& s : a.items())
        if (s.isString()) out.push_back(s.asString());
    return out;
}

bool rect(const Json& r, i64& x0, i64& y0, i64& x1, i64& y1) {
    if (!r.isArray() || r.size() != 4) return false;
    x0 = r[0].asInt(), y0 = r[1].asInt(), x1 = r[2].asInt(), y1 = r[3].asInt();
    return x1 > x0 && y1 > y0;
}

}  // namespace

std::optional<MapDef> MapDef::fromJson(const Json& j, std::string* error) {
    auto fail = [&](std::string m) -> std::optional<MapDef> {
        if (error) *error = std::move(m);
        return std::nullopt;
    };
    MapDef M;
    M.id = j["id"].asString("map");
    M.w = j["size"][0].asInt(0);
    M.h = j["size"][1].asInt(0);
    if (M.w < 16 || M.h < 16 || M.w > 4096 || M.h > 4096) return fail("Karte: size fehlt oder unplausibel");
    M.seed = (u64)j["seed"].asInt(1);
    M.startX = j["start"][0].asNumber(1.5);
    M.startY = j["start"][1].asNumber(1.5);
    M.startYaw = j["start"][2].asNumber(0.0);
    M.wrap = j["wrap"].asBool(false);
    for (const auto& r : j["rooms"].items()) {
        MapRoom R;
        if (!rect(r["rect"], R.x0, R.y0, R.x1, R.y1)) return fail(std::format("Karte {}: Raum {} ohne gueltiges rect", M.id, M.rooms.size()));
        R.type = r["type"].asString("");
        R.zone = r["zone"].asString("");
        R.floor = r["floor"].asNumber(0.0);
        R.height = r["height"].asNumber(3.0);
        R.seed = r.has("seed") ? (u64)r["seed"].asInt(0) : (u64)(M.seed * 1000003ull + M.rooms.size() * 7919ull);
        R.light = r["light"].asString("");
        R.circuit = (int)r["circuit"].asInt(-1);
        R.flicker = (int)r["flicker"].asInt(-1);
        R.ambient = r["ambient"].asNumber(-1.0);
        // pillars: [abstand_x, abstand_y, staerke, erste_x, erste_y] (Weltkoordinaten)
        if (const Json* p = r.find("pillars")) {
            R.pillarSp = (*p)[0].asInt(0), R.pillarSpY = (*p)[1].asInt(0), R.pillarTh = (*p)[2].asInt(1);
            R.pillarOx = (*p)[3].asInt(0), R.pillarOy = (*p)[4].asInt(0);
            if (R.pillarSpY <= 0) R.pillarSpY = R.pillarSp;
        }
        R.beamDrop = r["beams"].asNumber(0.0);
        for (const auto& l : r["lights"].items()) {
            MapLight L;
            L.x = l[0].asNumber(), L.y = l[1].asNumber();
            L.kind = (int)l[2].asInt(0);
            L.intensity = l[3].asNumber(0.8);
            L.radius = l[4].asNumber(4.5);
            L.dirX = (int)l[5].asInt(0), L.dirY = (int)l[6].asInt(0);
            L.z = l.size() > 7 ? l[7].asNumber(-999.0) : -999.0;
            L.panel = l.size() > 8 ? l[8].asInt(1) != 0 : true;
            if (l.size() > 11) L.r = l[9].asNumber(-1.0), L.g = l[10].asNumber(0.0), L.b = l[11].asNumber(0.0);
            R.lights.push_back(L);
        }
        for (const auto& b : r["blocks"].items())
            R.blocks.push_back({b[0].asInt(), b[1].asInt(), b[2].asInt(), b[3].asInt(), b[4].asNumber(1.0), b[5].asInt(1) != 0});
        R.water = r["water"].asNumber(-999.0);  // V11
        R.waterLook = (int)r["water_look"].asNumber(0.0);  // V12
        R.label = r["label"].asString("");
        R.tags = strings(r["tags"]);
        if (R.x0 < 1 || R.y0 < 1 || R.x1 > M.w - 1 || R.y1 > M.h - 1) return fail(std::format("Karte {}: Raum {} ragt aus der Karte", M.id, M.rooms.size()));
        if (R.height < 1.9) return fail(std::format("Karte {}: Raum {} zu niedrig", M.id, M.rooms.size()));
        M.rooms.push_back(std::move(R));
    }
    for (const auto& d : j["doors"].items()) {
        MapDoor D;
        if (!rect(d["rect"], D.x0, D.y0, D.x1, D.y1)) return fail(std::format("Karte {}: Tuer {} ohne gueltiges rect", M.id, M.doors.size()));
        D.axis = (int)d["axis"].asInt(0);
        D.a = (int)d["a"].asInt(-1);
        D.b = (int)d["b"].asInt(-1);
        const std::string k = d["kind"].asString("door");
        D.kind = k == "opening" ? MapDoorKind::Opening : k == "gate" ? MapDoorKind::Gate : k == "wide" ? MapDoorKind::Wide : MapDoorKind::Door;
        D.top = d["top"].asNumber(-1.0);
        D.style = d["style"].asString("");
        if (D.a < 0 || D.b < 0 || D.a >= (int)M.rooms.size() || D.b >= (int)M.rooms.size() || D.a == D.b)
            return fail(std::format("Karte {}: Tuer {} verbindet keine zwei Raeume", M.id, M.doors.size()));
        M.doors.push_back(std::move(D));
    }
    for (const auto& s : j["spawns"].items()) {
        MapSpawn S;
        S.kind = s["kind"].asString("");
        S.x = s["pos"][0].asNumber(), S.y = s["pos"][1].asNumber(), S.z = s["pos"][2].asNumber(0.0);
        S.yaw = s["yaw"].asNumber(0.0);
        S.tags = strings(s["tags"]);
        S.role = s["role"].asString("");
        S.set = s["set"].asString("");
        S.group = (int)s["group"].asInt(-1);
        S.weight = s["weight"].asNumber(1.0);
        S.room = M.roomAt(S.x, S.y);
        M.spawns.push_back(std::move(S));
    }
    for (const auto& o : j["objects"].items()) {
        MapObject O;
        O.model = o["model"].asString("");
        O.x = o["pos"][0].asNumber(), O.y = o["pos"][1].asNumber(), O.z = o["pos"][2].asNumber(0.0);
        O.yaw = o["yaw"].asNumber(0.0);
        if (const Json* sc = o.find("scale")) O.sx = (*sc)[0].asNumber(1), O.sy = (*sc)[1].asNumber(1), O.sz = (*sc)[2].asNumber(1);
        O.variant = (int)o["variant"].asInt(0);
        O.tag = o["tag"].asString("");
        M.objects.push_back(std::move(O));
    }
    for (const auto& t : j["transitions"].items()) {
        MapTransition T;
        T.id = t["id"].asString("");
        T.kind = t["kind"].asString("door");
        T.target = t["target"].asString("");
        T.x = t["pos"][0].asNumber(), T.y = t["pos"][1].asNumber(), T.yaw = t["pos"][2].asNumber(0.0);
        if (const Json* r = t.find("rect")) rect(*r, T.x0, T.y0, T.x1, T.y1);
        T.needs = t["requires"].asString("");  // V11
        if (T.id.empty()) return fail(std::format("Karte {}: Uebergang ohne id", M.id));
        M.transitions.push_back(std::move(T));
    }
    for (const auto& c : j["checkpoints"].items()) {
        MapCheckpoint C;
        C.id = c["id"].asString("");
        C.label = c["label"].asString(C.id);
        C.x = c["pos"][0].asNumber(), C.y = c["pos"][1].asNumber(), C.yaw = c["pos"][2].asNumber(0.0);
        if (C.id.empty() || M.roomAt(C.x, C.y) < 0) return fail(std::format("Karte {}: Sicherungspunkt ohne id oder ausserhalb", M.id));
        M.checkpoints.push_back(std::move(C));
    }
    return M;
}

std::optional<MapDef> MapDef::loadFile(const std::string& path, std::string* error) {
    auto j = loadJsonFile(path);
    if (!j) {
        if (error) *error = "Karte fehlt oder ist kein gueltiges JSON: " + path;
        return std::nullopt;
    }
    return fromJson(*j, error);
}

}  // namespace lim::world
