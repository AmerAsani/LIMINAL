// Spielstaende: mehrere Slots mit Name, Schwierigkeit und Vorschaubild.
//
// Ablage: %APPDATA%\LIMINAL\saves_v6\slot_<id>.json. Geschrieben wird atomar
// (temporaere Datei + Umbenennen); die vorige Fassung bleibt als .bak erhalten
// und wird bei Beschaedigung geladen. Die Welt selbst entsteht aus dem Seed.
//
// Spielstaende aus V5 (saves_v5), V4 (saves_v4) und V3 (save_v3.json) erscheinen
// automatisch in der Liste. Sie werden nur gelesen; beim Speichern entsteht ein
// neuer V6-Slot. Die Dateien aelterer Versionen werden nie veraendert.
#pragma once

#include <string>
#include <vector>

#include "core/json.hpp"

namespace lim::game {

constexpr int kSaveVersion = 6;

struct SaveEntry {
    std::string slot;
    std::string path;
    int version = kSaveVersion;
    Json data;
    std::vector<unsigned char> thumb;  // RGBA
    int thumbW = 0, thumbH = 0;
    bool legacy() const { return version < kSaveVersion; }
};

namespace savegame {

std::string saveDir();
std::string newSlotId();
std::vector<SaveEntry> list();
bool load(const std::string& path, SaveEntry& out);
// data: vollstaendiger Spielstand (JSON); thumbRgba optional
bool write(const std::string& slot, Json data, const std::vector<unsigned char>& thumbRgba, int tw, int th);
bool remove(const SaveEntry& e);

}  // namespace savegame
}  // namespace lim::game
