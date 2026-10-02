#include "gameplay/savegame.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>

#include "core/base64.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "platform/image_io.hpp"

namespace lim::game::savegame {

std::string saveDir() { return paths::join(paths::userDataDir(), "saves_v7"); }

static std::string timestamp(const char* fmt) {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[64];
    std::strftime(buf, sizeof(buf), fmt, &tm);
    return buf;
}

// Eindeutig, auch wenn zwei Spiele in derselben Sekunde angelegt werden
std::string newSlotId() {
    std::string base = timestamp("%Y%m%d_%H%M%S"), id = base;
    for (int n = 2; paths::exists(paths::join(saveDir(), "slot_" + id + ".json")); ++n) id = std::format("{}_{}", base, n);
    return id;
}

// Nur Buchstaben, Ziffern, '_' und '-': die Kennung wird Teil eines Dateinamens
static bool validSlot(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

// "…\slot_<id>.json" -> "<id>"
static std::string slotFromPath(const std::string& path) {
    std::string f = paths::fileName(path);
    if (f.rfind("slot_", 0) == 0) f = f.substr(5);
    if (f.size() > 5 && f.compare(f.size() - 5, 5, ".json") == 0) f.resize(f.size() - 5);
    return f;
}

// V4-Vorschaubild: 32 x 16 Farbcodes mit je 5 Bit R/G/B
static void convertV4Thumb(const Json& thumb, SaveEntry& e) {
    if (thumb.size() != 32 * 16) return;
    e.thumbW = 32;
    e.thumbH = 16;
    e.thumb.resize(32 * 16 * 4);
    for (size_t i = 0; i < 32 * 16; ++i) {
        long long ck = thumb[i].asInt();
        e.thumb[i * 4] = (unsigned char)(((ck >> 10) & 31) * 255 / 31);
        e.thumb[i * 4 + 1] = (unsigned char)(((ck >> 5) & 31) * 255 / 31);
        e.thumb[i * 4 + 2] = (unsigned char)((ck & 31) * 255 / 31);
        e.thumb[i * 4 + 3] = 255;
    }
}

bool load(const std::string& path, SaveEntry& out) {
    for (const std::string& cand : {path, path + ".bak"}) {
        auto j = loadJsonFile(cand);
        if (!j || !j->has("seed") || !j->has("player")) continue;
        int v = (int)(*j)["version"].asInt(3);
        if (v < 3 || v > kSaveVersion) continue;
        out.path = path;
        out.version = v;
        out.data = std::move(*j);
        Json& d = out.data;
        if (!d.has("name")) d.set("name", v == 3 ? "Spielstand aus V3" : "Wanderer");
        if (!d.has("difficulty")) d.set("difficulty", "medium");
        if (!d.has("level")) d.set("level", "level0");
        out.slot = v == 3 ? std::string("v3") : d["slot"].asString("");
        if (!validSlot(out.slot)) out.slot = slotFromPath(path);
        if (!validSlot(out.slot)) out.slot = newSlotId();
        d.set("slot", out.slot);
        if (v >= 5) {
            auto png = base64Decode(d["thumb_png"].asString(""));
            if (!png.empty()) gfx::decodePng(png.data(), png.size(), out.thumb, out.thumbW, out.thumbH);
        } else {
            convertV4Thumb(d["thumb"], out);
        }
        return true;
    }
    return false;
}

std::vector<SaveEntry> list() {
    std::vector<SaveEntry> out;
    for (const auto& f : paths::listFiles(saveDir(), "slot_", ".json")) {
        SaveEntry e;
        if (load(f, e)) out.push_back(std::move(e));
    }
    // V6-, V5- und V4-Spielstaende (nur lesen; beim Speichern als V7-Stand fortgefuehrt)
    std::string base = paths::userDataDir();
    for (const auto& f : paths::listFiles(paths::join(base, "saves_v6"), "slot_", ".json")) {
        SaveEntry e;
        if (load(f, e) && e.version == 6) {
            e.data.set("name", e.data["name"].asString("Wanderer") + "  (V6)");
            out.push_back(std::move(e));
        }
    }
    for (const auto& f : paths::listFiles(paths::join(base, "saves_v5"), "slot_", ".json")) {
        SaveEntry e;
        if (load(f, e) && e.version == 5) {
            e.data.set("name", e.data["name"].asString("Wanderer") + "  (V5)");
            out.push_back(std::move(e));
        }
    }
    for (const auto& f : paths::listFiles(paths::join(base, "saves_v4"), "slot_", ".json")) {
        SaveEntry e;
        if (load(f, e)) {
            e.data.set("name", e.data["name"].asString("Wanderer") + "  (V4)");
            out.push_back(std::move(e));
        }
    }
    std::string v3 = paths::join(base, "save_v3.json");
    if (paths::exists(v3)) {
        SaveEntry e;
        if (load(v3, e)) out.push_back(std::move(e));
    }
    std::sort(out.begin(), out.end(), [](const SaveEntry& a, const SaveEntry& b) {
        return a.data["saved_at"].asString("") > b.data["saved_at"].asString("");
    });
    return out;
}

bool write(const std::string& slot, Json data, const std::vector<unsigned char>& thumb, int tw, int th) {
    data.set("version", kSaveVersion);
    data.set("slot", slot);
    data.set("saved_at", timestamp("%Y-%m-%d %H:%M:%S"));
    if (!thumb.empty()) {
        std::vector<unsigned char> png;
        if (gfx::encodePng(thumb.data(), tw, th, png)) data.set("thumb_png", base64Encode(png.data(), png.size()));
    }
    std::string path = paths::join(saveDir(), "slot_" + slot + ".json");
    bool ok = paths::writeFileAtomic(path, data.dump(false), true);
    if (!ok) log::error("Spielstand konnte nicht geschrieben werden: {}", path);
    return ok;
}

bool remove(const SaveEntry& e) {
    if (e.legacy()) return false;  // alte Staende aus V3/V4/V5/V6 bleiben unangetastet
    paths::removeFile(e.path);
    paths::removeFile(e.path + ".bak");
    return true;
}

}  // namespace lim::game::savegame
