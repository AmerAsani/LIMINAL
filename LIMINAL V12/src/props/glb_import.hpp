// V12: Modelle aus Blender (glTF 2.0, binaer .glb) als Ausstattung.
//
// Eigener, kleiner Leser ohne fremden Code: Netze (Positionen, Normalen, optional UV), Indizes, Knoten-
// Transformationen und Materialien. glTF ist Y-oben und rechtshaendig, das Spiel Z-oben und linkshaendig
// (x vorn, y rechts); die Umrechnung spiegelt y, damit ein Modell im Spiel so aussieht wie in Blender.
//
// Materialnamen aus Blender steuern das Aussehen: Woerter "look=<art>", "tex=<name>", "glow=<staerke>",
// getrennt durch Leerzeichen oder Semikolon. <art> wie props::Look (paint, metal, wood, concrete, glass,
// glow ...), <name> eine Textur aus data/textures/textures.json. Die Farbe kommt aus baseColorFactor.
#pragma once

#include <string>
#include <vector>

#include "props/prop_library.hpp"

namespace lim::props {

// Liest eine .glb-Datei in ein Modell; false mit Fehlermeldung
bool loadGlb(const std::string& path, Model& out, std::string& err);
// Alle .glb in einem Ordner einlesen und unter ihrem Dateinamen (ohne Endung) als Modell anmelden.
// Gibt die Zahl der Modelle zurueck; fehlerhafte Dateien werden protokolliert und uebersprungen.
int loadModelDirectory(const std::string& dir);

}  // namespace lim::props
