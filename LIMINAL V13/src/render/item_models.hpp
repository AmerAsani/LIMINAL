// V13.3: Fundstuecke und Interaktionsobjekte aus Blender (tools/blender/models/models_items.py ->
// data/models/item_*.glb). Liegt eine Datei vor, ersetzt sie das Netz aus Code (render/meshes.cpp) unter
// demselben Namen: energy, almond, battery, note, fuse, fusebox, fusebox_empty, vending, exit_door,
// elev_frame, elev_door. Fehlt sie, bleibt das alte Netz (wie bei der Taschenlampe, MeshLibrary::addViewmodel).
//
// Materialien: Teile mit "fm=<name>" bekommen das feste Material des Renderers (Atlas der Etiketten,
// Automatenfront, Leuchtschild - die UV aus Blender sind Atlas-Koordinaten). Alle anderen Teile beschreiben
// ihr Aussehen wie die Ausstattung (look, tex, r, m); dafuer werden freie feste Materialplaetze ab
// FM_FIG_END belegt, und ihre UV (Meter, Kastenprojektion aus Blender) werden auf die Kachelgroesse der
// Textur umgerechnet.
#pragma once

#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"
#include "render/gpu_types.hpp"

namespace lim::gfx {

class Gpu;
class MeshLibrary;

// Traegt die Materialien der Fundstueck-Modelle in die Tabelle der festen Materialien ein (WorldRenderer::init)
void itemModelMaterials(std::vector<GpuMaterial>& table);
// Meldet die Fundstueck-Modelle in der Netzbibliothek an (MeshLibrary::init)
void addItemModels(MeshLibrary& lib, Gpu& gpu);

}  // namespace lim::gfx
