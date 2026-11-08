#include "world/imported_textures.hpp"

#include <format>

#include "core/json.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim::world {

namespace {
std::vector<ImportedTexture> gTextures;
std::string gLoadedFrom;
}  // namespace

bool loadImportedTextures(const std::string& dataDir, std::string& err) {
    if (gLoadedFrom == dataDir) return true;
    gTextures.clear();
    const std::string base = paths::join(dataDir, "textures");
    auto j = loadJsonFile(paths::join(base, "textures.json"));
    if (!j) {
        // ohne Datei: keine eingelesenen Texturen (aeltere Datenstaende, Tests)
        gLoadedFrom = dataDir;
        return true;
    }
    for (const Json& t : (*j)["textures"].items()) {
        if ((int)gTextures.size() >= kImportedLayerCount) {
            err = std::format("textures.json: mehr als {} Texturen", kImportedLayerCount);
            return false;
        }
        ImportedTexture it;
        it.name = t["name"].asString("");
        it.dir = paths::join(base, t["dir"].asString(it.name));
        it.source = t["source"].asString("");
        it.meters = (float)t["meters"].asNumber(2.0);
        it.normalStrength = (float)t["normal_strength"].asNumber(1.0);
        it.roughBias = (float)t["rough_bias"].asNumber(0.0);
        it.variation = (float)t["variation"].asNumber(0.25);
        // V13.3 (Abnahme Level 36): 2 = Kachelbruch mit schmalem Uebergang (koernige Boeden wie Terrazzo ohne Doppelbild)
        const double at = t["antitile"].asNumber(0.0);
        it.antitile = at > 1.5 ? 2.0f : (at > 0.5 ? 1.0f : 0.0f);
        if (it.name.empty() || it.meters <= 0.0f) {
            err = "textures.json: Eintrag ohne Namen oder Groesse";
            return false;
        }
        gTextures.push_back(std::move(it));
    }
    gLoadedFrom = dataDir;
    log::info("Texturen aus Bildern: {} (CC0, LICENSES.md)", gTextures.size());
    return true;
}

const std::vector<ImportedTexture>& importedTextures() { return gTextures; }

int importedLayer(const std::string& name) {
    for (size_t i = 0; i < gTextures.size(); ++i)
        if (gTextures[i].name == name) return kImportedLayerBase + (int)i;
    return -1;
}

}  // namespace lim::world
