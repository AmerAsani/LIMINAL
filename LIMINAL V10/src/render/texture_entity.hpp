// V10: Materialtexturen der Wesen - eigene Texturebenen mit eigener Aufloesung.
//
// Jede Ebene deckt je nach Material 0,12 bis 0,8 m ab (so wiederholt sich auf einem Koerper kaum etwas;
// das Fluegelmuster wiederholt sich gar nicht) und traegt die echte Farbe, Hoehe (-> Normalen), Rauheit und
// Hohlraumverdeckung. Dazu kommen feine Detailnormalen (Mikroebenen, ~8 cm), die aus der Naehe
// Poren, Fasern, Haarrisse und Schuppen zeigen. Aufloesung nach Qualitaet: 1024 / 2048 / 4096 (Ultra).
// Die grosse Fassung entsteht im Hintergrund; bis dahin ist die kleinere zu sehen (Streaming).
//
//   Haut      der Hound: fahle, kraenkliche Haut mit Raeude (kahle, rohe Flecken, Schorf),
//             Kratzspuren, Stoppeln und Adern
//   Haar      der Hound: schwarze, straehnige Haare in Buendeln, einzelne graue
//   Horn      Krallen: Wachstumsrillen, Absplitterungen
//   Zahn      Zahnschmelz: vergilbt, Flecken, feine Risse (auch die leuchtenden Zaehne des Smilers)
//   Fluegel   die Deathmoth: das ganze Fluegelmuster als Atlas der Ruhepose (render/moth_wing.hpp) -
//             graues, grobes Haar, Adern, wellige Querbinden, Mittelfleck, Saum, abgeriebene Stellen
//   Panzer    die Brust der Deathmoth: dunkelbraun, gefleckt, unregelmaessig (Fandom)
//   Pelz      der Hinterleib der Deathmoth: feines graues Haar
//   Nest      Deathmoth-Nest: fleischig, geadert, mit Harz ueberzogen, Einschluesse
#pragma once

#include <vector>

#include "core/core.hpp"

namespace lim {
class JobSystem;
}

namespace lim::gfx {

enum EntLayer : u8 { EL_SKIN, EL_HAIR, EL_HORN, EL_TOOTH, EL_WING, EL_CHITIN, EL_FUR, EL_NEST, EL_COUNT };
enum MicroLayer : u8 { ML_PORES, ML_FIBER, ML_CRAZE, ML_SCALE, ML_COUNT };

struct EntLayerDesc {
    const char* name;
    float meters;          // Kantenlaenge der Kachel
    float normalStrength;
};
extern const EntLayerDesc kEntLayers[EL_COUNT];
extern const EntLayerDesc kMicroLayers[ML_COUNT];
// Plaetze der Ebenen-Konstanten (gLayer im Shader) hinter den Weltebenen
constexpr int kEntLayerSlot = 40, kMicroLayerSlot = 48;

struct EntityTextures {
    int size = 0, mips = 0;
    std::vector<u8> albedo;  // BC1 (sRGB), Ebene fuer Ebene, je Ebene alle Mips
    std::vector<u8> normal;  // BC5 (x, y)
    std::vector<u8> orm;     // BC5 (Verdeckung, Rauheit)
    int microSize = 0, microMips = 0;
    std::vector<u8> micro;   // BC5 (x, y)
    double seconds = 0.0;
    size_t offset(int size, int mips, int layer, int mip, int blockBytes) const;
};

EntityTextures generateEntityTextures(int size, JobSystem& jobs);

}  // namespace lim::gfx
